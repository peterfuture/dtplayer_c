#if ENABLE_VO_SDL2

#include "../dtvideo_output.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_thread.h>
#include <stdio.h>

#define TAG "VO-SDL2"

static int dx, dy, dw, dh;
static dt_lock_t vo_mutex;

typedef struct{
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    dt_lock_t vo_mutex;
}sdl2_ctx_t;


static int vo_sdl2_init (dtvideo_output_t * vo)
{
    int flags;

    sdl2_ctx_t *ctx = malloc(sizeof(sdl2_ctx_t));

    putenv ("SDL_VIDEO_WINDOW_POS=center");
    putenv ("SDL_VIDEO_CENTERED=1");

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_Init(SDL_INIT_VIDEO);

    dx = dy = 0;
    dw = vo->para.d_width;
    dh = vo->para.d_height;
   
    flags = SDL_WINDOW_SHOWN; 
    ctx->win = SDL_CreateWindow("dtplayer",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,dw,dh,flags);
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
    
    //tex = SDL_CreateTexture(ren,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STATIC,dw,dh);
    ctx->tex = SDL_CreateTexture(ctx->ren,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STREAMING,dw,dh);
    if(ctx->tex == NULL)
    {
        dt_error(TAG,"SDL_CreateTexture Error:%s \n",SDL_GetError());
        return 1;
    }

    dt_lock_init (&ctx->vo_mutex, NULL);

#if 0   
    //set default color
    SDL_SetRenderDrawColor(ctx->ren,255,0,0,0);
    SDL_RenderClear(ctx->ren);
    SDL_RenderPresent(ctx->ren);
#endif
    vo->vout_ops->handle = (void *)ctx;

    dt_info (TAG, "sdl2 init OK\n");
    return 0;
}

static int vo_sdl2_stop (dtvideo_output_t * vo)
{
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)vo->vout_ops->handle;
    dt_lock (&ctx->vo_mutex);
   
    SDL_DestroyTexture(ctx->tex);
    SDL_DestroyRenderer(ctx->ren); 
    SDL_DestroyWindow(ctx->win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    dt_unlock (&ctx->vo_mutex);

    dt_info (TAG, "stop vo sdl\n");
    return 0;
}

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



static int vo_sdl2_render (dtvideo_output_t * vo, AVPicture_t * pict)
{
    sdl2_ctx_t *ctx = (sdl2_ctx_t *)vo->vout_ops->handle;
    dt_lock (&ctx->vo_mutex);

    SDL_Rect dst;
    dst.x = dx;
    dst.y = dy;
    dst.w = dw;
    dst.h = dh;

    dt_debug(TAG,"first 4 byte:%02x %02x %02x %02x dataaddr:%p",pict->data[0][0],pict->data[0][1],pict->data[0][2],pict->data[0][3],pict->data[0]);
    dt_debug(TAG," line size:%d %d %d \n",pict->linesize[0],pict->linesize[1],pict->linesize[2]);

#if 1
    //SDL_RenderReadPixels(ren,&dst,SDL_PIXELFORMAT_YV12,pict->data[0],pict->linesize[0]);   
    //SDL_UpdateYUVTexture(tex,NULL, pict->data[0], pict->linesize[0],  pict->data[1], pict->linesize[1],  pict->data[2], pict->linesize[2]);
    //SDL_UpdateYUVTexture(tex,NULL, pict->data[0], dw*dh,  pict->data[2],dw*dh/4,  pict->data[1],dw*dh/4);
    SDL_UpdateTexture(ctx->tex, &dst, pict->data[0], pict->linesize[0]);
    SDL_RenderClear(ctx->ren);
    SDL_RenderCopy(ctx->ren,ctx->tex,&dst,&dst);
    SDL_RenderPresent(ctx->ren);
#endif

#if 0
    int r = rand()%255;
    int g = rand()%255;
    int b = rand()%255;
    int a = rand()%255;
    SDL_SetRenderDrawColor(ctx->ren,0,255,0,0);
    SDL_RenderClear(ctx->ren);
    SDL_RenderPresent(ctx->ren);

    dt_info(TAG,"SCREEN SHOULD BE GREE\n");
#endif

    dt_unlock (&ctx->vo_mutex);
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
