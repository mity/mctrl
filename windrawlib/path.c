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


WD_HPATH
wdCreatePath(WD_HCANVAS hCanvas)
{
    if(d2d_enabled()) {
        ID2D1PathGeometry* g;
        HRESULT hr;

        wd_lock();
        hr = ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
        wd_unlock();
        if(FAILED(hr)) {
            WD_TRACE_HR("wdCreatePath: "
                        "ID2D1Factory::CreatePathGeometry() failed.");
            return NULL;
        }

        return (WD_HPATH) g;
    } else {
        dummy_GpPath* p;
        int status;

        status = gdix_CreatePath(dummy_FillModeAlternate, &p);
        if(status != 0) {
            WD_TRACE("wdCreatePath: GdipCreatePath() failed. [%d]", status);
            return NULL;
        }

        return (WD_HPATH) p;
    }
}

WD_HPATH
wdCreatePolygonPath(WD_HCANVAS hCanvas, const WD_POINT* pPoints, UINT uCount)
{
    WD_HPATH p;

    p = wdCreatePath(hCanvas);
    if(p == NULL) {
        WD_TRACE("wdCreatePolygonPath: wdCreatePath() failed.");
        return NULL;
    }

    if(uCount > 0) {
        WD_PATHSINK sink;
        UINT i;

        if(!wdOpenPathSink(&sink, p)) {
            WD_TRACE("wdCreatePolygonPath: wdOpenPathSink() failed.");
            wdDestroyPath(p);
            return NULL;
        }

        wdBeginFigure(&sink, &pPoints[0]);
        for(i = 1; i < uCount; i++)
            wdAddLine(&sink, &pPoints[i]);
        wdEndFigure(&sink, TRUE);

        wdClosePathSink(&sink);
    }

    return p;
}

void
wdDestroyPath(WD_HPATH hPath)
{
    if(d2d_enabled()) {
        ID2D1PathGeometry_Release((ID2D1PathGeometry*) hPath);
    } else {
        gdix_DeletePath((dummy_GpPath*) hPath);
    }
}

BOOL
wdOpenPathSink(WD_PATHSINK* pSink, WD_HPATH hPath)
{
    if(d2d_enabled()) {
        ID2D1PathGeometry* g = (ID2D1PathGeometry*) hPath;
        ID2D1GeometrySink* s;
        HRESULT hr;

        hr = ID2D1PathGeometry_Open(g, &s);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdOpenPathSink: ID2D1PathGeometry::Open() failed.");
            return FALSE;
        }

        pSink->pData = (void*) s;
        return TRUE;
    } else {
        /* GDI+ does no concept of path sink as Direct2D does, it operates
         * directly with the path object. */
        pSink->pData = (void*) hPath;
        return TRUE;
    }
}

void
wdClosePathSink(WD_PATHSINK* pSink)
{
    if(d2d_enabled()) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) pSink->pData;
        ID2D1GeometrySink_Close(s);
        ID2D1GeometrySink_Release(s);
    } else {
        /* noop */
    }
}

void
wdBeginFigure(WD_PATHSINK* pSink, const WD_POINT* pStartPoint)
{
    if(d2d_enabled()) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) pSink->pData;
        ID2D1GeometrySink_BeginFigure(s, *((D2D1_POINT_2F*) pStartPoint),
                                      D2D1_FIGURE_BEGIN_FILLED);
    } else {
        gdix_StartPathFigure(pSink->pData);
    }

    pSink->ptEnd.x = pStartPoint->x;
    pSink->ptEnd.y = pStartPoint->y;
}

void
wdEndFigure(WD_PATHSINK* pSink, BOOL bCloseFigure)
{
    if(d2d_enabled()) {
        ID2D1GeometrySink_EndFigure((ID2D1GeometrySink*) pSink->pData,
                (bCloseFigure ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN));
    } else {
        if(bCloseFigure)
            gdix_ClosePathFigure(pSink->pData);
    }
}

void
wdAddLine(WD_PATHSINK* pSink, const WD_POINT* pEndPoint)
{
    if(d2d_enabled()) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) pSink->pData;
        ID2D1GeometrySink_AddLine(s, *((D2D1_POINT_2F*) pEndPoint));
    } else {
        gdix_AddPathLine(pSink->pData, pSink->ptEnd.x, pSink->ptEnd.y,
                         pEndPoint->x, pEndPoint->y);
    }

    pSink->ptEnd.x = pEndPoint->x;
    pSink->ptEnd.y = pEndPoint->y;
}

void
wdAddArc(WD_PATHSINK* pSink, const WD_POINT* pCenter, float fSweepAngle)
{
    float cx = pCenter->x;
    float cy = pCenter->y;
    float ax = pSink->ptEnd.x;
    float ay = pSink->ptEnd.y;
    float xdiff = ax - cx;
    float ydiff = ay - cy;
    float r;
    float base_angle;

    r = sqrtf(xdiff * xdiff + ydiff * ydiff);

    /* Avoid undefined case for atan2f(). */
    if(r < 0.001f)
        return;

    base_angle = atan2f(ydiff, xdiff) * (180.0f / WD_PI);

    if(d2d_enabled()) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) pSink->pData;
        WD_CIRCLE circle = { cx, cy, r };
        D2D1_ARC_SEGMENT arc_seg;

        d2d_setup_arc_segment(&arc_seg, &circle, base_angle, fSweepAngle);
        ID2D1GeometrySink_AddArc(s, &arc_seg);
        pSink->ptEnd.x = arc_seg.point.x;
        pSink->ptEnd.y = arc_seg.point.y;
    } else {
        float d = 2.0f * r;
        float sweep_rads = (base_angle + fSweepAngle) * (WD_PI / 180.0f);

        gdix_AddPathArc(pSink->pData, cx - r, cy - r, d, d, base_angle, fSweepAngle);
        pSink->ptEnd.x = cx + r * cosf(sweep_rads);
        pSink->ptEnd.y = cy + r * sinf(sweep_rads);
    }
}

