#ifndef DTDEMUXER_API_H
#define DTDEMUXER_API_H

#include "dt_av.h"
#include "dt_media_info.h"

typedef struct
{
    char *file_name;
} dtdemuxer_para_t;

int dtdemuxer_open (void **priv, dtdemuxer_para_t * para, void *parent);
dt_media_info_t *dtdemuxer_get_media_info (void *priv);
int dtdemuxer_read_frame (void *priv, dt_av_frame_t * frame);
int dtdemuxer_seekto (void *priv, int timestamp);
int dtdemuxer_close (void *priv);

#endif
