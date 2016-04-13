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


WD_HBRUSH
wdCreateSolidBrush(WD_HCANVAS hCanvas, WD_COLOR color)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1SolidColorBrush* b;
        D2D_COLOR_F clr;
        HRESULT hr;

        d2d_init_color(&clr, color);
        hr = ID2D1RenderTarget_CreateSolidColorBrush(
                        c->target, &clr, NULL, &b);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdCreateSolidBrush: "
                        "ID2D1RenderTarget::CreateSolidColorBrush() failed.");
            return NULL;
        }
        return (WD_HBRUSH) b;
    } else {
        dummy_GpSolidFill* b;
        int status;

        status = gdix_vtable->fn_CreateSolidFill(color, &b);
        if(status != 0) {
            WD_TRACE("wdCreateSolidBrush: "
                     "GdipCreateSolidFill() failed. [%d]", status);
            return NULL;
        }
        return (WD_HBRUSH) b;
    }
}

void
wdDestroyBrush(WD_HBRUSH hBrush)
{
    if(d2d_enabled()) {
        ID2D1Brush_Release((ID2D1Brush*) hBrush);
    } else {
        gdix_vtable->fn_DeleteBrush((void*) hBrush);
    }
}

void
wdSetSolidBrushColor(WD_HBRUSH hBrush, WD_COLOR color)
{
    if(d2d_enabled()) {
        D2D_COLOR_F clr;

        d2d_init_color(&clr, color);
        ID2D1SolidColorBrush_SetColor((ID2D1SolidColorBrush*) hBrush, &clr);
    } else {
        dummy_GpSolidFill* b = (dummy_GpSolidFill*) hBrush;

        gdix_vtable->fn_SetSolidFillColor(b, (dummy_ARGB) color);
    }
}
