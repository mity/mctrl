/*
 * Copyright (c) 2015-2019 Martin Mitas
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

#include "xd2d.h"


/******************************
 ***   Direct2D factories   ***
 ******************************/

static mc_mutex_t xd2d_mutex = SRWLOCK_INIT;
static HMODULE xd2d_dll;
static c_ID2D1Factory* xd2d_factory;


static inline void
xd2d_lock(void)
{
    mc_mutex_lock(&xd2d_mutex);
}

static inline void
xd2d_unlock(void)
{
    mc_mutex_unlock(&xd2d_mutex);
}


static void
xd2d_setup_props(c_D2D1_RENDER_TARGET_PROPERTIES* props, DWORD flags)
{
    props->type = c_D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props->pixelFormat.format = c_DXGI_FORMAT_B8G8R8A8_UNORM;
    props->pixelFormat.alphaMode = c_D2D1_ALPHA_MODE_PREMULTIPLIED;
    /* We want to use raw pixels as units. Direct2D by default works with DIPs
     * ("device independent pixels") which map 1:1 to physical pixels when DPI
     * is 96. So we enforce the render target to think we have this DPI. */
    props->dpiX = 96.0f;
    props->dpiY = 96.0f;
    props->usage = (flags & XD2D_FLAG_NOGDICOMPAT) ? 0 : c_D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
    props->minLevel = c_D2D1_FEATURE_LEVEL_DEFAULT;
}


c_ID2D1HwndRenderTarget*
xd2d_CreateHwndRenderTarget(HWND win, PAINTSTRUCT* ps, DWORD flags)
{
    RECT client;
    c_D2D1_RENDER_TARGET_PROPERTIES props;
    c_D2D1_HWND_RENDER_TARGET_PROPERTIES props2;
    c_ID2D1HwndRenderTarget* target;
    HRESULT hr;

    GetClientRect(win, &client);

    xd2d_setup_props(&props, flags);

    props2.hwnd = win;
    props2.pixelSize.width = mc_width(&client);
    props2.pixelSize.height = mc_height(&client);
    props2.presentOptions = c_D2D1_PRESENT_OPTIONS_NONE;

    xd2d_lock();
    hr = c_ID2D1Factory_CreateHwndRenderTarget(xd2d_factory, &props, &props2, &target);
    xd2d_unlock();
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xd2d_CreateHwndRenderTarget: ID2D1Factory::CreateHwndRenderTarget() failed.");
        goto err_CreateHwndRenderTarget;
    }

    /* Success. */
    return target;

    /* Error unrolling. */
err_CreateHwndRenderTarget:
    return NULL;
}

c_ID2D1DCRenderTarget*
xd2d_CreateDCRenderTarget(HDC dc, const RECT* rect, DWORD flags)
{
    c_D2D1_RENDER_TARGET_PROPERTIES props;
    c_ID2D1DCRenderTarget* target;
    HRESULT hr;

    xd2d_setup_props(&props, flags);

    xd2d_lock();
    hr = c_ID2D1Factory_CreateDCRenderTarget(xd2d_factory, &props, &target);
    xd2d_unlock();
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xd2d_CreateDCRenderTarget: ID2D1Factory::CreateDCRenderTarget() failed.");
        goto err_CreateDCRenderTarget;
    }

    hr = c_ID2D1DCRenderTarget_BindDC(target, dc, rect);
    if(FAILED(hr)) {
        MC_TRACE_HR("xd2d_CreateDCRenderTarget: ID2D1DCRenderTarget::BindDC() failed.");
        goto err_BindDC;
    }

    /* Success. */
    return target;

    /* Error unrolling. */
err_BindDC:
    c_ID2D1DCRenderTarget_Release(target);
err_CreateDCRenderTarget:
    return NULL;
}


int
xd2d_init_module(void)
{
    static const c_D2D1_FACTORY_OPTIONS factory_options = { c_D2D1_DEBUG_LEVEL_NONE };
    HRESULT (WINAPI* fn_D2D1CreateFactory)(c_D2D1_FACTORY_TYPE, REFIID,
                const c_D2D1_FACTORY_OPTIONS*, void**);
    HRESULT hr;

    xd2d_dll = mc_load_sys_dll(_T("D2D1.DLL"));
    if(MC_ERR(xd2d_dll == NULL)) {
        MC_TRACE_ERR("xd2d_init_module: mc_load_sys_dll(D2D1.DLL) failed.");
        goto err_LoadLibrary;
    }

    fn_D2D1CreateFactory = (HRESULT (WINAPI*)(c_D2D1_FACTORY_TYPE, REFIID,
                                        const c_D2D1_FACTORY_OPTIONS*, void**))
                GetProcAddress(xd2d_dll, "D2D1CreateFactory");
    if(MC_ERR(fn_D2D1CreateFactory == NULL)) {
        MC_TRACE_ERR("xd2d_init_module: GetProcAddress(D2D1CreateFactory) failed.");
        goto err_GetProcAddress;
    }

    /* Create D2D factory object. Note we use D2D1_FACTORY_TYPE_SINGLE_THREADED
     * for performance reasons (we want the objects it creates to not synchronize
     * as those are only used from control that creates them.
     *
     * The downside is we need to manually synchronize calls to the factory
     * itself. */
    hr = fn_D2D1CreateFactory(c_D2D1_FACTORY_TYPE_SINGLE_THREADED,
                &c_IID_ID2D1Factory, &factory_options, (void**) &xd2d_factory);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xd2d_init_module: D2D1CreateFactory() failed.");
        goto err_CreateFactory;
    }

    return 0;

err_CreateFactory:
err_GetProcAddress:
    FreeLibrary(xd2d_dll);
err_LoadLibrary:
    return -1;
}

void
xd2d_fini_module(void)
{
    c_ID2D1Factory_Release(xd2d_factory);
    FreeLibrary(xd2d_dll);
}


/************************************
 ***   Control message handlers   ***
 ************************************/

LRESULT
xd2d_paint(HWND win, BOOL no_redraw, DWORD flags,
           xd2d_vtable_t* vtable, void* ctrl, xd2d_cache_t* cache)
{
    PAINTSTRUCT ps;
    xd2d_ctx_t* ctx;
    c_ID2D1HwndRenderTarget* rt;

    BeginPaint(win, &ps);
    if(no_redraw)
        goto no_paint;

    /* Make sure we have the drawing context. */
    if(cache != NULL  &&  *cache != NULL) {
        /* We already have a cached context. */
        ctx = *cache;
        ctx->dc = ps.hdc;
    } else {
        /* We need to initialize new context. */
        ctx = (xd2d_ctx_t*) malloc(vtable->ctx_size);
        if(MC_ERR(ctx == NULL)) {
            MC_TRACE("xd2d_paint: malloc() failed.");
            goto no_paint;
        }

        rt = xd2d_CreateHwndRenderTarget(win, &ps, flags);
        if(MC_ERR(rt == NULL)) {
            MC_TRACE_ERR("xd2d_paint: xd2d_CreateHwndRenderTarget() failed.");
            free(ctx);
            goto no_paint;
        }

        ctx->rt = (c_ID2D1RenderTarget*) rt;
        GetClientRect(win, &ctx->dirty_rect);   // FIXME?
        ctx->erase = TRUE;
        ctx->vtable = vtable;
        ctx->dc = ps.hdc;

        if(vtable->fn_init_ctx != NULL) {
            if(MC_ERR(vtable->fn_init_ctx(ctx) != 0)) {
                MC_TRACE_ERR("xd2d_paint: fn_init_ctx() failed.");
                c_ID2D1HwndRenderTarget_Release(rt);
                free(ctx);
                goto no_paint;
            }
        }
    }

    /* Do the painting. */
    c_ID2D1RenderTarget_BeginDraw(ctx->rt);
    if(!mc_rect_is_empty(&ctx->dirty_rect))
        vtable->fn_paint(ctrl, ctx);
    c_ID2D1RenderTarget_EndDraw(ctx->rt, NULL, NULL);

    /* If possible, store the context in the cache, or destroy it. */
    if(cache != NULL) {
        *cache = ctx;
        ctx->erase = FALSE;
        mc_rect_set(&ctx->dirty_rect, 0, 0, 0, 0);
    } else {
        if(vtable->fn_fini_ctx != NULL)
            vtable->fn_fini_ctx(ctx);
        c_ID2D1HwndRenderTarget_Release(rt);
        free(ctx);
    }

no_paint:
    EndPaint(win, &ps);
    return 0;
}

void
xd2d_free_cache(xd2d_cache_t* cache)
{
    xd2d_ctx_t* ctx;

    if(*cache == NULL)
        return;

    ctx = *cache;

    if(ctx->vtable->fn_fini_ctx != NULL)
        ctx->vtable->fn_fini_ctx(ctx);
    c_ID2D1RenderTarget_Release(ctx->rt);
    free(ctx);

    *cache = NULL;
}

void
xd2d_invalidate(HWND win, RECT* rect, BOOL erase, xd2d_cache_t* cache)
{
    InvalidateRect(win, rect, erase);

    if(cache != NULL  &&  *cache != NULL) {
        xd2d_ctx_t* ctx = *cache;

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

        ctx->erase |= erase;
    }
}

LRESULT
xd2d_printclient(HWND win, HDC dc, DWORD flags, xd2d_vtable_t* vtable, void* ctrl)
{
    xd2d_ctx_t* ctx;

    /* Initialize temp. context. */
    ctx = (xd2d_ctx_t*) _alloca(vtable->ctx_size);
    GetClientRect(win, &ctx->dirty_rect);
    ctx->rt = (c_ID2D1RenderTarget*) xd2d_CreateDCRenderTarget(dc, &ctx->dirty_rect, flags);
    if(MC_ERR(ctx->rt == NULL)) {
        MC_TRACE("xd2d_printclient: xd2d_CreateDCRenderTarget() failed.");
        goto err_CreateDCRenderTarget;
    }
    ctx->erase = TRUE;
    ctx->dc = dc;
    if(vtable->fn_init_ctx != NULL) {
        if(MC_ERR(vtable->fn_init_ctx(ctx) != 0)) {
            MC_TRACE_ERR("xd2d_printclient: fn_init_ctx() failed.");
            goto err_init_ctx;
        }
    }

    /* Do the painting. */
    vtable->fn_paint(ctrl, ctx);

    /* Destroy the context */
    if(vtable->fn_fini_ctx != NULL)
        vtable->fn_fini_ctx(ctx);
err_init_ctx:
    c_ID2D1RenderTarget_Release(ctx->rt);
err_CreateDCRenderTarget:
    return 0;
}

