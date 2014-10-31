#ifndef DTSUB_FILT_H
#define DTSUB_FILT_H

#include "sf_wrapper.h"

void sf_register_all();
void sf_remove_all();
void sf_register_ext(sf_wrapper_t *sf);

int sub_filter_init(dtsub_filter_t * filter);
int sub_filter_update(dtsub_filter_t * filter);
int sub_filter_process(dtsub_filter_t * filter, dt_av_frame_t *frame);
int sub_filter_stop(dtsub_filter_t * filter);

#endif
