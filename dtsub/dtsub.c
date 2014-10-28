/***********************************************************************
**
**  Module : dtsub.c
**  Summary: dtsub impl
**  Section: dtsub
**  Author : peter
**  Notes  : 
**           sub process module
**
***********************************************************************/

#define TAG "SUB-MODULE"

#include "dtsub.h"

/***********************************************************************
**
** dtsub_read_frame
**
** - read packet from dtport
**
***********************************************************************/
int dtsub_read_frame(void *priv, dt_av_pkt_t * frame)
{
    int type = DT_TYPE_SUBTITLE;
    int ret = 0;
    dtsub_context_t *sctx = (dtsub_context_t *) priv;
    ret = dthost_read_frame (sctx->parent, frame, type);
    return ret;
}

/***********************************************************************
**
** dtsub_output_read
**
** - read sub frame(decoded) from so_queue
** - removed from queue
**
***********************************************************************/
dt_av_frame_t *dtsub_output_read(void *priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)priv;
    queue_t *sub_queue = sctx->so_queue;
    if (sub_queue->length == 0)
    {
        return NULL;
    }
    return queue_pop_head(sub_queue);
}

/***********************************************************************
**
** dtsub_output_pre_read
**
** - pre read sub frame(decoded) from so_queue
** - not removed from queue
**
***********************************************************************/
dt_av_frame_t *dtsub_output_pre_read(void *priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)priv;
    queue_t *sub_queue = sctx->so_queue;
    if (sub_queue->length == 0)
    {
        return NULL;
    }
    return queue_pre_pop_head(sub_queue);
}

/***********************************************************************
**
** sub_get_current_pts
**
***********************************************************************/
int64_t sub_get_current_pts(dtsub_context_t * sctx)
{
    uint64_t pts;
    if (sctx->sub_status <= SUB_STATUS_INITED)
        return -1;
    return sctx->current_pts;
}

/***********************************************************************
**
** sub_get_first_pts
**
***********************************************************************/
int64_t sub_get_first_pts(dtsub_context_t * sctx)
{
    if (sctx->sub_status != SUB_STATUS_INITED)
        return -1;
    dt_debug (TAG, "fitst spts:%lld \n", sctx->sub_dec.pts_first);
    return sctx->sub_dec.pts_first;
}

/***********************************************************************
**
** sub_drop
**
***********************************************************************/
int sub_drop (dtsub_context_t * sctx, int64_t target_pts)
{
    int64_t diff = target_pts - sub_get_first_pts (sctx);
    int diff_time = diff / 90;
    if (diff_time > AVSYNC_DROP_THRESHOLD)
    {
        dt_info(TAG, "diff time exceed %d ms, do not drop!\n", diff_time);
        return 0;
    }
    dt_info (TAG, "[%s:%d]target pts:%lld \n", __FUNCTION__, __LINE__, target_pts);
    dt_av_frame_t *frame = NULL;
    int64_t cur_pts = sub_get_first_pts(sctx);
    int drop_count = 300;
    do
    {
        frame = dtsub_output_read((void *)sctx);
        if (!frame)
        {
            if (drop_count-- == 0)
            {
                dt_info (TAG, "3s read nothing, quit drop sub\n");
                break;
            }
            usleep (100);
            continue;
        }
        drop_count = 300;
        cur_pts = frame->pts;
        dt_debug (TAG, "read pts:%lld \n", frame->pts);
        free(frame);
        frame = NULL;
        if (cur_pts > target_pts)
            break;
    }
    while (1);
    dt_info(TAG, "sub drop finish,drop count:%d \n", drop_count);
    sctx->current_pts = cur_pts;
    dtsub_update_pts((void *)sctx);
    return 0;
}

/***********************************************************************
**
** dtsub_get_systime
**
***********************************************************************/
int64_t dtsub_get_systime (void *priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)priv;
    if (sctx->sub_status <= SUB_STATUS_INITED)
        return -1;
    return dthost_get_systime(sctx->parent);
}

/***********************************************************************
**
** dtsub_update_pts
**
***********************************************************************/
void dtsub_update_pts(void *priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)priv;
    if (sctx->sub_status < SUB_STATUS_INITED)
        return;
    dthost_update_spts(sctx->parent, sctx->current_pts);
    return;
}

/***********************************************************************
**
** sub_get_dec_state
**
***********************************************************************/
int sub_get_dec_state(dtsub_context_t * sctx, dec_state_t * dec_state)
{
    if (sctx->sub_status <= SUB_STATUS_INITED)
        return -1;
    return -1;
    dtsub_decoder_t *sdec = &sctx->sub_dec;
    dec_state->sdec_error_count = sdec->decode_err_cnt;
    dec_state->sdec_fps = sdec->para->rate;
    dec_state->sdec_width = sdec->para->d_width;
    dec_state->sdec_height = sdec->para->d_height;
    dec_state->sdec_status = sdec->status;
    return 0;
}

//reserve 10 frame
//if low level just return
int sub_get_out_closed (dtsub_context_t * sctx)
{
    return 1;
}

void sub_register_all()
{
    sd_register_all();
    so_register_all();
    sf_register_all();
}

void register_ext_sd(sd_wrapper_t *sd)
{
   register_sdec_ext(sd); 
}

void register_ext_so(so_wrapper_t *so)
{
   so_register_ext(vo); 
}

void register_ext_sf(sf_wrapper_t *sf)
{
   sf_register_ext(sf); 
}

int sub_start (dtsub_context_t * sctx)
{
    if (sctx->sub_status == SUB_STATUS_INITED)
    {
        dtsub_output_t *sub_out = &sctx->sub_out;
        sub_output_start(sub_out);
        sctx->sub_status = SUB_STATUS_ACTIVE;
    }
    else
        dt_error (TAG, "[%s:%d]sub output start failed \n", __FUNCTION__, __LINE__);
    return 0;
}

int sub_pause (dtsub_context_t * sctx)
{
    if (sctx->sub_status == SUB_STATUS_ACTIVE)
    {
        dtsub_output_t *sub_out = &sctx->sub_out;
        sub_output_pause (sub_out);
        sctx->sub_status = SUB_STATUS_PAUSED;
    }
    return 0;
}

int sub_resume (dtsub_context_t * sctx)
{
    if (sctx->sub_status == SUB_STATUS_PAUSED)
    {
        dtsub_output_t *sub_out = &sctx->sub_out;
        sub_output_resume (sub_out);
        sctx->sub_status = SUB_STATUS_ACTIVE;
        return 0;
    }
    return -1;
}

int sub_stop (dtsub_context_t * sctx)
{
    if (sctx->sub_status >= SUB_STATUS_INITED)
    {
        dtsub_output_t *sub_out = &sctx->sub_out;
        sub_output_stop (sub_out);
        dtsub_decoder_t *sub_decoder = &sctx->sub_dec;
        sub_decoder_stop (sub_decoder);
        sctx->sub_status = SUB_STATUS_STOPPED;
    }
    return 0;
}

int sub_init (dtsub_context_t * sctx)
{
    int ret = 0;
    dt_info (TAG, "[%s:%d] dtsub_mgt start init\n", __FUNCTION__, __LINE__);
    //call init
    sctx->sub_status = SUB_STATUS_INITING;
    sctx->current_pts = sctx->last_valid_pts = -1;

    dtsub_decoder_t *sub_dec = &sctx->sub_dec;
    dtsub_filter_t *sub_filt = &sctx->sub_filt;
    dtsub_output_t *sub_out = &sctx->sub_out;
    
    //vf ctx init
    memset (sub_filt, 0, sizeof (dtsub_filter_t));
    memcpy(&sub_filt->para, &sctx->sub_para, sizeof(dtsub_para_t));
    sub_filt->parent = sctx;

    //vd ctx init 
    memset (sub_dec, 0, sizeof (dtsub_decoder_t));
    sub_dec->para = &sctx->sub_para;
    sub_dec->parent = sctx;
    ret = sub_decoder_init(sub_dec);
    if (ret < 0)
        goto err1;
    dt_info (TAG, "[%s:%d] sdecoder init ok\n", __FUNCTION__, __LINE__);

    //vo ctx init
    memset (sub_out, 0, sizeof (dtsub_output_t));
    sub_out->para = &sctx->sub_para;
    sub_out->parent = sctx;
    ret = sub_output_init (sub_out, sctx->sub_para.sub_output);
    if (ret < 0)
        goto err2;

    sctx->sub_status = SUB_STATUS_INITED;
    dt_info (TAG, "dtsub init ok,status:%d \n", sctx->sub_status);
    return 0;
  err1:
    dt_info (TAG, "[%s:%d]sub decoder init failed \n", __FUNCTION__, __LINE__);
    return -1;
  err2:
    sub_decoder_stop (sub_dec);
    dt_info (TAG, "[%s:%d]sub output init failed \n", __FUNCTION__, __LINE__);
    return -3;
}
