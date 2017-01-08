#ifndef DT_PLAYER_H
#define DT_PLAYER_H

#include <stdint.h>

#include "dt_utils.h"

#include "dtp_plugin.h"

#include "gui.h"

typedef struct {
    int64_t cur_time;
    int64_t cur_time_ms;
    int64_t duration;
} play_info_t;

typedef struct {
    char *file_name;
    play_info_t info; // record player info from lowlevel core

    int disp_width;   // user specify display width
    int disp_height;  // user specify display height

    ao_wrapper_t ao;  // audio render in use
    vo_wrapper_t vo;  // video render in use

    int quit;         // quit player by user or player end

    void *handle; // player handle
    gui_ctx_t *gui;
} player_t;

#endif
