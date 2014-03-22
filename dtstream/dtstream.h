#ifndef DTSTREAM_H
#define DTSTREAM_H

#include "dt_av.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct stream_wrapper
{
    char *name;
    int (*open) (struct stream_wrapper * wrapper, char *stream_name, void *parent);
    int (*read) (struct stream_wrapper * wrapper, char *buf,int len);
    int (*seek) (struct stream_wrapper * wrapper, int64_t pos);
    int (*close) (struct stream_wrapper * wrapper);
    void *stream_priv;          // point to priv context
    void *parent;               // point to parent, dtstream_context_t
    struct stream_wrapper *next;
} stream_wrapper_t;

typedef struct
{
    char *stream_name;
    stream_wrapper_t *stream;
    void *parent;
} dtstream_context_t;

int stream_open (dtstream_context_t * stm_ctx);
int stream_read (dtstream_context_t * stm_ctx, char *buf,int len);
int stream_seek (dtstream_context_t * stm_ctx, int64_t pos);
int stream_close (dtstream_context_t * stm_ctx);

#endif
