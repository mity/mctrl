/*
 * WinDrawLib
 * Copyright (c) 2015-2016 Martin Mitas
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

#include "misc.h"
#include "backend-d2d.h"
#include "backend-gdix.h"
#include "lock.h"


void
wdDrawArc(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r,
          float fBaseAngle, float fSweepAngle, float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(cx, cy, r, fBaseAngle, fSweepAngle, FALSE);
        if(g == NULL) {
            WD_TRACE("wdDrawArc: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float d = 2.0f * r;

        gdix_vtable->fn_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_vtable->fn_SetPenWidth(c->pen, fStrokeWidth);
        gdix_vtable->fn_DrawArc(c->graphics, c->pen, cx - r, cy - r, d, d,
                     fBaseAngle, fSweepAngle);
    }
}

void
wdDrawCircle(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r,
             float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        D2D1_ELLIPSE e = { { cx, cy }, r, r };

        ID2D1RenderTarget_DrawEllipse(c->target, &e, b, fStrokeWidth, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float d = 2.0f * r;

        gdix_vtable->fn_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_vtable->fn_SetPenWidth(c->pen, fStrokeWidth);
        gdix_vtable->fn_DrawEllipse(c->graphics, (void*) c->pen,
                cx - r, cy - r, d, d);
    }
}

void
wdDrawLine(WD_HCANVAS hCanvas, WD_HBRUSH hBrush,
           float x0, float y0, float x1, float y1, float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        D2D1_POINT_2F pt0 = { x0, y0 };
        D2D1_POINT_2F pt1 = { x1, y1 };

        ID2D1RenderTarget_DrawLine(c->target, pt0, pt1, b, fStrokeWidth, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;

        gdix_vtable->fn_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_vtable->fn_SetPenWidth(c->pen, fStrokeWidth);
        gdix_vtable->fn_DrawLine(c->graphics, c->pen, x0, y0, x1, y1);
    }
}

void
wdDrawPath(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath,
           float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Geometry* g = (ID2D1Geometry*) hPath;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;

        gdix_vtable->fn_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_vtable->fn_SetPenWidth(c->pen, fStrokeWidth);
        gdix_vtable->fn_DrawPath(c->graphics, (void*) c->pen, (void*) hPath);
    }
}

void
wdDrawPie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r,
                float fBaseAngle, float fSweepAngle, float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(cx, cy, r, fBaseAngle, fStrokeWidth, TRUE);
        if(g == NULL) {
            WD_TRACE("wdDrawPie: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float d = 2.0f * r;

        gdix_vtable->fn_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_vtable->fn_SetPenWidth(c->pen, fStrokeWidth);
        gdix_vtable->fn_DrawPie(c->graphics, c->pen, cx - r, cy - r, d, d,
                                fBaseAngle, fSweepAngle);
    }
}

void
wdDrawRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush,
           float x0, float y0, float x1, float y1, float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        D2D1_RECT_F r = { x0, y0, x1, y1 };

        ID2D1RenderTarget_DrawRectangle(c->target, &r, b, fStrokeWidth, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float tmp;

        /* Make sure x0 <= x1 and y0 <= y1. */
        if(x0 > x1) { tmp = x0; x0 = x1; x1 = tmp; }
        if(y0 > y1) { tmp = y0; y0 = y1; y1 = tmp; }

        gdix_vtable->fn_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_vtable->fn_SetPenWidth(c->pen, fStrokeWidth);
        gdix_vtable->fn_DrawRectangle(c->graphics, c->pen, x0, y0, x1 - x0, y1 - y0);
    }
}

