#ifndef DTVIDEO_OUTPUT_H
#define DTVIDEO_OUTPUT_H

#include "vo_wrapper.h"

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
