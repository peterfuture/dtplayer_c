/*
 * =====================================================================================
 *
 *    Filename   :  so_sdl2.c
 *    Description:
 *    Version    :  1.0
 *    Created    :  2017年08月16日 18时34分16秒
 *    Revision   :  none
 *    Compiler   :  gcc
 *    Author     :  peter-s
 *    Email      :  peter_future@outlook.com
 *    Company    :  dt
 *
 * =====================================================================================
 */

#include "dtp_sub_plugin.h"

#define TAG "SO-SDL2"

typedef struct so_sdl2_ctx {
    int id;
} so_sdl2_ctx;

static int so_sdl2_init(so_context_t * soc)
{
    dt_info(TAG, "init so sdl2 OK\n");
    return 0;
}

static int so_sdl2_stop(so_context_t * soc)
{
    dt_info(TAG, "stop so sdl2\n");
    return 0;
}

static int so_sdl2_render(so_context_t * soc, dtav_sub_frame_t * frame)
{
    dt_info(TAG, "sub rennder ok\n");
    return 0;
}

so_wrapper_t so_sdl2_ops = {
    .id = SO_ID_SDL2,
    .name = "sdl2 so",
    .so_init = so_sdl2_init,
    .so_stop = so_sdl2_stop,
    .so_render = so_sdl2_render,
    .private_data_size = sizeof(so_sdl2_ctx),
};
