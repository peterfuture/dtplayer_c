#ifndef UI_H
#define UI_H

#include "dt_player.h"

typedef enum{
    EVENT_INVALID = -1,
    EVENT_NONE,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_STOP,
    EVENT_SEEK,
    EVENT_RESIZE,
    
    EVENT_VOLUME_ADD,

    //AE
    EVENT_AE,

    EVENT_MAX      = 0x800
}player_event_t;

typedef struct ui_ctx{
    int full_screen_flag;

    int max_width;
    int max_height;

    int orig_width;
    int orig_height;

    int cur_x_pos;
    int cur_y_pos;
    int cur_width;
    int cur_height;

    void *context;
}ui_ctx_t;

player_event_t get_event (args_t *arg,ply_ctx_t *ctx);
int ui_init(ply_ctx_t *ply_ctx, ui_ctx_t *ui_ctx);
int ui_get_orig_size(int *w, int *h);
int ui_get_max_size(int *w, int *h);
int ui_stop();
int ui_window_resize(int w, int h);
#endif
