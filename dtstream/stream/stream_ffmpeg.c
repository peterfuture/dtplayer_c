#ifdef ENABLE_STREAM_FFMPEG

#include "dtstream.h"
#include "dt_error.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

#define TAG "STREAM-FFMPEG"

static const char prefix[] = "ffmpeg://";

static int stream_ffmpeg_open (stream_wrapper_t * wrapper,char *stream_name)
{
    const char *filename = stream_name;
    AVIOContext *ctx = NULL;
    int64_t size;
    int dummy;
    int flags = AVIO_FLAG_READ;
    stream_ctrl_t *info = &wrapper->info;

    av_register_all();
    avformat_network_init();

    if (!strncmp(filename, prefix, strlen(prefix)))
        filename += strlen(prefix);
    dummy = !strncmp(filename, "rtsp:", 5);
    dt_info(TAG, "[ffmpeg] Opening %s\n", filename);
    if (!dummy && avio_open(&ctx, filename, flags) < 0)
        return -1;
    wrapper->stream_priv = ctx;
    size = dummy ? 0 : avio_size(ctx);
    info->stream_size = size;
    info->seek_support = ctx->seekable;
    
    return DTERROR_NONE;
}

static int stream_ffmpeg_read (stream_wrapper_t * wrapper,uint8_t *buf,int len)
{
    AVIOContext *ctx = (AVIOContext *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    int r = avio_read(ctx, buf, len);
    if(r>0)
        info->cur_pos += r;
    if(r<=0)
        info->eof_flag = 1;
    return (r <= 0) ? -1 : r;
}

static int stream_ffmpeg_seek (stream_wrapper_t * wrapper, int64_t pos, int whence)
{
    AVIOContext *ctx = (AVIOContext *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    if (avio_seek(ctx, pos, whence) < 0) {
        info->eof_flag = 1;
        return -1;
    }
    if(whence == SEEK_SET)
        info->cur_pos = pos;
    if(whence == SEEK_CUR)
        info->cur_pos += pos;
    return DTERROR_NONE;
}

static int stream_ffmpeg_close (stream_wrapper_t * wrapper)
{
    if(wrapper->stream_priv)
        avio_close(wrapper->stream_priv);
    wrapper->stream_priv = NULL;
    return 0;
}

stream_wrapper_t stream_ffmpeg = {
    .name = "FFMPEG STREAM",
    .id = STREAM_FFMPEG,
    .open = stream_ffmpeg_open,
    .read = stream_ffmpeg_read,
    .seek = stream_ffmpeg_seek,
    .close = stream_ffmpeg_close,
};

#endif
