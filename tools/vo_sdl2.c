#include "dtvideo_output.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <stdio.h>

#define TAG "VO-SDL2"

vo_wrapper_t vo_sdl2_ops;

typedef struct{
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    dt_lock_t vo_mutex;
    int dx,dy,dw,dh;
    int sdl_inited;
}sdl2_ctx_t;

static sdl2_ctx_t sdl2_ctx;

int sdl2_init()
{
    int flags = 0;
    
    if (SDL_Init (SDL_INIT_VIDEO)) {
        dt_info(TAG,"UI INIT FAILED \n");
        return -1;
    }
    memset(&sdl2_ctx,0,sizeof(sdl2_ctx_t));
    flags = SDL_WINDOW_SHOWN; 
    sdl2_ctx_t *ctx = &sdl2_ctx;
    ctx->dx = ctx->dy = 0;
    //ctx->dw = vo->para.d_width;
    //ctx->dh = vo->para.d_height;
    ctx->dw = 720;
    ctx->dh = 480;
    
    //sdl create win here, For audio only file need a sdl window to contrl too
    ctx->win = SDL_CreateWindow("dtplayer",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,ctx->dw,ctx->dh,flags);
    if(ctx->win == NULL)
    {
        dt_error(TAG,"SDL_CreateWindow Error:%s \n",SDL_GetError());
        return 1;
    }
    dt_info(TAG,"CREATE WIN OK\n");
    dt_lock_init (&sdl2_ctx.vo_mutex, NULL);
    return 0;
}

int sdl2_stop()
{
    sdl2_ctx_t *ctx = &sdl2_ctx;
    dt_lock (&ctx->vo_mutex);
    
    if(ctx->win)
    {
        SDL_DestroyWindow(ctx->win);
        ctx->win = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    dt_unlock (&ctx->vo_mutex);
    return 0;
}


static int vo_sdl2_init ()
{

    vo_wrapper_t *wrap = &vo_sdl2_ops;
    wrap->handle = (void *)&sdl2_ctx;
    dt_info (TAG, "sdl2 init OK\n");
    return 0;
}

static int vo_sdl2_render (AVPicture_t * pict)
{
    vo_wrapper_t *wrap = &vo_sdl2_ops;
    
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)wrap->handle;
    dt_lock (&ctx->vo_mutex);

    SDL_Rect dst;
    dst.x = ctx->dx;
    dst.y = ctx->dy;
    dst.w = ctx->dw;
    dst.h = ctx->dh;

    if(!ctx->ren)
        ctx->ren = SDL_CreateRenderer(ctx->win,-1,0);
    if(!ctx->tex)    
        ctx->tex = SDL_CreateTexture(ctx->ren,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STATIC,ctx->dw,ctx->dh);
    //ctx->tex = SDL_CreateTexture(ctx->ren,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STREAMING,ctx->dw,ctx->dh);

    //SDL_UpdateYUVTexture(ctx->tex,NULL, pict->data[0], pict->linesize[0],  pict->data[1], pict->linesize[1],  pict->data[2], pict->linesize[2]);
    SDL_UpdateTexture(ctx->tex, &dst, pict->data[0], pict->linesize[0]);
    SDL_RenderClear(ctx->ren);
    SDL_RenderCopy(ctx->ren,ctx->tex,&dst,&dst);
    SDL_RenderPresent(ctx->ren);

    dt_unlock (&ctx->vo_mutex);
    return 0;
}

static int vo_sdl2_stop ()
{
    vo_wrapper_t *wrap = &vo_sdl2_ops;
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)wrap->handle;
    if(ctx->ren)
    {
        SDL_DestroyRenderer(ctx->ren);
        ctx->ren = NULL;
    }
    if(ctx->tex)
    {
        SDL_DestroyTexture(ctx->tex);
        ctx->tex = NULL;
    }
    ctx->sdl_inited = 0;
    wrap->handle = NULL;
    dt_info (TAG, "stop vo sdl\n");
    return 0;
}

vo_wrapper_t vo_sdl2_ops = {
    .id = VO_ID_SDL2,
    .name = "sdl2",
    .vo_init = vo_sdl2_init,
    .vo_stop = vo_sdl2_stop,
    .vo_render = vo_sdl2_render,
};
