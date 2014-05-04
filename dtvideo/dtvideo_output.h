#ifndef DTVIDEO_OUTPUT_H
#define DTVIDEO_OUTPUT_H

#include "dtvideo_api.h"
#include "dt_av.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>

typedef struct vo_wrapper
{
    int id;
    char *name;

    int (*vo_init) ();
    int (*vo_stop) ();
    int (*vo_render) (AVPicture_t * pic);
    void *handle;
    struct vo_wrapper *next;
    void *vo_priv;
} vo_wrapper_t;

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
    VO_ID_EX,            // ex vo need set to 0 default
    VO_ID_SDL,
    VO_ID_X11,
    VO_ID_FB,
    VO_ID_GL,
    VO_ID_DIRECTX,
    VO_ID_SDL2,
} dt_vo_t;

typedef struct
{
    int vout_buf_size;
    int vout_buf_level;
} vo_state_t;

typedef struct dtvideo_output
{
    /*param */
    dtvideo_para_t para;
    vo_wrapper_t *wrapper;
    vo_status_t status;
    pthread_t output_thread_pid;
    vo_state_t state;

    uint64_t last_valid_latency;
    void *parent;               //point to dtaudio_t, can used for param of pcm get interface
}dtvideo_output_t;

void vout_register_all();
void vout_register_ext(vo_wrapper_t *vo);

int video_output_init (dtvideo_output_t * vo, int vo_id);
int video_output_release (dtvideo_output_t * vo);
int video_output_stop (dtvideo_output_t * vo);
int video_output_resume (dtvideo_output_t * vo);
int video_output_pause (dtvideo_output_t * vo);
int video_output_start (dtvideo_output_t * vo);

uint64_t video_output_get_latency (dtvideo_output_t * vo);
int video_output_get_level (dtvideo_output_t * vo);
#endif
