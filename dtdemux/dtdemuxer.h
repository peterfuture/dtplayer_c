#ifndef DEMUXER_CTRL_H
#define DEMUXER_CTRL_H

#include "dt_utils.h"
#include "demuxer_wrapper.h"

#define PROBE_LOCAL_SIZE 1024*1024*10 //10M
#define PROBE_STREAM_SIZE 1024*10 //10K

void demuxer_register_all();
void demuxer_remove_all();
int demuxer_open(dtdemuxer_context_t * dem_ctx);
int demuxer_read_frame(dtdemuxer_context_t * dem_ctx, dt_av_pkt_t ** frame);
int demuxer_seekto(dtdemuxer_context_t * dem_ctx, int64_t timestamp);
int demuxer_close(dtdemuxer_context_t * dem_ctx);

#endif
