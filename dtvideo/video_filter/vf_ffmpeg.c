#include "vf_wrapper.h"

#define TAG "VF-FFMPEG"

typedef struct vf_ffmpeg_ctx
{
    int need_process_flag;

}vf_ffmpeg_ctx_t;

static int ffmpeg_vf_capable(dtvideo_filter_t *filter)
{
    return VF_CAP_CONVERT | VF_CAP_CROP; 
}

static int need_process(dtvideo_para_t *para)
{
    int sw = para->s_width;
    int sh = para->s_height;
    int dw = para->d_width;
    int dh = para->d_height;
    int sf = para->s_pixfmt;
    int df = para->d_pixfmt;

    return !(sw == dw && sh == dh && sf == df);

}

static int ffmpeg_vf_init(dtvideo_filter_t *filter)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)malloc(sizeof(vf_ffmpeg_ctx_t));
    if(!vf_ctx)
        return -1;
    dtvideo_para_t *para = &filter->para;
    vf_ctx->need_process_flag = need_process(para);
    filter->vf_priv = vf_ctx;
    dt_info (TAG, "[%s:%d] vf init ok ,need process:%d \n", __FUNCTION__, __LINE__,vf_ctx->need_process_flag);
    return 0;
}

static int ffmpeg_vf_process(dtvideo_filter_t *filter)
{
    vf_ffmpeg_ctx_t *vf_ctx = (vf_ffmpeg_ctx_t *)(filter->vf_priv);
    if(!vf_ctx->need_process_flag) 
        return 0;
    dt_info (TAG, "[%s:%d] vf process ok \n", __FUNCTION__, __LINE__);
    return 0;
}

static int ffmpeg_vf_release(dtvideo_filter_t *filter)
{
    dt_info (TAG, "[%s:%d] vf release ok \n", __FUNCTION__, __LINE__);
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
