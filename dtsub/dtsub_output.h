#ifndef DTSUB_OUTPUT_H
#define DTSUB_OUTPUT_H

#include "so_wrapper.h"

void so_register_all();
void so_register_ext(so_wrapper_t *so);

int sub_output_init (dtsub_output_t * so, int so_id);
int sub_output_release (dtsub_output_t * so);
int sub_output_stop (dtsub_output_t * so);
int sub_output_resume (dtsub_output_t * so);
int sub_output_pause (dtsub_output_t * so);
int sub_output_start (dtsub_output_t * so);

#endif
