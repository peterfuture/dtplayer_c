#ifndef DT_AUDIO_DECODER_H
#define DT_AUDIO_DECODER_H

#include "ad_wrapper.h"

void adec_register_all();
void adec_remove_all();
int audio_decoder_init(dtaudio_decoder_t * decoder);
int audio_decoder_release(dtaudio_decoder_t * decoder);
int audio_decoder_stop(dtaudio_decoder_t * decoder);
int audio_decoder_start(dtaudio_decoder_t * decoder);
int64_t audio_decoder_get_pts(dtaudio_decoder_t * decoder);

#endif
