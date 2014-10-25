#ifndef DT_PLAYER_H
#define DT_PLAYER_H

#include "stdint.h"
#include "ao_wrapper.h"

typedef struct
{
    int arg1;
    int arg2;
}args_t;

typedef struct ply_ctx
{
    void * handle; // player handle
    
    int64_t cur_time;
    int64_t cur_time_ms;
    int64_t duration;
    int disp_width;
    int disp_height;

    ao_wrapper_t ao;
    int volume;

    int exit_flag;
}ply_ctx_t;

#endif
