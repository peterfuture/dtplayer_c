/***********************************************************************
**
**  Module : vf_ffmpeg.c
**  Summary: process video with ffmpeg
**  Section: dtvideo
**  Author : peter
**  Notes  : 
**           clip & colorspace convert for linux
**           colorspace convert for android 
**
***********************************************************************/

#include "vf_wrapper.h"

#include "libavcodec/avcodec.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"


#define TAG "VF-FFMPEG"

typedef struct vf_ffmpeg_ctx
{
    struct SwsContext *pSwsCtx;;
    int need_process;
    dt_av_frame_t *swap_frame;
    int swap_buf_size;
    uint8_t *swapbuf;
}vf_ffmpeg_ctx_t;


/***********************************************************************
**
** capability check
**
***********************************************************************/
static int ffmpeg_vf_capable(vf_cap_t cap)
{
#ifdef ENABLE_LINUX
    int ffmpeg_cap = VF_CAP_COLORSPACE_CONVERT | VF_CAP_CLIP;
#endif
#ifdef ENABLE_ANDROID
    int ffmpeg_cap = VF_CAP_COLORSPACE_CONVERT;
#endif

    dt_debug(TAG, "request cap: %x , %s support:%x \n", cap, "ffmpeg vf", ffmpeg_cap);
    return cap & ffmpeg_cap; 
}

/***********************************************************************
**
** Init ffmpeg filter
**
***********************************************************************/
static int ffmpeg_vf_init(dtvideo_filter_t *filter)
{
    int ret = -1; // -1 means not support or no need to process
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)malloc(sizeof(vf_ffmpeg_ctx_t));
    if(!vf_ctx)
        return ret;
    memset(vf_ctx,0,sizeof(*vf_ctx));

    dtvideo_para_t *para = &filter->para;
    int sw = para->s_width;
    int sh = para->s_height;
    int dw = para->d_width;
    int dh = para->d_height;
    int sf = para->s_pixfmt;
    int df = para->d_pixfmt;
    vf_ctx->need_process = !(sw == dw && sh == dh && sf == df);
    dt_info(TAG, "[%s:%d] sw:%d dw:%d sh:%d dh:%d sf:%d df:%d need_process:%d \n", __FUNCTION__, __LINE__, sw, dw, sh, dh, sf, df, vf_ctx->need_process);
    
    filter->vf_priv = vf_ctx;
    if(vf_ctx->need_process)
    {
        vf_ctx->swap_frame = dtav_new_frame();
        ret = 0;
    }
    
    dt_info(TAG, "[%s:%d] vf init ok\n", __FUNCTION__, __LINE__);
    return ret;
}

/***********************************************************************
**
** Process one frame with ffmpeg-libavfilter
**
***********************************************************************/
static int convert_picture (dtvideo_filter_t * filter, dt_av_frame_t * src)
{
    uint8_t *buffer;
    int buffer_size;
    
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    dtvideo_para_t *para = &filter->para;
    int sw = para->s_width; 
    int dw = para->d_width; 
    int sh = para->s_height; 
    int dh = para->d_height; 
    int sf = para->s_pixfmt; 
    int df = para->d_pixfmt; 

    const AVPixFmtDescriptor *pix_desc = av_pix_fmt_desc_get(sf);
    if(!vf_ctx->swap_frame)
        vf_ctx->swap_frame = dtav_new_frame();
    
    dt_av_frame_t *pict = vf_ctx->swap_frame;
    if(!pict)
    {
        dt_error(TAG,"[%s:%d] err: swap frame malloc failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    memset(pict, 0, sizeof(dt_av_frame_t));
    
    AVPicture *dst = (AVPicture *)pict;
    buffer_size = avpicture_get_size (df, dw, dh);
    if(buffer_size > vf_ctx->swap_buf_size)
    {
        if(vf_ctx->swapbuf)
            free(vf_ctx->swapbuf);
        vf_ctx->swap_buf_size = buffer_size;
        vf_ctx->swapbuf = (uint8_t *) malloc (buffer_size * sizeof (uint8_t));
    }
    buffer = vf_ctx->swapbuf;
    avpicture_fill ((AVPicture *) dst, buffer, df, dw, dh);

#ifdef ENABLE_ANDROID
    //re setup linesize
    src->linesize[0] = av_image_get_linesize(sf, sw, 0);
    src->linesize[1] = av_image_get_linesize(sf, sw, 1);
    src->linesize[2] = av_image_get_linesize(sf, sw, 2);

    src->data[0] = src->data[0];
    src->data[1] = src->data[0] + src->linesize[0] * sh;
    src->data[2] = src->data[1] + src->linesize[1] * -(-sh>>pix_desc->log2_chroma_h);
#endif

    vf_ctx->pSwsCtx = sws_getCachedContext (vf_ctx->pSwsCtx, sw, sh, sf, dw, dh, df, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale (vf_ctx->pSwsCtx, src->data, src->linesize, 0, sh, dst->data, dst->linesize);
    
    pict->pts = src->pts;
    dtav_unref_frame(src);
    memcpy(src,pict,sizeof(dt_av_frame_t));

    vf_ctx->swapbuf = NULL;
    vf_ctx->swap_buf_size = 0;

    return 0;
}

/***********************************************************************
**
** Process one frame
**
***********************************************************************/
static int ffmpeg_vf_process(dtvideo_filter_t *filter, dt_av_frame_t *frame)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    if(vf_ctx->need_process == 0)
    {
        dt_warning(TAG,"[%s:%d] no need to process but called \n", __FUNCTION__, __LINE__);
        return 0;
    }
   
    int ret = convert_picture(filter, frame);
    if(ret < 0)
        dt_info (TAG, "[%s:%d] vf process failed \n", __FUNCTION__, __LINE__);

    return ret;
}

/***********************************************************************
**
** Release ffmpeg filter
**
***********************************************************************/
static int ffmpeg_vf_release(dtvideo_filter_t *filter)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    if(!vf_ctx)
        return 0;

    if (vf_ctx->pSwsCtx)
        sws_freeContext (vf_ctx->pSwsCtx);
 
    free(vf_ctx);
    vf_ctx = NULL;

    dt_info(TAG, "[%s:%d] vf release ok \n", __FUNCTION__, __LINE__);
    return 0;
}

vf_wrapper_t vf_ffmpeg_ops = {
    .name       = "ffmpeg video filter",
    .type       = DT_TYPE_VIDEO,
    .capable    = ffmpeg_vf_capable,
    .init       = ffmpeg_vf_init,
    .process    = ffmpeg_vf_process,
    .release    = ffmpeg_vf_release,
};
