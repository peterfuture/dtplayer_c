#ifndef DTVIDEO_H
#define DTVIDEO_H

#include <pthread.h>
#include <unistd.h>

#include "dt_utils.h"

#include "dtp_av.h"
#include "dtvideo_api.h"
#include "dtvideo_decoder.h"
#include "dtvideo_output.h"
#include "dthost_api.h"

#define DTVIDEO_BUF_SIZE 1024*1024

typedef enum {
    VIDEO_STATUS_IDLE,
    VIDEO_STATUS_INITING,
    VIDEO_STATUS_INITED,
    VIDEO_STATUS_RUNNING,
    VIDEO_STATUS_ACTIVE,
    VIDEO_STATUS_PAUSED,
    VIDEO_STATUS_STOPPED,
    VIDEO_STATUS_TERMINATED,
} dtvideo_status_t;

typedef struct {
    /*param */
    dtvideo_para_t video_para;
    /*module */
    dtvideo_decoder_t video_dec;
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
void register_ext_vo(vo_wrapper_t *vo);

int dtvideo_read_frame(void *priv, dt_av_pkt_t ** frame);
dt_av_frame_t *dtvideo_output_read(void *priv);
dt_av_frame_t *dtvideo_output_pre_read(void *priv);
int dtvideo_get_avdiff(void *priv);
int64_t dtvideo_get_current_pts(dtvideo_context_t * vctx);
int video_first_frame_decoded(dtvideo_context_t * vctx);
int64_t video_get_first_pts(dtvideo_context_t * vctx);
int video_host_ioctl(void *priv, int cmd, unsigned long arg);
int video_drop(dtvideo_context_t * vctx, int64_t target_pts);
int64_t dtvideo_get_systime(void *priv);
void dtvideo_update_systime(void *priv, int64_t sys_time);
void dtvideo_update_pts(void *priv);
int video_get_dec_state(dtvideo_context_t * vctx, dec_state_t * dec_state);
int video_get_out_closed(dtvideo_context_t * vctx);
int video_start(dtvideo_context_t * vctx);
int video_pause(dtvideo_context_t * vctx);
int video_resume(dtvideo_context_t * vctx);
int video_stop(dtvideo_context_t * vctx);
int video_init(dtvideo_context_t * vctx);
int video_resize(dtvideo_context_t * vctx, int w, int h);

#endif
