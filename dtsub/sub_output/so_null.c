/***********************************************************************
**
**  Module : so_null.c
**  Summary: sub render example
**  Section: dtsub
**  Author : peter
**  Notes  :
**
***********************************************************************/
#include "dt_utils.h"
#include "dtp_sub_plugin.h"

#define TAG "SO-NULL"

static int so_null_init(so_context_t * soc)
{
    dt_info(TAG, "init so null OK\n");
    return 0;
}

static int so_null_stop(so_context_t * soc)
{
    dt_info(TAG, "stop so null\n");
    return 0;
}

static int so_null_render(so_context_t * soc, dtav_sub_frame_t * frame)
{
    dt_info(TAG, "sub rennder ok\n");
    return 0;
}

so_wrapper_t so_null_ops = {
    .id = SO_ID_NULL,
    .name = "null so",
    .so_init = so_null_init,
    .so_stop = so_null_stop,
    .so_render = so_null_render,
    .private_data_size = 0,
};
