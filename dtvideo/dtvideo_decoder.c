#include "dt_setting.h"

#include "dtvideo.h"
#include "dtvideo_decoder.h"

//#define DTVIDEO_DECODER_DUMP 0

#define TAG "VIDEO-DEC"

#define REGISTER_VDEC(X,x)      \
    {                           \
        extern vd_wrapper_t vd_##x##_ops;   \
        register_vdec(&vd_##x##_ops);   \
    }

//pts calc mode,
//0: calc with pts read from demuxer
//1: calc with duration
static int pts_mode = 0; //pts calc mode, 0: calc with pts 1: calc with duration

static vd_wrapper_t *g_vd = NULL;

static void register_vdec(vd_wrapper_t * vd)
{
    vd_wrapper_t **p;
    p = &g_vd;
    while (*p != NULL) {
        p = &(*p)->next;
    }
    *p = vd;
    dt_info(TAG, "[%s:%d] register vdec, name:%s fmt:%d \n", __FUNCTION__, __LINE__,
            (*p)->name, (*p)->vfmt);
    vd->next = NULL;
}

void register_vdec_ext(vd_wrapper_t *vd)
{
    vd_wrapper_t **p;

    p = &g_vd;
    while (1) {
        if (*p == NULL) {
            break;
        }

        dt_info(TAG, "[%s:%d] register vdec: %s comp:%s \n", __FUNCTION__, __LINE__,
                (*p)->name, vd->name);
        if (strstr((*p)->name, vd->name) != NULL) {
            dt_info(TAG, "[%s:%d] vdec already registerd, name:%s fmt:%d \n", __FUNCTION__,
                    __LINE__, (*p)->name, (*p)->vfmt);
            return;
        }
        p = &(*p)->next;
    }

    p = &g_vd;
    if (*p == NULL) {
        *p = vd;
        vd->next = NULL;
    } else {
        vd->next = *p;
        *p = vd;
    }

    dt_info(TAG, "[%s:%d]register ext vd. vfmt:%d name:%s \n", __FUNCTION__,
            __LINE__, vd->vfmt, vd->name);

}

void vdec_register_all()
{
    //comments:video using ffmpeg decoder only
#ifdef ENABLE_VDEC_FFMPEG
    REGISTER_VDEC(FFMPEG, ffmpeg);
#endif
    return;
}

void vdec_remove_all()
{
    g_vd = NULL;
    dt_info(TAG, "[%s:%d]remove all vd \n", __FUNCTION__, __LINE__);
}

static int select_video_decoder(dtvideo_decoder_t * decoder)
{
    vd_wrapper_t **p;
    dtvideo_para_t *para = &decoder->para;
    p = &g_vd;
    while (*p != NULL) {
        if ((*p)->vfmt == DT_VIDEO_FORMAT_UNKOWN) {
            break;
        }
        if ((*p)->vfmt == para->vfmt) {
            if ((*p)->is_hw == 1 && (para->flag & DTAV_FLAG_DISABLE_HW_CODEC) > 0) {
                dt_info(TAG, "[%s:%d]disable-  hw name:%s flag:%d \n", __FUNCTION__, __LINE__,
                        (*p)->name, para->flag);
                p = &(*p)->next;
                continue;
            }
            break;
        }
        p = &(*p)->next;
    }
    if (*p == NULL) {
        dt_info(TAG, "[%s:%d]no valid video decoder found vfmt:%d\n", __FUNCTION__,
                __LINE__, para->vfmt);
        return -1;
    }
    decoder->wrapper = *p;
    dt_info(TAG, "[%s:%d] select--%s video decoder \n", __FUNCTION__, __LINE__,
            (*p)->name);
    return 0;
}

static int64_t pts_exchange(dtvideo_decoder_t * decoder, int64_t pts)
{
    int num = decoder->para.num;
    int den = decoder->para.den;
    double exchange = DT_PTS_FREQ * ((double)num / (double)den);
    int64_t result = DT_NOPTS_VALUE;
    if (PTS_VALID(pts)) {
        result = (int64_t)(pts * exchange);
    } else {
        result = pts;
    }
    return result;
}

static void *video_decode_loop(void *arg)
{
    dt_av_pkt_t *frame = NULL;
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *) arg;
    vd_wrapper_t *wrapper = decoder->wrapper;
    vd_statistics_info_t *p_vd_statistics_info = &decoder->statistics_info;
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    queue_t *picture_queue = vctx->vo_queue;
    /*used for decode */
    dt_av_frame_t *picture = NULL;
    int ret;
    dt_info(TAG, "[%s:%d] start decode loop.\n", __FUNCTION__, __LINE__);
    int video_frame_in = 0;

    int64_t drop_timeout = 3 * 1000;
    int64_t drop_start = -1;
    int drop_done = 0;
    int need_drop = -1;
    int64_t first_apts = -1;
    int64_t first_vpts = -1;
    int packet_pending = 0;

    do {
        //exit check before idle, maybe recieve exit cmd in idle status
        if (decoder->status == VDEC_STATUS_EXIT) {
            dt_info(TAG, "[%s:%d] receive decode loop exit cmd \n", __FUNCTION__, __LINE__);
            break;
        }

        if (decoder->status == VDEC_STATUS_IDLE) {
            dt_info(TAG, "[%s:%d] Idle status ,please wait \n", __FUNCTION__, __LINE__);
            usleep(10 * 1000);
            continue;
        }

        /*read frame */
        if (!decoder->parent) {
            usleep(10 * 1000);
            continue;
        }

        if (picture_queue->length >= VIDEO_OUT_MAX_COUNT) {
            //vo queue full
            usleep(10 * 1000);
            continue;
        }
        ret = 0;
        if (packet_pending == 1) {
            goto send_packet;
        }
        ret = dtvideo_read_frame(decoder->parent, &frame);
        if (ret >= 0 && frame->data != NULL) {
            packet_pending = 1;
        } else {
            dt_usleep(10 * 1000);
            if (decoder->pts_first == DT_NOPTS_VALUE) {
                continue;
            }
            //no data left, maybe eof, need to flush left data
            memset(frame, 0, sizeof(dt_av_pkt_t));
            dt_info(TAG, "[%s:%d] no video frame left, flush left frames \n", __FUNCTION__,
                    __LINE__);
            goto recv_frame;
        }

send_packet:
        ret = wrapper->send_packet(decoder, frame);
        if (ret >= 0) {
            packet_pending = 0;
            video_frame_in++;
            //we successfully send one frame
            dtp_packet_free(frame);
        }
recv_frame:
        /*read one frame,enter decode frame module */
        //will exec once for one time
        ret = wrapper->receive_frame(decoder, &picture);
        if (ret <= 0) {
            decoder->decode_err_cnt++;
            dt_info(TAG, "[%s:%d]decode failed. ret:%d.\n", __FUNCTION__, __LINE__, ret);
            picture = NULL;
            usleep(10000);
            continue;
        }
        if (!picture) {
            continue;
        }

        // statistics collection
        {
            p_vd_statistics_info->decoded_frame_count++;
            p_vd_statistics_info->last_decode_ms = dt_gettime() / 1000;
        }
        decoder->frame_count++;
        //Got one frame
        //update current pts, clear the buffer size
        if (PTS_VALID(picture->pts)) {
            picture->pts = pts_exchange(decoder, picture->pts);
        }
        if (decoder->first_frame_decoded == 0 && PTS_INVALID(decoder->pts_first)) {
            decoder->pts_first = decoder->pts_current = picture->pts;
            decoder->first_frame_decoded = 1;
            first_vpts = picture->pts;
            video_host_ioctl(decoder->parent, HOST_CMD_SET_FIRST_VPTS,
                             (unsigned long)(&first_vpts));
            dt_info(TAG,
                    "[%s:%d]first frame decoded ok, pts:0x%llx dts:0x%llx used:%d frames\n",
                    __FUNCTION__, __LINE__, picture->pts, picture->dts, video_frame_in);
        } else {
            if (pts_mode || PTS_INVALID(picture->pts)) {
                int fps = decoder->para.fps;
                int dur_inc = (int)((double)90000 / fps);
                picture->pts = decoder->pts_current + dur_inc;
                decoder->pts_current = picture->pts;
                dt_debug(TAG, "vpts inc itself. pts_mode:%d pts:0x%llx inc:%d\n", pts_mode,
                         picture->pts, dur_inc);
            }
        }

        // drop video
        while (drop_done == 0) {

            if (decoder->status == VDEC_STATUS_EXIT) {
                break;
            }

            if (drop_start < 0) {
                drop_start = dt_gettime();
            }

            if (dt_gettime() - drop_start >= drop_timeout * 1000) {
                video_host_ioctl(decoder->parent, HOST_CMD_SET_DROP_DONE,
                                 (unsigned long)(&drop_done));
                drop_done = 1;
                break;
            }

            if (PTS_INVALID(first_apts)) {
                video_host_ioctl(decoder->parent, HOST_CMD_GET_FIRST_APTS,
                                 (unsigned long)(&first_apts));
                video_host_ioctl(decoder->parent, HOST_CMD_GET_DROP_DONE,
                                 (unsigned long)(&drop_done));
                if (PTS_INVALID(first_apts)) {
                    usleep(10 * 1000);
                    dt_info(TAG, "wait first audio decoded.\n");
                    continue;
                }

                // get first vpts
                if (first_vpts > first_apts) {
                    need_drop = 0;
                } else {
                    // calc drop size
                    int64_t diff = first_apts - first_vpts;
                    if (diff / 90 > AVSYNC_DROP_THRESHOLD || diff / 90 <= AVSYNC_THRESHOLD) {
                        video_host_ioctl(decoder->parent, HOST_CMD_SET_DROP_DONE,
                                         (unsigned long)(&drop_done));
                        drop_done = 1;
                        dt_info(TAG, "no need drop. first_apts:%lld first_vpts:%lld diff:%d \n",
                                first_apts, first_vpts, (int)diff / 90);
                        continue;
                    }
                    need_drop = 1;
                }

                dt_info(TAG, "get first vpts. first_apts:%lld first_vpts:%lld need_drop:%d \n",
                        first_apts, first_vpts, need_drop);
                continue;
            }

            video_host_ioctl(decoder->parent, HOST_CMD_GET_DROP_DONE,
                             (unsigned long)(&drop_done));
            if (need_drop == 0) {
                dt_info(TAG, "wait audio drop.\n");
                usleep(10 * 1000);
                continue;
            }

            // drop video
            if (picture->pts >= first_apts) {
                video_host_ioctl(decoder->parent, HOST_CMD_SET_DROP_DONE,
                                 (unsigned long)(&drop_done));
                drop_done = 1;
                dt_info(TAG, "drop finished.\n");
            }
            break;
        }

        /*queue in */
        queue_push_tail(picture_queue, picture);
        picture = NULL;
    } while (1);
    dt_info(TAG, "[file:%s][%s:%d]decoder loop thread exit ok\n", __FILE__,
            __FUNCTION__, __LINE__);

    pthread_exit(NULL);
    return NULL;
}

int video_decoder_init(dtvideo_decoder_t * decoder)
{
    int ret = 0;
    pthread_t tid;
    /*select decoder */
    ret = select_video_decoder(decoder);
    if (ret < 0) {
        return -1;
    }

    vd_wrapper_t *wrapper = decoder->wrapper;
    /*init decoder */
    decoder->pts_current = decoder->pts_first = DT_NOPTS_VALUE;
    decoder->vd_priv = decoder->para.avctx_priv;
    ret = wrapper->init(decoder);
    if (ret < 0) {
        return -1;
    }
    pts_mode = dtp_setting.video_pts_mode;
    dt_info(TAG, "pts mode:%d fps:%f \n", pts_mode, decoder->para.fps);

    dt_info(TAG, "[%s:%d] video decoder init ok\n", __FUNCTION__, __LINE__);
    /*init pcm buffer */
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    vctx->vo_queue = queue_new();
    queue_t *picture_queue = vctx->vo_queue;
    if (NULL == picture_queue) {
        dt_error(TAG, "create video out queue failed\n");
        return -1;
    }
    /*create thread */
    ret = pthread_create(&tid, NULL, video_decode_loop, (void *) decoder);
    if (ret != 0) {
        dt_error(TAG, "create audio decoder thread failed\n");
        return ret;
    }
    decoder->video_decoder_pid = tid;
    video_decoder_start(decoder);
    //decoder->status=ADEC_STATUS_RUNNING;//start decode after init
    return 0;
}

int video_decoder_start(dtvideo_decoder_t * decoder)
{
    decoder->status = VDEC_STATUS_RUNNING;
    return 0;
}

static void dump_vd_statistics_info(dtvideo_decoder_t * decoder)
{
    vd_statistics_info_t *p_info = &decoder->statistics_info;
    dt_info(TAG, "==============vd statistics info============== \n");
    dt_info(TAG, "DecodedVideoFrameCount: %d\n", p_info->decoded_frame_count);
    dt_info(TAG, "=================================================== \n");
}

static void dtav_clear_frame(void *pic)
{
    dt_av_frame_t *picture = (dt_av_frame_t *)(pic);
    dtp_frame_unref(picture, 0);
    return;
}

int video_decoder_stop(dtvideo_decoder_t * decoder)
{
    vd_wrapper_t *wrapper = decoder->wrapper;

    dump_vd_statistics_info(decoder);
    /*Decode thread exit */
    decoder->status = VDEC_STATUS_EXIT;
    pthread_join(decoder->video_decoder_pid, NULL);
    /*release queue */
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    queue_t *picture_queue = vctx->vo_queue;
    if (picture_queue) {
        queue_free(picture_queue, (free_func)dtav_clear_frame);
        picture_queue = NULL;
    }
    wrapper->release(decoder);
    return 0;
}
