/*
 * Copyright (c) 2019 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MC_XD2D_H
#define MC_XD2D_H

#include "misc.h"
#include "c-d2d1.h"


#define XD2D_COLOR_RGBA(r,g,b,a)    { (float)(r) / 255.0f, (float)(g) / 255.0f, (float)(b) / 255.0f, (a) / 255.0f };
#define XD2D_COLOR_RGB(r,g,b)       { (float)(r) / 255.0f, (float)(g) / 255.0f, (float)(b) / 255.0f, 1.0f };
#define XD2D_COLOR_CREFA(cref,a)    XD2D_COLOR_RGBA(GetRValue(cref), GetGValue(cref), GetBValue(cref), (a))
#define XD2D_COLOR_CREF(cref)       XD2D_COLOR_RGB(GetRValue(cref), GetGValue(cref), GetBValue(cref))


static inline void
xd2d_color_set_rgba(c_D2D1_COLOR_F* c, int r, int g, int b, int a)
{
    c->r = (float)r / 255.0f;
    c->g = (float)g / 255.0f;
    c->b = (float)b / 255.0f;
    c->a = (float)a / 255.0f;
}

static inline void
xd2d_color_set_rgb(c_D2D1_COLOR_F* c, int r, int g, int b)
{
    c->r = (float)r / 255.0f;
    c->g = (float)g / 255.0f;
    c->b = (float)b / 255.0f;
    c->a = 1.0f;
}

static inline void
xd2d_color_set_crefa(c_D2D1_COLOR_F* c, COLORREF cref, int a)
{
    c->r = (float)GetRValue(cref) / 255.0f;
    c->g = (float)GetGValue(cref) / 255.0f;
    c->b = (float)GetBValue(cref) / 255.0f;
    c->a = (float)a / 255.0f;
}

static inline void
xd2d_color_set_cref(c_D2D1_COLOR_F* c, COLORREF cref)
{
    c->r = (float)GetRValue(cref) / 255.0f;
    c->g = (float)GetGValue(cref) / 255.0f;
    c->b = (float)GetBValue(cref) / 255.0f;
    c->a = 1.0f;
}


/******************************
 ***   Direct2D factories   ***
 ******************************/

#define XD2D_FLAG_NOGDICOMPAT       0x0001

c_ID2D1HwndRenderTarget* xd2d_CreateHwndRenderTarget(HWND win, PAINTSTRUCT* ps, DWORD flags);
c_ID2D1DCRenderTarget* xd2d_CreateDCRenderTarget(HDC dc, const RECT* rect, DWORD flags);
c_ID2D1PathGeometry* xd2d_CreatePathGeometry(c_ID2D1GeometrySink** p_sink);


/************************************
 ***   Control message handlers   ***
 ************************************/

typedef struct xd2d_ctx_tag xd2d_ctx_t;
typedef struct xd2d_vtable_tag xd2d_vtable_t;

struct xd2d_ctx_tag {
    xd2d_vtable_t* vtable;
    HDC dc;
    c_ID2D1RenderTarget* rt;
    RECT dirty_rect;
    BOOL erase;
};

typedef xd2d_ctx_t* xd2d_cache_t;

struct xd2d_vtable_tag {
    size_t ctx_size;                            /* >= sizeof(xd2d_cache_t) */
    int (*fn_init_ctx)(xd2d_ctx_t* /*ctx*/);    /* Optional (if there is bigger wrapping struct) */
    void (*fn_fini_ctx)(xd2d_ctx_t* /*ctx*/);   /* Optional (if there is bigger wrapping struct) */
    void (*fn_paint)(void* /*ctrl*/, xd2d_ctx_t* /*ctx*/);
};

/* Initializer for simple draw context
 * (which is capable to cache only the canvas handle)
 *
 * If the particular control wants to cache more resources, it has to set
 * xd2d_vtable_t::ctx_size, fn_init_ctx and fn_fini_ctx accordingly to
 * describe, initialize and free some larger struct (but the larger struct
 * MUST begin with a member of xd2d_ctx_t type).
 */
#define XD2D_CTX_SIMPLE(fn_paint)      { sizeof(xd2d_ctx_t), NULL, NULL, (fn_paint) }


LRESULT xd2d_paint(HWND win, BOOL no_redraw, DWORD flags, xd2d_vtable_t* vtable,
                    void* ctrl, xd2d_cache_t* cache);

LRESULT xd2d_printclient(HWND win, HDC dc, DWORD flags, xd2d_vtable_t* vtable, void* ctrl);

void xd2d_free_cache(xd2d_cache_t* cache);
void xd2d_invalidate(HWND win, RECT* rect, BOOL erase, xd2d_cache_t* cache);


#endif  /* MC_XD2D_H */
