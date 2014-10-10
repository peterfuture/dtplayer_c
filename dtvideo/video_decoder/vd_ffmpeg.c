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

typedef struct vd_ffmpeg_ctx{
    AVCodecContext *avctxp;
    AVFrame *frame;
    struct SwsContext *pSwsCtx;
}vd_ffmpeg_ctx_t;

int ffmpeg_vdec_init (dtvideo_decoder_t *decoder)
{
    vd_wrapper_t *wrapper = decoder->wrapper;
    vd_ffmpeg_ctx_t *vd_ctx = malloc(sizeof(vd_ffmpeg_ctx_t));
    if(!vd_ctx)
        return -1;
    memset(vd_ctx, 0, sizeof(vd_ffmpeg_ctx_t));
    //select video decoder and call init
    AVCodec *codec = NULL;
    AVCodecContext *avctxp = (AVCodecContext *) decoder->vd_priv;
    vd_ctx->avctxp = (AVCodecContext *) decoder->vd_priv;
    avctxp->thread_count = 1;   //do not use multi thread,may crash
    enum AVCodecID id = avctxp->codec_id;
    codec = avcodec_find_decoder (id);
    if (NULL == codec)
    {
        dt_error (TAG, "[%s:%d] video codec find failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    if (avcodec_open2 (avctxp, codec, NULL) < 0)
    {
        dt_error (TAG, "[%s:%d] video codec open failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_info (TAG, " [%s:%d] ffmpeg dec init ok \n", __FUNCTION__, __LINE__);
    //alloc one frame for decode
    vd_ctx->frame = av_frame_alloc ();
    wrapper->para = decoder->para;
    decoder->vd_priv = (void *)vd_ctx;
    return 0;
}

//convert to dst fmt
#if 0
static int convert_frame (dtvideo_decoder_t * decoder, AVFrame * src, int64_t pts, dt_av_frame_t ** p_pict)
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
    dt_av_frame_t *pict = malloc (sizeof (dt_av_frame_t));
    memset (pict, 0, sizeof (dt_av_frame_t));
    //step2: convert to avpicture, ffmpeg struct
    AVPicture *dst = (AVPicture *) (pict);
    //step3: allocate an AVFrame structure
    buffer_size = avpicture_get_size (df, dw, dh);
    buffer = (uint8_t *) malloc (buffer_size * sizeof (uint8_t));
    avpicture_fill ((AVPicture *) dst, buffer, df, dw, dh);

    pSwsCtx = sws_getCachedContext (pSwsCtx, sw, sh, sf, dw, dh, df, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale (pSwsCtx, src->data, src->linesize, 0, sh, dst->data, dst->linesize);
    
    pict->pts = pts;
    *p_pict = pict;
    return 0;
}
#endif

static int copy_frame (dtvideo_decoder_t * decoder, AVFrame * src, int64_t pts, dt_av_frame_t ** p_pict)
{
    dtvideo_para_t *para = decoder->para;
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
    memset (pict, 0, sizeof (dt_av_frame_t));
    //step2: convert to avpicture, ffmpeg struct
    AVPicture *dst = (AVPicture *) (pict);
    //step3: allocate an AVFrame structure
    buffer_size = avpicture_get_size (df, dw, dh);
    buffer = (uint8_t *) malloc (buffer_size * sizeof (uint8_t));
    avpicture_fill ((AVPicture *) dst, buffer, df, dw, dh);


#if 1
    av_picture_copy(dst,(AVPicture *)src,sf,sw,sh);
#else
    pSwsCtx = sws_getCachedContext (pSwsCtx, sw, sh, sf, dw, dh, df, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale (pSwsCtx, src->data, src->linesize, 0, sh, dst->data, dst->linesize);
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

int ffmpeg_vdec_decode (dtvideo_decoder_t *decoder, dt_av_pkt_t * dt_frame, dt_av_frame_t ** pic)
{
    int ret = 0;
    vd_ffmpeg_ctx_t *vd_ctx = (vd_ffmpeg_ctx_t *)decoder->vd_priv;
    AVCodecContext *avctxp = (AVCodecContext *) vd_ctx->avctxp;
    dt_debug (TAG, "[%s:%d] param-- w:%d h:%d  extr_si:%d \n", __FUNCTION__, __LINE__, avctxp->width, avctxp->height, avctxp->extradata_size);
    int got_picture = 0;
    AVPacket pkt;

    pkt.data = dt_frame->data;
    pkt.size = dt_frame->size;
    pkt.pts = dt_frame->pts;
    pkt.dts = dt_frame->dts;
    pkt.side_data_elems = 0;
    pkt.buf = NULL;
    AVFrame *frame = vd_ctx->frame;
    avcodec_decode_video2 (avctxp, frame, &got_picture, &pkt);
    if (got_picture)
    {
        dt_info(TAG,"==got picture pts:%llu timestamp:%lld \n",frame->pkt_pts,frame->best_effort_timestamp);
        ret = copy_frame (decoder, frame, av_frame_get_best_effort_timestamp (frame), pic);
        //ret = convert_frame (decoder, frame, av_frame_get_best_effort_timestamp (frame), pic);
        if (ret == -1)
            ret = 0;
        else
            ret = 1;
        dt_debug(TAG,"==got picture pts:%llu timestamp:%lld \n",frame->pkt_pts,frame->best_effort_timestamp);
    }
    av_frame_unref (frame);
    //no need to free dt_frame
    //will be freed outside
    return ret;
}

int ffmpeg_vdec_info_changed (dtvideo_decoder_t *decoder)
{
    //vd_wrapper_t *wrapper = decoder->wrapper;
    return 0;
}

int ffmpeg_vdec_release (dtvideo_decoder_t *decoder)
{
    vd_ffmpeg_ctx_t *vd_ctx = (vd_ffmpeg_ctx_t *)decoder->vd_priv;
    AVCodecContext *avctxp = (AVCodecContext *) vd_ctx->avctxp;
    avcodec_close (avctxp);
    if(vd_ctx->frame)
        av_frame_free (&vd_ctx->frame);
    if (vd_ctx->pSwsCtx)
        sws_freeContext (vd_ctx->pSwsCtx);
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
