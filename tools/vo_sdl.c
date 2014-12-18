#include "dtvideo_output.h"
#include "dtvideo_filter.h"
#include "dt_player.h"
#include "ui.h"

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#define TAG "VO-SDL"

typedef struct
{
    SDL_Surface *screen;
    SDL_Overlay *overlay;
    dt_lock_t vo_mutex;

    dtvideo_filter_t vf;

    ui_ctx_t *ui_ctx;
    ply_ctx_t *ply_ctx;
}sdl_ctx_t;

static sdl_ctx_t sdl_ctx;

int sdl_init(ply_ctx_t *ply_ctx, ui_ctx_t *ui_ctx)
{
    int flags;

    memset(&sdl_ctx, 0, sizeof(sdl_ctx_t));
    sdl_ctx.ui_ctx = ui_ctx;
    sdl_ctx.ply_ctx = ply_ctx;
 
    putenv ("SDL_VIDEO_WINDOW_POS=center");
    putenv ("SDL_VIDEO_CENTERED=1");

    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if(SDL_Init(flags))
    {
        dt_error ("initialize SDL: %s\n", SDL_GetError ());
        return -1;
    }

    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    const SDL_VideoInfo *vi = SDL_GetVideoInfo();
    ui_ctx->full_screen_flag = 0;
    ui_ctx->max_width = vi->current_w;
    ui_ctx->max_height = vi->current_h;

    ui_ctx->cur_width = ui_ctx->orig_width;
    ui_ctx->cur_height = ui_ctx->orig_height;

    flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE;

    //SDL_WM_SetIcon(SDL_LoadBMP("icon.bmp"), NULL);
    sdl_ctx.screen = SDL_SetVideoMode(ui_ctx->cur_width, ui_ctx->cur_height, 0, flags);
    SDL_WM_SetCaption("dtplayer", "dtplayer");
    
    dt_lock_init(&sdl_ctx.vo_mutex, NULL);
    return 0;
}

int sdl_window_resize(int w, int h)
{
    sdl_ctx_t *ctx = &sdl_ctx;
    dt_lock (&ctx->vo_mutex);

    ctx->ui_ctx->cur_width = w;
    ctx->ui_ctx->cur_height = h;

    sdl_ctx.vf.para.d_width = w;
    sdl_ctx.vf.para.d_height = h;
    video_filter_update(&sdl_ctx.vf);
    
    SDL_FreeYUVOverlay(sdl_ctx.overlay);
    dt_info(TAG, "W:%d H:%d \n", w, h);
    int flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
    if(sdl_ctx.ui_ctx->max_width == w)
        flags |= SDL_FULLSCREEN;
    else
        flags |= SDL_RESIZABLE;
    
    sdl_ctx.screen = SDL_SetVideoMode(w, h, 0, flags);
    SDL_WM_SetCaption("dtplayer", "dtplayer");
    sdl_ctx.overlay = SDL_CreateYUVOverlay(w, h, SDL_YV12_OVERLAY, sdl_ctx.screen);
    
    dt_unlock (&ctx->vo_mutex);
    return 0;
}

int sdl_stop()
{
    return 0;
}

static int vo_sdl_init(dtvideo_output_t * vo)
{
    int cur_width = sdl_ctx.ui_ctx->cur_width;
    int cur_height = sdl_ctx.ui_ctx->cur_height;
    
    sdl_ctx.overlay = SDL_CreateYUVOverlay(cur_width, cur_height, SDL_YV12_OVERLAY, sdl_ctx.screen);

    //Init vf
    memset(&sdl_ctx.vf, 0, sizeof(dtvideo_filter_t));
    memcpy(&sdl_ctx.vf.para, vo->para, sizeof(dtvideo_para_t));
    sdl_ctx.vf.para.s_pixfmt = sdl_ctx.vf.para.d_pixfmt;
    sdl_ctx.vf.para.d_width = sdl_ctx.ui_ctx->cur_width;
    sdl_ctx.vf.para.d_height = sdl_ctx.ui_ctx->cur_height;
    video_filter_init(&sdl_ctx.vf); 
 
    dt_lock_init(&sdl_ctx.vo_mutex, NULL);

    dt_info(TAG, "w:%d h:%d planes:%d \n", sdl_ctx.overlay->w, sdl_ctx.overlay->h, sdl_ctx.overlay->planes);
    dt_info(TAG, "pitches:%d %d %d \n", sdl_ctx.overlay->pitches[0], sdl_ctx.overlay->pitches[1], sdl_ctx.overlay->pitches[2]);
    dt_info(TAG, "init vo sdl OK\n");

    return 0;
}

static int vo_sdl_stop(dtvideo_output_t * vo)
{
    dt_lock(&sdl_ctx.vo_mutex);

    SDL_FreeYUVOverlay(sdl_ctx.overlay);
    sdl_ctx.overlay = NULL;
    video_filter_stop(&sdl_ctx.vf);
    dt_unlock(&sdl_ctx.vo_mutex);

    dt_info(TAG, "uninit vo sdl\n");
    return 0;
}

static int vo_sdl_render(dtvideo_output_t * vo, dt_av_frame_t * frame)
{
    dt_lock(&sdl_ctx.vo_mutex);

    video_filter_process(&sdl_ctx.vf, frame);
    
    SDL_Rect rect;
    SDL_LockYUVOverlay(sdl_ctx.overlay);

    int cur_x_pos = sdl_ctx.ui_ctx->cur_x_pos;
    int cur_y_pos = sdl_ctx.ui_ctx->cur_y_pos;
    int cur_width = sdl_ctx.ui_ctx->cur_width;
    int cur_height = sdl_ctx.ui_ctx->cur_height;

    memcpy(sdl_ctx.overlay->pixels[0], frame->data[0], cur_width * cur_height);
    memcpy(sdl_ctx.overlay->pixels[1], frame->data[2], cur_width * cur_height / 4);
    memcpy(sdl_ctx.overlay->pixels[2], frame->data[1], cur_width * cur_height / 4);
    SDL_UnlockYUVOverlay(sdl_ctx.overlay);

    rect.x = cur_x_pos;
    rect.y = cur_y_pos;
    rect.w = cur_width;
    rect.h = cur_height;
    SDL_DisplayYUVOverlay(sdl_ctx.overlay, &rect);

    dt_unlock (&sdl_ctx.vo_mutex);
    return 0;
}

const char *vo_sdl_name = "SDL VO";

void vo_sdl_setup(vo_wrapper_t *wrapper)
{
    if(!wrapper) return;
    wrapper->id = VO_ID_SDL;
    wrapper->name = vo_sdl_name;
    wrapper->vo_init = vo_sdl_init;
    wrapper->vo_stop = vo_sdl_stop;
    wrapper->vo_render = vo_sdl_render;
    return;
}

vo_wrapper_t vo_sdl_ops = {
    .id = VO_ID_SDL,
    .name = "sdl",
    .vo_init = vo_sdl_init,
    .vo_stop = vo_sdl_stop,
    .vo_render = vo_sdl_render,
};
