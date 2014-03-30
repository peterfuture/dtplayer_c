#ifdef ENABLE_VO_SDL2

#include "../dtvideo_output.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <stdio.h>

#define TAG "VO-SDL2"

typedef struct{
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    dt_lock_t vo_mutex;
    int dx,dy,dw,dh;
    int sdl_inited;
}sdl2_ctx_t;

static int vo_sdl2_init (dtvideo_output_t * vo)
{
    sdl2_ctx_t *ctx = malloc(sizeof(sdl2_ctx_t));
    memset(ctx,0,sizeof(*ctx));
    dt_lock_init (&ctx->vo_mutex, NULL);
    vo->vout_ops->handle = (void *)ctx;
    dt_info (TAG, "sdl2 init OK\n");
    return 0;
}

static int sdl2_pre_init (dtvideo_output_t * vo)
{
    int flags;
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)vo->vout_ops->handle;
    
    putenv ("SDL_VIDEO_WINDOW_POS=center");
    putenv ("SDL_VIDEO_CENTERED=1");
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_Init(SDL_INIT_VIDEO);
   
    ctx->dx = ctx->dy = 0;
    ctx->dw = vo->para.d_width;
    ctx->dh = vo->para.d_height;
   
    flags = SDL_WINDOW_SHOWN; 
    ctx->win = SDL_CreateWindow("dtplayer",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,ctx->dw,ctx->dh,flags);
    if(ctx->win == NULL)
    {
        dt_error(TAG,"SDL_CreateWindow Error:%s \n",SDL_GetError());
        return -1;
    }
    flags = SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED; 
    ctx->ren = SDL_CreateRenderer(ctx->win,-1,0);
    if(ctx->ren == NULL)
    {
        dt_error(TAG,"SDL_CreateRenderer Error:%s \n",SDL_GetError());
        return 1;
    }
    
    ctx->tex = SDL_CreateTexture(ctx->ren,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STATIC,ctx->dw,ctx->dh);
    //ctx->tex = SDL_CreateTexture(ctx->ren,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STREAMING,ctx->dw,ctx->dh);
    if(ctx->tex == NULL)
    {
        dt_error(TAG,"SDL_CreateTexture Error:%s \n",SDL_GetError());
        return 1;
    }

    dt_info (TAG, "sdl2 pre init OK\n");
    return 0;
}

#if 0
static void SaveFrame (AVPicture_t * pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int y;

    // Open file
    sprintf (szFilename, "frame%d.ppm", iFrame);
    pFile = fopen (szFilename, "wb");
    if (pFile == NULL)
        return;

    // Write header
    fprintf (pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for (y = 0; y < height; y++)
        fwrite (pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

    // Close file
    fclose (pFile);
}
#endif

static int vo_sdl2_render (dtvideo_output_t * vo, AVPicture_t * pict)
{
    int ret = 0;
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)vo->vout_ops->handle;
    if(!ctx->sdl_inited)
    {
        ret = sdl2_pre_init(vo);
        ctx->sdl_inited = !ret;
    }

    dt_lock (&ctx->vo_mutex);

    SDL_Rect dst;
    dst.x = ctx->dx;
    dst.y = ctx->dy;
    dst.w = ctx->dw;
    dst.h = ctx->dh;

    SDL_UpdateYUVTexture(ctx->tex,NULL, pict->data[0], pict->linesize[0],  pict->data[1], pict->linesize[1],  pict->data[2], pict->linesize[2]);
    //SDL_UpdateTexture(ctx->tex, &dst, pict->data[0], pict->linesize[0]);
    SDL_RenderClear(ctx->ren);
    SDL_RenderCopy(ctx->ren,ctx->tex,&dst,&dst);
    SDL_RenderPresent(ctx->ren);

    //add event poll, avoid window fade
    static SDL_Event event;
    SDL_PollEvent(&event);
    
    dt_unlock (&ctx->vo_mutex);
    return 0;
}

static int vo_sdl2_stop (dtvideo_output_t * vo)
{
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)vo->vout_ops->handle;
    dt_lock (&ctx->vo_mutex);
    if(ctx->sdl_inited) 
    {
        SDL_DestroyTexture(ctx->tex);
        SDL_DestroyRenderer(ctx->ren); 
        SDL_DestroyWindow(ctx->win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    dt_unlock (&ctx->vo_mutex);
    free(ctx);
    vo->vout_ops->handle = NULL;
    dt_info (TAG, "stop vo sdl\n");
    return 0;
}

vo_operations_t vo_sdl2_ops = {
    .id = VO_ID_SDL2,
    .name = "sdl2",
    .vo_init = vo_sdl2_init,
    .vo_stop = vo_sdl2_stop,
    .vo_render = vo_sdl2_render,
};

#endif /* ENABLE_VO_SDL2 */
