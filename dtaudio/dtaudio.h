#ifndef DT_AUDIO_H
#define DT_AUDIO_H

#include "dtp_av.h"
#include "dt_utils.h"
#include "dt_event.h"
#include "dtaudio_api.h"

#include "dtp_audio_plugin.h"
#include "dtaudio_decoder.h"
#include "dtaudio_filter.h"
#include "dtaudio_output.h"
#include "dthost_api.h"

#include <pthread.h>
#include <unistd.h>

#define DTAUDIO_LOG_TAG "dtaudio"
// pcm out buffer size
#define DTAUDIO_PCM_BUF_SIZE_MS 1000
//max out size for decode one time
#define MAX_DECODED_OUT_SIZE 19200

typedef struct {
    int audio_decoder_format;
    int audio_filter_flag;
    int audio_output_id;
} dtaudio_ctrl_t;

typedef enum {
    AUDIO_STATUS_IDLE,
    AUDIO_STATUS_INITING,
    AUDIO_STATUS_INITED,
    AUDIO_STATUS_RUNNING,
    AUDIO_STATUS_ACTIVE,
    AUDIO_STATUS_PAUSED,
    AUDIO_STATUS_STOP,
    AUDIO_STATUS_TERMINATED,
} dtaudio_status_t;

typedef struct {
    /*param */
    dtaudio_para_t audio_param;
    /*ctrl */
    dtaudio_ctrl_t audio_ctrl;
    /*buf */
    //dt_packet_list_t *audio_pack_list;
    dt_buffer_t audio_decoded_buf;
    dt_buffer_t audio_filtered_buf;
    /*module */
    dtaudio_decoder_t audio_dec;
    dtaudio_filter_t audio_filt;
    dtaudio_output_t audio_out;
    //dtaudio_pts_t    audio_pts;
    /*other */
    pthread_t event_loop_id;
    dtaudio_status_t audio_state;
    void *audio_service;
    void *dtport_priv;          //data source
    void *parent;               //dtcodec
} dtaudio_context_t;

void audio_register_all();
void audio_remove_all();
void register_ext_ao(ao_wrapper_t *ao);

int audio_read_frame(void *priv, dt_av_pkt_t ** frame);
int audio_output_read(void *priv, uint8_t * buf, int size);
int64_t audio_get_current_pts(dtaudio_context_t * actx);
int audio_first_frame_decoded(dtaudio_context_t * actx);
int64_t audio_get_first_pts(dtaudio_context_t * actx);
int audio_host_ioctl(void *priv, int cmd, unsigned long arg);
int audio_drop(dtaudio_context_t * actx, int64_t target_pts);
void audio_update_pts(void *priv);
int audio_get_dec_state(dtaudio_context_t * actx, dec_state_t * dec_state);
int audio_get_out_closed(dtaudio_context_t * actx);
int audio_start(dtaudio_context_t * actx);
int audio_pause(dtaudio_context_t * actx);
int audio_resume(dtaudio_context_t * actx);
int audio_stop(dtaudio_context_t * actx);
int audio_init(dtaudio_context_t * actx);

#endif
