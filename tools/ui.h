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
}player_event_t;

player_event_t get_event (int *arg,ply_ctx_t *ctx);
int ui_init();
int ui_stop();
#endif
