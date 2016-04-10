/*
 * Copyright (c) 2015-2016 Martin Mitas
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

#include "misc.h"
#include "backend-d2d.h"
#include "backend-gdix.h"
#include "lock.h"


WD_HCANVAS
wdCreateCanvasWithPaintStruct(HWND hWnd, PAINTSTRUCT* pPS, DWORD dwFlags)
{
    if(d2d_enabled()) {
        D2D1_RENDER_TARGET_PROPERTIES props = {
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            0.0f, 0.0f,
            ((dwFlags & WD_CANVAS_NOGDICOMPAT) ?
                        0 : D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE),
            D2D1_FEATURE_LEVEL_DEFAULT
        };
        D2D1_HWND_RENDER_TARGET_PROPERTIES props2;
        d2d_canvas_t* c;
        RECT rect;
        ID2D1HwndRenderTarget* target;
        HRESULT hr;

        GetClientRect(hWnd, &rect);

        props2.hwnd = hWnd;
        props2.pixelSize.width = rect.right - rect.left;
        props2.pixelSize.height = rect.bottom - rect.top;
        props2.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

        wd_lock();
        /* Note ID2D1HwndRenderTarget is implicitly double-buffered. */
        hr = ID2D1Factory_CreateHwndRenderTarget(d2d_factory, &props, &props2, &target);
        wd_unlock();
        if(FAILED(hr)) {
            WD_TRACE_HR("wdCreateCanvasWithPaintStruct: "
                        "ID2D1Factory::CreateHwndRenderTarget() failed.");
            return NULL;
        }

        c = d2d_canvas_alloc((ID2D1RenderTarget*)target, D2D_CANVASTYPE_HWND);
        if(c == NULL) {
            WD_TRACE("wdCreateCanvasWithPaintStruct: d2d_canvas_alloc() failed.");
            ID2D1RenderTarget_Release((ID2D1RenderTarget*)target);
            return NULL;
        }

        return (WD_HCANVAS) c;
    } else {
        BOOL use_doublebuffer = (dwFlags & WD_CANVAS_DOUBLEBUFFER);
        gdix_canvas_t* c;

        c = gdix_canvas_alloc(pPS->hdc, (use_doublebuffer ? &pPS->rcPaint : NULL));
        if(c == NULL) {
            WD_TRACE("wdCreateCanvasWithPaintStruct: gdix_canvas_alloc() failed.");
            return NULL;
        }
        return (WD_HCANVAS) c;
    }
}

WD_HCANVAS
wdCreateCanvasWithHDC(HDC hDC, const RECT* pRect, DWORD dwFlags)
{
    if(d2d_enabled()) {
        D2D1_RENDER_TARGET_PROPERTIES props = {
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            0.0f, 0.0f,
            ((dwFlags & WD_CANVAS_NOGDICOMPAT) ?
                        0 : D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE),
            D2D1_FEATURE_LEVEL_DEFAULT
        };
        d2d_canvas_t* c;
        ID2D1DCRenderTarget* target;
        HRESULT hr;

        wd_lock();
        hr = ID2D1Factory_CreateDCRenderTarget(d2d_factory, &props, &target);
        wd_unlock();
        if(FAILED(hr)) {
            WD_TRACE_HR("wgCreateCanvasWithHDC: "
                        "ID2D1Factory::CreateDCRenderTarget() failed.");
            goto err_CreateDCRenderTarget;
        }

        hr = ID2D1DCRenderTarget_BindDC(target, hDC, pRect);
        if(FAILED(hr)) {
            WD_TRACE_HR("wgCreateCanvasWithHDC: "
                        "ID2D1Factory::BindDC() failed.");
            goto err_BindDC;
        }

        c = d2d_canvas_alloc((ID2D1RenderTarget*)target, D2D_CANVASTYPE_DC);
        if(c == NULL) {
            WD_TRACE("wgCreateCanvasWithHDC: d2d_canvas_alloc() failed.");
            goto err_d2d_canvas_alloc;
        }

        return (WD_HCANVAS) c;

err_d2d_canvas_alloc:
err_BindDC:
        ID2D1RenderTarget_Release((ID2D1RenderTarget*)target);
err_CreateDCRenderTarget:
        return NULL;
    } else {
        BOOL use_doublebuffer = (dwFlags & WD_CANVAS_DOUBLEBUFFER);
        gdix_canvas_t* c;

        c = gdix_canvas_alloc(hDC, (use_doublebuffer ? pRect : NULL));
        if(c == NULL) {
            WD_TRACE("wdCreateCanvasWithHDC: gdix_canvas_alloc() failed.");
            return NULL;
        }
        return (WD_HCANVAS) c;
    }
}

void
wdDestroyCanvas(WD_HCANVAS hCanvas)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;

        /* Check for common logical errors. */
        if(c->clip_layer != NULL  ||  (c->flags & D2D_CANVASFLAG_RECTCLIP))
            WD_TRACE("wdDestroyCanvas: Logical error: Canvas has dangling clip.");
        if(c->gdi_interop != NULL)
            WD_TRACE("wdDestroyCanvas: Logical error: Unpaired wdStartGdi()/wdEndGdi().");

        ID2D1RenderTarget_Release(c->target);
        free(c);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;

        gdix_DeleteStringFormat(c->string_format);
        gdix_DeletePen(c->pen);
        gdix_DeleteGraphics(c->graphics);

        if(c->real_dc != NULL) {
            HBITMAP mem_bmp;

            mem_bmp = SelectObject(c->dc, c->orig_bmp);
            DeleteObject(mem_bmp);
            DeleteObject(c->dc);
        }

        free(c);
    }
}

void
wdBeginPaint(WD_HCANVAS hCanvas)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1RenderTarget_BeginDraw(c->target);
    } else {
        /* noop */
    }
}

BOOL
wdEndPaint(WD_HCANVAS hCanvas)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        HRESULT hr;

        d2d_reset_clip(c);

        hr = ID2D1RenderTarget_EndDraw(c->target, NULL, NULL);
        if(FAILED(hr)) {
            if(hr != D2DERR_RECREATE_TARGET)
                WD_TRACE_HR("wdEndPaint: ID2D1RenderTarget::EndDraw() failed.");
            return FALSE;
        }
        return TRUE;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;

        /* If double-buffering, blit the memory DC to the display DC. */
        if(c->real_dc != NULL)
            BitBlt(c->real_dc, c->x, c->y, c->cx, c->cy, c->dc, 0, 0, SRCCOPY);

        /* For GDI+, disable caching. */
        return FALSE;
    }
}

BOOL
wdResizeCanvas(WD_HCANVAS hCanvas, UINT uWidth, UINT uHeight)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        if(c->type == D2D_CANVASTYPE_HWND) {
            D2D1_SIZE_U size = { uWidth, uHeight };
            HRESULT hr;

            hr = ID2D1HwndRenderTarget_Resize(c->hwnd_target, &size);
            if(FAILED(hr)) {
                WD_TRACE_HR("wdResizeCanvas: "
                            "ID2D1HwndRenderTarget_Resize() failed.");
                return FALSE;
            }
            return TRUE;
        } else {
            /* Operation not supported. */
            WD_TRACE("wdResizeCanvas: Not supported (not ID2D1HwndRenderTarget).");
            return FALSE;
        }
    } else {
        /* Actually we should never be here as GDI+ back-end never allows
         * caching of the canvas so there is no need to ever resize it. */
        WD_TRACE("wdResizeCanvas: Not supported (GDI+ back-end).");
        return FALSE;
    }
}

HDC
wdStartGdi(WD_HCANVAS hCanvas, BOOL bKeepContents)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1GdiInteropRenderTarget* gdi_interop;
        D2D1_DC_INITIALIZE_MODE init_mode;
        HRESULT hr;
        HDC dc;

        hr = ID2D1RenderTarget_QueryInterface(c->target,
                    &IID_ID2D1GdiInteropRenderTarget, (void**) &gdi_interop);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdStartGdi: ID2D1RenderTarget::"
                        "QueryInterface(IID_ID2D1GdiInteropRenderTarget) failed.");
            return NULL;
        }

        init_mode = (bKeepContents ? D2D1_DC_INITIALIZE_MODE_COPY
                        : D2D1_DC_INITIALIZE_MODE_CLEAR);
        hr = ID2D1GdiInteropRenderTarget_GetDC(gdi_interop, init_mode, &dc);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdStartGdi: ID2D1GdiInteropRenderTarget::GetDC() failed.");
            ID2D1GdiInteropRenderTarget_Release(gdi_interop);
            return NULL;
        }

        c->gdi_interop = gdi_interop;
        return dc;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        int status;
        HDC dc;

        status = gdix_GetDC(c->graphics, &dc);
        if(status != 0) {
            WD_TRACE_ERR_("wdStartGdi: GdipGetDC() failed.", status);
            return NULL;
        }

        return dc;
    }
}

void
wdEndGdi(WD_HCANVAS hCanvas, HDC hDC)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;

        ID2D1GdiInteropRenderTarget_ReleaseDC(c->gdi_interop, NULL);
        ID2D1GdiInteropRenderTarget_Release(c->gdi_interop);
        c->gdi_interop = NULL;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        gdix_ReleaseDC(c->graphics, hDC);
    }
}

void
wdClear(WD_HCANVAS hCanvas, WD_COLOR color)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        D2D_COLOR_F clr;

        d2d_init_color(&clr, color);
        ID2D1RenderTarget_Clear(c->target, &clr);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        gdix_GraphicsClear(c->graphics, color);
    }
}

void
wdSetClip(WD_HCANVAS hCanvas, const WD_RECT* pRect, const WD_HPATH hPath)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;

        d2d_reset_clip(c);

        if(hPath != NULL) {
            ID2D1PathGeometry* g = (ID2D1PathGeometry*) hPath;
            D2D1_LAYER_PARAMETERS layer_params;
            HRESULT hr;

            hr = ID2D1RenderTarget_CreateLayer(c->target, NULL, &c->clip_layer);
            if(FAILED(hr)) {
                WD_TRACE_HR("wdSetClip: ID2D1RenderTarget::CreateLayer() failed.");
                return;
            }

            if(pRect != NULL) {
                layer_params.contentBounds.left = pRect->x0;
                layer_params.contentBounds.top = pRect->y0;
                layer_params.contentBounds.right = pRect->x1;
                layer_params.contentBounds.bottom = pRect->y1;
            } else {
                layer_params.contentBounds.left = FLT_MIN;
                layer_params.contentBounds.top = FLT_MIN;
                layer_params.contentBounds.right = FLT_MAX;
                layer_params.contentBounds.bottom = FLT_MAX;
            }
            layer_params.geometricMask = (ID2D1Geometry*) g;
            layer_params.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
            layer_params.maskTransform._11 = 1.0f;
            layer_params.maskTransform._12 = 0.0f;
            layer_params.maskTransform._21 = 0.0f;
            layer_params.maskTransform._22 = 1.0f;
            layer_params.maskTransform._31 = 0.0f;
            layer_params.maskTransform._32 = 0.0f;
            layer_params.opacity = 1.0f;
            layer_params.opacityBrush = NULL;
            layer_params.layerOptions = D2D1_LAYER_OPTIONS_NONE;

            ID2D1RenderTarget_PushLayer(c->target, &layer_params, c->clip_layer);
        } else if(pRect != NULL) {
            ID2D1RenderTarget_PushAxisAlignedClip(c->target,
                    (const D2D1_RECT_F*) pRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            c->flags |= D2D_CANVASFLAG_RECTCLIP;
        }
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        int mode;

        if(pRect == NULL  &&  hPath == NULL) {
            gdix_ResetClip(c->graphics);
            return;
        }

        mode = dummy_CombineModeReplace;

        if(pRect != NULL) {
            gdix_SetClipRect(c->graphics, pRect->x0, pRect->y0,
                             pRect->x1, pRect->y1, mode);
            mode = dummy_CombineModeIntersect;
        }

        if(hPath != NULL)
            gdix_SetClipPath(c->graphics, (void*) hPath, mode);
    }
}

void
wdRotateWorld(WD_HCANVAS hCanvas, float cx, float cy, float fAngle)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        D2D1_MATRIX_3X2_F m;
        float a_rads = fAngle * (WD_PI / 180.0f);
        float a_sin = sinf(a_rads);
        float a_cos = cosf(a_rads);

        m._11 = a_cos;
        m._12 = a_sin;
        m._21 = -a_sin;
        m._22 = a_cos;
        m._31 = cx - cx*a_cos + cy*a_sin;
        m._32 = cy - cx*a_sin - cy*a_cos;

        d2d_apply_transform(c->target, &m);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;

        gdix_TranslateWorldTransform(c->graphics, -cx, -cy, dummy_MatrixOrderAppend);
        gdix_RotateWorldTransform(c->graphics, fAngle, dummy_MatrixOrderAppend);
        gdix_TranslateWorldTransform(c->graphics, cx, cy, dummy_MatrixOrderAppend);
    }
}

void
wdTranslateWorld(WD_HCANVAS hCanvas, float dx, float dy)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        D2D1_MATRIX_3X2_F matrix;

        ID2D1RenderTarget_GetTransform(c->target, &matrix);
        matrix._31 += dx;
        matrix._32 += dy;
        ID2D1RenderTarget_SetTransform(c->target, &matrix);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        gdix_TranslateWorldTransform(c->graphics, dx, dy, dummy_MatrixOrderAppend);
    }
}

void
wdResetWorld(WD_HCANVAS hCanvas)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        d2d_reset_transform(c->target);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        gdix_ResetWorldTransform(c->graphics);
    }
}

