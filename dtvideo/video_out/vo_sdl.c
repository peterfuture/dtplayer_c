/**********************************************
 * Dawn Light Player
 *
 *   vo_sdl.c
 *
 * Created by kf701
 * 14:11:50 02/26/08 CST
 *
 * $Id: vo_sdl.c 59 2008-04-20 09:25:53Z kf701 $
 **********************************************/

#if ENABLE_VO_SDL

#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "../dtvideo_output.h"

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#define TAG "VO-SDL"

static pthread_mutex_t vo_mutex;

static void vo_lock_init()
{
	pthread_mutex_init(&vo_mutex, NULL);
}

static void vo_lock_free()
{
	pthread_mutex_destroy(&vo_mutex);
}

static void vo_lock()
{
	pthread_mutex_lock(&vo_mutex);
}

static void vo_unlock()
{
	pthread_mutex_unlock(&vo_mutex);
}

static int fs_screen_width = 0, fs_screen_height = 0;

static SDL_Surface *screen = NULL;
static SDL_Overlay *overlay = NULL;

static int dx, dy, dw, dh, ww, wh;

static int vo_sdl_init(dtvideo_output_t * vo)
{
	int flags;

	putenv("SDL_VIDEO_WINDOW_POS=center");
	putenv("SDL_VIDEO_CENTERED=1");

	flags = SDL_INIT_VIDEO | SDL_INIT_VIDEO | SDL_INIT_TIMER;
	if (SDL_Init(flags)) {
		//av_log(NULL, AV_LOG_ERROR, "initialize SDL: %s\n", SDL_GetError());
		dt_error("initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

	{
		const SDL_VideoInfo *vi = SDL_GetVideoInfo();
		fs_screen_width = vi->current_w;
		fs_screen_height = vi->current_h;
	}

	dx = dy = 0;
	//ww = dw = dlpctxp->pwidth;
	//wh = dh = dlpctxp->pheight;
	ww = dw = vo->para.d_width;
	wh = dh = vo->para.d_height;

	flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE;
	SDL_WM_SetIcon(SDL_LoadBMP("icon.bmp"), NULL);
	screen = SDL_SetVideoMode(ww, wh, 0, flags);
	SDL_WM_SetCaption("dtplayer", NULL);

	overlay = SDL_CreateYUVOverlay(dw, dh, SDL_YV12_OVERLAY, screen);

	vo_lock_init();

	//if ( dlpctxp->fs )
	//      toggle_full_screen();

	dt_info(TAG,"init vo sdl OK\n");

	return 0;
}

static int vo_sdl_uninit(dtvideo_output_t * vo)
{
	vo_lock();

	SDL_FreeYUVOverlay(overlay);
	overlay = NULL;

	vo_lock_free();

	dt_info(TAG,"uninit vo sdl\n");
	return 0;
}

static int vo_sdl_sws(dtvideo_output_t * vo, AVPicture * dst, AVPicture * src)
{
	static struct SwsContext *img_convert_ctx;
	//int dst_pix_fmt = PIX_FMT_YUV420P;
	int dst_pix_fmt = vo->para.d_pixfmt;
#if 1
	img_convert_ctx = sws_getCachedContext(img_convert_ctx,
					       vo->para.d_width,
					       vo->para.d_height, dst_pix_fmt,
					       vo->para.d_width,
					       vo->para.d_height, dst_pix_fmt,
					       SWS_BICUBIC, NULL, NULL, NULL);
#endif
#if 0
	img_convert_ctx =
	    sws_getContext(vo->para.d_width, vo->para.d_height, dst_pix_fmt,
			   vo->para.d_width, vo->para.d_height, dst_pix_fmt,
			   SWS_BICUBIC, NULL, NULL, NULL);
#endif

	sws_scale(img_convert_ctx, src->data, src->linesize,
		  0, vo->para.d_height, dst->data, dst->linesize);
	//sws_freeContext(img_convert_ctx);
	return 0;
}

static void vo_sdl_display(dtvideo_output_t * vo, AVPicture_t * pict)
{
	SDL_Rect rect;
	AVPicture p;

	vo_lock();

	SDL_LockYUVOverlay(overlay);
    p.data[0] = overlay->pixels[0];
	p.data[1] = overlay->pixels[2];
	p.data[2] = overlay->pixels[1];
	p.linesize[0] = overlay->pitches[0];
	p.linesize[1] = overlay->pitches[2];
	p.linesize[2] = overlay->pitches[1];
	vo_sdl_sws(vo, &p, pict);	/* only do memcpy */
    SDL_UnlockYUVOverlay(overlay);

	rect.x = dx;
	rect.y = dy;
	rect.w = dw;
	rect.h = dh;
	SDL_DisplayYUVOverlay(overlay, &rect);

	vo_unlock();
}

vo_operations_t vo_sdl_ops = {
	.id = VO_ID_SDL,
	.name = "sdl",
	.vo_init = vo_sdl_init,
	.vo_stop = vo_sdl_uninit,
	.vo_write = vo_sdl_display,
};

#endif /* ENABLE_VO_SDL */
