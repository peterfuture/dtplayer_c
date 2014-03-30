#ifdef ENABLE_VO_SDL

#include "../dtvideo_output.h"

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#define TAG "VO-SDL"

static int fs_screen_width = 0, fs_screen_height = 0;
static SDL_Surface *screen = NULL;
static SDL_Overlay *overlay = NULL;
static int dx, dy, dw, dh, ww, wh;
static dt_lock_t vo_mutex;

static int vo_sdl_init (dtvideo_output_t * vo)
{
    int flags;

    putenv ("SDL_VIDEO_WINDOW_POS=center");
    putenv ("SDL_VIDEO_CENTERED=1");

    flags = SDL_INIT_VIDEO;
    if (SDL_Init (flags))
    {
        dt_error ("initialize SDL: %s\n", SDL_GetError ());
        return -1;
    }
    SDL_EventState (SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState (SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState (SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState (SDL_USEREVENT, SDL_IGNORE);

    const SDL_VideoInfo *vi = SDL_GetVideoInfo ();
    fs_screen_width = vi->current_w;
    fs_screen_height = vi->current_h;

    dx = dy = 0;
    ww = dw = vo->para.d_width;
    wh = dh = vo->para.d_height;

    flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE;
    //SDL_WM_SetIcon(SDL_LoadBMP("icon.bmp"), NULL);
    screen = SDL_SetVideoMode (ww, wh, 0, flags);
    SDL_WM_SetCaption ("dtplayer", NULL);

    overlay = SDL_CreateYUVOverlay (dw, dh, SDL_YV12_OVERLAY, screen);

    dt_lock_init (&vo_mutex, NULL);

    dt_info (TAG, "w:%d h:%d planes:%d \n", overlay->w, overlay->h, overlay->planes);
    dt_info (TAG, "pitches:%d %d %d \n", overlay->pitches[0], overlay->pitches[1], overlay->pitches[2]);
    dt_info (TAG, "init vo sdl OK\n");

    return 0;
}

static int vo_sdl_stop (dtvideo_output_t * vo)
{
    dt_lock (&vo_mutex);

    SDL_FreeYUVOverlay (overlay);
    overlay = NULL;

    dt_unlock (&vo_mutex);

    dt_info (TAG, "uninit vo sdl\n");
    return 0;
}

static int vo_sdl_render (dtvideo_output_t * vo, AVPicture_t * pict)
{
    dt_lock (&vo_mutex);

    SDL_Rect rect;
    SDL_LockYUVOverlay (overlay);

    memcpy (overlay->pixels[0], pict->data[0], dw * dh);
    memcpy (overlay->pixels[1], pict->data[2], dw * dh / 4);
    memcpy (overlay->pixels[2], pict->data[1], dw * dh / 4);
    SDL_UnlockYUVOverlay (overlay);

    rect.x = dx;
    rect.y = dy;
    rect.w = dw;
    rect.h = dh;
    SDL_DisplayYUVOverlay (overlay, &rect);

    dt_unlock (&vo_mutex);
    return 0;
}

vo_operations_t vo_sdl_ops = {
    .id = VO_ID_SDL,
    .name = "sdl",
    .vo_init = vo_sdl_init,
    .vo_stop = vo_sdl_stop,
    .vo_render = vo_sdl_render,
};

#endif /* ENABLE_VO_SDL */
