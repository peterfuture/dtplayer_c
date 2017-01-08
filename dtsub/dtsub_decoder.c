/***********************************************************************
**
**  Module : dtsub_decoder.c
**  Summary: sub decoder module
**  Section: dtsub
**  Author : peter
**  Notes  :
**
***********************************************************************/

#include "dtsub.h"
#include "dtsub_decoder.h"

#define TAG "SUB-DEC"

#define REGISTER_SDEC(X,x)      \
    {                           \
        extern sd_wrapper_t sd_##x##_ops;   \
        register_sd(&sd_##x##_ops);     \
    }

static sd_wrapper_t *g_sd = NULL;

/***********************************************************************
**
** register_sd
**
** - register sub decoder
**
***********************************************************************/
static void register_sd(sd_wrapper_t * sd)
{
    sd_wrapper_t **p;
    p = &g_sd;
    while (*p != NULL) {
        p = &(*p)->next;
    }
    *p = sd;
    dt_info(TAG, "[%s:%d] register sdec, name:%s fmt:%d \n", __FUNCTION__, __LINE__,
            (*p)->name, (*p)->sfmt);
    sd->next = NULL;
}

/***********************************************************************
**
** register_sd_ext
**
** - register external sub decoder
**
***********************************************************************/
void register_sd_ext(sd_wrapper_t *sd)
{
    sd_wrapper_t **p;

    p = &g_sd;
    while (1) {
        if (*p == NULL) {
            break;
        }
        if (strstr((*p)->name, sd->name) != NULL) {
            dt_info(TAG, "[%s:%d] sdec already registerd, name:%s fmt:%d \n", __FUNCTION__,
                    __LINE__, (*p)->name, (*p)->sfmt);
            return;
        }
        p = &(*p)->next;
    }

    p = &g_sd;
    if (*p == NULL) {
        *p = sd;
        sd->next = NULL;
    } else {
        sd->next = *p;
        *p = sd;
    }

    dt_info(TAG, "[%s:%d]register ext sd. sfmt:%d name:%s \n", __FUNCTION__,
            __LINE__, sd->sfmt, sd->name);
}

/***********************************************************************
**
** sd_register_all
**
** - register all internal sub decoder
**
***********************************************************************/
void sd_register_all()
{
#ifdef ENABLE_SDEC_FFMPEG
    REGISTER_SDEC(FFMPEG, ffmpeg);
#endif
    return;
}

/***********************************************************************
**
** sd_remove_all
**
***********************************************************************/
void sd_remove_all()
{
    g_sd = NULL;
}

/***********************************************************************
**
** select_sub_decoder
**
***********************************************************************/
static int select_sub_decoder(dtsub_decoder_t * decoder)
{
    sd_wrapper_t **p;
    dtsub_para_t *para = decoder->para;
    p = &g_sd;
    while (*p != NULL) {
        if ((*p)->sfmt == DT_SUB_FORMAT_UNKOWN) {
            break;
        }
        if ((*p)->sfmt == para->sfmt) {
            break;
        }
        p = &(*p)->next;
    }
    if (*p == NULL) {
        dt_info(TAG, "[%s:%d]no valid sub decoder found sfmt:%d\n", __FUNCTION__,
                __LINE__, para->sfmt);
        return -1;
    }
    decoder->wrapper = *p;
    dt_info(TAG, "[%s:%d] select--%s sub decoder \n", __FUNCTION__, __LINE__,
            (*p)->name);
    return 0;
}

/***********************************************************************
**
** pts_exchange
**
***********************************************************************/
static int64_t pts_exchange(dtsub_decoder_t * decoder, int64_t pts)
{
    return pts;
}

/***********************************************************************
**
** sub_decode_loop
**
** - sub decode main thread
**
***********************************************************************/
static void *sub_decode_loop(void *arg)
{
    dt_av_pkt_t pkt;
    dtsub_decoder_t *decoder = (dtsub_decoder_t *) arg;
    sd_wrapper_t *wrapper = decoder->wrapper;
    dtsub_context_t *sctx = (dtsub_context_t *) decoder->parent;
    queue_t *frame_queue = sctx->so_queue;
    /*used for decode */
    dtav_sub_frame_t *frame = NULL;
    int ret;
    dt_info(TAG, "[%s:%d] start decode loop \n", __FUNCTION__, __LINE__);

    do {
        //exit check before idle, maybe recieve exit cmd in idle status
        if (decoder->status == SDEC_STATUS_EXIT) {
            dt_info(TAG, "[%s:%d] receive decode loop exit cmd \n", __FUNCTION__, __LINE__);
            break;
        }

        if (decoder->status == SDEC_STATUS_IDLE) {
            dt_info(TAG, "[%s:%d] Idle status ,please wait \n", __FUNCTION__, __LINE__);
            usleep(100);
            continue;
        }

        /*read pkt */
        if (!decoder->parent) {
            usleep(100);
            continue;
        }

        if (frame_queue->length >= SUB_OUT_MAX_COUNT) {
            //vo queue full
            usleep(1000);
            continue;
        }
        ret = dtsub_read_pkt(decoder->parent, &pkt);
        if (ret < 0) {
            if (decoder->pts_first == -1 || decoder->pts_first == DT_NOPTS_VALUE) {
                usleep(1000);
                continue;
            }
            //no data left, maybe eof, need to flush left data
            memset(&pkt, 0, sizeof(dt_av_pkt_t));
            dt_debug(TAG, "[%s:%d] no sub pkt left, flush left frames \n", __FUNCTION__,
                     __LINE__);
        }
        /*read one pkt, enter decode pkt module */
        //will exec once for one time
        ret = wrapper->decode_frame(decoder, &pkt, &frame);
        if (ret <= 0) {
            decoder->decode_err_cnt++;
            dt_debug(TAG, "[%s:%d]decode failed \n", __FUNCTION__, __LINE__);
            frame = NULL;
            usleep(10000);
            goto DECODE_END;
        }
        if (!frame) {
            goto DECODE_END;
        }

        decoder->frame_count++;
        //Got one frame
        //frame->pts = frame.pts;

        //update current pts, clear the buffer size
        if (pkt.pts >= 0 && decoder->pts_first == -1) {
            //we will use first pts to estimate pts
            dt_info(TAG, "[%s:%d]first pkt decoded ok, pts:0x%llx dts:0x%llx size:%d\n",
                    __FUNCTION__, __LINE__, pkt.pts, pkt.dts, pkt.size);
            decoder->pts_first = pts_exchange(decoder, frame->pts);
            decoder->pts_current = decoder->pts_first;
        }

        /*queue in */
        queue_push_tail(frame_queue, frame);
        frame = NULL;
DECODE_END:
        //we successfully decodec one pkt
        if (pkt.data) {
            if (pkt.data) {
                free(pkt.data);
            }
            pkt.data = NULL;
            pkt.size = 0;
            pkt.pts = -1;
        }
    } while (1);
    dt_info(TAG, "[file:%s][%s:%d]decoder loop thread exit ok\n", __FILE__,
            __FUNCTION__, __LINE__);

    pthread_exit(NULL);
    return NULL;
}

/***********************************************************************
**
** sub_decoder_init
**
***********************************************************************/
int sub_decoder_init(dtsub_decoder_t * decoder)
{
    int ret = 0;
    pthread_t tid;
    /*select decoder */
    ret = select_sub_decoder(decoder);
    if (ret < 0) {
        return -1;
    }

    sd_wrapper_t *wrapper = decoder->wrapper;
    /*init decoder */
    decoder->pts_current = decoder->pts_first = -1;
    decoder->sd_priv = decoder->para->avctx_priv;
    ret = wrapper->init(decoder);
    if (ret < 0) {
        return -1;
    }

    dt_info(TAG, "[%s:%d] sub decoder init ok\n", __FUNCTION__, __LINE__);
    /*init pcm buffer */
    dtsub_context_t *sctx = (dtsub_context_t *) decoder->parent;
    sctx->so_queue = queue_new();
    queue_t *frame_queue = sctx->so_queue;
    if (NULL == frame_queue) {
        dt_error(TAG, "create sub out queue failed\n");
        return -1;
    }
    /*create thread */
    ret = pthread_create(&tid, NULL, sub_decode_loop, (void *) decoder);
    if (ret != 0) {
        dt_error(TAG, "create audio decoder thread failed\n");
        return ret;
    }
    decoder->sub_decoder_pid = tid;
    sub_decoder_start(decoder);
    return 0;
}

/***********************************************************************
**
** sub_decoder_start
**
***********************************************************************/
int sub_decoder_start(dtsub_decoder_t * decoder)
{
    decoder->status = SDEC_STATUS_RUNNING;
    return 0;
}

/***********************************************************************
**
** dtsub_frame_free
**
***********************************************************************/
static void dtsub_frame_free(void *frame)
{
    int i;

    dtav_sub_frame_t *sub = (dtav_sub_frame_t *)(frame);
    for (i = 0; i < sub->num_rects; i++) {
        free(&sub->rects[i]->pict.data[0]);
        free(&sub->rects[i]->pict.data[1]);
        free(&sub->rects[i]->pict.data[2]);
        free(&sub->rects[i]->pict.data[3]);
        free(&sub->rects[i]->text);
        free(&sub->rects[i]->ass);
        free(&sub->rects[i]);
    }

    free(&sub->rects);
    memset(sub, 0, sizeof(dtav_sub_frame_t));
    return;
}

/***********************************************************************
**
** sub_decoder_stop
**
***********************************************************************/
int sub_decoder_stop(dtsub_decoder_t * decoder)
{
    sd_wrapper_t *wrapper = decoder->wrapper;

    /*Decode thread exit */
    decoder->status = SDEC_STATUS_EXIT;
    pthread_join(decoder->sub_decoder_pid, NULL);
    wrapper->release(decoder);

    /*release queue */
    dtsub_context_t *sctx = (dtsub_context_t *) decoder->parent;
    queue_t *frame_queue = sctx->so_queue;
    if (frame_queue) {
        queue_free(frame_queue, (free_func) dtsub_frame_free);
        frame_queue = NULL;
    }
    return 0;
}
