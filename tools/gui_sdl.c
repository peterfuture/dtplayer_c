/*
 * include two parts
 *
 * part1: window manager
 *
 * part2: sdlao
 *
 * part2: sdlvo
 *
 * */

#ifdef ENABLE_VO_SDL

#include "gui.h"
#include "dt_player.h"
#include "dtvideo_filter.h"
#include <SDL/SDL.h>

#define TAG "SDL-GUI"
#define VO_TAG "SDL-VO"

typedef struct {
    SDL_Surface *screen;
    SDL_Overlay *overlay;
    dt_lock_t mutex;
} sdl_window_t;

static sdl_window_t window;
static gui_ctx_t sdl_gui;
static dtvideo_filter_t sdl_vf;

//==================window part=====================

static int sdl_init(gui_ctx_t *gui)
{
    // setup info from para
    gui->mode = 0;
    gui->rect.x = 0;
    gui->rect.y = 0;
    gui->rect.w = gui->para.width;
    gui->rect.h = gui->para.height;
    gui->media_width = gui->para.width;
    gui->media_height = gui->para.height;
    gui->window_w = gui->para.width;
    gui->window_h = gui->para.height;
    gui->pixfmt = gui->para.pixfmt;

    // get max-width max-height
    putenv("SDL_VIDEO_WINDOW_POS=center");
    putenv("SDL_VIDEO_CENTERED=1");
    int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (SDL_Init(flags)) {
        dt_error("initialize SDL: %s\n", SDL_GetError());
        return -1;
    }
    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    const SDL_VideoInfo *vi = SDL_GetVideoInfo();
    gui->max_width = vi->current_w;
    gui->max_height = vi->current_h;


    // create window
    flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE;
    //SDL_WM_SetIcon(SDL_LoadBMP("icon.bmp"), NULL);
    window.screen = SDL_SetVideoMode(gui->window_w, gui->window_h, 0, flags);
    SDL_WM_SetCaption("dtplayer", "dttv");
    dt_lock_init(&window.mutex, NULL);

    dt_info(TAG, "Create window with size<%d - %d> ok \n", gui->window_w, gui->window_h);
    return 0;
}

static gui_event_t sdl_get_event(gui_ctx_t *ctx , args_t *arg)
{
    SDL_Event event;
    SDL_PollEvent(&event);
    switch (event.type) {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_q:
            return EVENT_STOP;
        case SDLK_f:
            if (sdl_gui.window_w == sdl_gui.max_width) {
                arg->arg1 = sdl_gui.media_width;
                arg->arg2 = sdl_gui.media_height;
            } else {
                arg->arg1 = sdl_gui.max_width;
                arg->arg2 = sdl_gui.max_height;
            }
            return EVENT_RESIZE;
        case SDLK_p:
        case SDLK_SPACE:
            //toggle_pause(cur_stream);
            return EVENT_PAUSE;
        case SDLK_s: // S: Step to next frame
            break;
        case SDLK_a:
            break;
        case SDLK_v:
            return EVENT_VOLUME_ADD;
        case SDLK_t:
            break;
        case SDLK_w:
            break;
#ifdef ENABLE_DTAP
        case SDLK_e:
            return EVENT_AE;
#endif
        case SDLK_PAGEUP:
            arg->arg1 = 600;
            return EVENT_SEEK;
        case SDLK_PAGEDOWN:
            arg->arg1 = 600;
            return EVENT_SEEK;
        case SDLK_LEFT:
            arg->arg1 = -10;
            return EVENT_SEEK;
        case SDLK_RIGHT:
            arg->arg1 = 10;
            return EVENT_SEEK;
        case SDLK_UP:
            arg->arg1 = -60;
            return EVENT_SEEK;
        case SDLK_DOWN:
            arg->arg1 = 60;
            return EVENT_SEEK;
        default:
            break;
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEMOTION: {
        double x;
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            x = event.button.x;
        } else {
            if (event.motion.state != SDL_PRESSED) {
                break;
            }
            x = event.motion.x;
        }
        arg->arg1 = (int)x;
        arg->arg2 = sdl_gui.window_w;
        return EVENT_SEEK_RATIO;
    }
    break;
    case SDL_VIDEORESIZE:
        arg->arg1 = event.resize.w;
        arg->arg2 = event.resize.h;
        return EVENT_RESIZE;
    case SDL_QUIT:
        return EVENT_STOP;
    default:
        break;
    }
    return EVENT_NONE;
}

static int sdl_get_info(gui_ctx_t *ctx, gui_cmd_t cmd, args_t *arg)
{
    return 0;
}

static int sdl_set_info(gui_ctx_t *ctx, gui_cmd_t cmd, args_t arg)
{
    switch (cmd) {
    case GUI_CMD_SET_SIZE: {
        dt_lock(&window.mutex);
        int width = arg.arg1;
        int height = arg.arg2;
        int flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
        if (width == sdl_gui.max_width) {
            flags |= SDL_FULLSCREEN;
        } else {
            flags |= SDL_RESIZABLE;
        }
        window.screen = SDL_SetVideoMode(width , height, 0, flags);
        SDL_WM_SetCaption("dtplayer", "dttv");
        sdl_gui.window_w = width;
        sdl_gui.window_h = height;
        sdl_gui.rect.w = width;
        sdl_gui.rect.h = height;
        dt_unlock(&window.mutex);
    }
    break;
    default:
        break;
    }
    return 0;
}

static int sdl_stop(gui_ctx_t *ctx)
{
    return 0;
}

/*
 * init gui function pointer
 *
 * */
int setup_gui(gui_ctx_t **gui, gui_para_t *para)
{
    memset(&sdl_gui, 0 , sizeof(gui_ctx_t));
    sdl_gui.init = sdl_init;
    sdl_gui.get_event = sdl_get_event;
    sdl_gui.get_info = sdl_get_info;
    sdl_gui.set_info = sdl_set_info;
    sdl_gui.stop = sdl_stop;

    memcpy(&sdl_gui.para, para, sizeof(gui_para_t));
    *gui = &sdl_gui;
    return sdl_gui.init(*gui);
}

//==================vo part=====================
#include <math.h>
static void calculate_display_rect(SDL_Rect *rect,
                                   int scr_xleft, int scr_ytop, int scr_width, int scr_height,
                                   int pic_width, int pic_height)
{
    float aspect_ratio;
    int width, height, x, y;
    aspect_ratio = 1.0;
    aspect_ratio *= (float)pic_width / (float)pic_height;

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = lrintf(height * aspect_ratio) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = lrintf(width / aspect_ratio) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop  + y;
    rect->w = width;
    rect->h = height;
}

static int vo_sdl_init(dtvideo_output_t * vo)
{
    int width = sdl_gui.window_w;
    int height = sdl_gui.window_h;

    //Init vf
    memset(&sdl_vf, 0, sizeof(dtvideo_filter_t));
    memcpy(&sdl_vf.para, vo->para, sizeof(dtvideo_para_t));
    sdl_vf.para.s_pixfmt = sdl_vf.para.d_pixfmt;
    sdl_vf.para.d_width = width;
    sdl_vf.para.d_height = height;
    video_filter_init(&sdl_vf);
    dt_lock_init(&window.mutex, NULL);

    // recalc max window widht & height
    SDL_Rect rect;
    calculate_display_rect(&rect, 0, 0, sdl_gui.max_width, sdl_gui.max_height, sdl_gui.para.width, sdl_gui.para.height);
    dt_info(TAG, "recalc max [w:%d -h:%d] \n", rect.w, rect.h);
    sdl_gui.max_width = rect.w;
    sdl_gui.max_height = rect.h;
    dt_info(TAG, "init vo sdl OK\n");
    return 0;
}

static int vo_sdl_render(dtvideo_output_t * vo, dt_av_frame_t * frame)
{
    dt_lock(&window.mutex);
    // reset sdl vf and window size
    dtvideo_filter_t *vf = &sdl_vf;
    if (vf->para.d_width != sdl_gui.window_w || vf->para.d_height != sdl_gui.window_h || vf->para.d_pixfmt != sdl_gui.pixfmt) {
        vf->para.d_width = sdl_gui.window_w;
        vf->para.d_height = sdl_gui.window_h;
        vf->para.d_pixfmt = sdl_gui.pixfmt;
        video_filter_update(vf);
    }

    video_filter_process(&sdl_vf, frame);
    SDL_Rect rect;

    int x = sdl_gui.rect.x;
    int y = sdl_gui.rect.y;
    int w = sdl_gui.rect.w;
    int h = sdl_gui.rect.h;
    int width = sdl_gui.window_w;
    int height = sdl_gui.window_h;

    if (window.overlay) {
        SDL_FreeYUVOverlay(window.overlay);
    }
    // Create overlay
    if (sdl_gui.pixfmt == 0) {
        window.overlay = SDL_CreateYUVOverlay(width, height, SDL_YV12_OVERLAY, window.screen);
    } else if (sdl_gui.pixfmt == 1) {
        window.overlay = SDL_CreateYUVOverlay(width, height, SDL_YV12_OVERLAY, window.screen);
    }

    SDL_LockYUVOverlay(window.overlay);

    memcpy(window.overlay->pixels[0], frame->data[0], width * height);
    memcpy(window.overlay->pixels[1], frame->data[2], width * height / 4);
    memcpy(window.overlay->pixels[2], frame->data[1], width * height / 4);
    SDL_UnlockYUVOverlay(window.overlay);

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    dt_debug(TAG, "rect[%d-%d-%d-%d] size[%d-%d]\n", rect.x, rect.y, rect.w, rect.h, width, height);
    SDL_DisplayYUVOverlay(window.overlay, &rect);

    dt_unlock(&window.mutex);
    return 0;
}

static int vo_sdl_stop(dtvideo_output_t * vo)
{
    dt_lock(&window.mutex);
    if (window.overlay) {
        SDL_FreeYUVOverlay(window.overlay);
    }
    window.overlay = NULL;
    video_filter_stop(&sdl_vf);
    dt_unlock(&window.mutex);
    dt_info(TAG, "uninit vo sdl\n");
    return 0;
}

const char *vo_sdl_name = "SDL VO";

vo_wrapper_t vo_sdl_ops = {
    .id = VO_ID_SDL,
    .name = "sdl",
    .vo_init = vo_sdl_init,
    .vo_stop = vo_sdl_stop,
    .vo_render = vo_sdl_render,
};

int setup_vo(vo_wrapper_t *wrapper)
{
    if (!wrapper) {
        return -1;
    }
    wrapper->id = VO_ID_SDL;
    wrapper->name = vo_sdl_name;
    wrapper->vo_init = vo_sdl_init;
    wrapper->vo_stop = vo_sdl_stop;
    wrapper->vo_render = vo_sdl_render;
    return 0;
}

#endif
