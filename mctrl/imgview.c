/*
 * Copyright (c) 2013-2019 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "imgview.h"
#include "theme.h"

#include "xdraw.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define IMGVIEW_DEBUG     1*/

#ifdef IMGVIEW_DEBUG
    #define IMGVIEW_TRACE   MC_TRACE
#else
    #define IMGVIEW_TRACE   MC_NOOP
#endif


static const TCHAR imgview_wc[] = MC_WC_IMGVIEW;    /* Window class name */
static DWORD imgview_wdl_flags = WD_INIT_IMAGEAPI;


typedef struct imgview_tag imgview_t;
struct imgview_tag {
    HWND win;
    HWND notify_win;
    xdraw_cache_t xdraw_cache;
    WD_HIMAGE image;
    DWORD style       : 16;
    DWORD no_redraw   :  1;
    DWORD rtl         :  1;
};

static void
imgview_paint(void* ctrl, xdraw_ctx_t* ctx)
{
    imgview_t* iv = (imgview_t*) ctrl;
    WD_HCANVAS canvas = ctx->canvas;

    if(ctx->erase) {
        if(iv->style & MC_IVS_TRANSPARENT) {
            HDC dc;

            dc = wdStartGdi(canvas, FALSE);
            if(dc != NULL) {
                mcDrawThemeParentBackground(iv->win, dc, NULL);
                wdEndGdi(canvas, dc);
            } else {
                MC_TRACE("imgview_paint_to_canvas: wdStartGdi() failed.");
            }
        } else {
            COLORREF c = GetSysColor(COLOR_WINDOW);
            wdClear(canvas, WD_RGB(GetRValue(c), GetGValue(c), GetBValue(c)));
        }
    }

    if(iv->image != NULL) {
        RECT client;
        UINT w, h;
        WD_RECT src;
        WD_RECT dst;

        GetClientRect(iv->win, &client);
        wdGetImageSize(iv->image, &w, &h);

        src.x0 = 0.0f;
        src.y0 = 0.0f;
        src.x1 = (float) w;
        src.y1 = (float) h;

        if(iv->style & MC_IVS_REALSIZECONTROL) {
            dst.x0 = 0.0f;
            dst.y0 = 0.0f;
            dst.x1 = client.right;
            dst.y1 = client.bottom;
        } else if(iv->style & MC_IVS_REALSIZEIMAGE) {
            dst.x0 = (client.right - src.x1) / 2.0f;
            dst.y0 = (client.bottom - src.y1) / 2.0f;
            dst.x1 = dst.x0 + src.x1;
            dst.y1 = dst.y0 + src.y1;
        } else {
            float ratio_w = client.right / (src.x1 - src.x0);
            float ratio_h = client.bottom / (src.y1 - src.y0);
            if(ratio_w >= ratio_h) {
                float w = (src.x1 - src.x0) * ratio_h;
                dst.x0 = (client.right - w) / 2.0f;
                dst.y0 = 0.0f;
                dst.x1 = dst.x0 + w;
                dst.y1 = client.bottom;
            } else {
                float h = (src.y1 - src.y0) * ratio_w;
                dst.x0 = 0.0f;
                dst.y0 = (client.bottom - h) / 2.0f;
                dst.x1 = client.right;
                dst.y1 = dst.y0 + h;
            }
        }

        wdBitBltImage(canvas, iv->image, &dst, &src);
    }
}

static xdraw_vtable_t imgview_xdraw_vtable = XDRAW_CTX_SIMPLE(imgview_paint);

static void
imgview_style_changed(imgview_t* iv, STYLESTRUCT* ss)
{
    iv->style = ss->styleNew;
    xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);
}

static void
imgview_exstyle_changed(imgview_t* iv, STYLESTRUCT* ss)
{
    BOOL rtl;

    rtl = mc_is_rtl_exstyle(ss->styleNew);
    if(iv->rtl == rtl) {
        iv->rtl = rtl;
        xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);
    }
}

static int
imgview_load_resource(imgview_t* iv, HINSTANCE instance, const void* res_name, BOOL unicode)
{
    static TCHAR* allowed_res_types[] = {
        RT_RCDATA, _T("PNG"), RT_BITMAP, RT_HTML
    };
    WD_HIMAGE image = NULL;

    if(res_name != NULL) {
        int i;
        WCHAR* tmp;

        if(unicode) {
            tmp = (WCHAR*) res_name;
        } else {
            tmp = mc_str(res_name, MC_STRA, MC_STRW);
            if(MC_ERR(tmp == NULL)) {
                MC_TRACE("imgview_load_resource: mc_str() failed.");
                return -1;
            }
        }

        for(i = 0; i < MC_SIZEOF_ARRAY(allowed_res_types); i++) {
            image = wdLoadImageFromResource(instance, allowed_res_types[i], tmp);
            if(image != NULL)
                break;
        }

        if(tmp != res_name)
            free(tmp);

        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_resource: wdLoadIMageFromResource() failed.");
            return -1;
        }
    }

    if(iv->image != NULL)
        wdDestroyImage(iv->image);
    iv->image = image;
    xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);

    return 0;
}

static int
imgview_load_file(imgview_t* iv, void* path, BOOL unicode)
{
    WD_HIMAGE image;

    if(path != NULL) {
        WCHAR* tmp;

        if(unicode) {
            tmp = path;
        } else {
            tmp = mc_str(path, MC_STRA, MC_STRW);
            if(MC_ERR(tmp == NULL)) {
                MC_TRACE("imgview_load_file: mc_str() failed.");
                return -1;
            }
        }

        image = wdLoadImageFromFile(tmp);
        if(tmp != path)
            free(tmp);
        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_file: wdLoadImageFromFile() failed.");
            return -1;
        }
    } else {
        image = NULL;
    }

    if(iv->image != NULL)
        wdDestroyImage(iv->image);
    iv->image = image;
    xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);

    return 0;
}

static imgview_t*
imgview_nccreate(HWND win, CREATESTRUCT* cs)
{
    imgview_t* iv;

    iv = (imgview_t*) malloc(sizeof(imgview_t));
    if(MC_ERR(iv == NULL)) {
        MC_TRACE("imgview_nccreate: malloc() failed.");
        return NULL;
    }

    memset(iv, 0, sizeof(imgview_t));
    iv->win = win;
    iv->notify_win = cs->hwndParent;
    iv->style = cs->style;
    iv->rtl = mc_is_rtl_exstyle(cs->dwExStyle);

    if(cs->lpszName != NULL) {
#ifdef UNICODE
        if(cs->lpszName != NULL  &&  cs->lpszName[0] == 0xffff)
            cs->lpszName = MAKEINTRESOURCE(cs->lpszName[1]);
#endif
        imgview_load_resource(iv, cs->hInstance, cs->lpszName, MC_IS_UNICODE);

        /* Do not propagate cs->lpszName into WM_CREATE and WM_SETTEXT */
        cs->lpszName = NULL;
    }

    return iv;
}

static void
imgview_ncdestroy(imgview_t* iv)
{
    if(iv->image != NULL)
        wdDestroyImage(iv->image);
    xdraw_free_cache(&iv->xdraw_cache);
    free(iv);
}

static LRESULT CALLBACK
imgview_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    imgview_t* iv = (imgview_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            return xdraw_paint(win, iv->no_redraw,
                        (iv->rtl ? WD_CANVAS_LAYOUTRTL : 0),
                        &imgview_xdraw_vtable, (void*) iv, &iv->xdraw_cache);

        case WM_PRINTCLIENT:
            return xdraw_printclient(win, (HDC) wp,
                        (iv->rtl ? WD_CANVAS_LAYOUTRTL : 0),
                        &imgview_xdraw_vtable, (void*) iv);

        case WM_SIZE:
            xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);
            break;

        case WM_DISPLAYCHANGE:
            xdraw_free_cache(&iv->xdraw_cache);
            xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);
            break;

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

        case MC_IVM_LOADRESOURCEW:
        case MC_IVM_LOADRESOURCEA:
            return (imgview_load_resource(iv, (HINSTANCE) wp, (void*) lp, (msg == MC_IVM_LOADRESOURCEW)) == 0);

        case MC_IVM_LOADFILEW:
        case MC_IVM_LOADFILEA:
            return (imgview_load_file(iv, (void*) lp, (msg == MC_IVM_LOADRESOURCEW)) == 0);

        case WM_SETREDRAW:
            iv->no_redraw = !wp;
            if(!iv->no_redraw)
                xdraw_invalidate(iv->win, NULL, TRUE, &iv->xdraw_cache);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_STATIC;

        case WM_STYLECHANGED:
            switch(wp) {
                case GWL_STYLE:
                    imgview_style_changed(iv, (STYLESTRUCT*) lp);
                    break;

                case GWL_EXSTYLE:
                    imgview_exstyle_changed(iv, (STYLESTRUCT*) lp);
                    break;
            }
            break;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = iv->notify_win;
            iv->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case WM_NCCREATE:
            iv = (imgview_t*) imgview_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(iv == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)iv);
            break;

        case WM_NCDESTROY:
            if(iv)
                imgview_ncdestroy(iv);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}

int
imgview_init_module(void)
{
    WNDCLASS wc = { 0 };

    if(MC_ERR(!wdInitialize(imgview_wdl_flags))) {
        MC_TRACE("imgview_init_module: wdInitialize() failed");
        return -1;
    }

    wc.style = CS_GLOBALCLASS | CS_PARENTDC;
    wc.lpfnWndProc = imgview_proc;
    wc.cbWndExtra = sizeof(imgview_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = imgview_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("imgview_init_module: RegisterClass() failed");
        wdTerminate(imgview_wdl_flags);
        return -1;
    }

    return 0;
}

void
imgview_fini_module(void)
{
    UnregisterClass(imgview_wc, NULL);
    wdTerminate(imgview_wdl_flags);
}
