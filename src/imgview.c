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
#include "gdix.h"
#include "memstream.h"
#include "theme.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define IMGVIEW_DEBUG     1*/

#ifdef IMGVIEW_DEBUG
    #define IMGVIEW_TRACE       MC_TRACE
#else
    #define IMGVIEW_TRACE(...)  do {} while(0)
#endif


/*********************
 *** Image Loading ***
 *********************/

static gdix_Image*
imgview_load_image_from_resource(HINSTANCE instance, const TCHAR* name)
{
    static TCHAR* allowed_res_types[] = {
        RT_RCDATA, _T("PNG"), RT_BITMAP, RT_HTML
    };
    int i;
    IStream* stream;
    gdix_Image* img;
    gdix_Status status;

    for(i = 0; i < MC_ARRAY_SIZE(allowed_res_types); i++) {
        stream = memstream_create_from_resource(instance, allowed_res_types[i], name);
        if(stream != NULL)
            break;
    }
    if(MC_ERR(stream == NULL)) {
        MC_TRACE_ERR("imgview_load_image_from_resource: "
                     "memstream_create_from_resource() failed");
        return NULL;
    }

    status = gdix_LoadImageFromStream(stream, &img);
    if(MC_ERR(status != gdix_Ok)) {
        MC_TRACE("imgview_load_image_from_resource: "
                 "GdipLoadImageFromStream() failed [%lu]", status);
        img = NULL;
    }

    IStream_Release(stream);
    return img;
}

static gdix_Image*
imgview_load_image_from_file(const WCHAR* path)
{
    gdix_Image* img;
    gdix_Status status;

    status = gdix_LoadImageFromFile(path, &img);
    if(MC_ERR(status != gdix_Ok)) {
        MC_TRACE("imgview_load_image_from_file: "
                 "GdipLoadImageFromFile() failed [%lu]", status);
        img = NULL;
    }

    return img;
}


/******************************
 *** Control implementation ***
 ******************************/

static const TCHAR imgview_wc[] = MC_WC_IMGVIEW;    /* Window class name */


typedef struct imgview_tag imgview_t;
struct imgview_tag {
    HWND win;
    HWND notify_win;
    gdix_Image* image;
    DWORD style       : 16;
    DWORD no_redraw   : 1;
};


static void
imgview_paint(imgview_t* iv, HDC dc, RECT* dirty, BOOL erase)
{
    gdix_Graphics* gfx;
    gdix_Status status;
    gdix_RectF src, dst;
    gdix_Unit unit;
    RECT client;

    if(erase) {
        if(iv->style & MC_IVS_TRANSPARENT)
            mcDrawThemeParentBackground(iv->win, dc, dirty);
        else
            FillRect(dc, dirty, GetSysColorBrush(COLOR_WINDOW));
    }

    if(iv->image == NULL)
        return;

    GetClientRect(iv->win, &client);

    status = gdix_CreateFromHDC(dc, &gfx);
    if(MC_ERR(status != gdix_Ok)) {
        MC_TRACE("imgview_paint: gdix_CreateFromHDC() failed [%ld]", status);
        goto err_CreateFromHDC;
    }

    gdix_GetImageBounds(iv->image, &src, &unit);
    if(iv->style & MC_IVS_REALSIZECONTROL) {
        dst.x = 0;
        dst.y = 0;
        dst.w = client.right;
        dst.h = client.bottom;
    } else if(iv->style & MC_IVS_REALSIZEIMAGE) {
        dst.x = (client.right - src.w) / 2.0f;
        dst.y = (client.bottom - src.h) / 2.0f;
        dst.w = src.w;
        dst.h = src.h;
    } else {
        gdix_Real ratio_w = client.right / src.w;
        gdix_Real ratio_h = client.bottom / src.h;
        if(ratio_w >= ratio_h) {
            dst.w = src.w * ratio_h;
            dst.h = client.bottom;
            dst.x = (client.right - dst.w) / 2.0f;
            dst.y = 0;
        } else {
            dst.w = client.right;
            dst.h = src.h * ratio_w;
            dst.x = 0;
            dst.y = (client.bottom - dst.h) / 2.0f;
        }
    }

    status = gdix_DrawImageRectRect(gfx, iv->image, dst.x, dst.y, dst.w, dst.h,
                src.x, src.y, src.w, src.h, unit, NULL, NULL, NULL);
    if(MC_ERR(status != gdix_Ok)) {
        MC_TRACE("imgview_paint: gdix_DrawImage() failed [%ld]", status);
        goto err_DrawImage;
    }

err_DrawImage:
    gdix_DeleteGraphics(gfx);
err_CreateFromHDC:
    /* noop */;
}

static void
imgview_style_changed(imgview_t* iv, STYLESTRUCT* ss)
{
    iv->style = ss->styleNew;
    if(!iv->no_redraw)
        RedrawWindow(iv->win, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
}

static int
imgview_load_resource(imgview_t* iv, HINSTANCE instance, void* res_name, BOOL unicode)
{
    gdix_Image* image;

    if(res_name != NULL) {
        TCHAR* tmp;

        if(unicode == MC_IS_UNICODE) {
            tmp = res_name;
        } else {
            tmp = mc_str(res_name, (unicode ? MC_STRW : MC_STRA), MC_STRT);
            if(MC_ERR(tmp == NULL)) {
                MC_TRACE("imgview_load_resource: mc_str() failed.");
                return -1;
            }
        }

        image = imgview_load_image_from_resource(instance, res_name);
        if(tmp != res_name)
            free(tmp);
        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_resource: imgview_load_image_from_resource() failed");
            return -1;
        }
    } else {
        image = NULL;
    }

    if(iv->image)
        gdix_DisposeImage(iv->image);
    iv->image = image;

    return 0;
}

static int
imgview_load_file(imgview_t* iv, void* path, BOOL unicode)
{
    gdix_Image* image;

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

        image = imgview_load_image_from_file(tmp);
        if(tmp != path)
            free(tmp);
        if(MC_ERR(image == NULL)) {
            MC_TRACE("imgview_load_file: imgview_load_image_from_file() failed");
            return -1;
        }
    } else {
        image = NULL;
    }

    if(iv->image)
        gdix_DisposeImage(iv->image);
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

        iv->image = imgview_load_image_from_resource(cs->hInstance, cs->lpszName);

        /* Do not propagate cs->lpszName into WM_CREATE and WM_SETTEXT */
        cs->lpszName = NULL;
    }

    return iv;
}

static void
imgview_ncdestroy(imgview_t* iv)
{
    if(iv->image)
        gdix_DisposeImage(iv->image);
    free(iv);
}

static LRESULT CALLBACK
imgview_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    imgview_t* iv = (imgview_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(win, &ps);
            if(!iv->no_redraw)
                imgview_paint(iv, ps.hdc, &ps.rcPaint, ps.fErase);
            EndPaint(win, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            RECT rect;
            GetClientRect(win, &rect);
            imgview_paint(iv, (HDC) wp, &rect, TRUE);
            return 0;
        }

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
                RedrawWindow(win, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);

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
