#ifndef DTVIDEO_DECODER_H
#define DTVIDEO_DECODER_H

#include "vd_wrapper.h"

void vdec_register_all();
void vdec_remove_all();
void register_vdec_ext(vd_wrapper_t *vd);
int video_decoder_init(dtvideo_decoder_t * decoder);
int video_decoder_release(dtvideo_decoder_t * decoder);
int video_decoder_stop(dtvideo_decoder_t * decoder);
int video_decoder_start(dtvideo_decoder_t * decoder);

#endif
