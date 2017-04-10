#include "libavformat/avformat.h"
#include "libavformat/avio.h"

#include "dtstream.h"

#define TAG "STREAM-FFMPEG"

static const char prefix[] = "ffmpeg://";

#ifndef AVSEEK_SIZE
#define AVSEEK_SIZE 0x10000
#endif

static int stream_ffmpeg_open(stream_wrapper_t * wrapper, char *stream_name)
{
    int ret = 0;
    const char *filename = stream_name;
    AVIOContext *ctx = NULL;
    int64_t size;
    int dummy;
    int flags = AVIO_FLAG_READ;
    stream_ctrl_t *info = &wrapper->info;

    av_register_all();
    avformat_network_init();

    if (!strncmp(filename, prefix, strlen(prefix))) {
        filename += strlen(prefix);
    }
    dummy = strncmp(filename, "rtsp:", 5);
    if (!dummy) {
        dt_info(TAG, "[ffmpeg] rtsp use ffmpeg inside streamer.\n");
        return -1;
    }
    dt_info(TAG, "[ffmpeg] Opening %s\n", filename);
    ret = avio_open(&ctx, filename, flags);
    if (ret < 0) {
        dt_info(TAG, "[ffmpeg] Opening %s failed. ret:%d\n", filename, ret);
        return -1;
    }
    wrapper->stream_priv = ctx;
    size = dummy ? 0 : avio_size(ctx);
    info->stream_size = size;
    info->seek_support = ctx->seekable;

    dt_info(TAG, "[ffmpeg] Opening %s ok. ret:%d\n", filename, ret);
    return DTERROR_NONE;
}

static int stream_ffmpeg_read(stream_wrapper_t * wrapper, uint8_t *buf, int len)
{
    AVIOContext *ctx = (AVIOContext *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    int r = avio_read(ctx, buf, len);
    if (r > 0) {
        info->cur_pos += r;
    }
    if (r <= 0) {
        dt_info(TAG, "[ffmpeg] ffmpeg read failed, ret :%d \n", r);
        info->eof_flag = 1;
    }
    return (r <= 0) ? -1 : r;
}

static int stream_ffmpeg_seek(stream_wrapper_t * wrapper, int64_t pos,
                              int whence)
{
    AVIOContext *ctx = (AVIOContext *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    info->eof_flag = 0;

    if (whence == AVSEEK_SIZE) {
        dt_debug(TAG, "REQUEST STREAM SIZE:%lld \n", info->stream_size);
        return avio_size(ctx);
    }

    return avio_seek(ctx, pos, whence);
}

static int stream_ffmpeg_close(stream_wrapper_t * wrapper)
{
    if (wrapper->stream_priv) {
        avio_close((AVIOContext *)wrapper->stream_priv);
    }
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
