#ifndef DTSUB_OUTPUT_H
#define DTSUB_OUTPUT_H

#include "dtp_sub_plugin.h"

typedef enum {
    SO_STATUS_IDLE,
    SO_STATUS_PAUSE,
    SO_STATUS_RUNNING,
    SO_STATUS_EXIT,
} so_status_t;

typedef struct {
    int sout_buf_size;
    int sout_buf_level;
} so_state_t;

typedef struct dtsub_output {
    dtsub_para_t *para;
    so_context_t *soc;
    so_status_t status;
    pthread_t output_thread_pid;
    so_state_t state;

    void *parent;               //point to dtsub_output_t
} dtsub_output_t;

void so_register_all();
void so_remove_all();
void so_register_ext(so_wrapper_t *so);

int sub_output_init(dtsub_output_t * so, int so_id);
int sub_output_release(dtsub_output_t * so);
int sub_output_stop(dtsub_output_t * so);
int sub_output_resume(dtsub_output_t * so);
int sub_output_pause(dtsub_output_t * so);
int sub_output_start(dtsub_output_t * so);

#endif
