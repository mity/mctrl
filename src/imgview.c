/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2013-2020 Martin Mitas
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

#include "xd2d.h"
#include "xwic.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define IMGVIEW_DEBUG     1*/

#ifdef IMGVIEW_DEBUG
    #define IMGVIEW_TRACE   MC_TRACE
#else
    #define IMGVIEW_TRACE   MC_NOOP
#endif


static const TCHAR imgview_wc[] = MC_WC_IMGVIEW;    /* Window class name */

#define IMGVIEW_XD2D_CACHE_TIMER_ID    1


typedef struct imgview_tag imgview_t;
struct imgview_tag {
    HWND win;
    HWND notify_win;
    xd2d_cache_t xd2d_cache;
    IWICBitmapSource* image;
    DWORD style       : 16;
    DWORD no_redraw   :  1;
    DWORD rtl         :  1;
};

static void
imgview_paint(void* ctrl, xd2d_ctx_t* ctx)
{
    imgview_t* iv = (imgview_t*) ctrl;
    c_ID2D1RenderTarget* rt = ctx->rt;
    RECT client;
    HRESULT hr;

    GetClientRect(iv->win, &client);

    /* Erase background. */
    if(ctx->erase) {
        if(iv->style & MC_IVS_TRANSPARENT) {
            c_ID2D1GdiInteropRenderTarget* gdi_interop;
            HDC dc;

            hr = c_ID2D1RenderTarget_QueryInterface(rt, &c_IID_ID2D1GdiInteropRenderTarget, (void**) &gdi_interop);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("imgview_paint: ID2D1RenderTarget::QueryInterface(IID_ID2D1GdiInteropRenderTarget) failed.");
                goto err_QueryInterface;
            }

            hr = c_ID2D1GdiInteropRenderTarget_GetDC(gdi_interop, c_D2D1_DC_INITIALIZE_MODE_CLEAR, &dc);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("imgview_paint: ID2D1GdiInteropRenderTarget::GetDC() failed.");
                goto err_GetDC;
            }

            DrawThemeParentBackground(iv->win, dc, NULL);

            c_ID2D1GdiInteropRenderTarget_ReleaseDC(gdi_interop, NULL);
err_GetDC:
            c_ID2D1GdiInteropRenderTarget_Release(gdi_interop);
err_QueryInterface:
            ;
        } else {
            COLORREF cref = GetSysColor(COLOR_WINDOW);
            c_D2D1_COLOR_F c = XD2D_COLOR_CREF(cref);
            c_ID2D1RenderTarget_Clear(rt, &c);
        }
    }

    /* Paint the image. */
    if(iv->image != NULL) {
        UINT w, h;
        c_D2D1_RECT_F src;
        c_D2D1_RECT_F dst;
        c_ID2D1Bitmap* b;

        IWICBitmapSource_GetSize(iv->image, &w, &h);

        src.left = 0.0f;
        src.top = 0.0f;
        src.right = (float) w;
        src.bottom = (float) h;

        /* Compute destination rectangle. */
        if(iv->style & MC_IVS_REALSIZECONTROL) {
            /* Stretch the image to accommodate full client area. */
            dst.left = 0.0f;
            dst.top = 0.0f;
            dst.right = client.right;
            dst.bottom = client.bottom;
        } else if(iv->style & MC_IVS_REALSIZEIMAGE) {
            /* Center the image in the control. */
            dst.left = (client.right - src.right) / 2.0f;
            dst.top = (client.bottom - src.bottom) / 2.0f;
            dst.right = dst.left + src.right;
            dst.bottom = dst.top + src.bottom;
        } else {
            /* Stretch the image but keep its width/height ratio. */
            float ratio_w = client.right / (src.right - src.left);
            float ratio_h = client.bottom / (src.bottom - src.top);
            if(ratio_w >= ratio_h) {
                float w = (src.right - src.left) * ratio_h;
                dst.left = (client.right - w) / 2.0f;
                dst.top = 0.0f;
                dst.right = dst.left + w;
                dst.bottom = client.bottom;
            } else {
                float h = (src.bottom - src.top) * ratio_w;
                dst.left = 0.0f;
                dst.top = (client.bottom - h) / 2.0f;
                dst.right = client.right;
                dst.bottom = dst.top + h;
            }
        }

        /* With RTL style, apply right-to-left transformation. */
        if(iv->rtl) {
            c_D2D1_MATRIX_3X2_F m;
            m._11 = -1.0f;  m._12 = 0.0f;
            m._21 = 0.0f;   m._22 = 1.0f;
            m._31 = (float) mc_width(&client);
            m._32 = 0.0f;
            c_ID2D1RenderTarget_SetTransform(rt, &m);
        }

        /* Output he image. */
        hr = c_ID2D1RenderTarget_CreateBitmapFromWicBitmap(rt, iv->image, NULL, &b);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("imgview_paint: ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed.");
            goto err_CreateBitmapFromWicBitmap;
        }
        c_ID2D1RenderTarget_DrawBitmap(rt, b, &dst, 1.0f,
                c_D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
        c_ID2D1Bitmap_Release(b);
err_CreateBitmapFromWicBitmap:
        ;

        /* With RTL style, reset the transformation. */
        if(iv->rtl) {
            c_D2D1_MATRIX_3X2_F m;
            m._11 = 1.0f;   m._12 = 0.0f;
            m._21 = 0.0f;   m._22 = 1.0f;
            m._31 = 0.0f;   m._32 = 0.0f;
            c_ID2D1RenderTarget_SetTransform(rt, &m);
        }
    }
}

static xd2d_vtable_t imgview_xd2d_vtable = XD2D_CTX_SIMPLE(imgview_paint);

static void
imgview_style_changed(imgview_t* iv, STYLESTRUCT* ss)
{
    iv->style = ss->styleNew;
    xd2d_invalidate(iv->win, NULL, TRUE, &iv->xd2d_cache);
}

static void
imgview_exstyle_changed(imgview_t* iv, STYLESTRUCT* ss)
{
    BOOL rtl;

    rtl = mc_is_rtl_exstyle(ss->styleNew);
    if(iv->rtl == rtl) {
        iv->rtl = rtl;
        xd2d_invalidate(iv->win, NULL, TRUE, &iv->xd2d_cache);
    }
}

static int
imgview_load_resource(imgview_t* iv, HINSTANCE instance, const void* res_name, BOOL unicode)
{
    static TCHAR* allowed_res_types[] = {
        RT_RCDATA, _T("PNG"), RT_BITMAP, RT_HTML
    };
    IWICBitmapSource* image = NULL;

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
            image = xwic_from_resource(instance, allowed_res_types[i], tmp);
            if(image != NULL)
                break;
        }

        if(tmp != res_name)
            free(tmp);

        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_resource: xwic_from_resource() failed.");
            return -1;
        }
    }

    if(iv->image != NULL)
        IWICBitmapSource_Release(iv->image);
    iv->image = image;
    xd2d_invalidate(iv->win, NULL, TRUE, &iv->xd2d_cache);

    return 0;
}

static int
imgview_load_file(imgview_t* iv, void* path, BOOL unicode)
{
    IWICBitmapSource* image;

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

        image = xwic_from_file(tmp);
        if(tmp != path)
            free(tmp);
        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_file: xwic_from_file() failed.");
            return -1;
        }
    } else {
        image = NULL;
    }

    if(iv->image != NULL)
        IWICBitmapSource_Release(iv->image);
    iv->image = image;
    xd2d_invalidate(iv->win, NULL, TRUE, &iv->xd2d_cache);

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
            cs->lpszName = MC_RES_ID(cs->lpszName[1]);
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
        IWICBitmapSource_Release(iv->image);
    xd2d_free_cache(&iv->xd2d_cache);
    free(iv);
}

static LRESULT CALLBACK
imgview_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    imgview_t* iv = (imgview_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            xd2d_paint(win, iv->no_redraw, 0, &imgview_xd2d_vtable, (void*) iv, &iv->xd2d_cache);
            if(iv->xd2d_cache != NULL)
                SetTimer(win, IMGVIEW_XD2D_CACHE_TIMER_ID, 30 * 1000, NULL);
            return 0;

        case WM_PRINTCLIENT:
            xd2d_printclient(win, (HDC) wp, 0, &imgview_xd2d_vtable, (void*) iv);
            return 0;

        case WM_SIZE:
        case WM_DISPLAYCHANGE:
            xd2d_free_cache(&iv->xd2d_cache);
            xd2d_invalidate(win, NULL, TRUE, &iv->xd2d_cache);
            return 0;

        case WM_TIMER:
            if(wp == IMGVIEW_XD2D_CACHE_TIMER_ID) {
                xd2d_free_cache(&iv->xd2d_cache);
                KillTimer(win, IMGVIEW_XD2D_CACHE_TIMER_ID);
                return 0;
            }
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
                xd2d_invalidate(iv->win, NULL, TRUE, &iv->xd2d_cache);
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

    wc.style = CS_GLOBALCLASS | CS_PARENTDC;
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
