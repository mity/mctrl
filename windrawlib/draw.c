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


void
wdDrawArc(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_CIRCLE* pCircle,
          float fBaseAngle, float fSweepAngle, float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(pCircle, fBaseAngle, fSweepAngle, FALSE);
        if(g == NULL) {
            WD_TRACE("wdDrawArc: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float d = 2.0f * pCircle->r;

        gdix_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_SetPenWidth(c->pen, fStrokeWidth);
        gdix_DrawArc(c->graphics, c->pen, pCircle->x - pCircle->r,
                     pCircle->y - pCircle->r, d, d, fBaseAngle, fSweepAngle);
    }
}

void
wdDrawLine(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_LINE* pLine,
           float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        D2D1_POINT_2F pt0 = { pLine->x0, pLine->y0 };
        D2D1_POINT_2F pt1 = { pLine->x1, pLine->y1 };

        ID2D1RenderTarget_DrawLine(c->target, pt0, pt1, b, fStrokeWidth, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;

        gdix_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_SetPenWidth(c->pen, fStrokeWidth);
        gdix_DrawLine(c->graphics, c->pen, pLine->x0, pLine->y0, pLine->x1, pLine->y1);
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

        gdix_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_SetPenWidth(c->pen, fStrokeWidth);
        gdix_DrawPath(c->graphics, (void*) c->pen, (void*) hPath);
    }
}

void
wdDrawPie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_CIRCLE* pCircle,
                float fBaseAngle, float fSweepAngle, float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(pCircle, fBaseAngle, fStrokeWidth, TRUE);
        if(g == NULL) {
            WD_TRACE("wdDrawPie: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float d = 2.0f * pCircle->r;

        gdix_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_SetPenWidth(c->pen, fStrokeWidth);
        gdix_DrawPie(c->graphics, c->pen, pCircle->x - pCircle->r,
                     pCircle->y - pCircle->r, d, d, fBaseAngle, fSweepAngle);
    }
}

void
wdDrawRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_RECT* pRect,
           float fStrokeWidth)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Brush* b = (ID2D1Brush*) hBrush;

        ID2D1RenderTarget_DrawRectangle(c->target, (const D2D1_RECT_F*) pRect,
                                        b, fStrokeWidth, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        float x0 = WD_MIN(pRect->x0, pRect->x1);
        float y0 = WD_MIN(pRect->y0, pRect->y1);
        float x1 = WD_MAX(pRect->x0, pRect->x1);
        float y1 = WD_MAX(pRect->y0, pRect->y1);

        gdix_SetPenBrushFill(c->pen, (void*) hBrush);
        gdix_SetPenWidth(c->pen, fStrokeWidth);
        gdix_DrawRectangle(c->graphics, c->pen, x0, y0, x1 - x0, y1 - y0);
    }
}

