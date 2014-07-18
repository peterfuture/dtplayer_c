//ex video decoder wrapper
#include "dtvideo_decoder.h"

static int ex_vdec_init (vd_wrapper_t *wrapper, void *parent)
{
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *)parent;
    wrapper->parent = parent; 
    return 0;
}

static int ex_vdec_decode (vd_wrapper_t *wrapper, dt_av_frame_t * dt_frame, AVPicture_t ** pic)
{
    int ret = 0;
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *)wrapper->parent; 
    return ret;
}

static int ex_vdec_release (vd_wrapper_t *wrapper)
{
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *)wrapper->parent; 
    return 0;
}

vd_wrapper_t vd_ex_ops = {
    .name = "ex video decoder",
    .vfmt = VIDEO_FORMAT_H264,
    .type = DT_TYPE_VIDEO,
    .init = ex_vdec_init,
    .decode_frame = ex_vdec_decode,
    .release = ex_vdec_release,
};
