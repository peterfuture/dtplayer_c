#include "dthost_api.h"
#include "dtplayer_api.h"
#include "dtplayer.h"
#include "dt_event.h"

#include "pthread.h"

#define TAG "PLAYER-API"

static dtplayer_context_t ply_ctx;

dtplayer_para_t *dtplayer_alloc_para ()
{
    dtplayer_para_t *para = (dtplayer_para_t *) malloc (sizeof (dtplayer_para_t));
    if (!para)
    {
        dt_info (TAG, "DTPLAYER PARA ALLOC FAILED \n");
        return NULL;
    }
    para->no_audio = para->no_video = para->no_sub = -1;
    para->height = para->width = -1;
    para->loop_mode = 0;
    para->audio_index = para->video_index = para->sub_index = -1;
    para->update_cb = NULL;
    para->sync_enable = -1;
    return para;
}

int dtplayer_release_para (dtplayer_para_t * para)
{
    if (para)
        free (para);
    para = NULL;
    return 0;
}

int dtplayer_init (void **player_priv, dtplayer_para_t * para)
{
    int ret = 0;
    if (!para)
        return -1;
    player_register_all();
    //dtplayer_context_t *dtp_ctx = malloc(sizeof(dtplayer_context_t));
    dtplayer_context_t *dtp_ctx = &ply_ctx;
    if (!dtp_ctx)
    {
        dt_error (TAG, "dtplayer context malloc failed \n");
        return -1;
    }
    memset (dtp_ctx, 0, sizeof (*dtp_ctx));
    memcpy (&dtp_ctx->player_para, para, sizeof (dtplayer_para_t));

    /*init server for player and process */
    dt_event_server_init ();
    /*init player */
    ret = player_init (dtp_ctx);
    if (ret < 0)
    {
        dt_error (TAG, "PLAYER INIT FAILED \n");
        return -1;
    }
    *player_priv = dtp_ctx;
    return 0;
}

int dtplayer_start (void *player_priv)
{
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_PLAYER;
    event->type = PLAYER_EVENT_START;
    dt_send_event (event);
    return 0;
}

int dtplayer_pause (void *player_priv)
{
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_PLAYER;
    event->type = PLAYER_EVENT_PAUSE;

    dt_send_event (event);
    return 0;
}

int dtplayer_resume (void *player_priv)
{
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_PLAYER;
    event->type = PLAYER_EVENT_RESUME;

    dt_send_event (event);
    return 0;
}

int dtplayer_stop (void *player_priv)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *) player_priv;
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_PLAYER;
    event->type = PLAYER_EVENT_STOP;
    dt_send_event (event);

    /*need to wait until player stop ok */
    dt_info (TAG, "EVENT_LOOP_ID:%lu \n", dtp_ctx->event_loop_id);
    pthread_join (dtp_ctx->event_loop_id, NULL);
    return 0;
}

int dtplayer_seek (void *player_priv, int s_time)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *) player_priv;

    //get current time
    int64_t current_time = dtp_ctx->state.cur_time;
    int64_t full_time = dtp_ctx->media_info->duration;
    int seek_time = current_time + s_time;
    if (seek_time < 0)
        seek_time = 0;
    if (seek_time > full_time)
        seek_time = full_time;
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_PLAYER;
    event->type = PLAYER_EVENT_SEEK;
    event->para.np = seek_time;
    dt_send_event (event);

    return 0;
}

int dtplayer_get_states (void *player_priv, player_state_t * state)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *) player_priv;
    memcpy (state, &dtp_ctx->state, sizeof (player_state_t));
    return 0;
}
