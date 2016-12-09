#include "dthost_api.h"
#include "dtplayer_api.h"
#include "dtplayer.h"
#include "dtaudio.h"
#include "dtvideo.h"
#include "dt_event.h"

#include "pthread.h"

#define TAG "PLAYER-API"

void dtplayer_register_ext_stream(stream_wrapper_t *wrapper)
{

}

void dtplayer_register_ext_demuxer(demuxer_wrapper_t *wrapper)
{

}

void dtplayer_register_ext_ao(ao_wrapper_t *wrapper)
{
    register_ext_ao(wrapper);
}

void dtplayer_register_ext_ad(ad_wrapper_t *wrapper)
{

}

void dtplayer_register_ext_vo(vo_wrapper_t *wrapper)
{
    register_ext_vo(wrapper);
}

void dtplayer_register_ext_vd(vd_wrapper_t *wrapper)
{
    register_ext_vd(wrapper);
}

void dtplayer_register_ext_vf(vf_wrapper_t *wrapper)
{
    register_ext_vf(wrapper);
}

void *dtplayer_init(dtplayer_para_t * para)
{
    int ret = 0;
    if (!para) {
        return NULL;
    }
    player_register_all();
    dtplayer_context_t *dtp_ctx = dt_malloc(sizeof(dtplayer_context_t)); // will free in dtplayer.c
    if (!dtp_ctx) {
        dt_error(TAG, "dtplayer context malloc failed \n");
        return NULL;
    }
    memset(dtp_ctx, 0, sizeof(dtplayer_context_t));
    memcpy(&dtp_ctx->player_para, para, sizeof(dtplayer_para_t));
    dtp_ctx->cookie = para->cookie;
    dt_info(TAG, "start playing :%s \n", para->file_name);

    ret = player_init(dtp_ctx);
    if (ret < 0) {
        dt_error(TAG, "PLAYER INIT FAILED \n");
        free(dtp_ctx);
        dtp_ctx = NULL;
    }
    return dtp_ctx;
}

int dtplayer_set_video_size(void *player_priv, int width, int height)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    return player_set_video_size(dtp_ctx, width, height);
}

int dtplayer_start(void *player_priv)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    //Comments:
    //register evetn service and player service in player_start
    //Since some app just want to get mediaInfo in dtplayer_init
    //No need to create loop there
    dtp_ctx->service_mgt = dt_service_create();
    int ret = player_start(dtp_ctx);
    return ret;
}

int dtplayer_pause(void *player_priv)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_PLAYER, PLAYER_EVENT_PAUSE);
    dt_send_event(dtp_ctx->service_mgt, event);
    return 0;
}

int dtplayer_resume(void *player_priv)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_PLAYER, PLAYER_EVENT_RESUME);
    dt_send_event(dtp_ctx->service_mgt, event);
    return 0;
}

int dtplayer_stop(void *player_priv)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_PLAYER, PLAYER_EVENT_STOP);
    dt_send_event(dtp_ctx->service_mgt, event);

    /*need to wait until player stop ok */
    dt_info(TAG, "EVENT_LOOP_ID:%lu \n", dtp_ctx->event_loop_id);

    //comments: after sending quit cmd
    //player will enter quit process
    //here has no need to block ,player will
    //exit after receiving quit status through update_cb in dtplayer.c

    pthread_join(dtp_ctx->event_loop_id, NULL);
    return 0;
}

// seek to cur_time + s_time
int dtplayer_seek(void *player_priv, int s_time)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;

    //get current time
    int64_t current_time = dtp_ctx->state.cur_time;
    int64_t full_time = dtp_ctx->media_info->duration;
    int seek_time = current_time + s_time;
    if (seek_time < 0) {
        seek_time = 0;
    }
    if (seek_time > full_time) {
        seek_time = full_time;
    }

    event_t *event = dt_alloc_event(EVENT_SERVER_ID_PLAYER, PLAYER_EVENT_SEEK);
    event->para.np = seek_time;
    dt_send_event(dtp_ctx->service_mgt, event);
    return 0;
}

// seek to cur_time + s_time
int dtplayer_seekto(void *player_priv, int s_time)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;

    //get current time
    dt_info(TAG, "seek to :%d s \n", s_time);
    int64_t full_time = dtp_ctx->media_info->duration;
    int seek_time = s_time;
    if (seek_time < 0) {
        seek_time = 0;
    }
    if (seek_time > (int)full_time) {
        seek_time = (int)full_time;
    }

    event_t *event = dt_alloc_event(EVENT_SERVER_ID_PLAYER, PLAYER_EVENT_SEEK);
    event->para.np = seek_time;
    dt_send_event_sync(dtp_ctx->service_mgt, event);

    dt_info(TAG, "seek cmd send ok \n");
    return 0;
}

int dtplayer_get_mediainfo(void *player_priv, dt_media_info_t *info)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    return player_get_mediainfo(dtp_ctx, info);
}

int dtplayer_get_states(void *player_priv, player_state_t * state)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)player_priv;
    memcpy(state, &dtp_ctx->state, sizeof(player_state_t));
    return 0;
}
