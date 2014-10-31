#ifndef DTVIDEO_H
#define DTVIDEO_H

#include "dt_av.h"
#include "dtvideo_api.h"
#include "dt_buffer.h"
#include "dt_queue.h"
#include "dtvideo_decoder.h"
#include "dtvideo_filter.h"
#include "dtvideo_output.h"
#if 0
#include "dtvideo_filter.h"
#include "dtvideo_output.h"
#endif

#include <pthread.h>
#include <unistd.h>

#define DTVIDEO_BUF_SIZE 1024*1024

typedef enum
{
    VIDEO_STATUS_IDLE,
    VIDEO_STATUS_INITING,
    VIDEO_STATUS_INITED,
    VIDEO_STATUS_RUNNING,
    VIDEO_STATUS_ACTIVE,
    VIDEO_STATUS_PAUSED,
    VIDEO_STATUS_STOPPED,
    VIDEO_STATUS_TERMINATED,
} dtvideo_status_t;

typedef struct
{
    /*param */
    dtvideo_para_t video_para;
    //dt_buffer_t video_decoded_buf;
    dt_buffer_t video_filtered_buf;
    /*module */
    dtvideo_decoder_t video_dec;
    dtvideo_filter_t video_filt;
    dtvideo_output_t video_out;
    
    queue_t *vo_queue;

    /*pts relative */
    int64_t first_pts;
    int64_t current_pts;
    int64_t last_valid_pts;
    /*other */
    pthread_t msg_thread_pid;
    dtvideo_status_t video_status;

    int event_loop_id;
    void *video_server;
    void *parent;               //dthost
} dtvideo_context_t;

void video_register_all();
void video_remove_all();
void register_ext_vd(vd_wrapper_t *vo);
void register_ext_vf(vf_wrapper_t *vo);
void register_ext_vo(vo_wrapper_t *vo);

int dtvideo_read_frame (void *priv, dt_av_pkt_t * frame);
dt_av_frame_t *dtvideo_output_read (void *priv);
dt_av_frame_t *dtvideo_output_pre_read (void *priv);
int dtvideo_get_avdiff (void *priv);
int64_t dtvideo_get_current_pts (dtvideo_context_t * vctx);
int64_t video_get_first_pts (dtvideo_context_t * vctx);
int video_drop (dtvideo_context_t * vctx, int64_t target_pts);
int64_t dtvideo_get_systime (void *priv);
void dtvideo_update_systime (void *priv, int64_t sys_time);
void dtvideo_update_pts (void *priv);
int video_get_dec_state (dtvideo_context_t * vctx, dec_state_t * dec_state);
int video_get_out_closed (dtvideo_context_t * vctx);
int video_start (dtvideo_context_t * vctx);
int video_pause (dtvideo_context_t * vctx);
int video_resume (dtvideo_context_t * vctx);
int video_stop (dtvideo_context_t * vctx);
int video_init (dtvideo_context_t * vctx);
int video_resize (dtvideo_context_t * vctx, int w, int h);

#endif
