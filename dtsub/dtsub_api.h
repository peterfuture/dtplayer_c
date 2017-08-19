#ifndef DTSUB_API_H
#define DTSUB_API_H

#include "dtp_sub_plugin.h"
#include "dtp_state.h"

int dtsub_init(void **sub_priv, dtsub_para_t * para, void *parent);
int dtsub_start(void *sub_priv);
int dtsub_pause(void *sub_priv);
int dtsub_resume(void *sub_priv);
int dtsub_stop(void *sub_priv);

int dtsub_read_pkt(void *priv, dt_av_pkt_t * pkt);
dt_av_frame_t *dtsub_output_read(void *priv);
dt_av_frame_t *dtsub_output_pre_read(void *priv);

int64_t dtsub_get_systime(void *priv);
void dtsub_update_pts(void *priv);
int64_t dtsub_get_pts(void *sub_priv);
int64_t dtsub_get_first_pts(void *sub_priv);
int dtsub_drop(void *sub_priv, int64_t target_pts);
int dtsub_get_state(void *sub_priv, dec_state_t * dec_state);
int dtsub_get_out_closed(void *sub_priv);

#endif
