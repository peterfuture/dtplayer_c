/***********************************************************************
**
**  Module : sd_ffmpeg.c
**  Summary: sub decoder with ffmpeg
**  Section: dtsub
**  Author : peter
**  Notes  :
**
***********************************************************************/

#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#include "dt_utils.h"
#include "dtsub_decoder.h"

/***********************************************************************
**
** MACRO FROM libavutil/colorspace.h
**
***********************************************************************/
#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define YUV_TO_RGB1_CCIR(cb1, cr1)\
{\
    cb = (cb1) - 128;\
    cr = (cr1) - 128;\
    r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;\
    g_add = - FIX(0.34414*255.0/224.0) * cb - FIX(0.71414*255.0/224.0) * cr + \
            ONE_HALF;\
    b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;\
}

#define YUV_TO_RGB2_CCIR(r, g, b, y1)\
{\
    y = ((y1) - 16) * FIX(255.0/219.0);\
    r = cm[(y + r_add) >> SCALEBITS];\
    g = cm[(y + g_add) >> SCALEBITS];\
    b = cm[(y + b_add) >> SCALEBITS];\
}

#define YUV_TO_RGB1(cb1, cr1)\
{\
    cb = (cb1) - 128;\
    cr = (cr1) - 128;\
    r_add = FIX(1.40200) * cr + ONE_HALF;\
    g_add = - FIX(0.34414) * cb - FIX(0.71414) * cr + ONE_HALF;\
    b_add = FIX(1.77200) * cb + ONE_HALF;\
}

#define YUV_TO_RGB2(r, g, b, y1)\
{\
    y = (y1) << SCALEBITS;\
    r = cm[(y + r_add) >> SCALEBITS];\
    g = cm[(y + g_add) >> SCALEBITS];\
    b = cm[(y + b_add) >> SCALEBITS];\
}

#define Y_CCIR_TO_JPEG(y)\
 cm[((y) * FIX(255.0/219.0) + (ONE_HALF - 16 * FIX(255.0/219.0))) >> SCALEBITS]

#define Y_JPEG_TO_CCIR(y)\
 (((y) * FIX(219.0/255.0) + (ONE_HALF + (16 << SCALEBITS))) >> SCALEBITS)

#define C_CCIR_TO_JPEG(y)\
 cm[(((y) - 128) * FIX(127.0/112.0) + (ONE_HALF + (128 << SCALEBITS))) >> SCALEBITS]

/* NOTE: the clamp is really necessary! */
static inline int C_JPEG_TO_CCIR(int y)
{
    y = (((y - 128) * FIX(112.0 / 127.0) + (ONE_HALF + (128 << SCALEBITS))) >>
         SCALEBITS);
    if (y < 16) {
        y = 16;
    }
    return y;
}


#define RGB_TO_Y(r, g, b) \
((FIX(0.29900) * (r) + FIX(0.58700) * (g) + \
  FIX(0.11400) * (b) + ONE_HALF) >> SCALEBITS)

#define RGB_TO_U(r1, g1, b1, shift)\
(((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +         \
     FIX(0.50000) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V(r1, g1, b1, shift)\
(((FIX(0.50000) * r1 - FIX(0.41869) * g1 -           \
   FIX(0.08131) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_Y_CCIR(r, g, b) \
((FIX(0.29900*219.0/255.0) * (r) + FIX(0.58700*219.0/255.0) * (g) + \
  FIX(0.11400*219.0/255.0) * (b) + (ONE_HALF + (16 << SCALEBITS))) >> SCALEBITS)

#define RGB_TO_U_CCIR(r1, g1, b1, shift)\
(((- FIX(0.16874*224.0/255.0) * r1 - FIX(0.33126*224.0/255.0) * g1 +         \
     FIX(0.50000*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V_CCIR(r1, g1, b1, shift)\
(((FIX(0.50000*224.0/255.0) * r1 - FIX(0.41869*224.0/255.0) * g1 -           \
   FIX(0.08131*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)


/***********************************************************************
**
** MACRO FROM FFPLAY.C
**
***********************************************************************/

#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}


#define BPP 1


/***********************************************************************
**
** sd ffmpeg context structure def
**
***********************************************************************/
#define TAG "SDEC-FFMPEG"
typedef struct sd_ffmpeg_ctx {
    AVCodecContext *avctxp;
    struct AVSubtitle sub;
} sd_ffmpeg_ctx_t;

/***********************************************************************
**
** ffmpeg_sdec_init
**
***********************************************************************/
int ffmpeg_sdec_init(dtsub_decoder_t *decoder)
{
    sd_wrapper_t *wrapper = decoder->wrapper;
    sd_ffmpeg_ctx_t *sd_ctx = (sd_ffmpeg_ctx_t *)malloc(sizeof(sd_ffmpeg_ctx_t));
    if (!sd_ctx) {
        return -1;
    }
    memset(sd_ctx, 0, sizeof(sd_ffmpeg_ctx_t));
    //select video decoder and call init
    AVCodec *codec = NULL;
    AVCodecContext *avctxp = (AVCodecContext *)decoder->sd_priv;
    sd_ctx->avctxp = (AVCodecContext *)decoder->sd_priv;
    avctxp->thread_count = 1;   //do not use multi thread,may crash
    enum AVCodecID id = avctxp->codec_id;
    codec = avcodec_find_decoder(id);
    if (NULL == codec) {
        dt_error(TAG, "[%s:%d] sub codec find failed. id:%d \n", __FUNCTION__, __LINE__,
                 id);
        return -1;
    }
    if (avcodec_open2(avctxp, codec, NULL) < 0) {
        dt_error(TAG, "[%s:%d] sub codec open failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_info(TAG, " [%s:%d] ffmpeg sub decoder init ok, codectype:%d  \n",
            __FUNCTION__, __LINE__, avctxp->codec_type);
    wrapper->para = decoder->para;
    decoder->sd_priv = (void *)sd_ctx;

    return 0;
}

/***********************************************************************
**
** ffmpeg_sdec_decode
**
***********************************************************************/
int ffmpeg_sdec_decode(dtsub_decoder_t *decoder, dt_av_pkt_t * dt_frame,
                       dtav_sub_frame_t ** sub_frame)
{
    int ret = 0;
    sd_ffmpeg_ctx_t *sd_ctx = (sd_ffmpeg_ctx_t *)decoder->sd_priv;
    AVCodecContext *avctxp = (AVCodecContext *)sd_ctx->avctxp;
    dt_debug(TAG, "[%s:%d] param-- w:%d h:%d  \n", __FUNCTION__, __LINE__,
             avctxp->width, avctxp->height);
    int got_sub = 0;
    AVPacket pkt;
    int i, j;
    int r, g, b, y, u, v, a;

    pkt.data = dt_frame->data;
    pkt.size = dt_frame->size;
    pkt.pts = dt_frame->pts;
    pkt.dts = dt_frame->dts;
    pkt.side_data_elems = 0;
    pkt.buf = NULL;
    struct AVSubtitle *sub = &sd_ctx->sub;
    ret = avcodec_decode_subtitle2(avctxp, sub, &got_sub, &pkt);
    if (ret < 0) {
        return -1;
    }

    if (got_sub) {
        dt_debug(TAG, "get sub, type:%d size:%d starttime:%u endtime:%u pts:%lld \n",
                 (int)sub->format, pkt.size, sub->start_display_time, sub->end_display_time,
                 sub->pts);
        dtav_sub_frame_t *sub_frame_tmp = (dtav_sub_frame_t *)malloc(sizeof(
                                              dtav_sub_frame_t));
        if (sub->format == 0) {  // graphics
            for (i = 0; i < sub->num_rects; i++) {
                for (j = 0; j < sub->rects[i]->nb_colors; j++) {
                    RGBA_IN(r, g, b, a, (uint32_t*)sub->rects[i]->pict.data[1] + j);
                    y = RGB_TO_Y_CCIR(r, g, b);
                    u = RGB_TO_U_CCIR(r, g, b, 0);
                    v = RGB_TO_V_CCIR(r, g, b, 0);
                    YUVA_OUT((uint32_t*)sub->rects[i]->pict.data[1] + j, y, u, v, a);
                }
            }
            sub->pts = (sub->pts * 9 / 1000); // use ffplay calc way
        }
        memcpy(sub_frame_tmp, sub, sizeof(dtav_sub_frame_t));
        *sub_frame = sub_frame_tmp;
        memset(sub, 0, sizeof(AVSubtitle));
        dt_debug(TAG, "get sub, type:%d size:%d starttime:%u endtime:%u pts:%lld\n",
                 (int)sub_frame_tmp->format, pkt.size, sub_frame_tmp->start_display_time,
                 sub_frame_tmp->end_display_time, sub_frame_tmp->pts);
    }

    return got_sub;
}

/***********************************************************************
**
** ffmpeg_sdec_decode
**
***********************************************************************/
int ffmpeg_sdec_release(dtsub_decoder_t *decoder)
{
    sd_ffmpeg_ctx_t *sd_ctx = (sd_ffmpeg_ctx_t *)decoder->sd_priv;
    AVCodecContext *avctxp = (AVCodecContext *)sd_ctx->avctxp;
    avcodec_close(avctxp);
    free(sd_ctx);

    return 0;
}

sd_wrapper_t sd_ffmpeg_ops = {
    .name = "ffmpeg sub decoder",
    .sfmt = DT_SUB_FORMAT_UNKOWN, //support all vfmt
    .type = DTP_MEDIA_TYPE_SUBTITLE,
    .init = ffmpeg_sdec_init,
    .decode_frame = ffmpeg_sdec_decode,
    .release = ffmpeg_sdec_release,
};
