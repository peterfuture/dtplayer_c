#ifndef DTVIDEO_API_H
#define DTVIDEO_API_H

#include <stdint.h>

#include "dtp_state.h"
#include "dtp_video_plugin.h"

int dtvideo_init(void **video_priv, dtvideo_para_t * para, void *parent);
int dtvideo_start(void *video_priv);
int dtvideo_pause(void *video_priv);
int dtvideo_resume(void *video_priv);
int dtvideo_stop(void *video_priv);
int dtvideo_resize(void *video_priv, int w, int h);

int64_t dtvideo_external_get_pts(void *video_priv);
int dtvideo_get_first_pts(void *video_priv, int64_t *pts);
int dtvideo_drop(void *video_priv, int64_t target_pts);
int dtvideo_get_state(void *video_priv, dec_state_t * dec_state);
int dtvideo_get_out_closed(void *video_priv);

#endif
