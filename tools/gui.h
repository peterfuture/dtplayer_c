#ifndef UI_H
#define UI_H

#include "dtp_plugin.h"

typedef enum {
    GUI_CMD_UNKOWN = -1,
    GUI_CMD_GET_WINDOW_SIZE = 0,
    GUI_CMD_GET_MAX_SIZE,
    GUI_CMD_GET_MODE,

    GUI_CMD_SET_SIZE = 0x100,
    GUI_CMD_SET_MODE,

    GUI_CMD_MAX      = 0xFFFF
} gui_cmd_t;

typedef enum {
    EVENT_INVALID = -1,
    EVENT_NONE,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_STOP,
    EVENT_SEEK,
    EVENT_SEEK_RATIO,
    EVENT_RESIZE,

    EVENT_VOLUME_ADD,

    //AE
    EVENT_AE,

    EVENT_MAX      = 0xFFFF
} gui_event_t;

typedef enum {
    DISPLAY_MODE_DEFAULT = 0,
    DISP_MODE_FULLSCREEN = 1,
    DISPLAY_MODE_4_3,
    DISPLAY_MODE_16_9
} disp_mode_t;

typedef struct {
    int width;
    int height;
    int pixfmt;
} gui_para_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} gui_rect_t;

typedef struct {
    int arg1;
    int arg2;
} args_t;

typedef struct gui_ctx {
    gui_para_t para;
    disp_mode_t mode;
    int max_width;     // max window height - fullscreen - not change
    int max_height;    // max window height - fullscreen - not change
    int media_width;   // media width, from player - not change
    int media_height;  // media height, from player - not change
    int window_w;      // current window width - maybe change by user
    int window_h;      // current window height - maybe change by user
    int pixfmt;
    gui_rect_t rect;   // current display region
    void *context;     // pointer to player_t, NOT used now

    int (*init)(struct gui_ctx *ctx);
    gui_event_t (*get_event)(struct gui_ctx *ctx, args_t *arg);
    int (*get_info)(struct gui_ctx *ctx, gui_cmd_t cmd, args_t *arg);
    int (*set_info)(struct gui_ctx *ctx, gui_cmd_t cmd, args_t arg);
    int (*stop)(struct gui_ctx *ctx);
} gui_ctx_t;

int setup_gui(gui_ctx_t **gui, gui_para_t *para);
int setup_ao(ao_wrapper_t *ao);
int setup_vo(vo_wrapper_t *vo);

#endif
