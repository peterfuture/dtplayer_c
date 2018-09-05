#include "dt_setting.h"

#include "dtvideo.h"
#include "dthost_api.h"

#define TAG "VIDEO-MGT"

/*read frame from dtport*/
int dtvideo_read_frame(void *priv, dt_av_pkt_t **frame)
{
    int type = DTP_MEDIA_TYPE_VIDEO;
    int ret = 0;
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    ret = dthost_read_frame(vctx->parent, frame, type);
    return ret;
}

/*get picture from vo_queue,remove*/
dt_av_frame_t *dtvideo_output_read(void *priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    queue_t *picture_queue = vctx->vo_queue;
    if (picture_queue->length == 0) {
        return NULL;
    }
    return queue_pop_head(picture_queue);
}

/*pre get picture from vo_queue, not remove*/
dt_av_frame_t *dtvideo_output_pre_read(void *priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    queue_t *picture_queue = vctx->vo_queue;
    if (picture_queue->length == 0) {
        return NULL;
    }
    return queue_pre_pop_head(picture_queue);
}

int dtvideo_get_avdiff(void *priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    int avdiff = 0;
    dthost_get_info(vctx->parent, HOST_CMD_GET_AVDIFF, (unsigned long)(&avdiff));
    return avdiff;
}

int64_t dtvideo_get_current_pts(dtvideo_context_t * vctx)
{
    uint64_t pts;
    if (vctx->video_status <= VIDEO_STATUS_INITED) {
        return -1;
    }
    pts = vctx->current_pts;
    return pts;
}

int video_first_frame_decoded(dtvideo_context_t * vctx)
{
    return vctx->video_dec.first_frame_decoded;
}

int64_t video_get_first_pts(dtvideo_context_t * vctx)
{
    if (vctx->video_status != VIDEO_STATUS_INITED) {
        return DT_NOPTS_VALUE;
    }
    dt_debug(TAG, "fitst vpts:%llx \n", vctx->video_dec.pts_first);
    return vctx->video_dec.pts_first;
}

int video_host_ioctl(void *priv, int cmd, unsigned long arg)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    if (!priv) {
        return 0;
    }
    switch (cmd) {
    case HOST_CMD_GET_FIRST_APTS:
    case HOST_CMD_GET_DROP_DONE:
        return dthost_get_info(vctx->parent, cmd, arg);
    case HOST_CMD_SET_FIRST_VPTS:
    case HOST_CMD_SET_DROP_DONE:
        return dthost_set_info(vctx->parent, cmd, arg);
    default:
        break;
    }

    return 0;
}

int video_drop(dtvideo_context_t * vctx, int64_t target_pts)
{
    int64_t diff = target_pts - video_get_first_pts(vctx);
    int diff_time = diff / 90;
    if (diff_time > AVSYNC_DROP_THRESHOLD) {
        dt_info(TAG, "diff time exceed %d ms, do not drop!\n", diff_time);
        return 0;
    }
    dt_info(TAG, "[%s:%d]target pts:%lld \n", __FUNCTION__, __LINE__, target_pts);
    dt_av_frame_t *pic = NULL;
    int64_t cur_pts = video_get_first_pts(vctx);
    int drop_count = 300;
    int64_t drop_start = dt_gettime();
    int64_t drop_timeout = dtp_setting.host_drop_timeout;
    do {
        if ((dt_gettime() - drop_start) / 1000 >= drop_timeout) {
            dt_info(TAG, "video drop timeout: %lld\n", drop_timeout);
            break;
        }
        pic = dtvideo_output_read((void *) vctx);
        if (!pic) {
            if (drop_count-- == 0) {
                dt_info(TAG, "3s read nothing, quit drop video\n");
                break;
            }
            usleep(100);
            continue;
        }
        drop_count = 300;
        cur_pts = pic->pts;
        dt_debug(TAG, "read pts:%lld \n", pic->pts);
        dtp_frame_free(pic, 0);
        if (cur_pts > target_pts) {
            break;
        }
    } while (1);
    dt_info(TAG, "video drop finish,drop count:%d \n", drop_count);
    vctx->current_pts = cur_pts;
    dtvideo_update_pts((void *) vctx);
    return 0;
}

int64_t dtvideo_get_systime(void *priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    if (vctx->video_status <= VIDEO_STATUS_INITED) {
        return -1;
    }
    int64_t systime = 0;
    dthost_get_info(vctx->parent, HOST_CMD_GET_SYSTIME, (unsigned long)(&systime));
    return systime;
}

void dtvideo_update_systime(void *priv, int64_t sys_time)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    if (vctx->video_status <= VIDEO_STATUS_INITED) {
        return;
    }
    dthost_set_info(vctx->parent, HOST_CMD_SET_SYSTIME, (unsigned long)(&sys_time));
    return;
}

void dtvideo_update_pts(void *priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) priv;
    if (vctx->video_status < VIDEO_STATUS_INITED) {
        return;
    }
    dthost_set_info(vctx->parent, HOST_CMD_SET_VPTS,
                    (unsigned long)(&vctx->current_pts));
    return;
}

int video_get_dec_state(dtvideo_context_t * vctx, dec_state_t * dec_state)
{
    if (vctx->video_status < VIDEO_STATUS_INITED) {
        return -1;
    }
    dtvideo_decoder_t *vdec = &vctx->video_dec;
    dec_state->vdec_error_count = vdec->decode_err_cnt;
    dec_state->vdec_fps = vdec->para.rate;
    dec_state->vdec_width = vdec->para.d_width;
    dec_state->vdec_height = vdec->para.d_height;
    dec_state->vdec_status = vdec->status;
    dec_state->vdec_last_ms = vdec->statistics_info.last_decode_ms;
    dec_state->vdec_type = vdec->type;
    return 0;
}

//reserve 10 frame
//if low level just return
int video_get_out_closed(dtvideo_context_t * vctx)
{
    return 1;
}

void video_register_all()
{
    vdec_register_all();
    vout_register_all();
    vf_register_all();
}

void video_remove_all()
{
    vdec_remove_all();
    vout_remove_all();
    vf_remove_all();
}

void register_ext_vd(vd_wrapper_t *vd)
{
    register_vdec_ext(vd);
}

void register_ext_vo(vo_wrapper_t *vo)
{
    vout_register_ext(vo);
}

int video_start(dtvideo_context_t * vctx)
{
    if (vctx->video_status == VIDEO_STATUS_INITED) {
        dtvideo_output_t *video_out = &vctx->video_out;
        video_output_start(video_out);
        vctx->video_status = VIDEO_STATUS_ACTIVE;
    } else {
        dt_error(TAG, "[%s:%d]video output start failed \n", __FUNCTION__, __LINE__);
    }
    return 0;
}

int video_pause(dtvideo_context_t * vctx)
{
    if (vctx->video_status == VIDEO_STATUS_ACTIVE) {
        dtvideo_output_t *video_out = &vctx->video_out;
        video_output_pause(video_out);
        vctx->video_status = VIDEO_STATUS_PAUSED;
    }
    return 0;
}

int video_resume(dtvideo_context_t * vctx)
{
    if (vctx->video_status == VIDEO_STATUS_PAUSED) {
        dtvideo_output_t *video_out = &vctx->video_out;
        video_output_resume(video_out);
        vctx->video_status = VIDEO_STATUS_ACTIVE;
        return 0;
    }
    return -1;
}

int video_stop(dtvideo_context_t * vctx)
{
    if (vctx->video_status >= VIDEO_STATUS_INITED) {
        dtvideo_output_t *video_out = &vctx->video_out;
        video_output_stop(video_out);
        dtvideo_decoder_t *video_decoder = &vctx->video_dec;
        video_decoder_stop(video_decoder);
        vctx->video_status = VIDEO_STATUS_STOPPED;
    }
    return 0;
}

int video_init(dtvideo_context_t * vctx)
{
    int ret = 0;
    dt_info(TAG, "[%s:%d] dtvideo_mgt start init\n", __FUNCTION__, __LINE__);
    //call init
    vctx->video_status = VIDEO_STATUS_INITING;
    vctx->current_pts = vctx->last_valid_pts = -1;

    dtvideo_decoder_t *video_dec = &vctx->video_dec;
    dtvideo_output_t *video_out = &vctx->video_out;

    //vd ctx init
    memset(video_dec, 0, sizeof(dtvideo_decoder_t));
    memcpy(&video_dec->para, &vctx->video_para, sizeof(dtvideo_para_t));
    video_dec->parent = vctx;
    ret = video_decoder_init(video_dec);
    if (ret < 0) {
        goto err1;
    }
    dt_info(TAG, "[%s:%d] vdecoder init ok\n", __FUNCTION__, __LINE__);

    //vo ctx init
    memset(video_out, 0, sizeof(dtvideo_output_t));
    memcpy(&video_out->para, &vctx->video_para, sizeof(dtvideo_para_t));
    video_out->parent = vctx;
    ret = video_output_init(video_out, vctx->video_para.video_output);
    if (ret < 0) {
        goto err2;
    }

    vctx->video_status = VIDEO_STATUS_INITED;
    dt_info(TAG, "dtvideo init ok,status:%d \n", vctx->video_status);
    return 0;
err1:
    dt_info(TAG, "[%s:%d]video decoder init failed \n", __FUNCTION__, __LINE__);
    return -1;
err2:
    video_decoder_stop(video_dec);
    dt_info(TAG, "[%s:%d]video output init failed \n", __FUNCTION__, __LINE__);
    return -3;
}

int video_reset(dtvideo_context_t *vctx)
{

    dtvideo_output_t *video_out = &vctx->video_out;

    video_output_stop(video_out);
    video_output_init(video_out, vctx->video_para.video_output);
    video_output_start(video_out);

    return 0;
}

int video_resize(dtvideo_context_t * vctx, int w, int h)
{
    dtvideo_para_t *para = &vctx->video_para;

    if (para->d_width == w && para->d_height == h) {
        dt_error(TAG, "size equal to current \n");
        return -1;
    }

    para->d_width = w;
    para->d_height = h;
    video_reset(vctx);
    return 0;
}
