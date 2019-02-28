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

#include "xdraw.h"


LRESULT
xdraw_paint(HWND win, BOOL no_redraw, DWORD flags,
            xdraw_vtable_t* vtable, void* ctrl, xdraw_cache_t* cache)
{
    PAINTSTRUCT ps;
    xdraw_ctx_t* ctx;
    WD_HCANVAS canvas;
    BOOL is_cache_allowed;

    BeginPaint(win, &ps);
    if(no_redraw)
        goto no_paint;

    /* Make sure we have the context with canvas handle and, potentially,
     * other WDL resources. */
    if(cache != NULL  &&  *cache != NULL) {
        /* We already have a cached context. */
        ctx = *cache;
    } else {
        /* We need to initialize new (cachable) context. */
        ctx = (xdraw_ctx_t*) malloc(vtable->ctx_size);
        if(MC_ERR(ctx == NULL)) {
            MC_TRACE("xdraw_paint: malloc() failed.");
            goto no_paint;
        }

        canvas = wdCreateCanvasWithPaintStruct(win, &ps, flags);
        if(MC_ERR(canvas == NULL)) {
            MC_TRACE_ERR("xdraw_paint: wdCreateCanvasWithPaintStruct() failed.");
            free(ctx);
            goto no_paint;
        }

        if(vtable->fn_init_ctx != NULL) {
            if(MC_ERR(vtable->fn_init_ctx(ctx) != 0)) {
                MC_TRACE_ERR("xdraw_paint: fn_init_ctx() failed.");
                wdDestroyCanvas(canvas);
                free(ctx);
                goto no_paint;
            }
        }

        ctx->canvas = canvas;
        GetClientRect(win, &ctx->dirty_rect);
        ctx->erase = TRUE;
        ctx->vtable = vtable;

        if(cache != NULL)
            *cache = ctx;
    }

    /* Do the painting. */
    wdBeginPaint(ctx->canvas);
    if(!mc_rect_is_empty(&ctx->dirty_rect))
        vtable->fn_paint(ctrl, ctx);
    is_cache_allowed = wdEndPaint(ctx->canvas);

    /* If possible, store the context in the cache, or destroy it. */
    if(is_cache_allowed  &&  cache != NULL) {
        *cache = ctx;
        ctx->erase = FALSE;
        mc_rect_set(&ctx->dirty_rect, 0, 0, 0, 0);
    } else {
        if(vtable->fn_fini_ctx != NULL)
            vtable->fn_fini_ctx(ctx);
        wdDestroyCanvas(ctx->canvas);

        free(ctx);

        if(cache != NULL)
            *cache = NULL;
    }

no_paint:
    EndPaint(win, &ps);
    return 0;
}

void
xdraw_free_cache(xdraw_cache_t* cache)
{
    xdraw_ctx_t* ctx;

    if(*cache == NULL)
        return;

    ctx = *cache;

    if(ctx->vtable->fn_fini_ctx != NULL)
        ctx->vtable->fn_fini_ctx(ctx);
    wdDestroyCanvas(ctx->canvas);
    free(ctx);

    *cache = NULL;
}

void
xdraw_invalidate(HWND win, RECT* rect, BOOL erase, xdraw_cache_t* cache)
{
    InvalidateRect(win, rect, erase);

    if(cache != NULL  &&  *cache != NULL) {
        xdraw_ctx_t* ctx = *cache;

        if(rect == NULL) {
            GetClientRect(win, &ctx->dirty_rect);
        } else if(mc_rect_is_empty(&ctx->dirty_rect)) {
            mc_rect_copy(&ctx->dirty_rect, rect);
        } else {
            ctx->dirty_rect.left = MC_MIN(ctx->dirty_rect.left, rect->left);
            ctx->dirty_rect.top = MC_MIN(ctx->dirty_rect.top, rect->top);
            ctx->dirty_rect.right = MC_MAX(ctx->dirty_rect.right, rect->right);
            ctx->dirty_rect.bottom = MC_MAX(ctx->dirty_rect.bottom, rect->bottom);
        }

        if(erase)
            ctx->erase = TRUE;
    }
}

LRESULT
xdraw_printclient(HWND win, HDC dc, DWORD flags,
            xdraw_vtable_t* vtable, void* ctrl)
{
    xdraw_ctx_t* ctx;

    flags &= ~WD_CANVAS_DOUBLEBUFFER;

    /* Initialize temp. context. */
    ctx = (xdraw_ctx_t*) _alloca(vtable->ctx_size);
    GetClientRect(win, &ctx->dirty_rect);
    ctx->canvas = wdCreateCanvasWithHDC(dc, &ctx->dirty_rect, flags);
    ctx->erase = TRUE;
    if(vtable->fn_init_ctx != NULL) {
        if(MC_ERR(vtable->fn_init_ctx(ctx) != 0)) {
            MC_TRACE_ERR("xdraw_printclient: fn_init_ctx() failed.");
            goto err_init_ctx;
        }
    }

    /* Do the painting. */
    wdBeginPaint(ctx->canvas);
    vtable->fn_paint(ctrl, ctx);
    wdEndPaint(ctx->canvas);

    /* Destroy the context */
    if(vtable->fn_fini_ctx != NULL)
        vtable->fn_fini_ctx(ctx);
err_init_ctx:
    wdDestroyCanvas(ctx->canvas);
    return 0;
}
