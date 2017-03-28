#if 1 //ENABLE_VO_NULL

#include "dt_utils.h"
#include "dtvideo_output.h"
#include "dtp_vf.h"

#define TAG "VO-NULL"

static int vo_null_init(vo_context_t * voc)
{
    dt_info(TAG, "init vo null OK\n");
    return 0;
}

static int vo_null_stop(vo_context_t * voc)
{
    dt_info(TAG, "uninit vo sdl\n");
    return 0;
}

static int vo_null_render(vo_context_t * voc, dt_av_frame_t * frame)
{
    dt_info(TAG, "vo null render one frame\n");
    return 0;
}

vo_wrapper_t vo_null_ops = {
    .id = VO_ID_NULL,
    .name = "null vo",
    .init = vo_null_init,
    .stop = vo_null_stop,
    .render = vo_null_render,
    .private_data_size = 0
};
#endif
