#ifndef DTAUDIO_OUTPUT_H
#define DTAUDIO_OUTPUT_H

#include "ao_wrapper.h"

#define LOG_TAG "DTAUDIO-OUTPUT"
#define PCM_WRITE_SIZE 1024

void aout_register_all();
void aout_register_ext(ao_wrapper_t *ao);

int audio_output_init (dtaudio_output_t * ao, int ao_id);
int audio_output_release (dtaudio_output_t * ao);
int audio_output_stop (dtaudio_output_t * ao);
int audio_output_resume (dtaudio_output_t * ao);
int audio_output_pause (dtaudio_output_t * ao);
int audio_output_start (dtaudio_output_t * ao);

int audio_output_latency (dtaudio_output_t * ao);
int64_t audio_output_get_latency (dtaudio_output_t * ao);
int audio_output_get_level (dtaudio_output_t * ao);

#endif
