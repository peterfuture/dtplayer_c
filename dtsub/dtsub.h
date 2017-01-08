#ifndef DTSUB_H
#define DTSUB_H

#include "dt_utils.h"

#include "dtp_av.h"
#include "dtsub_api.h"
#include "dtsub_decoder.h"
#include "dtsub_filter.h"
#include "dtsub_output.h"

typedef enum {
    SUB_STATUS_IDLE,
    SUB_STATUS_INITING,
    SUB_STATUS_INITED,
    SUB_STATUS_RUNNING,
    SUB_STATUS_ACTIVE,
    SUB_STATUS_PAUSED,
    SUB_STATUS_STOPPED,
    SUB_STATUS_TERMINATED,
} dtsub_status_t;

typedef struct {
    dtsub_para_t sub_para;

    dtsub_decoder_t sub_dec;
    dtsub_filter_t sub_filt;
    dtsub_output_t sub_out;

    int64_t first_pts;
    int64_t current_pts;
    int64_t last_valid_pts;

    queue_t *so_queue;
    dtsub_status_t sub_status;

    void *parent;
} dtsub_context_t;

void sub_register_all();
void sub_remove_all();
void register_ext_sd(sd_wrapper_t *sd);
void register_ext_so(so_wrapper_t *so);
void register_ext_sf(sf_wrapper_t *sf);
int64_t sub_get_current_pts(dtsub_context_t * sctx);
int64_t sub_get_first_pts(dtsub_context_t * sctx);
int sub_drop(dtsub_context_t * sctx, int64_t target_pts);
int sub_get_dec_state(dtsub_context_t * sctx, dec_state_t * dec_state);
int sub_get_out_closed(dtsub_context_t * sctx);

int sub_start(dtsub_context_t * sctx);
int sub_pause(dtsub_context_t * sctx);
int sub_resume(dtsub_context_t * sctx);
int sub_stop(dtsub_context_t * sctx);
int sub_init(dtsub_context_t * sctx);

#endif
