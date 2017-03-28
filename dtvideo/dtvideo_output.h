#ifndef DTVIDEO_OUTPUT_H
#define DTVIDEO_OUTPUT_H

#include "dtp_video_plugin.h"

typedef enum {
    VO_STATUS_IDLE,
    VO_STATUS_PAUSE,
    VO_STATUS_RUNNING,
    VO_STATUS_EXIT,
} vo_status_t;

typedef struct {
    int vout_buf_size;
    int vout_buf_level;
} vo_state_t;

typedef struct dtvideo_output {
    /*param */
    dtvideo_para_t para;
    vo_context_t *voc;
    vo_status_t status;
    pthread_t output_thread_pid;
    vo_state_t state;

    uint64_t last_valid_latency;
    void *parent;               //point to dtvideo_output_t
} dtvideo_output_t;


void vout_register_all();
void vout_remove_all();
void vout_register_ext(vo_wrapper_t *vo);

int video_output_init(dtvideo_output_t * vo, int vo_id);
int video_output_release(dtvideo_output_t * vo);
int video_output_stop(dtvideo_output_t * vo);
int video_output_resume(dtvideo_output_t * vo);
int video_output_pause(dtvideo_output_t * vo);
int video_output_start(dtvideo_output_t * vo);

uint64_t video_output_get_latency(dtvideo_output_t * vo);
int video_output_get_level(dtvideo_output_t * vo);

#endif
