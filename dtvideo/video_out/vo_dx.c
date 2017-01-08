/**********************************************
 * Dawn Light Player
 *
 *   vo_dx.c
 *
 * Created by kf701
 * 13:39:05 04/22/08 CST
 *
 * $Id$
 **********************************************/

#if ENABLE_VO_DIRECTX

#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>

#include "swscale.h"
#include "avoutput.h"
#include "avdecode.h"
#include "avinput.h"
#include "avdecode.h"
#include "avinput.h"
#include "cmdutils.h"
#include "global.h"

#define LINE()  printf("%s,%d\n", __func__, __LINE__)

#ifndef FALSE
#define FALSE 0
#define TRUE  (!FALSE)
#endif

#define WND_CLASSNAME   "DawnLightPlayer"
#define FS_CLASSNAME    "FullScreen"
#define DLP_TITLE   "Dawn Light Player"

static LPDIRECTDRAW7 g_lpdd = NULL;
static LPDIRECTDRAWSURFACE7 g_lpddsPrimary = NULL;
static LPDIRECTDRAWSURFACE7 g_lpddsOverlay = NULL;
static LPDIRECTDRAWSURFACE7 g_lpddsBack = NULL;
static LPDIRECTDRAWCLIPPER g_lpddclipper = NULL;
static HINSTANCE hddraw_dll = NULL;
static HWND g_hwnd = NULL;
static HWND g_fs_hwnd = NULL;
static RECT rd, rs;

static int g_screen_width, g_screen_height;
static int g_image_width, g_image_height;
static int dx, dy, dw, dh;
static uint8_t *g_image = NULL;
static uint8_t *tmp_image = NULL;
static int g_dstride = 0;
static int g_image_format = 0;
static int g_doublebuffer = 1;
static int g_vidmode = 0;
static int g_inited = 0;

static AVPicture *my_pic;

static int g_StretchFactor1000;
static int g_align_src, g_align_dst;
static int g_boundary_src, g_boundary_dst;

static int g_overlay_key;
static int g_allow_shrink_x;
static int g_allow_shrink_y;
static int g_allow_zoom_x;
static int g_allow_zoom_y;

typedef struct directx_fourcc_caps {
    char *img_format_name;
    uint32_t img_format;
    DDPIXELFORMAT g_ddpfOverlay;
} directx_fourcc_caps;

static directx_fourcc_caps g_ddpf[] = {
    {
        "RGB32", PIX_FMT_RGB32,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32, 0x000000FF, 0x0000FF00,
            0x00FF0000, 0
        }
    }
    ,
    {
        "BGR32", PIX_FMT_BGR32,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32, 0x00FF0000, 0x0000FF00,
            0x000000FF, 0
        }
    }
    ,
    {
        "RGB24", PIX_FMT_RGB24,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24, 0x000000FF, 0x0000FF00,
            0x00FF0000, 0
        }
    }
    ,
    {
        "BGR24", PIX_FMT_BGR24,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24, 0x00FF0000, 0x0000FF00,
            0x000000FF, 0
        }
    }
    ,
    {
        "RGB16", PIX_FMT_RGB565,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x0000001F, 0x000007E0,
            0x0000F800, 0
        }
    }
    ,
    {
        "BGR16", PIX_FMT_BGR565,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x0000F800, 0x000007E0,
            0x0000001F, 0
        }
    }
    ,
    {
        "RGB15", PIX_FMT_RGB555,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x0000001F, 0x000003E0,
            0x00007C00, 0
        }
    }
    ,
    {
        "BGR15", PIX_FMT_BGR555,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16, 0x00007C00, 0x000003E0,
            0x0000001F, 0
        }
    }
    ,
    {
        "BGR8", PIX_FMT_BGR8,
        {
            sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 8, 0x00000000, 0x00000000,
            0x00000000, 0
        }
    }
    ,
};

#define NUM_FORMATS (sizeof(g_ddpf) / sizeof(g_ddpf[0]))

static void vo_dx_zoom_size(int rate);
static int DxManageOverlay(void);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{
    switch (message) {
    case WM_CLOSE:
        dlp_exit(-2);
        break;
    case WM_WINDOWPOSCHANGED:
        if (g_lpddsBack) {
            DxManageOverlay();
        }
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_LEFT:
            dlp_seek(-10);
            break;
        case VK_RIGHT:
            dlp_seek(10);
            break;
        case VK_ESCAPE:
            dlp_exit(-2);
            break;
        case VK_PRIOR:
            dlp_seek(-60);
            break;
        case VK_NEXT:
            dlp_seek(60);
            break;
        }
        break;
    case WM_CHAR:
        switch (wParam) {
        case 'h':
            show_ui_key();
            break;
        case 's':
            dlp_screen_shot();
            break;
        case 'q':
            dlp_exit(-2);
            break;
        case '9':
            dlp_sub_volume();
            break;
        case '0':
            dlp_add_volume();
            break;
        case 'p':
            dlp_pause_play();
            break;
        case 'f':
            dlp_full_screen();
            break;
        case '1':
            vo_dx_zoom_size(1);
            break;
        case '2':
            vo_dx_zoom_size(2);
            break;
        case '3':
            vo_dx_zoom_size(3);
            break;
        case '4':
            vo_dx_zoom_size(4);
            break;
        }
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

static int DxCreateWindow(void)
{
    WNDCLASS wc;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HBRUSH blackbrush = (HBRUSH) GetStockObject(BLACK_BRUSH);

    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hbrBackground = blackbrush;
    wc.lpszClassName = WND_CLASSNAME;
    wc.lpszMenuName = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);;

    if (0 == RegisterClass(&wc)) {
        av_log(NULL, AV_LOG_ERROR, "vo_dx: RegisterClass: %ld\n", GetLastError());
        return FALSE;
    }

    if (g_vidmode) {
        g_hwnd = CreateWindowEx(WS_EX_TOPMOST, WND_CLASSNAME, "", WS_POPUP,
                                CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, hInstance, NULL);
    } else {
        g_hwnd = CreateWindowEx(0, WND_CLASSNAME, "", WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, hInstance, NULL);
    }

    SetWindowText(g_hwnd, DLP_TITLE);

    if (NULL == g_hwnd) {
        av_log(NULL, AV_LOG_ERROR, "vo_dx: create window: %ld\n", GetLastError());
        return FALSE;
    }

    wc.hbrBackground = blackbrush;
    wc.lpszClassName = FS_CLASSNAME;

    if (0 == RegisterClass(&wc)) {
        av_log(NULL, AV_LOG_ERROR, "vo_dx: RegisterClass: %ld\n", GetLastError());
        return FALSE;
    }

    g_fs_hwnd = CreateWindow(FS_CLASSNAME, "", WS_POPUP, 0, 0, g_screen_width,
                             g_screen_height, g_hwnd, NULL, hInstance, NULL);
    if (NULL == g_fs_hwnd) {
        av_log(NULL, AV_LOG_ERROR, "vo_dx: create fs window: %ld\n", GetLastError());
    }

    return TRUE;
}

static int DxInitDirectDraw(void)
{
    HRESULT(WINAPI * OurDirectDrawCreateEx)(GUID *, LPVOID *, REFIID,
                                            IUnknown FAR *);

    hddraw_dll = LoadLibrary("DDRAW.DLL");
    if (hddraw_dll == NULL) {
        av_log(NULL, AV_LOG_ERROR, "vo_directx: failed loading ddraw.dll\n");
        return FALSE;
    }

    OurDirectDrawCreateEx = (void *) GetProcAddress(hddraw_dll,
                            "DirectDrawCreateEx");
    if (OurDirectDrawCreateEx == NULL) {
        FreeLibrary(hddraw_dll);
        hddraw_dll = NULL;
        av_log(NULL, AV_LOG_ERROR,
               "vo_directx: ddraw.dll DirectDrawCreateEx not found\n");
        return FALSE;
    }

    if (OurDirectDrawCreateEx(NULL, (VOID **) & g_lpdd, &IID_IDirectDraw7,
                              NULL) != DD_OK) {
        av_log(NULL, AV_LOG_ERROR, "vo_dx: can't create draw object\n");
        return FALSE;
    }

    if (g_vidmode) {
        if (IDirectDraw_SetCooperativeLevel(g_lpdd, g_hwnd,
                                            DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) != DD_OK) {
            av_log(NULL, AV_LOG_ERROR, "vo_dx: can't set cooperative\n");
            return FALSE;
        }
    } else {
        if (IDirectDraw_SetCooperativeLevel(g_lpdd, g_hwnd, DDSCL_NORMAL) != DD_OK) {
            av_log(NULL, AV_LOG_ERROR, "vo_dx: can't set cooperative\n");
            return FALSE;
        }
    }

    av_log(NULL, AV_LOG_INFO, "DirectDraw Object Init OK\n");
    return TRUE;
}

static int DxCreatePrimarySurface(void)
{
    DDSURFACEDESC2 ddsd;

    if (g_lpddsPrimary) {
        IDirectDrawSurface_Release(g_lpddsPrimary);
        g_lpddsPrimary = NULL;
    }

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    if (IDirectDraw_CreateSurface(g_lpdd, &ddsd, &g_lpddsPrimary, NULL) == DD_OK) {
        return TRUE;
    }
    return FALSE;
}

static int DxCreateOverlay(void)
{
    int i;
    DDSURFACEDESC2 ddsdOverlay;

    if (!g_lpdd || !g_lpddsPrimary) {
        return FALSE;
    }

    if (g_lpddsOverlay) {
        IDirectDrawSurface_Release(g_lpddsOverlay);
        g_lpddsOverlay = NULL;
    }
    if (g_lpddsBack) {
        IDirectDrawSurface_Release(g_lpddsBack);
        g_lpddsBack = NULL;
    }

    /* create double buffer overlay */
    ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
    ddsdOverlay.dwSize = sizeof(ddsdOverlay);
    ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_FLIP | DDSCAPS_COMPLEX |
                                 DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                          DDSD_BACKBUFFERCOUNT | DDSD_PIXELFORMAT;
    ddsdOverlay.dwWidth = g_image_width;
    ddsdOverlay.dwHeight = g_image_height;
    ddsdOverlay.dwBackBufferCount = 1;

    /* try every pixfmt */
    for (i = 0; i < NUM_FORMATS; i++) {
        ddsdOverlay.ddpfPixelFormat = g_ddpf[i].g_ddpfOverlay;
        if (IDirectDraw_CreateSurface(g_lpdd, &ddsdOverlay, &g_lpddsOverlay,
                                      NULL) == DD_OK) {
            g_image_format = g_ddpf[i].img_format;
            av_log(NULL, AV_LOG_INFO, "vo_dx: overlay with format %s created\n",
                   g_ddpf[i].img_format_name);

            /* get the surface directly attached to the primary (the back buffer) */
            ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
            if (IDirectDrawSurface_GetAttachedSurface(g_lpddsOverlay, &ddsdOverlay.ddsCaps,
                    &g_lpddsBack) != DD_OK) {
                av_log(NULL, AV_LOG_ERROR, "vo_dx: can't get back surface from overlay\n");
                return FALSE;
            }
            return TRUE;
        }
    }

    g_doublebuffer = 0;

    /* create single buffer overlay */
    av_log(NULL, AV_LOG_INFO, "vo_dx: try single buffer overlay now\n");
    ddsdOverlay.dwBackBufferCount = 0;
    ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;

    /* try every pixfmt */
    for (i = 0; i < NUM_FORMATS; i++) {
        ddsdOverlay.ddpfPixelFormat = g_ddpf[i].g_ddpfOverlay;
        if (DD_OK == g_lpdd->lpVtbl->CreateSurface(g_lpdd, &ddsdOverlay,
                &g_lpddsOverlay, NULL)) {
            g_image_format = g_ddpf[i].img_format;
            av_log(NULL, AV_LOG_INFO, "vo_dx: overlay with format %s created\n",
                   g_ddpf[i].img_format_name);
            break;
        }
    }
    if (i >= NUM_FORMATS) {
        return FALSE;
    }

    g_lpddsBack = g_lpddsOverlay;
    return TRUE;
}

static int DxCreateOffbuffer(void)
{
    DDSURFACEDESC2 ddsd;

    if (g_lpddsBack) {
        IDirectDrawSurface_Release(g_lpddsBack);
        g_lpddsBack = NULL;
    }

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth = g_image_width;
    ddsd.dwHeight = g_image_height;

    if (IDirectDraw_CreateSurface(g_lpdd, &ddsd, &g_lpddsBack, 0) != DD_OK) {
        return FALSE;
    }
    return TRUE;
}

static int DxGetCaps(void)
{
    DDCAPS capsDrv;

    ZeroMemory(&capsDrv, sizeof(capsDrv));
    capsDrv.dwSize = sizeof(capsDrv);
    if (IDirectDraw_GetCaps(g_lpdd, &capsDrv, NULL) != DD_OK) {
        return FALSE;
    }

    if (capsDrv.dwCaps & DDCAPS_OVERLAYSTRETCH) {
        g_StretchFactor1000 = capsDrv.dwMinOverlayStretch > 1000 ?
                              capsDrv.dwMinOverlayStretch : 1000;
    } else {
        g_StretchFactor1000 = 1000;
    }

    av_log(NULL, AV_LOG_INFO, "StretchFactor1000: %u\n", g_StretchFactor1000);

    g_align_src = (capsDrv.dwCaps & DDCAPS_ALIGNSIZESRC) ? capsDrv.dwAlignSizeSrc :
                  0;
    g_align_dst = (capsDrv.dwCaps & DDCAPS_ALIGNSIZEDEST) ?
                  capsDrv.dwAlignSizeDest : 0;
    g_boundary_src = (capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) ?
                     capsDrv.dwAlignBoundarySrc : 0;
    g_boundary_dst = (capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) ?
                     capsDrv.dwAlignBoundaryDest : 0;

    av_log(NULL, AV_LOG_INFO,
           "align_src: %u, align_dst: %u, bd_src: %u, db_dst: %u\n", g_align_src,
           g_align_dst, g_boundary_src, g_boundary_dst);

    g_allow_shrink_x = capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKX;
    g_allow_shrink_y = capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKY;
    g_allow_zoom_x = capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHX;
    g_allow_zoom_y = capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHY;

    g_overlay_key = capsDrv.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY;

    return TRUE;
}

static void vo_dx_zoom_size(int rate)
{
    if (g_image_width * rate > g_screen_width
        || g_image_height * rate > g_screen_height) {
        av_log(NULL, AV_LOG_INFO, "vo dx: zoom %d will > fullscrren\n", rate);
        return;
    }
    dw = g_image_width * rate;
    dh = g_image_height * rate;
    dx = (g_screen_width - dw) / 2;
    dy = (g_screen_height - dh) / 2;

    DxManageOverlay();
}

static int DxManageOverlay(void)
{
    HRESULT ddrval;
    DDOVERLAYFX ovfx;
    DWORD dwUpdateFlags = 0;

    int width, height;

    /* --------- fix to limit -------------- */
    rs.left = 0;
    rs.top = 0;
    rs.right = rs.left + g_image_width;
    rs.bottom = rs.top + g_image_height;
    if (g_align_src) {
        rs.right -= rs.right % g_align_src;
    }

    if (g_inited) {
        RECT cur;
        GetWindowRect(g_hwnd, &cur);
        if (cur.left != rd.left || cur.top != rd.top) {
            rd.left = cur.left;
            rd.top = cur.top;
            rd.right = cur.right;
            rd.bottom = cur.bottom;
        } else {
            rd.left = dx;
            rd.top = dy;
            rd.right = rd.left + dw;
            rd.bottom = rd.top + dh;
        }
    } else {
        rd.left = dx;
        rd.top = dy;
        rd.right = rd.left + dw;
        rd.bottom = rd.top + dh;
    }

    if (g_StretchFactor1000 > 1000) {
        rd.right = (rd.right * g_StretchFactor1000 + 999) / 1000;
        rd.bottom = rd.bottom * g_StretchFactor1000 / 1000;
    }
    if (g_align_dst) {
        rd.right = ((rd.right + g_align_dst - 1) / g_align_dst) * g_align_dst;
    }

    width = rd.right - rd.left;
    height = rd.bottom - rd.top;

    /*do not allow to zoom or shrink if hardware isn't able to do so */
    if ((width < g_image_width) && !g_allow_shrink_x) {
        rd.right = rd.left + g_image_width;
    } else if ((width > g_image_width) && !g_allow_zoom_x) {
        rd.right = rd.left + g_image_width;
    }
    if ((height < g_image_height) && !g_allow_shrink_y) {
        rd.bottom = rd.top + g_image_height;
    } else if ((height > g_image_height) && !g_allow_zoom_y) {
        rd.bottom = rd.top + g_image_height;
    }

    SetWindowPos(g_hwnd, HWND_TOPMOST, rd.left, rd.top, rd.right - rd.left,
                 rd.bottom - rd.top, SWP_NOOWNERZORDER);
    ShowWindow(g_hwnd, SW_SHOW);

    /* --------- setup update flags --------- */
    ZeroMemory(&ovfx, sizeof(ovfx));
    ovfx.dwSize = sizeof(ovfx);
    ovfx.dckDestColorkey.dwColorSpaceLowValue = 0;
    ovfx.dckDestColorkey.dwColorSpaceHighValue = 0;

    dwUpdateFlags = DDOVER_SHOW | DDOVER_DDFX;
    if (g_overlay_key) {
        dwUpdateFlags |= DDOVER_KEYDESTOVERRIDE;
    }

    ddrval = IDirectDrawSurface_UpdateOverlay(g_lpddsOverlay, &rs, g_lpddsPrimary,
             &rd, dwUpdateFlags, &ovfx);
    /*ok we can't do anything about it -> hide overlay */
    if (ddrval != DD_OK) {
        av_log(NULL, AV_LOG_ERROR, "update overlay faild , hide overlay\n");
        IDirectDrawSurface_UpdateOverlay(g_lpddsOverlay, NULL, g_lpddsPrimary, NULL,
                                         DDOVER_HIDE, NULL);
        return FALSE;
    }
    return TRUE;
}

static void flip_page(void)
{
    DDSURFACEDESC2 ddsd;
    HRESULT dxresult;

    IDirectDrawSurface_Unlock(g_lpddsBack, NULL);

    if (g_doublebuffer) {
        dxresult = IDirectDrawSurface_Flip(g_lpddsOverlay, NULL, DDFLIP_WAIT);
        if (dxresult == DDERR_SURFACELOST) {
            av_log(NULL, AV_LOG_INFO, "vo_directx: Lost, Restoring Surface\n");
            IDirectDrawSurface_Restore(g_lpddsBack);
            IDirectDrawSurface_Restore(g_lpddsOverlay);
            IDirectDrawSurface_Restore(g_lpddsPrimary);
            dxresult = IDirectDrawSurface_Flip(g_lpddsOverlay, NULL, DDFLIP_WAIT);
        }
        if (dxresult != DD_OK) {
            av_log(NULL, AV_LOG_ERROR, "vo_directx: can not flip page\n");
        }
    } else {
        DDBLTFX ddbltfx;
        memset(&ddbltfx, 0, sizeof(DDBLTFX));
        ddbltfx.dwSize = sizeof(DDBLTFX);
        ddbltfx.dwDDFX = DDBLTFX_NOTEARING;
        IDirectDrawSurface_Blt(g_lpddsPrimary, &rd, g_lpddsBack, NULL, DDBLT_WAIT,
                               &ddbltfx);
    }

    memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
    ddsd.dwSize = sizeof(DDSURFACEDESC2);
    if (IDirectDrawSurface_Lock(g_lpddsBack, NULL, &ddsd,
                                DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL) == DD_OK) {
        if (tmp_image) {
            free(tmp_image);
            tmp_image = NULL;
        }
        g_dstride = ddsd.lPitch;
        g_image = ddsd.lpSurface;
    } else {
        if (!tmp_image) {
            av_log(NULL, AV_LOG_INFO,
                   "Locking the surface failed, rendering to a hidden surface!\n");
            tmp_image = g_image = calloc(1, g_image_height * g_dstride * 2);
        }
    }
}

static int vo_dx_uninit(void)
{
    if (NULL != g_lpddclipper) {
        g_lpddclipper->lpVtbl->Release(g_lpddclipper);
    }
    g_lpddclipper = NULL;
    av_log(NULL, AV_LOG_INFO, "vo_directx: clipper released\n");

    if (NULL != g_lpddsBack) {
        IDirectDrawSurface_Release(g_lpddsBack);
    }
    g_lpddsBack = NULL;
    av_log(NULL, AV_LOG_INFO, "vo_directx: back surface released\n");

    if (g_lpddsOverlay != NULL) {
        IDirectDrawSurface_Release(g_lpddsOverlay);
    }
    g_lpddsOverlay = NULL;
    av_log(NULL, AV_LOG_INFO, "vo_directx: overlay surface released\n");

    if (g_lpddsPrimary != NULL) {
        IDirectDrawSurface_Release(g_lpddsPrimary);
    }
    g_lpddsPrimary = NULL;
    av_log(NULL, AV_LOG_INFO, "vo_directx: primary released\n");

    if (g_fs_hwnd) {
        DestroyWindow(g_fs_hwnd);
    }
    g_fs_hwnd = NULL;

    if (g_hwnd) {
        DestroyWindow(g_hwnd);
    }
    g_hwnd = NULL;
    av_log(NULL, AV_LOG_INFO, "vo_directx: window destroyed\n");

    UnregisterClass(WND_CLASSNAME, GetModuleHandle(NULL));
    UnregisterClass(FS_CLASSNAME, GetModuleHandle(NULL));

    if (g_lpdd != NULL) {
        IDirectDraw_Release(g_lpdd);
        g_lpdd = NULL;
    }

    if (hddraw_dll) {
        FreeLibrary(hddraw_dll);
        hddraw_dll = NULL;
    }

    return 0;
}

static int vo_dx_init(void)
{
    DDSURFACEDESC2 ddsd;

    dw = g_image_width = dlpctxp->pwidth;
    dh = g_image_height = dlpctxp->pheight;
    g_screen_width = GetSystemMetrics(SM_CXSCREEN);
    g_screen_height = GetSystemMetrics(SM_CYSCREEN);
    dx = (g_screen_width - dw) / 2;
    dy = (g_screen_height - dh) / 2;

    if (FALSE == DxCreateWindow()) {
        av_log(NULL, AV_LOG_ERROR, "DxCreateWindow step\n");
        return -1;
    }

    if (FALSE == DxInitDirectDraw()) {
        av_log(NULL, AV_LOG_ERROR, "DxInitDirectDraw step\n");
        return -1;
    }

    if (FALSE == DxCreatePrimarySurface()) {
        av_log(NULL, AV_LOG_ERROR, "DxCreatePrimarySurface step\n");
        return -1;
    }

    if (FALSE == DxCreateOverlay()) {
        av_log(NULL, AV_LOG_ERROR, "DxCreateOverlay step\n");
        if (FALSE == DxCreateOffbuffer()) {
            av_log(NULL, AV_LOG_ERROR, "DxCreateOffbuffer step\n");
            return -1;
        }
    }

    if (FALSE == DxGetCaps()) {
        av_log(NULL, AV_LOG_ERROR, "DxGetCaps step\n");
        return -1;
    }
    if (FALSE == DxManageOverlay()) {
        av_log(NULL, AV_LOG_ERROR, "DxManageOverlay step\n");
        return -1;
    }

    memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
    ddsd.dwSize = sizeof(DDSURFACEDESC2);
    if (IDirectDrawSurface_Lock(g_lpddsBack, NULL, &ddsd,
                                DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL) != DD_OK) {
        av_log(NULL, AV_LOG_ERROR, "Lock Back Buffer step\n");
        return -1;
    }
    g_dstride = ddsd.lPitch;
    g_image = ddsd.lpSurface;
    av_log(NULL, AV_LOG_ERROR, "vo dx init ok\n");

    /* -------- create picture ----------------- */
    my_pic = av_mallocz(sizeof(AVPicture));
    if (-1 == avpicture_alloc(my_pic, g_image_format, g_image_width,
                              g_image_height)) {
        av_log(NULL, AV_LOG_ERROR, "avpicture alloc error\n");
        return -1;
    }

    g_inited = 1;
    return 0;
}

static int vo_dx_vfmt2rgb(AVPicture * dst, AVPicture * src)
{
    static struct SwsContext *img_convert_ctx;

    img_convert_ctx = sws_getCachedContext(img_convert_ctx, dlpctxp->pwidth,
                                           dlpctxp->pheight, dlpctxp->pixfmt, dlpctxp->pwidth, dlpctxp->pheight,
                                           g_image_format, 0, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, src->data, src->linesize, 0, dlpctxp->pheight,
              dst->data, dst->linesize);

    return 0;
}

static void vo_dx_display(AVPicture * pic)
{
    vo_dx_vfmt2rgb(my_pic, pic);
    memcpy(g_image, my_pic->data[0], my_pic->linesize[0] * dlpctxp->pheight);
    flip_page();
}

static int vo_dx_control(int cmd, void *arg)
{
    switch (cmd) {
#if 0
    case VO_TOGGLE_FULLSCREEN:
        break;
#endif
    case VO_ZOOM:
        vo_dx_zoom_size(*((int *) arg));
        break;
    default:
        av_log(NULL, AV_LOG_ERROR, "vo directx: cmd not support now!\n");
        break;
    }
    return 0;
}

static void dx_check_events(void *arg)
{
    MSG msg;
    while (1) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        usleep(200 * 1000);
    }
}

vo_t vo_directx = {
    .id = VO_ID_DIRECTX,
    .name = "directx",
    .vo_init = vo_dx_init,
    .vo_uninit = vo_dx_uninit,
    .vo_display = vo_dx_display,
    .vo_event_loop = dx_check_events,
    .vo_control = vo_dx_control,
};

#endif
