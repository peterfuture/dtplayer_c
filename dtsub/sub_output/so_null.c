/***********************************************************************
**
**  Module : so_null.c
**  Summary: sub render example
**  Section: dtsub
**  Author : peter
**  Notes  : 
**
***********************************************************************/

#include "dtsub_output.h"

#define TAG "SO-SDL"

static int so_null_init (dtsub_output_t * so)
{
    dt_info (TAG, "init so null OK\n");
    return 0;
}

static int so_null_stop (dtsub_output_t * so)
{
    dt_info (TAG, "stop so null\n");
    return 0;
}

static int so_null_render (dtsub_output_t * so, dtav_sub_frame_t * frame)
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
};
