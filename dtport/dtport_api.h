#ifndef DTPORT_API_H
#define DTPORT_API_H

#include "dtp_av.h"
#include "dtp_state.h"

typedef struct {
    int has_video;
    int has_audio;
    int has_subtitle;
} dtport_para_t;

int dtport_stop(void *port);
int dtport_init(void **port, dtport_para_t * para, void *parent);
int dtport_write_frame(void *port, dt_av_pkt_t * frame, int type);
int dtport_read_frame(void *port, dt_av_pkt_t ** frame, int type);
int dtport_get_state(void *port, buf_state_t * buf_state, int type);

#endif
