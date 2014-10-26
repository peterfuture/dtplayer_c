#include "dt_player.h"
#include "ui.h"

#ifdef ENABLE_VO_SDL2 
#include <SDL2/SDL.h>
int sdl2_init();
int sdl2_stop();
int sdl2_get_orig_size(int *w, int *h);
int sdl2_get_max_size(int *w, int *h);
int sdl2_window_resize(int w, int h);
#endif

#ifdef ENABLE_VO_SDL
#include <SDL/SDL.h>
int sdl_init();
int sdl_stop();
#endif



int ui_init(int w, int h)
{
#ifdef ENABLE_VO_SDL2 
    sdl2_init(w,h);
#endif
#ifdef ENABLE_VO_SDL
    sdl_init(w,h);
#endif
    return 0;
}

int ui_stop()
{
#ifdef ENABLE_VO_SDL2 
    sdl2_stop(); 
#endif
#ifdef ENABLE_VO_SDL
    sdl_stop(); 
#endif
    return 0;
}

int ui_get_orig_size(int *w, int *h)
{
#ifdef ENABLE_VO_SDL2
    sdl2_get_orig_size(w, h);
#endif
    return 0;
}

int ui_get_max_size(int *w, int *h)
{
#ifdef ENABLE_VO_SDL2
    sdl2_get_max_size(w, h);
#endif
    return 0;
}

int ui_window_resize(int w, int h)
{
#ifdef ENABLE_VO_SDL2 
    sdl2_window_resize(w, h); 
#endif
    return 0;
}

#ifdef ENABLE_VO_SDL
player_event_t get_event (args_t *arg,ply_ctx_t *ctx)
{
    SDL_Event event;
    SDL_PollEvent(&event);
    switch (event.type) 
    {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) 
            {
                case SDLK_ESCAPE:
                case SDLK_q:
                    return EVENT_STOP;
                case SDLK_f:
                     return EVENT_NONE;
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
                    arg->arg1 = ctx->cur_time + 600;
                    return EVENT_SEEK;
                case SDLK_PAGEDOWN:
                    arg->arg1 = ctx->cur_time - 600;
                    return EVENT_SEEK;
                case SDLK_LEFT:
                    arg->arg1 = ctx->cur_time - 10;
                    return EVENT_SEEK;
                case SDLK_RIGHT:
                    arg->arg1 = ctx->cur_time + 10;
                    return EVENT_SEEK;
                case SDLK_UP:
                    arg->arg1 = ctx->cur_time + 60;
                    return EVENT_SEEK;
                case SDLK_DOWN:
                    arg->arg1 = ctx->cur_time - 60;
                    return EVENT_SEEK;
                default:
                    break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEMOTION:
            {
                double x;
                double frac;
                if (event.type == SDL_MOUSEBUTTONDOWN)
                    x = event.button.x;
                else 
                {
                    if (event.motion.state != SDL_PRESSED)
                        break;
                    x = event.motion.x;
                }
                int64_t ts;
                frac = x / ctx->disp_width;
                ts = frac * ctx->duration;
                arg->arg1 = (int)ts;
                printf("Seek to %d s\n", (int)ts);
                return EVENT_SEEK;
            }

            break;
        case SDL_QUIT:
            return EVENT_STOP;
        default:
            break;
    }
    return EVENT_NONE;

}
#endif

#ifdef ENABLE_VO_SDL2 
player_event_t get_event (args_t *arg,ply_ctx_t *ctx)
{
    static int full_screen = 0;
    SDL_Event event;
    SDL_WaitEvent(&event);
    //SDL_PollEvent(&event);
    switch (event.type) 
    {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) 
            {
                case SDLK_ESCAPE:
                case SDLK_q:
                    return EVENT_STOP;
                case SDLK_f:
                    if(!full_screen)
                    {
                        ui_get_max_size(&arg->arg1, &arg->arg2);
                        full_screen = 1;
                    }
                    else
                    {
                        ui_get_orig_size(&arg->arg1, &arg->arg2);
                        full_screen = 0;
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
                    arg->arg1 = ctx->cur_time + 600;
                    return EVENT_SEEK;
                case SDLK_PAGEDOWN:
                    arg->arg1 = ctx->cur_time - 600;
                    return EVENT_SEEK;
                case SDLK_LEFT:
                    arg->arg1 = ctx->cur_time - 10;
                    return EVENT_SEEK;
                case SDLK_RIGHT:
                    arg->arg1 = ctx->cur_time + 10;
                    return EVENT_SEEK;
                case SDLK_UP:
                    arg->arg1 = ctx->cur_time + 60;
                    return EVENT_SEEK;
                case SDLK_DOWN:
                    arg->arg1 = ctx->cur_time - 60;
                    return EVENT_SEEK;
                default:
                    break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEMOTION:
            {
                double x;
                double frac;
                if (event.type == SDL_MOUSEBUTTONDOWN)
                    x = event.button.x;
                else 
                {
                    if (event.motion.state != SDL_PRESSED)
                        break;
                    x = event.motion.x;
                }
                int64_t ts;
                frac = x / ctx->disp_width;
                ts = frac * ctx->duration;
                arg->arg1 = (int)ts;
                printf("Seek to %d s\n", (int)ts);
                return EVENT_SEEK;
            }

            break;
        case SDL_QUIT:
            return EVENT_STOP;
        default:
            break;
    }
    return EVENT_NONE;
}
#endif


