#ifndef UI_H
#define UI_H

typedef enum{
    EVENT_INVALID = -1,
    EVENT_NONE,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_STOP,
    EVENT_SEEK,
}player_event_t;

player_event_t get_event (int *arg);
int ui_init();
int ui_stop();
#endif
