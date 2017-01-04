/*
 *video decoder interface using ffmpeg
 * */

#include "dt_av.h"

#include "libavcodec/avcodec.h"
#include "../dtvideo_decoder.h"

//include ffmpeg header
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#define TAG "VDEC-FFMPEG"

typedef struct vd_ffmpeg_ctx {
    AVCodecContext *avctxp;
    AVFrame *frame;
    struct SwsContext *pSwsCtx;
    int use_hwaccel;
} vd_ffmpeg_ctx_t;

static enum AVCodecID convert_to_id(int format)
{
    switch (format) {
    case DT_VIDEO_FORMAT_H264:
        return AV_CODEC_ID_H264;
    default:
        return -1;
    }
    return -1;
}

static AVCodecContext * alloc_ffmpeg_ctx(dtvideo_decoder_t *decoder)
{
    AVCodecContext *ctx = avcodec_alloc_context3(NULL);
    if (!ctx) {
        return NULL;
    }

    ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx->codec_id = convert_to_id(decoder->para.vfmt);
    dt_info(TAG, "vfmt->id  %d->%d \n", decoder->para.vfmt, ctx->codec_id);
    if (ctx->codec_id == -1) {
        av_free(ctx);
        return NULL;
    }
    return ctx;
}

#if ENABLE_ANDROID

#include <libavcodec/mediacodec.h>
#include <libavutil/pixdesc.h>

static enum AVPixelFormat mediacodec_hwaccel_get_format(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    vd_ffmpeg_ctx_t *ctx = avctx->opaque;
    const enum AVPixelFormat *p = NULL;

    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        AVMediaCodecContext *mediacodec_ctx = NULL;
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);

        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            break;
        }

        if (*p != AV_PIX_FMT_MEDIACODEC) {
            continue;
        }

        mediacodec_ctx = av_mediacodec_alloc_context();
        if (!mediacodec_ctx) {
            fprintf(stderr, "Failed to allocate hwaccel ctx\n");
            continue;
        }
        avctx->hwaccel_context = mediacodec_ctx;
        ctx->use_hwaccel = 1;
        break;
    }

    return *p;
}

#endif

int ffmpeg_vdec_init(dtvideo_decoder_t *decoder)
{
    vd_wrapper_t *wrapper = decoder->wrapper;
    dtvideo_para_t *para = &decoder->para;
    vd_ffmpeg_ctx_t *vd_ctx = malloc(sizeof(vd_ffmpeg_ctx_t));
    if (!vd_ctx) {
        return -1;
    }
    memset(vd_ctx, 0, sizeof(vd_ffmpeg_ctx_t));

    AVCodecContext *avctxp = NULL;
    if (decoder->vd_priv) {
        avctxp = (AVCodecContext *) decoder->vd_priv;
    } else {
        avctxp = alloc_ffmpeg_ctx(decoder);
    }
    if (!avctxp) {
        return -1;
    }
    avctxp->thread_count = 1;   //do not use multi thread,may crash
    vd_ctx->avctxp = avctxp;
    //select video decoder and call init
    AVCodec *codec = NULL;
    enum AVCodecID id = avctxp->codec_id;
    avctxp->opaque = vd_ctx;
    int hw_codec_opened = 0;
    if (para->flag & DTAV_FLAG_DISABLE_HW_CODEC) {
        dt_info(TAG, "disable hw codec, use sw codec.\n");
        goto use_sw_decoder;
    }

#if ENABLE_ANDROID
    dt_info(TAG, "Check Android HW Codec.\n");
    //use hw decoder
    if (avctxp->codec_id == AV_CODEC_ID_H264 ||
        avctxp->codec_id == AV_CODEC_ID_HEVC ||
        avctxp->codec_id == AV_CODEC_ID_MPEG4 ||
        avctxp->codec_id == AV_CODEC_ID_VP8 ||
        avctxp->codec_id == AV_CODEC_ID_VP9) {
        const char *codec_name = NULL;
        switch (avctxp->codec_id) {
        case AV_CODEC_ID_H264:
            codec_name = "h264_mediacodec";
            break;
        case AV_CODEC_ID_HEVC:
            codec_name = "hevc_mediacodec";
            break;
        case AV_CODEC_ID_MPEG4:
            codec_name = "mpeg4_mediacodec";
            break;
        case AV_CODEC_ID_VP8:
            codec_name = "vp8_mediacodec";
            break;
        case AV_CODEC_ID_VP9:
            codec_name = "vp9_mediacodec";
            break;
        default:
            codec_name = "h264_mediacodec";
            break;
            //av_assert0(0);
        }

        codec = avcodec_find_decoder_by_name(codec_name);
        if (NULL != codec) {
            avctxp->get_format = mediacodec_hwaccel_get_format;
            dt_info(TAG, "Android use hw decoder. name:%s \n", codec_name);
            if (avcodec_open2(avctxp, codec, NULL) >= 0) {
                hw_codec_opened = 1;
            } else {
                dt_info(TAG, "Android hw decoder open failed.\n");
            }
        } else {
            dt_info(TAG, "Android find hw decoder failed.\n");
        }
    }
#endif
use_sw_decoder:
    if (hw_codec_opened == 0) {
        codec = avcodec_find_decoder(id);
        if (NULL == codec) {
            dt_error(TAG, "[%s:%d] video codec find failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        if (avcodec_open2(avctxp, codec, NULL) < 0) {
            dt_error(TAG, "[%s:%d] video codec open failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
    }
    dt_info(TAG, " [%s:%d] Pixfmt:%d \n", __FUNCTION__, __LINE__, avctxp->pix_fmt);
    dt_info(TAG, " [%s:%d] ffmpeg dec init ok. hw decoder enable:%d \n", __FUNCTION__, __LINE__, hw_codec_opened);
    //alloc one frame for decode
    vd_ctx->frame = av_frame_alloc();
    wrapper->para = &decoder->para;
    decoder->vd_priv = (void *)vd_ctx;
    return 0;
}

//convert to dst fmt
#if 0
static int convert_frame(dtvideo_decoder_t * decoder, AVFrame * src, int64_t pts, dt_av_frame_t ** p_pict)
{
    uint8_t *buffer;
    int buffer_size;

    dtvideo_para_t *para = &decoder->para;
    int sw = para->s_width;
    int dw = para->d_width;
    int sh = para->s_height;
    int dh = para->d_height;
    int sf = para->s_pixfmt;
    int df = para->d_pixfmt;

    //step1: malloc avpicture_t
    dt_av_frame_t *pict = malloc(sizeof(dt_av_frame_t));
    memset(pict, 0, sizeof(dt_av_frame_t));
    //step2: convert to avpicture, ffmpeg struct
    AVPicture *dst = (AVPicture *)(pict);
    //step3: allocate an AVFrame structure
    buffer_size = avpicture_get_size(df, dw, dh);
    buffer = (uint8_t *) malloc(buffer_size * sizeof(uint8_t));
    avpicture_fill((AVPicture *) dst, buffer, df, dw, dh);

    pSwsCtx = sws_getCachedContext(pSwsCtx, sw, sh, sf, dw, dh, df, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(pSwsCtx, src->data, src->linesize, 0, sh, dst->data, dst->linesize);

    pict->pts = pts;
    *p_pict = pict;
    return 0;
}
#endif

static int copy_frame(dtvideo_decoder_t * decoder, AVFrame * src, int64_t pts, dt_av_frame_t ** p_pict)
{
    dtvideo_para_t *para = &decoder->para;
    uint8_t *buffer;
    int buffer_size;
    int sw = para->s_width;
    int dw = sw;
    int sh = para->s_height;
    int dh = sh;
    int sf = para->s_pixfmt;
    int df = sf;
    //step1: malloc dt_av_frame_t
    dt_av_frame_t *pict = dtav_new_frame();
    memset(pict, 0, sizeof(dt_av_frame_t));
    //step2: convert to avpicture, ffmpeg struct
    AVPicture *dst = (AVPicture *)(pict);
    //step3: allocate an AVFrame structure
    buffer_size = avpicture_get_size(df, dw, dh);
    buffer = (uint8_t *) malloc(buffer_size * sizeof(uint8_t));
    avpicture_fill((AVPicture *) dst, buffer, df, dw, dh);


#if 1
    av_picture_copy(dst, (AVPicture *)src, sf, sw, sh);
#else
    pSwsCtx = sws_getCachedContext(pSwsCtx, sw, sh, sf, dw, dh, df, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(pSwsCtx, src->data, src->linesize, 0, sh, dst->data, dst->linesize);
#endif
    pict->pts = pts;
    *p_pict = pict;

    return 0;
}

/*
 *decode one frame using ffmpeg
 *
 * return value
 * 1  dec one frame and get one frame
 * 0  dec one frame without out
 * -1 err occured while decoding
 *
 * */

int ffmpeg_vdec_decode(dtvideo_decoder_t *decoder, dt_av_pkt_t * dt_frame, dt_av_frame_t ** pic)
{
    int ret = 0;
    vd_ffmpeg_ctx_t *vd_ctx = (vd_ffmpeg_ctx_t *)decoder->vd_priv;
    AVCodecContext *avctxp = (AVCodecContext *) vd_ctx->avctxp;
    dt_debug(TAG, "[%s:%d] param-- w:%d h:%d  extr_si:%d \n", __FUNCTION__, __LINE__, avctxp->width, avctxp->height, avctxp->extradata_size);
    int got_picture = 0;
    AVPacket pkt;
    memset(&pkt, 0, sizeof(AVPacket));
    pkt.data = dt_frame->data;
    pkt.size = dt_frame->size;
    pkt.pts = dt_frame->pts;
    pkt.dts = dt_frame->dts;
    pkt.side_data_elems = 0;
    pkt.buf = NULL;
    AVFrame *frame = vd_ctx->frame;
    avcodec_decode_video2(avctxp, frame, &got_picture, &pkt);
    if (got_picture) {
        ret = copy_frame(decoder, frame, av_frame_get_best_effort_timestamp(frame), pic);
        //ret = copy_frame(decoder, frame, frame->pkt_pts, pic);
        dt_debug(TAG, "==got picture pkt_pts:%llx best_effort:%llx \n", frame->pkt_pts, av_frame_get_best_effort_timestamp(frame));
        //ret = convert_frame (decoder, frame, av_frame_get_best_effort_timestamp (frame), pic);
        if (ret == -1) {
            ret = 0;
        } else {
            ret = 1;
        }
    }
    av_frame_unref(frame);
    //no need to free dt_frame
    //will be freed outside
    return ret;
}

int ffmpeg_vdec_info_changed(dtvideo_decoder_t *decoder)
{
    //vd_wrapper_t *wrapper = decoder->wrapper;
    return 0;
}

int ffmpeg_vdec_release(dtvideo_decoder_t *decoder)
{
    vd_ffmpeg_ctx_t *vd_ctx = (vd_ffmpeg_ctx_t *)decoder->vd_priv;
    AVCodecContext *avctxp = (AVCodecContext *) vd_ctx->avctxp;
    avcodec_close(avctxp);
    if (vd_ctx->frame) {
        av_frame_free(&vd_ctx->frame);
    }
    if (vd_ctx->pSwsCtx) {
        sws_freeContext(vd_ctx->pSwsCtx);
    }
    vd_ctx->pSwsCtx = NULL;
    free(vd_ctx);
    return 0;
}

vd_wrapper_t vd_ffmpeg_ops = {
    .name = "ffmpeg video decoder",
    .vfmt = DT_VIDEO_FORMAT_UNKOWN, //support all vfmt
    .type = DT_TYPE_VIDEO,
    .is_hw = 0,
    .init = ffmpeg_vdec_init,
    .decode_frame = ffmpeg_vdec_decode,
    .info_changed = ffmpeg_vdec_info_changed,
    .release = ffmpeg_vdec_release,
};
