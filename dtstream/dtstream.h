#ifndef DTSTREAM_H
#define DTSTREAM_H

#include "dt_av.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef enum{
    STREAM_INVALID = -1,
    STREAM_FILE,
    STREAM_FFMPEG,
    STREAM_UNSUPPORT
}stream_format_t;

typedef struct{
    int is_stream;
    int seek_support;
    int64_t cur_pos;
    int64_t stream_size;
    int eof_flag;
}stream_ctrl_t;

typedef struct stream_wrapper
{
    char *name;
    int id;
    int (*open) (struct stream_wrapper * wrapper,char *stream_name);
    int64_t (*tell) (struct stream_wrapper * wrapper);
    int (*read) (struct stream_wrapper * wrapper, uint8_t *buf,int len);
    int (*seek) (struct stream_wrapper * wrapper, int64_t pos, int whence);
    int (*close) (struct stream_wrapper * wrapper);
    void *stream_priv;          // point to priv context
    void *parent;               // point to parent, dtstream_context_t
    stream_ctrl_t info;
    struct stream_wrapper *next;
} stream_wrapper_t;

typedef struct
{
    char *stream_name;
    stream_wrapper_t *stream;
    void *parent;
} dtstream_context_t;

void stream_register_all();
int stream_open (dtstream_context_t * stm_ctx);
int stream_eof (dtstream_context_t * stm_ctx);
int64_t stream_tell (dtstream_context_t *stm_ctx);
int64_t stream_get_size (dtstream_context_t * stm_ctx);
int stream_local (dtstream_context_t * stm_ctx);
int stream_read (dtstream_context_t * stm_ctx, uint8_t *buf,int len);
int stream_seek (dtstream_context_t * stm_ctx, int64_t pos,int whence);
int stream_close (dtstream_context_t * stm_ctx);

#endif
