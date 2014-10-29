/***********************************************************************
**
**  Module : sd_ffmpeg.c
**  Summary: sub decoder with ffmpeg
**  Section: dtsub
**  Author : peter
**  Notes  : 
**
***********************************************************************/

#include "dt_av.h"

#include "dtsub_decoder.h"

#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#define TAG "SDEC-FFMPEG"

typedef struct sd_ffmpeg_ctx{
    AVCodecContext *avctxp;
    struct AVSubtitle sub;
    dtav_sub_frame_t *frame; 
}sd_ffmpeg_ctx_t;

int ffmpeg_sdec_init (dtsub_decoder_t *decoder)
{
    sd_wrapper_t *wrapper = decoder->wrapper;
    sd_ffmpeg_ctx_t *sd_ctx = (sd_ffmpeg_ctx_t *)malloc(sizeof(sd_ffmpeg_ctx_t));
    if(!sd_ctx)
        return -1;
    memset(sd_ctx, 0, sizeof(sd_ffmpeg_ctx_t));
    //select video decoder and call init
    AVCodec *codec = NULL;
    AVCodecContext *avctxp = (AVCodecContext *)decoder->sd_priv;
    sd_ctx->avctxp = (AVCodecContext *)decoder->sd_priv;
    avctxp->thread_count = 1;   //do not use multi thread,may crash
    enum AVCodecID id = avctxp->codec_id;
    codec = avcodec_find_decoder(id);
    if (NULL == codec)
    {
        dt_error (TAG, "[%s:%d] video codec find failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    if (avcodec_open2(avctxp, codec, NULL) < 0)
    {
        dt_error (TAG, "[%s:%d] sub codec open failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_info (TAG, " [%s:%d] ffmpeg sub decoder init ok, codectype:%d  \n", __FUNCTION__, __LINE__, avctxp->codec_type);
    //alloc one frame for decode
    sd_ctx->frame = (dtav_sub_frame_t *)malloc(sizeof(dtav_sub_frame_t));
    wrapper->para = decoder->para;
    decoder->sd_priv = (void *)sd_ctx;

    return 0;
}

int ffmpeg_sdec_decode (dtsub_decoder_t *decoder, dt_av_pkt_t * dt_frame, dtav_sub_frame_t ** sub_frame)
{
    int ret = 0;
    sd_ffmpeg_ctx_t *sd_ctx = (sd_ffmpeg_ctx_t *)decoder->sd_priv;
    AVCodecContext *avctxp = (AVCodecContext *)sd_ctx->avctxp;
    dt_debug (TAG, "[%s:%d] param-- w:%d h:%d  \n", __FUNCTION__, __LINE__, avctxp->width, avctxp->height);
    int got_sub = 0;
    AVPacket pkt;

    pkt.data = dt_frame->data;
    pkt.size = dt_frame->size;
    pkt.pts = dt_frame->pts;
    pkt.dts = dt_frame->dts;
    pkt.side_data_elems = 0;
    pkt.buf = NULL;
    struct AVSubtitle *sub = &sd_ctx->sub;
    ret = avcodec_decode_subtitle2(avctxp, sub, &got_sub, &pkt);

    if(ret < 0)
        return -1;

    if (got_sub)
    {
        dtav_sub_frame_t *sub_frame_tmp = (dtav_sub_frame_t *)malloc(sizeof(dtav_sub_frame_t));
        memcpy(sub_frame_tmp, sub, sizeof(dtav_sub_frame_t));
        *sub_frame = sub_frame_tmp; 
        dt_info(TAG,"get sub, type:%d starttime:%d endtime:%d \n", sub_frame_tmp->start_display_time, sub_frame_tmp->end_display_time);
    }
    //no need to free dt_frame
    //will be freed outside
    return ret;
}

int ffmpeg_sdec_release (dtsub_decoder_t *decoder)
{
    return 0;
}

sd_wrapper_t sd_ffmpeg_ops = {
    .name = "ffmpeg sub decoder",
    .sfmt = DT_SUB_FORMAT_UNKOWN, //support all vfmt
    .type = DT_TYPE_SUBTITLE,
    .init = ffmpeg_sdec_init,
    .decode_frame = ffmpeg_sdec_decode,
    .release = ffmpeg_sdec_release,
};
