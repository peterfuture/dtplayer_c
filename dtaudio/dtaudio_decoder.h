#ifndef DT_AUDIO_DECODER_H
#define DT_AUDIO_DECODER_H

#include "dt_buffer.h"
#include "dt_av.h"
#include "dtaudio_api.h"

typedef enum
{
    ADEC_STATUS_IDLE,
    ADEC_STATUS_RUNNING,
    ADEC_STATUS_PAUSED,
    ADEC_STATUS_EXIT
} adec_status_t;

typedef struct dtaudio_decoder dtaudio_decoder_t;

typedef struct{
    uint8_t *inptr;
    int inlen;
    int consume;
    uint8_t *outptr;
    int outsize; // buffer size
    int outlen;  // buffer level

    int info_change;
    int channels;
    int samplerate;
    int bps;
}adec_ctrl_t;

typedef struct dec_audio_wrapper
{
    int (*init) (struct dec_audio_wrapper * wrapper,void *parent);
    int (*decode_frame) (struct dec_audio_wrapper * wrapper, adec_ctrl_t *pinfo);
    int (*release) (struct dec_audio_wrapper * wrapper);
    char *name;
    audio_format_t afmt;        //not used, for ffmpeg
    int type;
    void *adec_priv;
    struct dec_audio_wrapper *next;
    void *parent;
} dec_audio_wrapper_t;

struct dtaudio_decoder
{
    dtaudio_para_t aparam;
    dec_audio_wrapper_t *dec_wrapper;
    pthread_t audio_decoder_pid;
    adec_status_t status;
    int decode_err_cnt;
    int decode_offset;

    int64_t pts_current;
    int64_t pts_first;
    int64_t pts_last_valid;
    int pts_buffer_size;
    int pts_cache_size;
    
    adec_ctrl_t info;
    dt_buffer_t *buf_out;
    void *parent;
    void *decoder_priv;         //point to avcodeccontext
};

void adec_register_all();
int audio_decoder_init (dtaudio_decoder_t * decoder);
int audio_decoder_release (dtaudio_decoder_t * decoder);
int audio_decoder_stop (dtaudio_decoder_t * decoder);
int audio_decoder_start (dtaudio_decoder_t * decoder);
int64_t audio_decoder_get_pts (dtaudio_decoder_t * decoder);

#endif
