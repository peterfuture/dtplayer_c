#include "ui.h"

#ifdef ENABLE_VO_SDL2 
#include <SDL2/SDL.h>

int sdl2_init();
int sdl2_stop();

#endif

int ui_init()
{
#ifdef ENABLE_VO_SDL2 
    sdl2_init();    
#endif
    return 0;
}

int ui_stop()
{
#ifdef ENABLE_VO_SDL2 
    sdl2_stop(); 
#endif
    return 0;
}

player_event_t get_event (int *arg,ply_ctx_t *ctx)
{
#ifdef ENABLE_VO_SDL2 
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
                    //toggle_full_screen(cur_stream);
                    //cur_stream->force_refresh = 1;
                    break;
                case SDLK_p:
                case SDLK_SPACE:
                    //toggle_pause(cur_stream);
                    return EVENT_PAUSE;
                case SDLK_s: // S: Step to next frame
                    break;
                case SDLK_a:
                    break;
                case SDLK_v:
                    break;
                case SDLK_t:
                    break;
                case SDLK_w:
                    break;
                case SDLK_PAGEUP:
                    *arg = ctx->cur_time + 600;
                    return EVENT_SEEK;
                case SDLK_PAGEDOWN:
                    *arg = ctx->cur_time - 600;
                    return EVENT_SEEK;
                case SDLK_LEFT:
                    *arg = ctx->cur_time - 10;
                    return EVENT_SEEK;
                case SDLK_RIGHT:
                    *arg = ctx->cur_time + 10;
                    return EVENT_SEEK;
                case SDLK_UP:
                    *arg = ctx->cur_time + 60;
                    return EVENT_SEEK;
                case SDLK_DOWN:
                    *arg = ctx->cur_time - 60;
                    return EVENT_SEEK;
                default:
                    break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEMOTION:
            {
                double x;
                double incr, pos, frac;
                if (event.type == SDL_MOUSEBUTTONDOWN)
                    x = event.button.x;
                else 
                {
                    if (event.motion.state != SDL_PRESSED)
                        break;
                    x = event.motion.x;
                }
                int64_t ts;
                int ns, hh, mm, ss;
                int tns, thh, tmm, tss;
                tns  = ctx->duration / 1000000LL;
                thh  = tns / 3600;
                tmm  = (tns % 3600) / 60;
                tss  = (tns % 60);
                frac = x / ctx->disp_width;
                ns   = frac * tns;
                hh   = ns / 3600;
                mm   = (ns % 3600) / 60;
                ss   = (ns % 60);
                fprintf(stderr, "Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)       \n", frac*100,
                    hh, mm, ss, thh, tmm, tss);
                ts = frac * ctx->duration;
                *arg = ts;
                return EVENT_SEEK;
            }

            break;
        case SDL_QUIT:
            return EVENT_STOP;
        default:
            break;
    }
#endif
    return EVENT_NONE;
}


