#ifndef DTVIDEO_OUTPUT_H
#define DTVIDEO_OUTPUT_H

#include "dtvideo_api.h"
#include "dt_av.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>

typedef struct dtvideo_output dtvideo_output_t;

typedef struct _vo_t
{
    int id;
    char *name;

    int (*vo_init) (dtvideo_output_t * ao);
    int (*vo_stop) (dtvideo_output_t * ao);
    int (*vo_render) (dtvideo_output_t * ao, AVPicture_t * pic);
    void *handle;
    struct _vo_t *next;
} vo_operations_t;

typedef enum
{
    VO_STATUS_IDLE,
    VO_STATUS_PAUSE,
    VO_STATUS_RUNNING,
    VO_STATUS_EXIT,
} vo_status_t;

typedef enum _VO_ID_
{
    VO_ID_EXAMPLE = -1,
    VO_ID_SDL,
    VO_ID_X11,
    VO_ID_FB,
    VO_ID_GL,
    VO_ID_DIRECTX,
} dt_vo_t;

typedef struct
{
    int vout_buf_size;
    int vout_buf_level;
} vo_state_t;

struct dtvideo_output
{
    /*param */
    dtvideo_para_t para;
    vo_operations_t *vout_ops;
    vo_status_t status;
    pthread_t output_thread_pid;
    vo_state_t state;

    uint64_t last_valid_latency;
    void *parent;               //point to dtaudio_t, can used for param of pcm get interface
};

int video_output_init (dtvideo_output_t * vo, int vo_id);
int video_output_release (dtvideo_output_t * vo);
int video_output_stop (dtvideo_output_t * vo);
int video_output_resume (dtvideo_output_t * vo);
int video_output_pause (dtvideo_output_t * vo);
int video_output_start (dtvideo_output_t * vo);

uint64_t video_output_get_latency (dtvideo_output_t * vo);
int video_output_get_level (dtvideo_output_t * vo);
#endif
