#include "dtaudio_output.h"
#include "dtaudio.h"

#define TAG "AUDIO-OUT"
//#define DTAUDIO_DUMP_PCM 1
#define REGISTER_AO(X, x)       \
    {                           \
        extern ao_wrapper_t ao_##x##_ops;   \
        register_ao(&ao_##x##_ops);     \
    }
static ao_wrapper_t *g_ao = NULL;

static void register_ao(ao_wrapper_t * ao)
{
    ao_wrapper_t **p;
    p = &g_ao;
    while (*p != NULL) {
        p = &((*p)->next);
    }
    *p = ao;
    ao->next = NULL;
    dt_info(TAG, "register ao:%s \n", ao->name);
}

void aout_register_ext(ao_wrapper_t * ao)
{
    ao_wrapper_t **p;
    p = &g_ao;

    if (*p == NULL) {
        *p = ao;
        ao->next = NULL;
    } else {
        ao->next = *p;
        *p = ao;
    }
    dt_info(TAG, "register ext ao:%s \n", ao->name);
}

void aout_register_all()
{
    /*Register all audio_output */
    //REGISTER_AO (NULL, null);
#ifdef ENABLE_AO_ALSA
    REGISTER_AO(ALSA, alsa);
#endif
#ifdef ENABLE_AO_OSS
    REGISTER_AO(OSS, oss);
#endif
    return;
}

void aout_remove_all()
{
    g_ao = NULL;
}

ao_wrapper_t *select_ao_device(int id)
{
    ao_wrapper_t **p = &g_ao;

    if (id == -1) { // user did not choose ao,use default one
        if (!*p) {
            return NULL;
        }
        return *p;
    }

    while (*p != NULL && (*p)->id != id) {
        p = &(*p)->next;
    }
    if (!*p) {
        dt_info(LOG_TAG, "no valid ao device found\n");
        return NULL;
    }
    dt_info(TAG, "[%s:%d]select--%s audio device \n", __FUNCTION__, __LINE__,
            (*p)->name);
    return *p;
}

static ao_context_t *alloc_ao_context(ao_wrapper_t *wrapper)
{
    ao_context_t *aoc = (ao_context_t *)malloc(sizeof(ao_context_t));
    if (!aoc) {
        return NULL;
    }
    if (wrapper->private_data_size > 0) {
        aoc->private_data = malloc(wrapper->private_data_size);
        if (!aoc->private_data) {
            free(aoc);
            return NULL;
        }
    }
    aoc->wrapper = wrapper;
    return aoc;
}

static void free_ao_context(ao_context_t *aoc)
{
    if (!aoc) {
        return;
    }
    if (!aoc->private_data) {
        free(aoc->private_data);
    }
    free(aoc);
    return;
}

int audio_output_start(dtaudio_output_t * ao)
{
    /*start playback */
    ao->status = AO_STATUS_RUNNING;
    return 0;
}

int audio_output_pause(dtaudio_output_t * ao)
{
    ao->status = AO_STATUS_PAUSE;
    ao_context_t *aoc = ao->aoc;
    if(aoc) {
        ao_wrapper_t *wrapper = aoc->wrapper;
        wrapper->pause(aoc);
    }
    return 0;
}

int audio_output_resume(dtaudio_output_t * ao)
{
    ao->status = AO_STATUS_RUNNING;
    ao_context_t *aoc = ao->aoc;
    ao_wrapper_t *wrapper = aoc->wrapper;
    wrapper->resume(aoc);
    return 0;
}

int audio_output_stop(dtaudio_output_t * ao)
{
    ao_context_t *aoc = ao->aoc;
    ao->status = AO_STATUS_EXIT;
    pthread_join(ao->output_thread_pid, NULL);
    if (aoc) {
        ao_wrapper_t *wrapper = aoc->wrapper;
        wrapper->stop(aoc);
        free_ao_context(aoc);
    }
    dt_info(TAG, "[%s:%d] aout stop ok \n", __FUNCTION__, __LINE__);
    return 0;
}

int audio_output_get_level(dtaudio_output_t * ao)
{
    int level = 0;
    ao_context_t *aoc = ao->aoc;
    if (aoc && ao->first_frame_rendered == 1) {
        ao_wrapper_t *wrapper = aoc->wrapper;
        wrapper->get_parameter(aoc, DTP_AO_CMD_GET_LEVEL, (unsigned long)(&level));
    }
    return level;
}

static void *audio_output_thread(void *args)
{
    dtaudio_output_t *ao = (dtaudio_output_t *) args;
    ao_context_t *aoc = NULL;
    ao_wrapper_t *wrapper = NULL;
    int rlen = 0, wlen = 0;
    dtaudio_para_t *para = ao->para;
    int render_inited = 0;

    int bytes_per_sample = 0;
    int unit_size = 0;
    uint8_t *buffer = NULL;


    int data_width = (para->data_width <= 2) ? 2 : para->data_width;
    int channels = (para->channels <= 2) ? 2 : para->channels;
    int samplerate = (para->samplerate <= 0) ? 48000 : para->samplerate;

    bytes_per_sample = data_width * channels / 8;
    unit_size = PCM_WRITE_SIZE * samplerate * bytes_per_sample / 1000;
    unit_size = unit_size - unit_size % (4 * bytes_per_sample);
    buffer = malloc(unit_size);
    if (!buffer) {
        return NULL;
    }

    for (;;) {
        if (ao->status == AO_STATUS_EXIT) {
            goto EXIT;
        }
        if (ao->status == AO_STATUS_IDLE || ao->status == AO_STATUS_PAUSE) {
            usleep(100);
            continue;
        }

        /* update audio pts */
        audio_update_pts(ao->parent);

        /*read data from filter or decode buffer */
        if (rlen <= 0) {
            rlen = audio_output_read(ao->parent, buffer, unit_size);
            if (rlen <= 0) {
                dt_debug(LOG_TAG, "pcm read failed! \n");
                usleep(1000);
                continue;
            }
#ifdef DTAUDIO_DUMP_PCM
            FILE *fp = fopen("pcm.out", "ab+");
            int flen = 0;
            if (fp) {
                flen = fwrite(buffer, 1, rlen, fp);
                if (flen <= 0) {
                    dt_info(LOG_TAG, "pcm dump failed! \n");
                }
                fclose(fp);
            } else {
                dt_error(TAG, "pcm out open failed! \n");
            }
#endif
        }

        // Here to init audio render - incase audio parameter not set
        if (render_inited == 0) {
            render_inited = 1;

            wrapper = select_ao_device(-1);
            if (!wrapper) {
                return NULL;
            }
            aoc = alloc_ao_context(wrapper);
            if (!aoc) {
                return NULL;
            }
            memcpy(&aoc->para, ao->para, sizeof(dtaudio_para_t));
            ao->aoc = aoc;
            if (wrapper->init(aoc) < 0) {
                dt_info(TAG, "[%s:%d] audio output init failed.\n", __FUNCTION__, __LINE__);
                goto EXIT;
            }
            dt_info(TAG,
                    "[%s:%d] audio output init success. dst channels[%d - %d] sample[%d - %d]\n",
                    __FUNCTION__, __LINE__, aoc->para.channels, aoc->para.dst_channels,
                    aoc->para.samplerate, aoc->para.dst_samplerate);
        }

        /*write to ao device */
        wlen = wrapper->write(aoc, buffer, rlen);
        if (wlen <= 0) {
            usleep(1000);
            continue;
        }

        if (ao->first_frame_rendered == 0) {
            ao->first_frame_rendered = 1;
        }

        rlen -= wlen;
        if (rlen > 0) {
            memmove(buffer, buffer + wlen, rlen);
        }
        wlen = 0;
    }
EXIT:
    dt_info(LOG_TAG, "[file:%s][%s:%d]ao playback thread exit\n", __FILE__,
            __FUNCTION__, __LINE__);
    if (buffer) {
        free(buffer);
    }
    buffer = NULL;
    pthread_exit(NULL);
    return NULL;

}

int audio_output_init(dtaudio_output_t * ao, int ao_id)
{
    int ret = 0;
    pthread_t tid;
    /*start aout pthread */
    ret = pthread_create(&tid, NULL, audio_output_thread, (void *) ao);
    if (ret != 0) {
        dt_error(TAG, "[%s:%d] create audio output thread failed\n", __FUNCTION__,
                 __LINE__);
        return ret;
    }
    ao->output_thread_pid = tid;
    dt_info(TAG, "[%s:%d] create audio output thread success\n", __FUNCTION__,
            __LINE__);
    return ret;
}

int64_t audio_output_get_latency(dtaudio_output_t * ao)
{
    if (ao->status == AO_STATUS_IDLE || ao->status == AO_STATUS_EXIT) {
        return 0;
    }
    if (ao->status == AO_STATUS_PAUSE) {
        return ao->last_valid_latency;
    }

    ao_context_t *aoc = ao->aoc;
    if (aoc && ao->first_frame_rendered == 1) {
        ao_wrapper_t *wrapper = aoc->wrapper;
        wrapper->get_parameter(aoc, DTP_AO_CMD_GET_LATENCY,
                               (unsigned long)(&ao->last_valid_latency));
    }
    return ao->last_valid_latency;
}
