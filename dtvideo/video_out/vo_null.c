#if 1 //ENABLE_VO_NULL

#include "dtvideo_output.h"
#include "dtvideo_filter.h"

#define TAG "VO-NULL"

static int vo_null_init(dtvideo_output_t * vo)
{
    dt_info(TAG, "init vo null OK\n");
    return 0;
}

static int vo_null_stop(dtvideo_output_t * vo)
{
    dt_info(TAG, "uninit vo sdl\n");
    return 0;
}

static int vo_null_render(dtvideo_output_t * vo, dt_av_frame_t * frame)
{
    dt_info(TAG, "vo null render one frame\n");
    return 0;
}

vo_wrapper_t vo_null_ops = {
    .id = VO_ID_NULL,
    .name = "null vo",
    .vo_init = vo_null_init,
    .vo_stop = vo_null_stop,
    .vo_render = vo_null_render,
};
#endif
