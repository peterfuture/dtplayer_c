#include "dt_error.h"

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
        dt_info(TAG, "[%s:%d] dthost_context_t malloc failed\n", __FUNCTION__,
                __LINE__);
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

int dthost_read_frame(void *host_priv, dt_av_pkt_t **frame, int type)
{
    int ret = 0;
    if (!host_priv) {
        dt_error(TAG, "[%s:%d] host_priv is Null\n", __FUNCTION__, __LINE__);
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *) host_priv;
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    ret = host_read_frame(hctx, frame, type);
    dt_debug(TAG, "[%s:%d]READ FRAME END \n", __FUNCTION__, __LINE__);
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
static int64_t dthost_set_first_apts(void *host_priv, int64_t pts)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    hctx->pts_audio_first = hctx->pts_audio_last = hctx->pts_audio_current =
                                pts;
    hctx->notify(hctx->parent, DTP_EVENTS_FIRST_AUDIO_DECODE, 0, 0);
    return 0;
}

static int64_t dthost_get_first_apts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return hctx->pts_audio_first;
}

static int64_t dthost_set_first_vpts(void *host_priv, int64_t pts)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    hctx->pts_video_first = hctx->pts_video_last = hctx->pts_video_current =
                                pts;
    hctx->notify(hctx->parent, DTP_EVENTS_FIRST_VIDEO_DECODE, 0, 0);
    return 0;
}

static int64_t dthost_get_first_vpts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return hctx->pts_video_first;
}

static void dthost_set_drop_done(void *host_priv)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    hctx->drop_done = 1;
    return;
}

static int64_t dthost_get_drop_done(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return hctx->drop_done;
}

static int64_t dthost_get_apts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_apts(hctx);
}

static int64_t dthost_update_apts(void *host_priv, int64_t pts)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_update_apts(hctx, pts);
}

static int64_t dthost_get_vpts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_vpts(hctx);
}

static void dthost_update_vpts(void *host_priv, int64_t vpts)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_update_vpts(hctx, vpts);
    return;
}

static int64_t dthost_get_spts(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_spts(hctx);
}

static void dthost_update_spts(void *host_priv, int64_t spts)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_update_spts(hctx, spts);
    return;
}


static int dthost_get_avdiff(void *host_priv)
{
    if (!host_priv) {
        return 0;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_avdiff(hctx);
}

static int64_t dthost_get_current_time(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_current_time(hctx);
}

static int64_t dthost_get_systime(void *host_priv)
{
    if (!host_priv) {
        return -1;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    return host_get_systime(hctx);
}

static void dthost_update_systime(void *host_priv, int64_t systime)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_update_systime(hctx, systime);
    return;
}

static void dthost_clear_discontinue_flag(void *host_priv)
{
    if (!host_priv) {
        return;
    }
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    host_clear_discontinue_flag(hctx);
    return;
}

//==Part4:Status Relative

static int dthost_get_state(void *host_priv, host_state_t * state)
{
    dthost_context_t *hctx = (dthost_context_t *)(host_priv);
    if (!host_priv) {
        return -1;
    }
    host_get_state(hctx, state);
    return 0;
}

static int dthost_get_render_closed(void *host_priv)
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

int dthost_get_info(void *host_priv, enum HOST_CMD cmd, unsigned long arg)
{
    int ret = DTERROR_NONE;
    if (!host_priv) {
        dt_error(TAG, "host PRIV IS NULL \n");
        return -1;
    }

    switch (cmd) {
    case HOST_CMD_GET_FIRST_APTS:
        *(int64_t *) arg = dthost_get_first_apts(host_priv);
        break;
    case HOST_CMD_GET_FIRST_VPTS:
        *(int64_t *) arg = dthost_get_first_vpts(host_priv);
        break;
    case HOST_CMD_GET_DROP_DONE:
        *(int *) arg = dthost_get_drop_done(host_priv);
        break;
    case HOST_CMD_GET_APTS:
        *((int64_t *)arg) = dthost_get_apts(host_priv);
        break;
    case HOST_CMD_GET_VPTS:
        *((int64_t *)arg) = dthost_get_vpts(host_priv);
        break;
    case HOST_CMD_GET_SPTS:
        *((int64_t *)arg) = dthost_get_spts(host_priv);
        break;
    case HOST_CMD_GET_SYSTIME:
        *((int64_t *)arg) = dthost_get_systime(host_priv);
        break;
    case HOST_CMD_GET_AVDIFF:
        *((int64_t *)arg) = dthost_get_avdiff(host_priv);
        break;
    case HOST_CMD_GET_CURRENT_TIME:
        *((int64_t *)arg) = dthost_get_current_time(host_priv);
        break;
#if 0
    case HOST_CMD_GET_DISCONTINUE_FLAG:
        *((int *)arg) = dthost_get_discontinue_flag(host_priv);
        break;
#endif
    case HOST_CMD_GET_RENDER_CLOSED:
        *((int *)arg) = dthost_get_render_closed(host_priv);
        break;

    case HOST_CMD_GET_STATE:
        ret = dthost_get_state(host_priv, (host_state_t *)arg);
        break;
    default:
        ret = DTERROR_INVALID_CMD;
        break;
    }

    return ret;
}

int dthost_set_info(void *host_priv, enum HOST_CMD cmd, unsigned long arg)
{
    int ret = 0;
    if (!host_priv) {
        dt_error(TAG, "host PRIV IS NULL \n");
        return -1;
    }

    switch (cmd) {
    case HOST_CMD_SET_FIRST_APTS:
        dthost_set_first_apts(host_priv, *((int64_t *)arg));
        break;
    case HOST_CMD_SET_FIRST_VPTS:
        dthost_set_first_vpts(host_priv, *((int64_t *)arg));
        break;
    case HOST_CMD_SET_DROP_DONE:
        dthost_set_drop_done(host_priv);
        break;
    case HOST_CMD_SET_APTS:
        dthost_update_apts(host_priv, *((int64_t *)arg));
        break;
    case HOST_CMD_SET_VPTS:
        dthost_update_vpts(host_priv, *((int64_t *)arg));
        break;
    case HOST_CMD_SET_SPTS:
        dthost_update_spts(host_priv, *((int64_t *)arg));
        break;
    case HOST_CMD_SET_SYSTIME:
        dthost_update_systime(host_priv, *((int64_t *)arg));
        break;
    case HOST_CMD_SET_DISCONTINUE_FLAG:
        if ((*(int *)arg) == 0) {
            dthost_clear_discontinue_flag(host_priv);
        }
        break;
    default:
        ret = DTERROR_INVALID_CMD;
        break;
    }

    return ret;
}
