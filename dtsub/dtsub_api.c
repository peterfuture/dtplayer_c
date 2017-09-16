/***********************************************************************
**
**  Module : dtsub_api.c
**  Summary: dtsub API
**  Section: dtsub
**  Author : peter
**  Notes  :
**           provide dtsub api
**
***********************************************************************/

#define TAG "SUB-API"

#include "dtp_plugin.h"
#include "dtsub.h"
#include "dthost_api.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/***********************************************************************
**
** dtsub_read_frame
**
** - read packet from dtport
**
***********************************************************************/
int dtsub_read_pkt(void *priv, dt_av_pkt_t * pkt)
{
    int type = DTP_MEDIA_TYPE_SUBTITLE;
    int ret = 0;
    dtsub_context_t *sctx = (dtsub_context_t *) priv;
    ret = dthost_read_frame(sctx->parent, pkt, type);
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
    if (sub_queue->length == 0) {
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
    if (sub_queue->length == 0) {
        return NULL;
    }
    return queue_pre_pop_head(sub_queue);
}


/***********************************************************************
**
** dtsub_init
**
***********************************************************************/
int dtsub_init(void **sub_priv, dtsub_para_t *para, void *parent)
{
    int ret = 0;
    dtsub_context_t *sctx = (dtsub_context_t *)malloc(sizeof(dtsub_context_t));
    if (!sctx) {
        dt_error(TAG, "[%s:%d]dtsub module init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    memset(sctx, 0, sizeof(dtsub_context_t));
    memcpy(&sctx->sub_para, para, sizeof(dtsub_para_t));

    //we need to set parent early
    sctx->parent = parent;
    ret = sub_init(sctx);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] sub init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR1;
    }
    *sub_priv = (void *)sctx;
    return ret;
ERR1:
    free(sctx);
ERR0:
    return ret;
}

/***********************************************************************
**
** dtsub_start
**
***********************************************************************/
int dtsub_start(void *sub_priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)sub_priv;
    return sub_start(sctx);
}

/***********************************************************************
**
** dtsub_pause
**
***********************************************************************/
int dtsub_pause(void *sub_priv)
{
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    return sub_pause(vctx);
}

/***********************************************************************
**
** dtsub_resume
**
***********************************************************************/
int dtsub_resume(void *sub_priv)
{
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    return sub_resume(vctx);
}

/***********************************************************************
**
** dtsub_stop
**
** - stop sub module
**
***********************************************************************/
int dtsub_stop(void *sub_priv)
{
    int ret;
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    if (!vctx) {
        dt_error(TAG, "[%s:%d] dt sub context == NULL\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    ret = sub_stop(vctx);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] DTVIDEO STOP FAILED\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    free(sub_priv);
    sub_priv = NULL;
    return ret;
ERR0:
    return ret;

}

/***********************************************************************
**
** dtsub_get_systime
**
***********************************************************************/
int64_t dtsub_get_systime(void *priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)priv;
    if (sctx->sub_status <= SUB_STATUS_INITED) {
        return -1;
    }
    int systime = 0;
    dthost_get_info(sctx->parent, HOST_CMD_GET_SYSTIME, (unsigned long)(&systime));
    return systime;
}

/***********************************************************************
**
** dtsub_update_pts
**
***********************************************************************/
void dtsub_update_pts(void *priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)priv;
    if (sctx->sub_status < SUB_STATUS_INITED) {
        return;
    }
    dthost_set_info(sctx->parent, HOST_CMD_SET_SPTS,
                    (unsigned long)(&sctx->current_pts));
    return;
}

/***********************************************************************
**
** dtsub_get_pts
**
** - get cur sub pts
**
***********************************************************************/
int64_t dtsub_get_pts(void *sub_priv)
{
    dtsub_context_t *sctx = (dtsub_context_t *)sub_priv;
    return sub_get_current_pts(sctx);
}

/***********************************************************************
**
** dtsub_get_first_pts
**
** - get first sub pts , no use
**
***********************************************************************/
int64_t dtsub_get_first_pts(void *sub_priv)
{
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    return sub_get_first_pts(vctx);
}

/***********************************************************************
**
** dtsub_drop
**
** - drop sub data for sync
**
***********************************************************************/
int dtsub_drop(void *sub_priv, int64_t target_pts)
{
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    return sub_drop(vctx, target_pts);
}

/***********************************************************************
**
** dtsub_get_state
**
** - get sub decoder state
**
***********************************************************************/
int dtsub_get_state(void *sub_priv, dec_state_t * dec_state)
{
    int ret;
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    ret = sub_get_dec_state(vctx, dec_state);
    return ret;
}

/***********************************************************************
**
** dtsub_get_out_closed
**
** - check sub data consumed completely
**
***********************************************************************/
int dtsub_get_out_closed(void *sub_priv)
{
    int ret;
    dtsub_context_t *vctx = (dtsub_context_t *)sub_priv;
    ret = sub_get_out_closed(vctx);
    return ret;
}
