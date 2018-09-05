#ifndef DTPORT_H
#define DTPORT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dt_utils.h"

#include "dt_packet_queue.h"
#include "dtp_av.h"
#include "dtport_api.h"

typedef enum {
    PORT_STATUS_IDLE = -1,
    PORT_STATUS_INITING,
    PORT_STATUS_INITED,
    PORT_STATUS_RUNNING,
    PORT_STATUS_EXIT,
} port_status_t;

typedef struct {
    dt_packet_queue_t queue_audio;
    dt_packet_queue_t queue_video;
    dt_packet_queue_t queue_subtitle;

    buf_state_t dps_audio;
    buf_state_t dps_video;
    buf_state_t dps_sub;

    dtport_para_t param;
    port_status_t status;
    void *parent;
} dtport_context_t;

int port_stop(dtport_context_t * pctx);
int port_init(dtport_context_t * pctx, dtport_para_t * para);
int port_write_frame(dtport_context_t * pctx, dt_av_pkt_t * frame, int type);
int port_read_frame(dtport_context_t * pctx, dt_av_pkt_t ** frame, int type);
int port_get_state(dtport_context_t * pctx, buf_state_t * buf_state, int type);

#endif
