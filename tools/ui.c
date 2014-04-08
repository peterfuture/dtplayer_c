#include "ui.h"
#include <SDL2/SDL.h>

int sdl2_init();
int sdl2_stop();

int ui_init()
{
    sdl2_init();    
    return 0;
}

int ui_stop()
{
    sdl2_stop(); 
    return 0;
}


player_event_t get_event (int *arg)
{
    SDL_Event event;
    SDL_WaitEvent(&event);
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
                    *arg = 600.0;
                    return EVENT_SEEK;
                case SDLK_PAGEDOWN:
                    *arg = -600.0;
                    return EVENT_SEEK;
                case SDLK_LEFT:
                    *arg = -10.0;
                    return EVENT_SEEK;
                case SDLK_RIGHT:
                    *arg = 10.0;
                    return EVENT_SEEK;
                case SDLK_UP:
                    *arg = 60.0;
                    return EVENT_SEEK;
                case SDLK_DOWN:
                    *arg = -60.0;
                    return EVENT_SEEK;
                default:
                    break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEMOTION:
            break;
        case SDL_QUIT:
            return EVENT_STOP;
        default:
            break;
    }
    return EVENT_NONE;
}


