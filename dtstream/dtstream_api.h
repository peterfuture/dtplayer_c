#ifndef DTSTREAM_API_H
#define DTSTREAM_API_H

#include "dtstream_para.h"

int dtstream_open(void **priv, dtstream_para_t * para, void *parent);
int dtstream_eof(void *priv);
int64_t dtstream_tell(void *priv);
int64_t dtstream_get_size(void *priv);
int dtstream_local(void *priv);
int dtstream_skip(void *priv, int64_t size);
int dtstream_read(void *priv, uint8_t *buf, int len);
int dtstream_seek(void *priv, int64_t pos, int whence);
int dtstream_close(void *priv);

#endif
