#ifndef DT_HOST_H
#define DT_HOST_H

#include "dthost_api.h"
#include "dt_av.h"

#include <stdlib.h>

typedef struct
{
    dthost_para_t para;
    /*av sync */
    dt_sync_mode_t av_sync;
    int64_t sys_time;
    int64_t pts_audio;
    int64_t pts_video;
    int sync_enable;
    int sync_mode;
    int64_t av_diff;
    /*a-v-s port part */
    void *port_priv;
    void *audio_priv;
    void *video_priv;
} dthost_context_t;

int host_start (dthost_context_t * hctx);
int host_pause (dthost_context_t * hctx);
int host_resume (dthost_context_t * hctx);
int host_stop (dthost_context_t * hctx);
int host_init (dthost_context_t * hctx);
int host_write_frame (dthost_context_t * hctx, dt_av_frame_t * frame, int type);
int host_read_frame (dthost_context_t * hctx, dt_av_frame_t * frame, int type);

int host_sync_enable (dthost_context_t * hctx);
int64_t host_get_apts (dthost_context_t * hctx);
int64_t host_get_vpts (dthost_context_t * hctx);
int64_t host_get_systime (dthost_context_t * hctx);
int64_t host_get_avdiff (dthost_context_t * hctx);
int64_t host_get_current_time (dthost_context_t * hctx);
int host_update_apts (dthost_context_t * hctx, int64_t apts);
int host_update_vpts (dthost_context_t * hctx, int64_t vpts);
int host_update_systime (dthost_context_t * hctx, int64_t sys_time);
int host_get_state (dthost_context_t * hctx, host_state_t * state);
int host_get_out_closed (dthost_context_t * hctx);

#endif
