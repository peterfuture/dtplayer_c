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
    AVFrame *frame;
    struct SwsContext *pSwsCtx;
}sd_ffmpeg_ctx_t;

int ffmpeg_sdec_init (dtsub_decoder_t *decoder)
{
    return 0;
}

int ffmpeg_sdec_decode (dtsub_decoder_t *decoder, dt_av_pkt_t * dt_frame, dt_av_frame_t ** pic)
{
    return -1;
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
