#ifndef DTDEMUXER_API_H
#define DTDEMUXER_API_H

#include "demuxer_wrapper.h"

int dtdemuxer_open(void **priv, dtdemuxer_para_t * para, void *parent);
int dtdemuxer_get_media_info(void *priv, dtp_media_info_t **info);
int dtdemuxer_read_frame(void *priv, dt_av_pkt_t ** frame);
int dtdemuxer_seekto(void *priv, int64_t timestamp);
int dtdemuxer_close(void *priv);

#endif
