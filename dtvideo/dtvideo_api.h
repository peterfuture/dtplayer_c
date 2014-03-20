#ifndef DTVIDEO_API_H
#define DTVIDEO_API_H

#include "dt_state.h"
#include <stdint.h>

#define VIDEO_EXTRADATA_SIZE 4096
typedef struct
{
    int vfmt;
    int d_width;                //dest w
    int d_height;               //dest h
    int s_width;                //src w
    int s_height;               //src h
    int d_pixfmt;               //dest pixel format
    int s_pixfmt;               //src pixel format
    int rate;
    int ratio;
    double fps;
    int num, den;               //for pts calc
    int extradata_size;
    unsigned char extradata[VIDEO_EXTRADATA_SIZE];
    int video_filter;
    int video_output;
    void *avctx_priv;
} dtvideo_para_t;

int dtvideo_init (void **video_priv, dtvideo_para_t * para, void *parent);
int dtvideo_start (void *video_priv);
int dtvideo_pause (void *video_priv);
int dtvideo_resume (void *video_priv);
int dtvideo_stop (void *video_priv);
int64_t dtvideo_external_get_pts (void *video_priv);
int64_t dtvideo_get_first_pts (void *video_priv);
int dtvideo_drop (void *video_priv, int64_t target_pts);
int dtvideo_get_state (void *video_priv, dec_state_t * dec_state);
int dtvideo_get_out_closed (void *video_priv);

#endif
