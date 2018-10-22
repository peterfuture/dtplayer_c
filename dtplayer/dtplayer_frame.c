// # Used For Entire Player

#include "dtp_av.h"
#include "dt_log.h"

#if ENABLE_FFMPEG
#include <libavutil/frame.h>
#include "libavformat/avformat.h"

struct MediaCodecBuffer;
int av_mediacodec_release_buffer(struct MediaCodecBuffer *buffer, int render);

#endif

#define TAG "DTP-UTILS"

dt_av_frame_t *dtp_frame_alloc()
{
    dt_av_frame_t *frame = malloc(sizeof(dt_av_frame_t));
    if (!frame) {
        return NULL;
    }
    memset(frame, 0, sizeof(dt_av_frame_t));
    return frame;
}

void dtp_frame_unref(dt_av_frame_t *frame, int render)
{
    if (!frame) {
        return;
    }

#if ENABLE_FFMPEG
    AVFrame *ff_frame = (AVFrame *)frame->opaque;
    if (frame->flags & DTP_FRAME_FLAG_MEDIACODEC && ff_frame != NULL) {
        struct MediaCodecBuffer *buffer = (struct MediaCodecBuffer *) ff_frame->data[3];
        dt_info(TAG, "[%s:%d] Render one mediacodec frame to surface. buffer:%p\n",
                __FUNCTION__, __LINE__, buffer);
        av_mediacodec_release_buffer(buffer, render);
    }
    av_frame_unref(ff_frame);
#endif

    if (frame->data[0]) {
        free(frame->data[0]);
    }
}

void dtp_frame_free(dt_av_frame_t *frame, int render)
{
    if (!frame) {
        return;
    }

#if ENABLE_FFMPEG
    AVFrame *ff_frame = (AVFrame *)frame->opaque;
    if (frame->flags & DTP_FRAME_FLAG_MEDIACODEC && ff_frame != NULL) {
        struct MediaCodecBuffer *buffer = (struct MediaCodecBuffer *) ff_frame->data[3];
        dt_info(TAG, "[%s:%d] Render one mediacodec frame to surface. buffer:%p\n",
                __FUNCTION__, __LINE__, buffer);
        av_mediacodec_release_buffer(buffer, render);
    }
    av_frame_free(ff_frame);
#endif

    if (frame->data[0]) {
        free(frame->data[0]);
    }

    free(frame);
}

dt_av_pkt_t *dtp_packet_alloc(void)
{
    dt_av_pkt_t *pkt = malloc(sizeof(dt_av_pkt_t));
    if (!pkt) {
        return NULL;
    }
    memset(pkt, 0, sizeof(dt_av_pkt_t));
    return pkt;
}

void dtp_packet_free(dt_av_pkt_t *pkt)
{
    if (pkt == NULL) {
        return;
    }
#if ENABLE_FFMPEG
    AVPacket *ff_pkt = (AVPacket *)pkt->opaque;
    if (pkt->flags & DTP_PACKET_FLAG_FFMPEG && ff_pkt != NULL) {
        av_packet_free(&ff_pkt);
    }
#endif
    if (pkt->flags == 0) {
        if (pkt->data[0]) {
            free(pkt->data[0]);
        }
    }
    free(pkt);
}
