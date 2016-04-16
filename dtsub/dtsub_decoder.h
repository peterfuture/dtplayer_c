#ifndef DTSUB_DECODER_H
#define DTSUB_DECODER_H

#include "sd_wrapper.h"

void sd_register_all();
void sd_remove_all();
void register_sd_ext(sd_wrapper_t *sd);
int sub_decoder_init(dtsub_decoder_t * decoder);
int sub_decoder_release(dtsub_decoder_t * decoder);
int sub_decoder_stop(dtsub_decoder_t * decoder);
int sub_decoder_start(dtsub_decoder_t * decoder);

#endif
