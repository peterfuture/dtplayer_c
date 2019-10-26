#ifndef DT_HOST_H
#define DT_HOST_H

#include <stdlib.h>

#include "dt_utils.h"
#include "dthost_api.h"

typedef struct {
    dthost_para_t para;
    /*av sync */
    dtp_sync_mode_t av_sync;
    int64_t sys_time_start;
    int64_t sys_time_start_time; // First SystemTime Assignment Time(us)
    int64_t sys_time_first;      // First System Time - Equal To First_APTS OR FIRST_VPTS
    int64_t sys_time_last;       // Last System Time
    int64_t sys_time_current;    // Current System Time

    int64_t pts_audio_first;
    int64_t pts_audio_last;
    int64_t pts_audio_current;
    int64_t audio_discontinue_point;
    int     audio_discontinue_flag;
    int64_t audio_discontinue_step;
    int64_t adec_last_ms;

    int64_t pts_video_first;
    int64_t pts_video_last;
    int64_t pts_video_current;
    int64_t video_discontinue_point;
    int     video_discontinue_flag;
    int64_t video_discontinue_step;
    int64_t vdec_last_ms;

    int64_t pts_sub;

    int sync_enable;
    int sync_mode;
    int64_t av_diff;

    // Callback to player
    Host_Notify notify;

    int drop_done;
    /*a-v-s port part */
    void *port_priv;
    void *audio_priv;
    void *video_priv;
    void *sub_priv;
    void *parent;
} dthost_context_t;

int host_start(dthost_context_t * hctx);
int host_pause(dthost_context_t * hctx);
int host_resume(dthost_context_t * hctx);
int host_stop(dthost_context_t * hctx);
int host_init(dthost_context_t * hctx);
int host_video_resize(dthost_context_t * hctx, int w, int h);

int host_write_frame(dthost_context_t * hctx, dt_av_pkt_t * frame, int type);
int host_read_frame(dthost_context_t * hctx, dt_av_pkt_t ** frame, int type);

int host_sync_enable(dthost_context_t * hctx);
int64_t host_get_apts(dthost_context_t * hctx);
int64_t host_get_vpts(dthost_context_t * hctx);
int64_t host_get_spts(dthost_context_t * hctx);
int64_t host_get_systime(dthost_context_t * hctx);
int64_t host_get_avdiff(dthost_context_t * hctx);
int64_t host_get_current_time(dthost_context_t * hctx);
int host_update_apts(dthost_context_t * hctx, int64_t apts);
int host_update_vpts(dthost_context_t * hctx, int64_t vpts);
int host_update_spts(dthost_context_t * hctx, int64_t spts);
int host_reset_systime(dthost_context_t * hctx, int64_t sys_time);
int host_update_systime(dthost_context_t * hctx, int64_t sys_time);
int host_clear_discontinue_flag(dthost_context_t * hctx);
int host_get_state(dthost_context_t * hctx, host_state_t * state);
int host_get_aout_closed(dthost_context_t * hctx);
int host_get_vout_closed(dthost_context_t * hctx);
int host_get_out_closed(dthost_context_t * hctx);

#endif
