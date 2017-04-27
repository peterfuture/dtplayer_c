#include "dtaudio_api.h"
#include "dtaudio.h"

#define TAG "AUDIO-API"

static int audio_service_init(dtaudio_context_t * actx)
{
    service_t *service = dt_alloc_service(EVENT_SERVER_ID_AUDIO,
                                          EVENT_SERVER_NAME_AUDIO);
    dt_info(TAG, "AUDIO SERVER INIT :%p \n", service);
    if (!service) {
        dt_error(TAG, "AUDIO SERVER ALLOC FAILED \n");
        return -1;
    }
    dt_register_service(actx->audio_param.service_mgt, service);
    actx->audio_service = (void *) service;
    return 0;
}

static int audio_service_release(dtaudio_context_t * actx)
{
    service_t *service = (service_t *) actx->audio_service;
    dt_remove_service(actx->audio_param.service_mgt, service);
    actx->audio_service = NULL;
    return 0;
}

//==Part1:Control
int dtaudio_init(void **audio_priv, dtaudio_para_t * para, void *parent)
{
    int ret = 0;
    dtaudio_context_t *actx = (dtaudio_context_t *) malloc(sizeof(
                                  dtaudio_context_t));
    if (!actx) {
        dt_error(TAG, "[%s:%d] dtaudio_init failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    memset(actx, 0, sizeof(dtaudio_context_t));
    memcpy(&actx->audio_param, para, sizeof(dtaudio_para_t));
    actx->audio_param.extradata_size = para->extradata_size;
    memcpy(&(actx->audio_param.extradata[0]), &(para->extradata[0]),
           para->extradata_size);
    /*init service */
    ret = audio_service_init(actx);
    if (ret < 0) {
        dt_error(TAG, "DTAUDIO INIT FAILED \n");
        return ret;
    }
    //we need to set parent early, Since enter audio decoder loop first,will crash for parent invalid
    actx->parent = parent;
    ret = audio_init(actx);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] audio_init failed \n", __FUNCTION__, __LINE__);
        return ret;
    }

    *audio_priv = (void *) actx;
    return ret;
}

int dtaudio_start(void *audio_priv)
{
    dtaudio_context_t *actx = (dtaudio_context_t *)audio_priv;
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_AUDIO, AUDIO_EVENT_START);
    dt_send_event(actx->audio_param.service_mgt, event);
    return 0;
}

int dtaudio_pause(void *audio_priv)
{
    dtaudio_context_t *actx = (dtaudio_context_t *)audio_priv;
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_AUDIO, AUDIO_EVENT_PAUSE);
    dt_send_event(actx->audio_param.service_mgt, event);
    return 0;
}

int dtaudio_resume(void *audio_priv)
{
    dtaudio_context_t *actx = (dtaudio_context_t *)audio_priv;
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_AUDIO, AUDIO_EVENT_RESUME);
    dt_send_event(actx->audio_param.service_mgt, event);
    return 0;
}

int dtaudio_stop(void *audio_priv)
{
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    if (!actx) {
        dt_error(TAG, "[%s:%d] dt audio context == NULL\n", __FUNCTION__, __LINE__);
        return -1;
    }
    event_t *event = dt_alloc_event(EVENT_SERVER_ID_AUDIO, AUDIO_EVENT_STOP);
    dt_send_event(actx->audio_param.service_mgt, event);

    ret = pthread_join(actx->event_loop_id, NULL);
    audio_service_release(actx);
    free(audio_priv);
    audio_priv = NULL;

    return ret;
}

int dtaudio_release(void *audio_priv)
{

    return 0;
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;

    event_t *event = dt_alloc_event(EVENT_SERVER_ID_AUDIO, AUDIO_EVENT_RELEASE);
    dt_send_event(actx->audio_param.service_mgt, event);

    ret = pthread_join(actx->event_loop_id, NULL);
    audio_service_release(actx);
    free(audio_priv);
    audio_priv = NULL;
    return ret;

}

//==Part2:PTS&STATUS Relative
int64_t dtaudio_get_pts(void *audio_priv)
{
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    return audio_get_current_pts(actx);
}

int dtaudio_drop(void *audio_priv, int64_t target_pts)
{
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    return audio_drop(actx, target_pts);
}

int dtaudio_get_first_pts(void *audio_priv, int64_t *pts)
{
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    if (audio_first_frame_decoded(actx) == 0) {
        return -1;
    }
    *pts = audio_get_first_pts(actx);
    return 0;
}

int dtaudio_get_state(void *audio_priv, dec_state_t * dec_state)
{
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    ret = audio_get_dec_state(actx, dec_state);
    return ret;
}

int dtaudio_get_out_closed(void *audio_priv)
{
    int ret;
    dtaudio_context_t *actx = (dtaudio_context_t *) audio_priv;
    ret = audio_get_out_closed(actx);
    return ret;
}
