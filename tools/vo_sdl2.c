#ifdef ENABLE_VO_SDL2

#include "dt_lock.h"

#include "gui.h"
#include "dt_player.h"
#include "dtp_vf.h"


#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <stdio.h>

#define TAG "VO-SDL2"

typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    dt_lock_t mutex;
    int sdl_inited;
} sdl2_ctx_t;

static sdl2_ctx_t sdl2_ctx;
static gui_ctx_t sdl2_gui;
static dtvideo_filter_t sdl2_vf;

static int sdl2_init(gui_ctx_t *gui)
{
    sdl2_ctx_t *ctx = &sdl2_ctx;
    int width = gui->para.width;
    int height = gui->para.height;

    // setup info from para
    gui->mode = 0;
    gui->rect.x = 0;
    gui->rect.y = 0;
    gui->rect.w = width;
    gui->rect.h = height;
    gui->media_width = width;
    gui->media_height = height;
    gui->window_w = width;
    gui->window_h = height;
    gui->pixfmt = gui->para.pixfmt;

    // get max-width max-height
    putenv("SDL_VIDEO_WINDOW_POS=center");
    putenv("SDL_VIDEO_CENTERED=1");
    int flags = SDL_INIT_VIDEO;
    if (SDL_Init(flags)) {
        dt_error("initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    //sdl create win here, For audio only file need a sdl window to contrl too
    flags = SDL_WINDOW_SHOWN;
    ctx->win = SDL_CreateWindow("dtplayer", SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, width, height, flags);
    if (ctx->win == NULL) {
        dt_error(TAG, "SDL_CreateWindow Error:%s \n", SDL_GetError());
        return 1;
    }

    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
        SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        return 1;
    }

    gui->max_width = dm.w;
    gui->max_height = dm.h;

    dt_info(TAG, "CREATE WIN OK, max w:%d , h:%d \n", dm.w, dm.h);
    dt_lock_init(&sdl2_ctx.mutex, NULL);

    return 0;
}

static gui_event_t sdl2_get_event(gui_ctx_t *ctx , args_t *arg)
{
    SDL_Event event;
    SDL_PumpEvents();
    //int count = SDL_PeepEvents(event, LEN(event), SDL_PEEKEVENT, SDL_EVENTMASK(SDL_KEYDOWN));
    SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
    switch (event.type) {
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_q:
            return EVENT_STOP;
        case SDLK_f:
            if (ctx->window_w == ctx->max_width) {
                arg->arg1 = ctx->media_width;
                arg->arg2 = ctx->media_height;
            } else {
                arg->arg1 = ctx->max_width;
                arg->arg2 = ctx->max_height;
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
        arg->arg2 = ctx->window_w;
        return EVENT_SEEK_RATIO;
    }
    break;
    case SDL_QUIT:
        return EVENT_STOP;
    default:
        break;
    }
    return EVENT_NONE;
}

static int sdl2_get_info(gui_ctx_t *ctx, gui_cmd_t cmd, args_t *arg)
{
    return 0;
}

static int sdl2_set_info(gui_ctx_t *gui, gui_cmd_t cmd, args_t arg)
{
    sdl2_ctx_t *ctx = &sdl2_ctx;
    switch (cmd) {
    case GUI_CMD_SET_SIZE: {
        dt_lock(&sdl2_ctx.mutex);
        int width = arg.arg1;
        int height = arg.arg2;

        sdl2_gui.window_w = width;
        sdl2_gui.window_h = height;
        sdl2_gui.rect.w = width;
        sdl2_gui.rect.h = height;

        SDL_SetWindowSize(ctx->win, width, height);
        if (ctx->ren) {
            SDL_DestroyRenderer(ctx->ren);
            ctx->ren = NULL;
        }
        if (ctx->tex) {
            SDL_DestroyTexture(ctx->tex);
            ctx->tex = NULL;
        }

        dt_unlock(&sdl2_ctx.mutex);
    }
    break;
    default:
        break;
    }
    return 0;
}

static int sdl2_stop(gui_ctx_t *gui)
{
    sdl2_ctx_t *ctx = &sdl2_ctx;
    dt_lock(&ctx->mutex);

    if (ctx->win) {
        SDL_DestroyWindow(ctx->win);
        ctx->win = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    dt_unlock(&ctx->mutex);

    return 0;
}

int setup_gui(gui_ctx_t **gui, gui_para_t *para)
{
    sdl2_gui.init = sdl2_init;
    sdl2_gui.get_event = sdl2_get_event;
    sdl2_gui.get_info = sdl2_get_info;
    sdl2_gui.set_info = sdl2_set_info;
    sdl2_gui.stop = sdl2_stop;

    memcpy(&sdl2_gui.para, para, sizeof(gui_para_t));
    *gui = &sdl2_gui;
    return sdl2_gui.init(*gui);
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

static int map_sdl_supported_pixfmt(int pixfmt)
{
    return DTAV_PIX_FMT_YUV420P;
}

static int vo_sdl2_init(vo_context_t * voc)
{
    int width = sdl2_gui.window_w;
    int height = sdl2_gui.window_h;

    //Init vf
    memset(&sdl2_vf, 0, sizeof(dtvideo_filter_t));
    memcpy(&sdl2_vf.para, &voc->para, sizeof(dtvideo_para_t));

    // Check SDL Support PixFmt
    sdl2_gui.pixfmt = sdl2_vf.para.d_pixfmt = map_sdl_supported_pixfmt(
                          voc->para.d_pixfmt);

    dt_lock_init(&sdl2_ctx.mutex, NULL);

#if 0
    // recalc max window widht & height
    SDL_Rect rect;
    calculate_display_rect(&rect, 0, 0, sdl2_gui.max_width, sdl2_gui.max_height,
                           sdl2_gui.para.width, sdl2_gui.para.height);
    dt_info(TAG, "recalc max [w:%d -h:%d] \n", rect.w, rect.h);
    sdl2_gui.max_width = rect.w;
    sdl2_gui.max_height = rect.h;
#endif
    dt_info(TAG, "sdl2 init OK\n");
    return 0;
}

static int vo_sdl2_render(vo_context_t * voc, dt_av_frame_t * frame)
{
    sdl2_ctx_t *ctx = &sdl2_ctx;
    dt_lock(&ctx->mutex);
    dt_debug(TAG, "frame size [%d:%d] \n", frame->width, frame->height);
    // reset sdl vf and window size
    dtvideo_filter_t *vf = &sdl2_vf;
    if (vf->para.d_width != sdl2_gui.window_w
        || vf->para.d_height != sdl2_gui.window_h
        || vf->para.s_width != frame->width
        || vf->para.s_height != frame->height
        || frame->pixfmt != sdl2_gui.pixfmt) {
        vf->para.d_width = sdl2_gui.window_w;
        vf->para.d_height = sdl2_gui.window_h;
        vf->para.s_width = frame->width;
        vf->para.s_height = frame->height;
        vf->para.s_pixfmt = frame->pixfmt;
        vf->para.d_pixfmt = sdl2_gui.pixfmt;
        dt_info(TAG,
                "Need to Update Video Filter Parameter. w:%d->%d h:%d->%d pixfmt:%d->%d \n",
                vf->para.s_width, vf->para.d_width, vf->para.s_height, vf->para.d_height ,
                vf->para.s_pixfmt, vf->para.d_pixfmt);
        video_filter_update(vf);
    }

    video_filter_process(vf, frame);
    SDL_Rect rect;
    rect.x = sdl2_gui.rect.x;
    rect.y = sdl2_gui.rect.y;
    rect.w = sdl2_gui.rect.w;
    rect.h = sdl2_gui.rect.h;

    int width = sdl2_gui.window_w;
    int height = sdl2_gui.window_h;

    if (!ctx->ren) {
        ctx->ren = SDL_CreateRenderer(ctx->win, -1, 0);
    }
    if (!ctx->tex) {
        //ctx->tex = SDL_CreateTexture(ctx->ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, width, height);
        //ctx->tex = SDL_CreateTexture(ctx->ren,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STATIC, width, height);
        ctx->tex = SDL_CreateTexture(ctx->ren, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
    }
    //SDL_UpdateYUVTexture(ctx->tex,NULL, frame->data[0], frame->linesize[0],  frame->data[1], frame->linesize[1],  frame->data[2], frame->linesize[2]);
    SDL_UpdateTexture(ctx->tex, &rect, frame->data[0], frame->linesize[0]);
    SDL_RenderClear(ctx->ren);
    SDL_RenderCopy(ctx->ren, ctx->tex, &rect, &rect);
    SDL_RenderPresent(ctx->ren);

    dt_unlock(&ctx->mutex);
    return 0;
}

static int vo_sdl2_stop(vo_context_t * voc)
{
    sdl2_ctx_t *ctx = &sdl2_ctx;
    if (ctx->ren) {
        SDL_DestroyRenderer(ctx->ren);
        ctx->ren = NULL;
    }
    if (ctx->tex) {
        SDL_DestroyTexture(ctx->tex);
        ctx->tex = NULL;
    }
    ctx->sdl_inited = 0;
    video_filter_stop(&sdl2_vf);
    dt_info(TAG, "stop vo sdl2\n");
    return 0;
}

vo_wrapper_t vo_sdl2_ops = {
    .id = VO_ID_SDL2,
    .name = "sdl2",
    .init = vo_sdl2_init,
    .stop = vo_sdl2_stop,
    .render = vo_sdl2_render,
    .private_data_size = 0
};

#endif
