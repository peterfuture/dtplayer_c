//ex video decoder wrapper
#include "dtvideo_decoder.h"

#define TAG "VD-EX"

static int ex_vdec_init (dtvideo_decoder_t *decoder)
{
    dt_info(TAG,"VD EX INIT OK \n");
    return 0;
}

static int ex_vdec_decode (dtvideo_decoder_t *decoder, dt_av_pkt_t * dt_frame, dt_av_frame_t ** pic)
{
    dt_info(TAG,"VD EX DECODE OK \n");
    return 0;
}

static int ex_vdec_release (dtvideo_decoder_t *decoder)
{
    dt_info(TAG,"VD EX RELEASE OK \n");
    return 0;
}

vd_wrapper_t vd_ex_ops = {
    .name = "ex video decoder",
    .vfmt = DT_VIDEO_FORMAT_INVALID,
    .type = DT_TYPE_VIDEO,
    .init = ex_vdec_init,
    .decode_frame = ex_vdec_decode,
    .release = ex_vdec_release,
};
