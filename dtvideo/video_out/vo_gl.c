/**********************************************
 * Dawn Light Player
 *
 *   vo_gl.c
 *
 * Created by kf701
 * 10:55:13 03/15/08 CST
 *
 * $Id: vo_gl.c 47 2008-04-12 08:09:21Z kf701 $
 **********************************************/

#if ENABLE_VO_GL

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "swscale.h"
#include "avoutput.h"
#include "global.h"

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

#define BLACK_INDEX     0
#define RED_INDEX       1
#define GREEN_INDEX     2
#define BLUE_INDEX      4

static volatile int redraw_flag;

static Display *XDisplay;
static XVisualInfo *gl_vinfo;
static GLuint texName;
static int dx, dy, dw, dh, ww, wh;
static uint8_t *image = NULL;
static int src_pic_fmt;
static int my_pic_fmt = PIX_FMT_RGB32; /* fix me, from GL */
static AVPicture *my_pic;

static GLint getInternalFormat(void)
{
    int rgb_sz, r_sz, g_sz, b_sz, a_sz;

#ifdef GL_WIN32
    PIXELFORMATDESCRIPTOR pfd;
    HDC vo_hdc = GetDC(vo_window);
    int pf = GetPixelFormat(vo_hdc);
    if (!DescribePixelFormat(vo_hdc, pf, sizeof pfd, &pfd)) {
        r_sz = g_sz = b_sz = a_sz = 0;
    } else {
        r_sz = pfd.cRedBits;
        g_sz = pfd.cGreenBits;
        b_sz = pfd.cBlueBits;
        a_sz = pfd.cAlphaBits;
    }
    ReleaseDC(vo_window, vo_hdc);
#else
    if (glXGetConfig(XDisplay, gl_vinfo, GLX_RED_SIZE, &r_sz) != 0) {
        r_sz = 0;
    }
    if (glXGetConfig(XDisplay, gl_vinfo, GLX_GREEN_SIZE, &g_sz) != 0) {
        g_sz = 0;
    }
    if (glXGetConfig(XDisplay, gl_vinfo, GLX_BLUE_SIZE, &b_sz) != 0) {
        b_sz = 0;
    }
    if (glXGetConfig(XDisplay, gl_vinfo, GLX_ALPHA_SIZE, &a_sz) != 0) {
        a_sz = 0;
    }
#endif

    rgb_sz = r_sz + g_sz + b_sz;
    if (rgb_sz <= 0) {
        rgb_sz = 24;
    }

#ifdef TEXTUREFORMAT_ALWAYS
    return TEXTUREFORMAT_ALWAYS;
#else
    if (r_sz == 3 && g_sz == 3 && b_sz == 2 && a_sz == 0) {
        return GL_R3_G3_B2;
    }
    if (r_sz == 4 && g_sz == 4 && b_sz == 4 && a_sz == 0) {
        return GL_RGB4;
    }
    if (r_sz == 5 && g_sz == 5 && b_sz == 5 && a_sz == 0) {
        return GL_RGB5;
    }
    if (r_sz == 8 && g_sz == 8 && b_sz == 8 && a_sz == 0) {
        return GL_RGB8;
    }
    if (r_sz == 10 && g_sz == 10 && b_sz == 10 && a_sz == 0) {
        return GL_RGB10;
    }
    if (r_sz == 2 && g_sz == 2 && b_sz == 2 && a_sz == 2) {
        return GL_RGBA2;
    }
    if (r_sz == 4 && g_sz == 4 && b_sz == 4 && a_sz == 4) {
        return GL_RGBA4;
    }
    if (r_sz == 5 && g_sz == 5 && b_sz == 5 && a_sz == 1) {
        return GL_RGB5_A1;
    }
    if (r_sz == 8 && g_sz == 8 && b_sz == 8 && a_sz == 8) {
        return GL_RGBA8;
    }
    if (r_sz == 10 && g_sz == 10 && b_sz == 10 && a_sz == 2) {
        return GL_RGB10_A2;
    }
#endif
    return GL_RGB;
}

static void init_texture(void)
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texName);
    glBindTexture(GL_TEXTURE_2D, texName);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dw, dh, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image);
#if 0
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
#endif
    glFlush();
}

static void resize(GLsizei width, GLsizei height)
{
    GLfloat aspect = (GLfloat) width / height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, aspect, 3.0, 7.0);
    glMatrixMode(GL_MODELVIEW);
}

static Bool WaitForNotify(Display * d, XEvent * e, char *arg)
{
    return (e->type == MapNotify) && (e->xmap.window == (Window) arg);
}

static int vo_gl_init(void)
{
    int ret;

    dx = dy = 0;
    dw = dlpctxp->pwidth;
    dh = dlpctxp->pheight;
    ww = dw;
    wh = dh;
    src_pic_fmt = dlpctxp->pixfmt;

    /*-----------------------------------------------------------------------------
     *  my picture for rgb
     *-----------------------------------------------------------------------------*/
    my_pic = av_mallocz(sizeof(AVPicture));
    ret = avpicture_alloc(my_pic, my_pic_fmt, dw, dh);
    if (-1 == ret) {
        av_log(NULL, AV_LOG_ERROR, "avpicture alloc error\n");
        return -1;
    }
    image = my_pic->data[0];

    /*-----------------------------------------------------------------------------
     *  create window for GL
     *-----------------------------------------------------------------------------*/
    {
        Colormap cmap;
        XSetWindowAttributes swa;
        Window win;
        GLXContext cx;
        XEvent event;
        int attributeList[] = { GLX_RGBA, None };

        XDisplay = XOpenDisplay(0);
        /* get an appropriate visual */
        gl_vinfo = glXChooseVisual(XDisplay, DefaultScreen(XDisplay), attributeList);
        /* create a GLX context */
        cx = glXCreateContext(XDisplay, gl_vinfo, None, GL_TRUE);
        /* create a color map */
        cmap = XCreateColormap(XDisplay, RootWindow(XDisplay, gl_vinfo->screen),
                               gl_vinfo->visual, AllocNone);
        /* create a window */
        swa.colormap = cmap;
        swa.border_pixel = 0;
        swa.event_mask = StructureNotifyMask;
        win = XCreateWindow(XDisplay, RootWindow(XDisplay, gl_vinfo->screen), 0, 0, dw,
                            dh, 0, gl_vinfo->depth, InputOutput, gl_vinfo->visual,
                            CWBorderPixel | CWColormap | CWEventMask, &swa);
#define WIN_TITLE "Dawn Light Player"
        XSetStandardProperties(XDisplay, win, WIN_TITLE, WIN_TITLE, None, NULL, 0,
                               NULL);
        XMapWindow(XDisplay, win);
        XIfEvent(XDisplay, &event, WaitForNotify, (char *) win);
        /* connect the gl context to the window */
        glXMakeCurrent(XDisplay, win, cx);
    }
    /*-----------------------------------------------------------------------------
     *  init GL texture, for picture display
     *-----------------------------------------------------------------------------*/
    init_texture();
    //resize(dw, dh);

    vo_lock_init();

    return 0;
}

static int vo_gl_uninit(void)
{
    vo_lock();

    glFinish();
    glFlush();

    glDeleteTextures(1, &texName);

    avpicture_free(my_pic);
    av_free(my_pic);
    my_pic = NULL;

    vo_lock_free();

    return 0;
}

static int vo_gl_vfmt2rgb(AVPicture * dst, AVPicture * src)
{
    static struct SwsContext *img_convert_ctx;

    img_convert_ctx = sws_getCachedContext(img_convert_ctx, dw, dh, src_pic_fmt, dw,
                                           dh, my_pic_fmt, SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, src->data, src->linesize, 0, dh, dst->data,
              dst->linesize);

    return 0;
}

static void update_texture(void)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texName);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE,
                    image);
#if 1
    glBegin(GL_POLYGON);
    glTexCoord2f(dx, dy);
    glVertex2f(-1.0, 1.0);
    glTexCoord2f(dw, dy);
    glVertex2f(1.0, 1.0);
    glTexCoord2f(dw, dh);
    glVertex2f(1.0, -1.0);
    glTexCoord2f(dx, dh);
    glVertex2f(-1.0, -1.0);
    glEnd();
#else
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);   /* lower left corner of image */
    glVertex3f(-10.0f, -10.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);   /* lower right corner of image */
    glVertex3f(10.0f, -10.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);   /* upper right corner of image */
    glVertex3f(10.0f, 10.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);   /* upper left corner of image */
    glVertex3f(-10.0f, 10.0f, 0.0f);
    glEnd();
#endif
    glFlush();
    glDisable(GL_TEXTURE_2D);   /* disable texture mapping */
}

static void vo_gl_display(AVPicture * pict)
{
    vo_gl_vfmt2rgb(my_pic, pict);
    image = my_pic->data[0];
    resize(100, 100);
    update_texture();
}

static void vo_gl_event_loop(void)
{
    while (1) {
        pause();
    }
}

vo_t vo_gl = {
    .id = VO_ID_GL,
    .name = "gl",
    .vo_init = vo_gl_init,
    .vo_uninit = vo_gl_uninit,
    .vo_display = vo_gl_display,
    .vo_event_loop = vo_gl_event_loop,
};

#endif /* ENABLE_VO_GL */
