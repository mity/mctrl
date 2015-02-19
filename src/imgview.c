/*
 * Copyright (c) 2013-2015 Martin Mitas
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
#include "memstream.h"
#include "theme.h"
#include "xdraw.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define IMGVIEW_DEBUG     1*/

#ifdef IMGVIEW_DEBUG
    #define IMGVIEW_TRACE       MC_TRACE
#else
    #define IMGVIEW_TRACE(...)  do {} while(0)
#endif


static const TCHAR imgview_wc[] = MC_WC_IMGVIEW;    /* Window class name */


typedef struct imgview_tag imgview_t;
struct imgview_tag {
    HWND win;
    HWND notify_win;
    xdraw_canvas_t* canvas;
    xdraw_image_t* image;
    DWORD style       : 16;
    DWORD no_redraw   : 1;
};


static BOOL
imgview_paint_to_canvas(imgview_t* iv, xdraw_canvas_t* canvas,
                        RECT* dirty, BOOL erase)
{
    xdraw_canvas_begin_paint(canvas);

    if(erase) {
        if(iv->style & MC_IVS_TRANSPARENT) {
            HDC dc;

            dc = xdraw_canvas_acquire_dc(canvas, FALSE);
            if(dc != NULL) {
                mcDrawThemeParentBackground(iv->win, dc, dirty);
                xdraw_canvas_release_dc(canvas, dc);
            } else {
                MC_TRACE("imgview_paint_to_canvas: xdraw_canvas_acquire_dc() failed.");
            }
        } else {
            xdraw_clear(canvas, XDRAW_COLORREF(GetSysColor(COLOR_WINDOW)));
        }
    }

    if(iv->image != NULL) {
        RECT client;
        xdraw_rect_t src;
        xdraw_rect_t dst;

        GetClientRect(iv->win, &client);

        src.x0 = 0.0f;
        src.y0 = 0.0f;
        xdraw_image_get_size(iv->image, &src.x1, &src.y1);

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

        xdraw_draw_image(canvas, iv->image, &dst, &src);
    }

    return xdraw_canvas_end_paint(canvas);
}

static void
imgview_paint(imgview_t* iv)
{
    PAINTSTRUCT ps;
    xdraw_canvas_t* canvas;

    BeginPaint(iv->win, &ps);
    if(iv->no_redraw)
        goto no_paint;

    canvas = iv->canvas;
    if(canvas == NULL) {
        canvas = xdraw_canvas_create_with_paintstruct(iv->win, &ps, XDRAW_CANVAS_GDICOMPAT);
        if(MC_ERR(canvas == NULL))
            goto no_paint;
    }

    if(imgview_paint_to_canvas(iv, canvas, &ps.rcPaint, ps.fErase)) {
        /* We are allowed to cache the context for reuse. */
        iv->canvas = canvas;
    } else {
        xdraw_canvas_destroy(canvas);
        iv->canvas = NULL;
    }

no_paint:
    EndPaint(iv->win, &ps);
}

static void
imgview_printclient(imgview_t* iv, HDC dc)
{
    RECT rect;
    xdraw_canvas_t* canvas;

    GetClientRect(iv->win, &rect);
    canvas = xdraw_canvas_create_with_dc(dc, &rect, XDRAW_CANVAS_GDICOMPAT);
    if(MC_ERR(canvas == NULL))
        return;
    imgview_paint_to_canvas(iv, canvas, &rect, TRUE);
    xdraw_canvas_destroy(canvas);
}

static void
imgview_style_changed(imgview_t* iv, STYLESTRUCT* ss)
{
    iv->style = ss->styleNew;
    if(!iv->no_redraw)
        InvalidateRect(iv->win, NULL, TRUE);
}

static int
imgview_load_resource(imgview_t* iv, HINSTANCE instance, const void* res_name, BOOL unicode)
{
    static TCHAR* allowed_res_types[] = {
        RT_RCDATA, _T("PNG"), RT_BITMAP, RT_HTML
    };
    xdraw_image_t* image = NULL;

    if(res_name != NULL) {
        IStream* stream = NULL;
        int i;
        TCHAR* tmp;

        if(unicode == MC_IS_UNICODE) {
            tmp = (TCHAR*) res_name;
        } else {
            tmp = mc_str(res_name, (unicode ? MC_STRW : MC_STRA), MC_STRT);
            if(MC_ERR(tmp == NULL)) {
                MC_TRACE("imgview_load_resource: mc_str() failed.");
                return -1;
            }
        }

        for(i = 0; i < MC_ARRAY_SIZE(allowed_res_types); i++) {
            stream = memstream_create_from_resource(instance, allowed_res_types[i], tmp);
            if(stream != NULL)
                break;
        }
        if(tmp != res_name)
            free(tmp);
        if(MC_ERR(stream == NULL)) {
             MC_TRACE("imgview_load_resource: "
                      "memstream_create_from_resource() failed.");
             return -1;
        }

        image = xdraw_image_load_from_stream(stream);
        IStream_Release(stream);
        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_resource: "
                     "xdraw_image_load_from_stream() failed.");
            return -1;
        }
    }

    if(iv->image != NULL)
        xdraw_image_destroy(iv->image);
    iv->image = image;

    return 0;
}

static int
imgview_load_file(imgview_t* iv, void* path, BOOL unicode)
{
    xdraw_image_t* image;

    if(path != NULL) {
        WCHAR* tmp;

        if(unicode == MC_IS_UNICODE) {
            tmp = path;
        } else {
            tmp = mc_str(path, MC_STRA, MC_STRT);
            if(MC_ERR(tmp == NULL)) {
                MC_TRACE("imgview_load_file: mc_str() failed.");
                return -1;
            }
        }

        image = xdraw_image_load_from_file(tmp);
        if(tmp != path)
            free(tmp);
        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_file: xdraw_image_load_from_file() failed");
            return -1;
        }
    } else {
        image = NULL;
    }

    if(iv->image != NULL)
        xdraw_image_destroy(iv->image);
    iv->image = image;

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
        xdraw_image_destroy(iv->image);
    if(iv->canvas != NULL)
        xdraw_canvas_destroy(iv->canvas);
    free(iv);
}

static LRESULT CALLBACK
imgview_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    imgview_t* iv = (imgview_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            imgview_paint(iv);
            return 0;

        case WM_PRINTCLIENT:
            imgview_printclient(iv, (HDC) wp);
            return 0;

        case WM_DISPLAYCHANGE:
            if(iv->canvas != NULL) {
                xdraw_canvas_destroy(iv->canvas);
                iv->canvas = NULL;
            }
            InvalidateRect(win, NULL, FALSE);
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
                InvalidateRect(win, NULL, TRUE);

        case WM_GETDLGCODE:
            return DLGC_STATIC;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE)
                imgview_style_changed(iv, (STYLESTRUCT*) lp);
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

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = imgview_proc;
    wc.cbWndExtra = sizeof(imgview_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = imgview_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("imgview_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
imgview_fini_module(void)
{
    UnregisterClass(imgview_wc, NULL);
}
