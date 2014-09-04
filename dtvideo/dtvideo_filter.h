#ifndef DTVIDEO_FILT_H
#define DTVIDEO_FILT_H

#include "vf_wrapper.h"

void vf_register_all();
void vf_register_ext(vf_wrapper_t *vf);

int video_filter_reset (dtvideo_filter_t * filter, dtvideo_para_t *para);
int video_filter_process (dtvideo_filter_t * filter, dt_av_pic_t *pic);
int video_filter_stop (dtvideo_filter_t * filter);

#endif
