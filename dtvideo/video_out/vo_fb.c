/**********************************************
 * Dawn Light Player
 *
 *   vo_fb.c
 *
 * Created by kf701
 * 19:58:43 03/09/08 CST
 *
 * $Id: vo_fb.c 44 2008-04-11 06:18:53Z kf701 $
 **********************************************/

#if ENABLE_VO_FB

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/page.h>
#include <linux/fb.h>

#include "swscale.h"
#include "avoutput.h"
#include "avinput.h"
#include "global.h"

static pthread_mutex_t vo_mutex;

static void vo_lock_init ()
{
    pthread_mutex_init (&vo_mutex, NULL);
}

static void vo_lock_free ()
{
    pthread_mutex_destroy (&vo_mutex);
}

static void vo_lock ()
{
    pthread_mutex_lock (&vo_mutex);
}

static void vo_unlock ()
{
    pthread_mutex_unlock (&vo_mutex);
}

typedef struct
{
    char *dev;
    int fb;
    uint8_t *orig_mem;
    uint8_t *mem;
    struct fb_fix_screeninfo fixinfo;
    struct fb_var_screeninfo varinfo;
    int pixfmt;
    int dx, dy, dh, dw;
} FBContext;

static FBContext fbctx, *fbctxp = &fbctx;
static AVPicture *my_pic;

void clear_fb_screen (void)
{
    memset (fbctxp->orig_mem, 0, fbctxp->fixinfo.smem_len);
}

static int vo_fb_init (void)
{
    if (!(fbctxp->dev = getenv ("FRAMEBUFFER")))
        fbctxp->dev = "/dev/fb0";

    fbctxp->fb = open (fbctxp->dev, O_RDWR);
    if (fbctxp->fb < 0)
    {
        av_log (NULL, AV_LOG_ERROR, "Error opening %s: %m. Check kernel config\n", fbctxp->dev);
        return -1;
    }
    if (-1 == ioctl (fbctxp->fb, FBIOGET_VSCREENINFO, &(fbctxp->varinfo)))
    {
        av_log (NULL, AV_LOG_ERROR, "ioctl FBIOGET_VSCREENINFO\n");
        return -1;
    }
    if (-1 == ioctl (fbctxp->fb, FBIOGET_FSCREENINFO, &(fbctxp->fixinfo)))
    {
        av_log (NULL, AV_LOG_ERROR, "ioctl FBIOGET_FSCREENINFO\n");
        return -1;
    }

    fbctxp->orig_mem = (uint8_t *) mmap (NULL, fbctxp->fixinfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbctxp->fb, 0);

    if (-1L == (long) fbctxp->orig_mem)
    {
        av_log (NULL, AV_LOG_ERROR, "mmap error\n");
        return -1;
    }

    if (dlpctxp->fs)
    {
        int t1, t2, drate;

        t1 = fbctxp->varinfo.xres / dlpctxp->pwidth;
        t2 = fbctxp->varinfo.yres / dlpctxp->pheight;

        drate = t1 > t2 ? t2 : t1;

        fbctxp->dw = dlpctxp->pwidth * drate;
        fbctxp->dh = dlpctxp->pheight * drate;
    }
    else
    {
        fbctxp->dw = dlpctxp->pwidth;
        fbctxp->dh = dlpctxp->pheight;
    }

    /* show at screen center */
    fbctxp->dx = (fbctxp->varinfo.xres - fbctxp->dw) / 2;
    fbctxp->dy = (fbctxp->varinfo.yres - fbctxp->dh) / 2;

    fbctxp->mem = fbctxp->orig_mem + (fbctxp->fixinfo.line_length * fbctxp->dy) + ((fbctxp->varinfo.bits_per_pixel / 8) * fbctxp->dx);

    switch (fbctxp->varinfo.bits_per_pixel)
    {
    case 32:
        fbctxp->pixfmt = PIX_FMT_RGB32;
        break;
    case 24:
        fbctxp->pixfmt = PIX_FMT_RGB24;
        break;
    case 16:
        fbctxp->pixfmt = PIX_FMT_RGB565;
        break;
    case 15:
        fbctxp->pixfmt = PIX_FMT_RGB555;
        break;
    case 8:
        fbctxp->pixfmt = PIX_FMT_RGB8;
        break;
    }

 /*-----------------------------------------------------------------------------
	 *  my picture for rgb
	 *-----------------------------------------------------------------------------*/
    my_pic = av_mallocz (sizeof (AVPicture));
    if (-1 == avpicture_alloc (my_pic, fbctxp->pixfmt, fbctxp->dw, fbctxp->dh))
    {
        av_log (NULL, AV_LOG_ERROR, "avpicture alloc error\n");
        return -1;
    }

    vo_lock_init ();

    return 0;
}

static int vo_fb_uninit (void)
{
    vo_lock ();

    close (fbctxp->fb);
    fbctxp->fb = -1;

    avpicture_free (my_pic);
    av_free (my_pic);
    my_pic = NULL;

    vo_lock_free ();

    return 0;
}

static int vo_fb_vfmt2rgb (AVPicture * dst, AVPicture * src)
{
    static struct SwsContext *img_convert_ctx;

    img_convert_ctx = sws_getCachedContext (img_convert_ctx, dlpctxp->pwidth, dlpctxp->pheight, dlpctxp->pixfmt, fbctxp->dw, fbctxp->dh, fbctxp->pixfmt, SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale (img_convert_ctx, src->data, src->linesize, 0, fbctxp->dh, dst->data, dst->linesize);

    return 0;
}

static void vo_fb_display (AVPicture * pic)
{
    int i;
    uint8_t *src, *dst = fbctxp->mem;

    vo_lock ();

    vo_fb_vfmt2rgb (my_pic, pic);
    src = my_pic->data[0];

    for (i = 0; i < fbctxp->dh; i++)
    {
        memcpy (dst, src, fbctxp->dw * (fbctxp->varinfo.bits_per_pixel / 8));
        dst += fbctxp->fixinfo.line_length;
        src += my_pic->linesize[0];
    }

    vo_unlock ();
}

static void toggle_full_screen (void)
{
    vo_lock ();

    avpicture_free (my_pic);
    av_free (my_pic);
    my_pic = NULL;

    if (dlpctxp->fs)
    {
        int t1, t2, drate;

        t1 = fbctxp->varinfo.xres / dlpctxp->pwidth;
        t2 = fbctxp->varinfo.yres / dlpctxp->pheight;

        drate = t1 > t2 ? t2 : t1;

        fbctxp->dw = dlpctxp->pwidth * drate;
        fbctxp->dh = dlpctxp->pheight * drate;
    }
    else
    {
        fbctxp->dw = dlpctxp->pwidth;
        fbctxp->dh = dlpctxp->pheight;
    }

    /* show at screen center */
    fbctxp->dx = (fbctxp->varinfo.xres - fbctxp->dw) / 2;
    fbctxp->dy = (fbctxp->varinfo.yres - fbctxp->dh) / 2;

    fbctxp->mem = fbctxp->orig_mem + (fbctxp->fixinfo.line_length * fbctxp->dy) + ((fbctxp->varinfo.bits_per_pixel / 8) * fbctxp->dx);

    clear_fb_screen ();

    my_pic = av_mallocz (sizeof (AVPicture));
    if (-1 == avpicture_alloc (my_pic, fbctxp->pixfmt, fbctxp->dw, fbctxp->dh))
    {
        av_log (NULL, AV_LOG_ERROR, "toggle full: avpicture alloc error\n");
        dlp_exit (-1);
    }

    vo_unlock ();
}

static void vo_fb_zoom (int mulriple)
{
    if (dlpctxp->pwidth * mulriple > fbctxp->varinfo.xres || dlpctxp->pheight * mulriple > fbctxp->varinfo.yres)
    {
        av_log (NULL, AV_LOG_INFO, "vo fb: zoom %d will > fullscrren\n", mulriple);
        return;
    }

    vo_lock ();

    avpicture_free (my_pic);
    av_free (my_pic);
    my_pic = NULL;

    fbctxp->dw = dlpctxp->pwidth * mulriple;
    fbctxp->dh = dlpctxp->pheight * mulriple;

    /* show at screen center */
    fbctxp->dx = (fbctxp->varinfo.xres - fbctxp->dw) / 2;
    fbctxp->dy = (fbctxp->varinfo.yres - fbctxp->dh) / 2;

    fbctxp->mem = fbctxp->orig_mem + (fbctxp->fixinfo.line_length * fbctxp->dy) + ((fbctxp->varinfo.bits_per_pixel / 8) * fbctxp->dx);

    clear_fb_screen ();

    my_pic = av_mallocz (sizeof (AVPicture));
    if (-1 == avpicture_alloc (my_pic, fbctxp->pixfmt, fbctxp->dw, fbctxp->dh))
    {
        av_log (NULL, AV_LOG_ERROR, "toggle full: avpicture alloc error\n");
        dlp_exit (-1);
    }

    vo_unlock ();
}

static int vo_fb_control (int cmd, void *arg)
{
    switch (cmd)
    {
    case VO_TOGGLE_FULLSCREEN:
        toggle_full_screen ();
        break;
    case VO_ZOOM:
        vo_fb_zoom (*((int *) arg));
        break;
    default:
        av_log (NULL, AV_LOG_ERROR, "vo x11: cmd not support now!\n");
        break;
    }
    return 0;
}

vo_t vo_fb = {
    .id = VO_ID_FB,
    .name = "fb",
    .vo_init = vo_fb_init,
    .vo_uninit = vo_fb_uninit,
    .vo_display = vo_fb_display,
    .vo_event_loop = NULL,
    .vo_control = vo_fb_control,
};

#endif /* ENABLE_VO_FB */
