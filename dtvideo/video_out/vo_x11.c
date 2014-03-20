/**********************************************
 * Dawn Light Player
 *
 *   vo_x11.c
 *
 * Created by maddrone
 * 16:31:36 02/27/08 CST
 *
 * $Id: vo_x11.c 54 2008-04-16 03:19:07Z kf701 $
 **********************************************/

#if ENABLE_VO_X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "swscale.h"
#include "avoutput.h"
#include "avdecode.h"
#include "avinput.h"
#include "cmdutils.h"
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

static Display *Xdisplay = NULL;
static int Xscreen, Xdepth;
static Window Xvowin = None, Xrootwin = None;
static GC Xvogc = NULL;
static XImage *Ximg = NULL;
static XFontStruct *Xfont_120, *Xfont_240, *Xfont;

static int screen_width, screen_height;
static int dx = 200, dy = 200, dw, dh;
static char *Xtitle = "Dawn Light Player";

static int vo_draw_string = 0;
static osd_t osd;

static int src_pic_fmt;
static int my_pic_fmt = PIX_FMT_RGB32;
static AVPicture *my_pic;

static int vo_x11_vfmt2rgb (AVPicture * dst, AVPicture * src)
{
    static struct SwsContext *img_convert_ctx;

    img_convert_ctx = sws_getCachedContext (img_convert_ctx, dlpctxp->pwidth, dlpctxp->pheight, src_pic_fmt, dw, dh, my_pic_fmt, SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale (img_convert_ctx, src->data, src->linesize, 0, dh, dst->data, dst->linesize);

    return 0;
}

/* Xlib font utils define */
static XFontStruct *LoadQueryScalableFont (Display * dpy, int screen, char *name, int size)
{
    int i, j, field;
    char newname[500];          /* big enough for a long font name */
    int res_x, res_y;           /* resolution values for this screen */
    /* catch obvious errors */
    if ((name == NULL) || (name[0] != '-'))
        return NULL;
    /* calculate our screen resolution in dots per inch. 25.4mm = 1 inch */
    res_x = DisplayWidth (dpy, screen) / (DisplayWidthMM (dpy, screen) / 25.4);
    res_y = DisplayHeight (dpy, screen) / (DisplayHeightMM (dpy, screen) / 25.4);
    /* copy the font name, changing the scalable fields as we do so */
    for (i = j = field = 0; name[i] != '\0' && field <= 14; i++)
    {
        newname[j++] = name[i];
        if (name[i] == '-')
        {
            field++;
            switch (field)
            {
            case 7:            /* pixel size */
            case 12:           /* average width */
                /* change from "-0-" to "-*-" */
                newname[j] = '*';
                j++;
                if (name[i + 1] != '\0')
                    i++;
                break;
            case 8:            /* point size */
                /* change from "-0-" to "-<size>-" */
                sprintf (&newname[j], "%d", size);
                while (newname[j] != '\0')
                    j++;
                if (name[i + 1] != '\0')
                    i++;
                break;
            case 9:            /* x-resolution */
            case 10:           /* y-resolution */
                /* change from an unspecified resolution to res_x or res_y */
                sprintf (&newname[j], "%d", (field == 9) ? res_x : res_y);
                while (newname[j] != '\0')
                    j++;
                while ((name[i + 1] != '-') && (name[i + 1] != '\0'))
                    i++;
                break;
            }
        }
    }
    newname[j] = '\0';
    /* if there aren't 14 hyphens, it isn't a well formed name */
    if (field != 14)
        return NULL;
    return XLoadQueryFont (dpy, newname);
}

/* Xlib font utils define End*/

static int vo_x11_init ()
{
    int ret;
    unsigned long xswamask, event_mask;
    XSetWindowAttributes xswa;
    XSizeHints hint;
    XVisualInfo xvinfo;
    char *dspname;

    xswamask = CWBackingStore | CWBorderPixel;

    src_pic_fmt = dlpctxp->pixfmt;
    dw = dlpctxp->pwidth;
    dh = dlpctxp->pheight;

    dspname = XDisplayName (NULL);
    av_log (NULL, AV_LOG_INFO, "Open X11 display %s\n", dspname);
    Xdisplay = XOpenDisplay (dspname);
    if (!Xdisplay)
    {
        av_log (NULL, AV_LOG_ERROR, "X11,%d: XOpenDisplay\n", __LINE__);
        return -1;
    }

    Xscreen = DefaultScreen (Xdisplay);
    Xrootwin = RootWindow (Xdisplay, Xscreen);
    screen_width = DisplayWidth (Xdisplay, Xscreen);
    screen_height = DisplayHeight (Xdisplay, Xscreen);

    dx = (screen_width - dw) / 2;
    dy = (screen_height - dh) / 2;

    Xdepth = XDefaultDepth (Xdisplay, 0);
    if (!XMatchVisualInfo (Xdisplay, Xscreen, Xdepth, DirectColor, &xvinfo))
        XMatchVisualInfo (Xdisplay, Xscreen, Xdepth, TrueColor, &xvinfo);

    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.backing_store = Always;
    xswa.bit_gravity = StaticGravity;

    Xvowin = XCreateWindow (Xdisplay, Xrootwin, dx, dy, dw, dh, 0, Xdepth, CopyFromParent, CopyFromParent, xswamask, &xswa);

    hint.x = dx;
    hint.y = dy;
    hint.width = dw;
    hint.height = dh;
    hint.flags = PPosition | PSize;
    XSetStandardProperties (Xdisplay, Xvowin, Xtitle, Xtitle, None, NULL, 0, &hint);

    XMapWindow (Xdisplay, Xvowin);
    XClearWindow (Xdisplay, Xvowin);

    event_mask = StructureNotifyMask | KeyPressMask | ExposureMask;
    XSelectInput (Xdisplay, Xvowin, event_mask);
    XSync (Xdisplay, False);

    Xvogc = XCreateGC (Xdisplay, Xrootwin, 0L, NULL);
    XSetForeground (Xdisplay, Xvogc, WhitePixel (Xdisplay, Xscreen));

    Ximg = XCreateImage (Xdisplay, xvinfo.visual, Xdepth, ZPixmap, 0, NULL, dw, dh, 8, 0);

    {
        int i, fontcount;
        char **list;
        list = XListFonts (Xdisplay, "-*-helvetica-*-*-*-*-0-0-*-*-*-0-*-*", 200, &fontcount);
        for (i = 0; i < fontcount; i++)
        {
            if (NULL == Xfont_120)
                Xfont_120 = LoadQueryScalableFont (Xdisplay, Xscreen, list[i], 120);
            if (NULL == Xfont_240)
                Xfont_240 = LoadQueryScalableFont (Xdisplay, Xscreen, list[i], 240);
            if (Xfont_120 && Xfont_240)
                break;
        }
        XFreeFontNames (list);

        if (NULL == Xfont_120 || NULL == Xfont_240)
        {
            av_log (NULL, AV_LOG_ERROR, "XLoadQueryFont: failed\n");
        }

        if (dw < 600)
            Xfont = Xfont_120;
        else
            Xfont = Xfont_240;

        if (Xfont)
            XSetFont (Xdisplay, Xvogc, Xfont->fid);
    }

    switch (Ximg->bits_per_pixel)
    {
    case 32:
        my_pic_fmt = PIX_FMT_RGB32;
        break;
    case 24:
        my_pic_fmt = PIX_FMT_RGB24;
        break;
    case 16:
        my_pic_fmt = PIX_FMT_RGB565;
        break;
    case 15:
        my_pic_fmt = PIX_FMT_RGB555;
        break;
    case 8:
        my_pic_fmt = PIX_FMT_RGB8;
        break;
    }
    av_log (NULL, AV_LOG_INFO, "bits_per_pixel: %d\n", Ximg->bits_per_pixel);
 /*-----------------------------------------------------------------------------
	 *  my picture for rgb
	 *-----------------------------------------------------------------------------*/
    my_pic = av_mallocz (sizeof (AVPicture));
    ret = avpicture_alloc (my_pic, my_pic_fmt, dw, dh);
    if (-1 == ret)
    {
        av_log (NULL, AV_LOG_ERROR, "avpicture alloc error\n");
        return -1;
    }

    vo_lock_init ();

    return 0;
}

static void x11_draw_string (osd_t * osd)
{
    int x, y, font_height, string_width;

    if (NULL == Xfont)
        return;

    font_height = Xfont->ascent + Xfont->descent;
    string_width = XTextWidth (Xfont, osd->string, osd->len);
    x = (dw - string_width) / 2;
    y = (dh - font_height) / 2;
    if (osd->x)
        x = osd->x;
    if (osd->y > 0)
        y = osd->y;
    if (osd->y < 0)
        y = dh + osd->y;

    XDrawString (Xdisplay, Xvowin, Xvogc, x, y, osd->string, osd->len);
    //XDrawImageString(Xdisplay, Xvowin, Xvogc, x, y, osd->string, osd->len);
}

static void toggle_full_screen (void)
{
    vo_lock ();
    av_log (NULL, AV_LOG_ERROR, "VO X11 not support full screen now!\n");
    vo_unlock ();
}

static void vo_x11_zoom (int mulriple)
{
    unsigned long xswamask, event_mask;
    XSetWindowAttributes xswa;
    XSizeHints hint;
    XVisualInfo xvinfo;

    if (dlpctxp->pwidth * mulriple > screen_width || dlpctxp->pheight * mulriple > screen_height)
    {
        av_log (NULL, AV_LOG_INFO, "vo x11: zoom %d will > fullscrren\n", mulriple);
        return;
    }

    vo_lock ();

    if (Xvowin != None)
    {
        XClearWindow (Xdisplay, Xvowin);
        XUnmapWindow (Xdisplay, Xvowin);
        XDestroyWindow (Xdisplay, Xvowin);
        Xvowin = None;
    }
    if (Ximg)
    {
        Ximg->data = NULL;
        XDestroyImage (Ximg);
        Ximg = NULL;
    }

    avpicture_free (my_pic);
    av_free (my_pic);

    xswamask = CWBackingStore | CWBorderPixel;

    dw = dlpctxp->pwidth * mulriple;
    dh = dlpctxp->pheight * mulriple;

    dx = (screen_width - dw) / 2;
    dy = (screen_height - dh) / 2;

    Xdepth = XDefaultDepth (Xdisplay, 0);
    if (!XMatchVisualInfo (Xdisplay, Xscreen, Xdepth, DirectColor, &xvinfo))
        XMatchVisualInfo (Xdisplay, Xscreen, Xdepth, TrueColor, &xvinfo);

    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.backing_store = Always;
    xswa.bit_gravity = StaticGravity;

    Xvowin = XCreateWindow (Xdisplay, Xrootwin, dx, dy, dw, dh, 0, Xdepth, CopyFromParent, CopyFromParent, xswamask, &xswa);

    hint.x = dx;
    hint.y = dy;
    hint.width = dw;
    hint.height = dh;
    hint.flags = PPosition | PSize;
    XSetStandardProperties (Xdisplay, Xvowin, Xtitle, Xtitle, None, NULL, 0, &hint);

    XMapWindow (Xdisplay, Xvowin);
    XClearWindow (Xdisplay, Xvowin);

    event_mask = StructureNotifyMask | KeyPressMask | ExposureMask;
    XSelectInput (Xdisplay, Xvowin, event_mask);
    XSync (Xdisplay, False);

    Ximg = XCreateImage (Xdisplay, xvinfo.visual, Xdepth, ZPixmap, 0, NULL, dw, dh, 8, 0);

    if (dw < 600)
        Xfont = Xfont_120;
    else
        Xfont = Xfont_240;

    if (Xfont)
        XSetFont (Xdisplay, Xvogc, Xfont->fid);

    my_pic = av_mallocz (sizeof (AVPicture));
    avpicture_alloc (my_pic, my_pic_fmt, dw, dh);

    vo_unlock ();
}

static void vo_x11_display (AVPicture * pic)
{
    vo_lock ();

    vo_x11_vfmt2rgb (my_pic, pic);

    Ximg->data = my_pic->data[0];
    XPutImage (Xdisplay, Xvowin, Xvogc, Ximg, 0, 0, 0, 0, dw, dh);

    if (vo_draw_string)
        x11_draw_string (&osd);

    XFlush (Xdisplay);
    vo_unlock ();
}

static void vo_x11_event_loop (void *arg)
{
    XEvent event;
    while (1)
    {
        XNextEvent (Xdisplay, &event);
        switch (event.type)
        {
        case KeyPress:
            {
                KeySym key = XKeycodeToKeysym (Xdisplay,
                    event.xkey.keycode, 0);
                if (XK_Escape == key || XK_q == key)
                    dlp_exit (-2);
                else if (XK_9 == key)
                    dlp_sub_volume ();
                else if (XK_0 == key)
                    dlp_add_volume ();
                else if (XK_1 == key)
                    vo_x11_zoom (1);
                else if (XK_2 == key)
                    vo_x11_zoom (2);
                else if (XK_3 == key)
                    vo_x11_zoom (3);
                else if (XK_4 == key)
                    vo_x11_zoom (4);
                else if (XK_p == key || XK_space == key)
                    dlp_pause_play ();
                else if (XK_Left == key)
                    dlp_seek (-10);
                else if (XK_Right == key)
                    dlp_seek (10);
                else if (XK_Page_Up == key)
                    dlp_seek (-60);
                else if (XK_Page_Down == key)
                    dlp_seek (60);
                else if (XK_h == key)
                    show_ui_key ();
                else if (XK_f == key)
                    dlp_full_screen ();
                else if (XK_s == key)
                    dlp_screen_shot ();
                else if (XK_o == key)
                    dlp_show_osd (OSD_TEST);
                break;
            }
        default:
            break;
        }
    }
}

static int vo_x11_uninit (void)
{
    vo_lock ();

    if (Xfont_120)
        XFreeFont (Xdisplay, Xfont_120);
    if (Xfont_240)
        XFreeFont (Xdisplay, Xfont_240);

    if (Xvogc)
    {
        XSetBackground (Xdisplay, Xvogc, 0);
        XFreeGC (Xdisplay, Xvogc);
        Xvogc = NULL;
    }
    if (Xvowin != None)
    {
        XClearWindow (Xdisplay, Xvowin);
        XUnmapWindow (Xdisplay, Xvowin);
        XDestroyWindow (Xdisplay, Xvowin);
        Xvowin = None;
    }
    if (Ximg)
    {
        Ximg->data = NULL;
        XDestroyImage (Ximg);
        Ximg = NULL;
    }

    avpicture_free (my_pic);
    av_free (my_pic);
    my_pic = NULL;

    vo_lock_free ();

    return 0;
}

static int vo_x11_control (int cmd, void *arg)
{
    switch (cmd)
    {
    case VO_TOGGLE_FULLSCREEN:
        toggle_full_screen ();
        break;
    case VO_ZOOM:
        vo_x11_zoom (*((int *) arg));
        break;
    case VO_DRAW_STRING:
        vo_draw_string = 1;
        vo_lock ();
        memcpy (&osd, (osd_t *) arg, sizeof (osd_t));
        vo_unlock ();
        break;
    case VO_ERASE_STRING:
        vo_draw_string = 0;
        break;
    default:
        av_log (NULL, AV_LOG_ERROR, "vo x11: cmd not support now!\n");
        break;
    }
    return 0;
}

vo_t vo_x11 = {
    .id = VO_ID_X11,
    .name = "x11",
    .vo_init = vo_x11_init,
    .vo_uninit = vo_x11_uninit,
    .vo_display = vo_x11_display,
    .vo_event_loop = vo_x11_event_loop,
    .vo_control = vo_x11_control,
};

#endif /* ENABLE_VO_X11 */
