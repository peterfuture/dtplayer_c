#include "dtaudio_api.h"
#include "dtaudio.h"

#define TAG "AUDIO-API"

static int audio_server_init (dtaudio_context_t * actx)
{
    event_server_t *server = dt_alloc_server ();
    dt_info (TAG, "AUDIO SERVER INIT :%p \n", server);
    if (!server)
    {
        dt_error (TAG, "AUDIO SERVER ALLOC FAILED \n");
        return -1;
    }
    server->id = EVENT_SERVER_AUDIO;
    strcpy (server->name, "SERVER-AUDIO");
    dt_register_server (server);
    actx->audio_server = (void *) server;
    return 0;
}

static int audio_server_release (dtaudio_context_t * actx)
{
    event_server_t *server = (event_server_t *) actx->audio_server;
    dt_remove_server (server);
    actx->audio_server = NULL;
    return 0;
}

//==Part1:Control
int dtaudio_init (void **audio_priv, dtaudio_para_t * para, void *parent)
{
    int ret = 0;
    dtaudio_context_t *actx = (dtaudio_context_t *) malloc (sizeof (dtaudio_context_t));
    if (!actx)
    {
        dt_error (TAG, "[%s:%d] dtaudio_init failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    memset (actx, 0, sizeof (dtaudio_context_t));
    memcpy (&actx->audio_param, para, sizeof (dtaudio_para_t));
    actx->audio_param.extradata_size = para->extradata_size;
    memcpy (&(actx->audio_param.extradata[0]), &(para->extradata[0]), para->extradata_size);
    /*init server */
    ret = audio_server_init (actx);
    if (ret < 0)
    {
        dt_error (TAG, "DTAUDIO INIT FAILED \n");
        return ret;
    }
    //we need to set parent early, Since enter audio decoder loop first,will crash for parent invalid
    actx->parent = parent;
    ret = audio_init (actx);
    if (ret < 0)
    {
        dt_error ("[%s:%d] audio_init failed \n", __FUNCTION__, __LINE__);
        return ret;
    }

    *audio_priv = (void *) actx;
    return ret;
}

int dtaudio_start (void *audio_priv)
{
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_AUDIO;
    event->type = AUDIO_EVENT_START;
    dt_send_event (event);
    return 0;
}

int dtaudio_pause (void *audio_priv)
{
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_AUDIO;
    event->type = AUDIO_EVENT_PAUSE;
    dt_send_event (event);
    return 0;
}

int dtaudio_resume (void *audio_priv)
{
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_AUDIO;
    event->type = AUDIO_EVENT_RESUME;
    dt_send_event (event);
    return 0;
}

int dtaudio_stop (void *audio_priv)
{
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    if (!actx)
    {
        dt_error (TAG, "[%s:%d] dt audio context == NULL\n", __FUNCTION__, __LINE__);
        return -1;
    }
    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_AUDIO;
    event->type = AUDIO_EVENT_STOP;
    dt_send_event (event);

    ret = pthread_join (actx->event_loop_id, NULL);
    audio_server_release (actx);
    free (audio_priv);
    audio_priv = NULL;

    return ret;
}

int dtaudio_release (void *audio_priv)
{

    return 0;
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;

    event_t *event = dt_alloc_event ();
    event->next = NULL;
    event->server_id = EVENT_SERVER_AUDIO;
    event->type = AUDIO_EVENT_RELEASE;
    dt_send_event (event);

    ret = pthread_join (actx->event_loop_id, NULL);
    audio_server_release (actx);
    free (audio_priv);
    audio_priv = NULL;
    return ret;

}

//==Part2:PTS&STATUS Relative
int64_t dtaudio_get_pts (void *audio_priv)
{
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    return audio_get_current_pts (actx);
}

int dtaudio_drop (void *audio_priv, int64_t target_pts)
{
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    return audio_drop (actx, target_pts);
}

int64_t dtaudio_get_first_pts (void *audio_priv)
{
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    return audio_get_first_pts (actx);
}

int dtaudio_get_state (void *audio_priv, dec_state_t * dec_state)
{
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    ret = audio_get_dec_state (actx, dec_state);
    return ret;
}

int dtaudio_get_out_closed (void *audio_priv)
{
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    ret = audio_get_out_closed (actx);
    return ret;
}
