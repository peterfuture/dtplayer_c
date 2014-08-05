#include "dtaudio_output.h"
#include "dtaudio.h"

#define TAG "AUDIO-OUT"
//#define DTAUDIO_DUMP_PCM 1
#define REGISTER_AO(X, x)	 	\
	{							\
		extern ao_wrapper_t ao_##x##_ops; 	\
		register_ao(&ao_##x##_ops); 	\
	}
static ao_wrapper_t *g_ao = NULL;

static void register_ao (ao_wrapper_t * ao)
{
    ao_wrapper_t **p;
    p = &g_ao;
    while (*p != NULL)
        p = &((*p)->next);
    *p = ao;
    ao->next = NULL;
    dt_info(TAG,"register ao:%s \n",ao->name);
}

void aout_register_ext (ao_wrapper_t * ao)
{
    ao_wrapper_t **p;
    p = &g_ao;

    if(*p == NULL)
    {
        *p = ao;
        ao->next = NULL;
    }
    else
    {
        ao->next = *p;
        *p = ao;
    }
    dt_info(TAG,"register ext ao:%s \n",ao->name);
}

void aout_register_all ()
{
    /*Register all audio_output */
    //REGISTER_AO (NULL, null);
#ifdef ENABLE_AO_SDL
    REGISTER_AO (SDL, sdl);
#endif
#if 0
#ifdef ENABLE_AO_SDL2
    REGISTER_AO (SDL2, sdl2);
#endif
#endif
#ifdef ENABLE_AO_ALSA
    REGISTER_AO (ALSA, alsa);
#endif
#ifdef ENABLE_AO_OSS
    REGISTER_AO (OSS, oss);
#endif
    return;
}

/*default alsa*/
static int select_ao_device (dtaudio_output_t * ao, int id)
{
    ao_wrapper_t **p;
    p = &g_ao;

    if(id == -1) // user did not choose ao,use default one
    {
        if(!*p)
            return -1;
        ao->aout_ops = *p;
        return 0;
    }

    while (*p != NULL && (*p)->id != id)
        p = &(*p)->next;
    if (!*p)
    {
        dt_info (LOG_TAG, "no valid ao device found\n");
        return -1;
    }
    ao->aout_ops = *p;
    dt_info (TAG, "[%s:%d]select--%s audio device \n", __FUNCTION__, __LINE__, (*p)->name);
    return 0;
}

int audio_output_start (dtaudio_output_t * ao)
{
    /*start playback */
    ao->status = AO_STATUS_RUNNING;
    return 0;
}

int audio_output_pause (dtaudio_output_t * ao)
{
    ao->status = AO_STATUS_PAUSE;
    ao_wrapper_t *wrapper = ao->aout_ops;
    wrapper->ao_pause ();
    return 0;
}

int audio_output_resume (dtaudio_output_t * ao)
{
    ao->status = AO_STATUS_RUNNING;
    ao_wrapper_t *wrapper = ao->aout_ops;
    wrapper->ao_resume ();
    return 0;
}

int audio_output_stop (dtaudio_output_t * ao)
{
    ao->status = AO_STATUS_EXIT;
    pthread_join (ao->output_thread_pid, NULL);
    ao_wrapper_t *wrapper = ao->aout_ops;
    wrapper->ao_stop ();
    dt_info (TAG, "[%s:%d] aout stop ok \n", __FUNCTION__, __LINE__);
    return 0;
}

int audio_output_latency (dtaudio_output_t * ao)
{
    if (ao->status == AO_STATUS_IDLE)
        return 0;
    if (ao->status == AO_STATUS_PAUSE)
        return ao->last_valid_latency;
    ao_wrapper_t *wrapper = ao->aout_ops;
    ao->last_valid_latency = wrapper->ao_latency ();
    return ao->last_valid_latency;
}

int audio_output_get_level (dtaudio_output_t * ao)
{

    ao_wrapper_t *wrapper = ao->aout_ops;
    return wrapper->ao_level();
}

static void *audio_output_thread (void *args)
{
    dtaudio_output_t *ao = (dtaudio_output_t *) args;
    ao_wrapper_t *wrapper = ao->aout_ops;
    
    uint8_t buffer[PCM_WRITE_SIZE];
    int rlen, ret, wlen;
    rlen = ret = wlen = 0;
   
    for (;;)
    {
        if (ao->status == AO_STATUS_EXIT)
            goto EXIT;
        if (ao->status == AO_STATUS_IDLE || ao->status == AO_STATUS_PAUSE)
        {
            usleep (100);
            continue;
        }

        /* update audio pts */
        audio_update_pts (ao->parent);

        /*read data from filter or decode buffer */
        if (rlen <= 0)
        {
            rlen = audio_output_read (ao->parent, buffer, PCM_WRITE_SIZE);
            if (rlen <= 0)
            {
                dt_debug (LOG_TAG, "pcm read failed! \n");
                usleep (1000);
                continue;
            }
#ifdef DTAUDIO_DUMP_PCM
            FILE *fp = fopen ("pcm.out", "ab+");
            int flen = 0;
            if (fp)
            {
                flen = fwrite (buffer, 1, rlen, fp);
                if (flen <= 0)
                    dt_info (LOG_TAG, "pcm dump failed! \n");
                fclose (fp);
            }
            else
                dt_error (TAG, "pcm out open failed! \n");
#endif
        }
        /*write to ao device */
        wlen = wrapper->ao_write (buffer, rlen);
        if (wlen <= 0)
        {
            usleep (1000);
            continue;
        }

        rlen -= wlen;
        if (rlen > 0)
            memmove (buffer, buffer + wlen, rlen);
        wlen = 0;
    }
  EXIT:
    dt_info (LOG_TAG, "[file:%s][%s:%d]ao playback thread exit\n", __FILE__, __FUNCTION__, __LINE__);
    pthread_exit (NULL);
    return NULL;

}

int audio_output_init (dtaudio_output_t * ao, int ao_id)
{
    int ret = 0;
    pthread_t tid;
    
    /*select ao device */
    ret = select_ao_device (ao, ao_id);
    if (ret < 0)
        return -1;
    
    ao_wrapper_t *wrapper = ao->aout_ops;
    wrapper->ao_init (&ao->para);
    dt_info (TAG, "[%s:%d] audio output init success\n", __FUNCTION__, __LINE__);
    
    /*start aout pthread */
    ret = pthread_create (&tid, NULL, audio_output_thread, (void *) ao);
    if (ret != 0)
    {
        dt_error (TAG, "[%s:%d] create audio output thread failed\n", __FUNCTION__, __LINE__);
        return ret;
    }
    ao->output_thread_pid = tid;
    dt_info (TAG, "[%s:%d] create audio output thread success\n", __FUNCTION__, __LINE__);
    return ret;
}

int64_t audio_output_get_latency (dtaudio_output_t * ao)
{
    if (ao->status == AO_STATUS_IDLE || ao->status == AO_STATUS_EXIT)
        return 0;
    if (ao->status == AO_STATUS_PAUSE)
        return ao->last_valid_latency;
    
    ao_wrapper_t *wrapper = ao->aout_ops;
    ao->last_valid_latency = wrapper->ao_latency ();
    return ao->last_valid_latency;
}
