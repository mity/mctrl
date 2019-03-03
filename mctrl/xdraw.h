/*
 * Copyright (c) 2019 Martin Mitas
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

#ifndef MC_XDRAW_H
#define MC_XDRAW_H

#include "misc.h"
#include <wdl.h>


typedef struct xdraw_ctx_tag xdraw_ctx_t;
typedef struct xdraw_vtable_tag xdraw_vtable_t;

struct xdraw_ctx_tag {
    xdraw_vtable_t* vtable;
    HDC dc;
    WD_HCANVAS canvas;
    RECT dirty_rect;
    BOOL erase;
};

typedef xdraw_ctx_t* xdraw_cache_t;

struct xdraw_vtable_tag {
    size_t ctx_size;                            /* >= sizeof(xdraw_cache_t) */
    int (*fn_init_ctx)(xdraw_ctx_t* /*ctx*/);   /* Optional (if there is bigger wrapping struct) */
    void (*fn_fini_ctx)(xdraw_ctx_t* /*ctx*/);  /* Optional (if there is bigger wrapping struct) */
    void (*fn_paint)(void* /*ctrl*/, xdraw_ctx_t* /*ctx*/);
};

/* Initializer for simple draw context
 * (which is capable to cache only the canvas handle)
 *
 * If the particular control wants to cache more resources, it has to set
 * xdraw_vtable_t::ctx_size, fn_init_ctx and fn_fini_ctx accordingly to
 * describe, initialize and free some larger struct (but the larger struct
 * MUST begin with a member of xdraw_ctx_t type).
 */
#define XDRAW_CTX_SIMPLE(fn_paint)      { sizeof(xdraw_ctx_t), NULL, NULL, (fn_paint) }


LRESULT xdraw_paint(HWND win, BOOL no_redraw, DWORD flags,
            xdraw_vtable_t* vtable, void* ctrl, xdraw_cache_t* cache);
void xdraw_free_cache(xdraw_cache_t* cache);
void xdraw_invalidate(HWND win, RECT* rect, BOOL erase, xdraw_cache_t* cache);

LRESULT xdraw_printclient(HWND win, HDC dc, DWORD flags,
            xdraw_vtable_t* vtable, void* ctrl);


#endif  /* MC_XDRAW_H */
