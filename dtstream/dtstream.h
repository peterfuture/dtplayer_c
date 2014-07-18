#ifndef DTSTREAM_H
#define DTSTREAM_H

#include "stream_wrapper.h"

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
