#ifndef DTAUDIO_OUTPUT_H
#define DTAUDIO_OUTPUT_H

#include "dt_av.h"
#include "dtaudio_api.h"

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define LOG_TAG "DTAUDIO-OUTPUT"
#define PCM_WRITE_SIZE 1024

typedef struct ao_wrapper
{
    int id;
    char *name;
    dtaudio_para_t para;
    
    int (*ao_init) ();
    int (*ao_start) ();
    int (*ao_pause) ();
    int (*ao_resume) ();
    int (*ao_stop) ();
    int64_t (*ao_latency) ();
    int (*ao_level) ();
    int (*ao_write) (uint8_t * buf, int size);
    struct ao_wrapper *next;
    void *ao_priv;
} ao_wrapper_t;

#define dtao_format_t ao_id_t

typedef enum
{
    AO_STATUS_IDLE,
    AO_STATUS_PAUSE,
    AO_STATUS_RUNNING,
    AO_STATUS_EXIT,
} ao_status_t;

typedef enum _AO_CTL_ID_
{
    AO_GET_VOLUME,
    AO_ADD_VOLUME,
    AO_SUB_VOLUME,
    AO_CMD_PAUSE,
    AO_CMD_RESUME,
} ao_cmd_t;

typedef struct
{
    int aout_buf_size;
    int aout_buf_level;
} ao_state_t;

typedef struct dtaudio_output
{
    /*para */
    dtaudio_para_t para;
    ao_wrapper_t *aout_ops;
    ao_status_t status;
    pthread_t output_thread_pid;
    ao_state_t state;

    uint64_t last_valid_latency;
    void *parent;               //point to dtaudio_t, can used for param of pcm get interface
}dtaudio_output_t;

void aout_register_all();
void aout_register_ext(ao_wrapper_t *ao);

int audio_output_init (dtaudio_output_t * ao, int ao_id);
int audio_output_release (dtaudio_output_t * ao);
int audio_output_stop (dtaudio_output_t * ao);
int audio_output_resume (dtaudio_output_t * ao);
int audio_output_pause (dtaudio_output_t * ao);
int audio_output_start (dtaudio_output_t * ao);

int audio_output_latency (dtaudio_output_t * ao);
int64_t audio_output_get_latency (dtaudio_output_t * ao);
int audio_output_get_level (dtaudio_output_t * ao);

#endif
