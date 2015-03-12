#include "dthost_api.h"
#include "dthost.h"

#define TAG "HOST-API"
//=====part1: control methods

int dthost_start(void *host_priv)
{
    int ret = 0;
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    dt_debug(TAG, "hctx :%p \n", hctx);
    ret = host_start(hctx);
    return ret;
}

int dthost_pause(void *host_priv)
{
    int ret = 0;
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    ret = host_pause(hctx);
    return ret;
}

int dthost_resume(void *host_priv)
{
    int ret = 0;
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    ret = host_resume(hctx);
    return ret;
}

int dthost_stop(void *host_priv)
{
    int ret = 0;
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    ret = host_stop(hctx);
    if (ret < 0) {
        goto FAIL;
    }
    free(hctx);
    hctx = NULL;
    host_priv = NULL;
FAIL:
    return ret;

}

int dthost_init(void **host_priv, dthost_para_t * para)
{
    int ret = 0;
    if (*host_priv) {
        dt_error(TAG, "[%s:%d] host_priv is Null\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    dthost_context_t *hctx = malloc(sizeof(dthost_context_t));
    if (!hctx) {
        dt_info(TAG, "[%s:%d] dthost_context_t malloc failed\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    dt_debug(TAG, "hctx :%p \n", hctx);
    memset(hctx, 0, sizeof(dthost_context_t));
    memcpy(&hctx->para, para, sizeof(dthost_para_t));
    ret = host_init(hctx);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] dthost_init failed\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR1;
    }
    *host_priv = (void *) hctx;
    return ret;
ERR1:
    free(hctx);
ERR0:
    return ret;
}

int dthost_video_resize(void **host_priv, int w, int h)
{
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    return host_video_resize(hctx, w, h);
}

//==Part2:Data IO Relative

int dthost_read_frame(void *host_priv, dt_av_pkt_t * frame, int type)
{
    int ret = 0;
    if (!host_priv) {
        dt_error(TAG, "[%s:%d] host_priv is Null\n", __FUNCTION__, __LINE__);
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    ret = host_read_frame(hctx, frame, type);
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    return ret;
}

int dthost_write_frame(void *host_priv, dt_av_pkt_t * frame, int type)
{
    int ret = 0;
    if (!host_priv) {
        dt_error(TAG, "[%s:%d] host_priv==NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    ret = host_write_frame(hctx, frame, type);
    return ret;
}

//==Part3: PTS Relative

int64_t dthost_get_apts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_apts(hctx);
}

int64_t dthost_update_apts(void *host_priv, int64_t pts)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_update_apts(hctx, pts);
}

int64_t dthost_get_vpts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_vpts(hctx);
}

void dthost_update_vpts(void *host_priv, int64_t vpts)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_update_vpts(hctx, vpts);
    return;
}

int64_t dthost_get_spts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_spts(hctx);
}

void dthost_update_spts(void *host_priv, int64_t spts)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_update_spts(hctx, spts);
    return;
}


int dthost_get_avdiff(void *host_priv)
{
    if (!host_priv) {
        return 0;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_avdiff(hctx);
}

int64_t dthost_get_current_time(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_current_time(hctx);
}

int64_t dthost_get_systime(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_systime(hctx);
}

void dthost_update_systime(void *host_priv, int64_t systime)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_update_systime(hctx, systime);
    return;
}

//==Part4:Status Relative

int dthost_get_state(void *host_priv, host_state_t * state)
{
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    if (!host_priv) {
        return -1;
    }
    host_get_state(hctx, state);
    return 0;
}

int dthost_get_out_closed(void *host_priv)
{
    int ret = 0;
    if (!host_priv) {
        dt_error(TAG, "host PRIV IS NULL \n");
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    ret = host_get_out_closed(hctx);
    return ret;
}
