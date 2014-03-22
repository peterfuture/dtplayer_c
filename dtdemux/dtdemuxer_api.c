#include "dtdemuxer_api.h"
#include "dtdemuxer.h"

#define TAG "DEMUX-API"
int dtdemuxer_open (void **priv, dtdemuxer_para_t * para, void *parent)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) malloc (sizeof (dtdemuxer_context_t));
    if (!dem_ctx)
    {
        dt_error (TAG, "demuxer context malloc failed \n");
        return -1;
    }
    memset (dem_ctx, 0, sizeof (dtdemuxer_context_t));
    dem_ctx->file_name = para->file_name;
    if (demuxer_open (dem_ctx) == -1)
    {
        dt_error (TAG, "demuxer context open failed \n");
        free(dem_ctx);
        return -1;
    }

    *priv = (void *) dem_ctx;
    dem_ctx->parent = parent;
    dt_info (TAG, "demuxer context open success \n");
    return 0;
}

dt_media_info_t *dtdemuxer_get_media_info (void *priv)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) priv;
    return &(dem_ctx->media_info);
}

int dtdemuxer_read_frame (void *priv, dt_av_frame_t * frame)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) priv;
    return demuxer_read_frame (dem_ctx, frame);
}

int dtdemuxer_seekto (void *priv, int timestamp)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) priv;
    return demuxer_seekto (dem_ctx, timestamp);
}

int dtdemuxer_close (void *priv)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) priv;
    demuxer_close (dem_ctx);
    dt_info (TAG, "dtdemuxer module close ok\n");
    return 0;
}
