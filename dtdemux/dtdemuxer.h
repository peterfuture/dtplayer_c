#ifndef DEMUXER_CTRL_H
#define DEMUXER_CTRL_H

#include "dt_media_info.h"
#include "dt_av.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef enum{
    DEMUXER_INVALID = -1,
    DEMUXER_AAC,
    DEMUXER_FFMPEG,
    DEMUXER_UNSUPPORT,
}demuxer_format_t;

typedef struct demuxer_wrapper
{
    char *name;
    int id;
    int (*probe) (struct demuxer_wrapper *wrapper,void *parent);
    int (*open) (struct demuxer_wrapper * wrapper);
    int (*read_frame) (struct demuxer_wrapper * wrapper, dt_av_frame_t * frame);
    int (*setup_info) (struct demuxer_wrapper * wrapper, dt_media_info_t * info);
    int (*seek_frame) (struct demuxer_wrapper * wrapper, int timestamp);
    int (*close) (struct demuxer_wrapper * wrapper);
    void *demuxer_priv;         // point to priv context
    void *parent;               // point to parent, dtdemuxer_context_t
    struct demuxer_wrapper *next;
} demuxer_wrapper_t;

typedef struct
{
    char *file_name;
    dt_media_info_t media_info;
    demuxer_wrapper_t *demuxer;
    void *stream_priv;
    void *parent;
} dtdemuxer_context_t;

int demuxer_open (dtdemuxer_context_t * dem_ctx);
int demuxer_read_frame (dtdemuxer_context_t * dem_ctx, dt_av_frame_t * frame);
int demuxer_seekto (dtdemuxer_context_t * dem_ctx, int timestamp);
int demuxer_close (dtdemuxer_context_t * dem_ctx);

#endif
