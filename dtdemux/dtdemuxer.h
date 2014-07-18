#ifndef DEMUXER_CTRL_H
#define DEMUXER_CTRL_H

#include "demuxer_wrapper.h"
#include "dtstream_api.h"

#define PROBE_LOCAL_SIZE 1024*1024*10 //10M
#define PROBE_STREAM_SIZE 1024*10 //10K

void demuxer_register_all();
int demuxer_open (dtdemuxer_context_t * dem_ctx);
int demuxer_read_frame (dtdemuxer_context_t * dem_ctx, dt_av_frame_t * frame);
int demuxer_seekto (dtdemuxer_context_t * dem_ctx, int64_t timestamp);
int demuxer_close (dtdemuxer_context_t * dem_ctx);

#endif
