/*
 *video decoder interface using ffmpeg
 * */

#include "dt_av.h"
#include "dtvideo_pic.h"

#include "libavcodec/avcodec.h"
#include "../dtvideo_decoder.h"

//include ffmpeg header
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#define TAG "VDEC-FFMPEG"
static AVFrame *frame;
static struct SwsContext *pSwsCtx = NULL;;

int ffmpeg_vdec_init (vd_wrapper_t *wrapper, void *parent)
{
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *)parent;
    wrapper->parent = parent; 
    //select video decoder and call init
    AVCodec *codec = NULL;
    AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
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
    frame = av_frame_alloc ();
    return 0;
}

//convert to dst fmt
static int convert_frame (dtvideo_decoder_t * decoder, AVFrame * src, int64_t pts, dt_av_pic_t ** p_pict)
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
    dt_av_pic_t *pict = malloc (sizeof (dt_av_pic_t));
    memset (pict, 0, sizeof (dt_av_pic_t));
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

static int copy_frame (dtvideo_decoder_t * decoder, AVFrame * src, int64_t pts, dt_av_pic_t ** p_pict)
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
    //step1: malloc dt_av_pic_t
    dt_av_pic_t *pict = dtav_new_pic();
    memset (pict, 0, sizeof (dt_av_pic_t));
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

int ffmpeg_vdec_decode (vd_wrapper_t *wrapper, dt_av_frame_t * dt_frame, dt_av_pic_t ** pic)
{
    int ret = 0;
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *)wrapper->parent; 
    AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
    dt_debug (TAG, "[%s:%d] param-- w:%d h:%d  extr_si:%d \n", __FUNCTION__, __LINE__, avctxp->width, avctxp->height, avctxp->extradata_size);
    int got_picture = 0;
    AVPacket pkt;

    pkt.data = dt_frame->data;
    pkt.size = dt_frame->size;
    pkt.pts = dt_frame->pts;
    pkt.dts = dt_frame->dts;
    pkt.side_data_elems = 0;
    pkt.buf = NULL;
    avcodec_decode_video2 (avctxp, frame, &got_picture, &pkt);
    if (got_picture)
    {
        ret = copy_frame (decoder, frame, av_frame_get_best_effort_timestamp (frame), pic);
        //ret = convert_frame (decoder, frame, av_frame_get_best_effort_timestamp (frame), pic);
        if (ret == -1)
            ret = 0;
        else
            ret = 1;
        //dt_info(TAG,"==got picture pts:%llu timestamp:%lld \n",frame->pkt_pts,frame->best_effort_timestamp);
    }
    av_frame_unref (frame);
    //no need to free dt_frame
    //will be freed outside
    return ret;
}

int ffmpeg_vdec_release (vd_wrapper_t *wrapper)
{
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *)wrapper->parent; 
    AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
    avcodec_close (avctxp);
    av_frame_free (&frame);
    if (pSwsCtx)
        sws_freeContext (pSwsCtx);
    pSwsCtx = NULL;
    return 0;
}

vd_wrapper_t vd_ffmpeg_ops = {
    .name = "ffmpeg video decoder",
    .vfmt = DT_VIDEO_FORMAT_UNKOWN, //support all vfmt
    .type = DT_TYPE_VIDEO,
    .init = ffmpeg_vdec_init,
    .decode_frame = ffmpeg_vdec_decode,
    .release = ffmpeg_vdec_release,
};
