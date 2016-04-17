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

#include "backend-d2d.h"
#include "lock.h"


static HMODULE d2d_dll = NULL;

ID2D1Factory* d2d_factory = NULL;


/* We want horizontal and vertical lines with non-fractional coordinates
 * and with stroke width 1.0 to really affect single line of pixels to match
 * GDI and GDI+. To achieve that, we need to setup our coordinate system
 * to match the pixel grid accordingly. */
const D2D1_MATRIX_3X2_F d2d_base_transform = {
    1.0f, 0.0f,
    0.0f, 1.0f,
    0.5f, 0.5f
};


int
d2d_init(void)
{
    static const D2D1_FACTORY_OPTIONS factory_options = { D2D1_DEBUG_LEVEL_NONE };
    HRESULT (WINAPI* fn_D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
    HRESULT hr;

    /* Load D2D1.DLL. */
    d2d_dll = wd_load_system_dll(_T("D2D1.DLL"));
    if(d2d_dll == NULL) {
        WD_TRACE_ERR("d2d_init: wd_load_system_dll(D2D1.DLL) failed.");
        goto err_LoadLibrary;
    }
    fn_D2D1CreateFactory = (HRESULT (WINAPI*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**))
                GetProcAddress(d2d_dll, "D2D1CreateFactory");
    if(fn_D2D1CreateFactory == NULL) {
        WD_TRACE_ERR("d2d_init: GetProcAddress(D2D1CreateFactory) failed.");
        goto err_GetProcAddress;
    }

    /* Create D2D factory object. Note we use D2D1_FACTORY_TYPE_SINGLE_THREADED
     * for performance reasons and manually synchronize calls to the factory.
     * This still allows usage in multi-threading environment but all the
     * created resources can only be used from the respective threads where
     * they were created. */
    hr = fn_D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                &IID_ID2D1Factory, &factory_options, (void**) &d2d_factory);
    if(FAILED(hr)) {
        WD_TRACE_HR("d2d_init: D2D1CreateFactory() failed.");
        goto err_CreateFactory;
    }

    /* Success */
    return 0;

    /* Error path unwinding */
err_CreateFactory:
err_GetProcAddress:
    FreeLibrary(d2d_dll);
    d2d_dll = NULL;
err_LoadLibrary:
    return -1;
}

void
d2d_fini(void)
{
    ID2D1Factory_Release(d2d_factory);
    FreeLibrary(d2d_dll);
    d2d_dll = NULL;
}

d2d_canvas_t*
d2d_canvas_alloc(ID2D1RenderTarget* target, WORD type)
{
    d2d_canvas_t* c;

    c = (d2d_canvas_t*) malloc(sizeof(d2d_canvas_t));
    if(c == NULL) {
        WD_TRACE("d2d_canvas_alloc: malloc() failed.");
        return NULL;
    }

    memset(c, 0, sizeof(d2d_canvas_t));

    c->type = type;
    c->target = target;

    /* We use raw pixels as units. D2D by default works with DIPs ("device
     * independent pixels"), which map 1:1 to physical pixels when DPI is 96.
     * So we enforce the render target to think we have this DPI. */
    ID2D1RenderTarget_SetDpi(c->target, 96.0f, 96.0f);

    d2d_reset_transform(target);

    return c;
}

void
d2d_reset_clip(d2d_canvas_t* c)
{
    if(c->clip_layer != NULL) {
        ID2D1RenderTarget_PopLayer(c->target);
        ID2D1Layer_Release(c->clip_layer);
        c->clip_layer = NULL;
    }
    if(c->flags & D2D_CANVASFLAG_RECTCLIP) {
        ID2D1RenderTarget_PopAxisAlignedClip(c->target);
        c->flags &= ~D2D_CANVASFLAG_RECTCLIP;
    }
}

void
d2d_reset_transform(ID2D1RenderTarget* target)
{
    ID2D1RenderTarget_SetTransform(target, &d2d_base_transform);
}

void
d2d_apply_transform(ID2D1RenderTarget* target, D2D1_MATRIX_3X2_F* matrix)
{
    D2D1_MATRIX_3X2_F res;
    D2D1_MATRIX_3X2_F old_matrix;
    D2D1_MATRIX_3X2_F* a = matrix;
    D2D1_MATRIX_3X2_F* b = &old_matrix;

    ID2D1RenderTarget_GetTransform(target, b);

    res._11 = a->_11 * b->_11 + a->_12 * b->_21;
    res._12 = a->_11 * b->_12 + a->_12 * b->_22;
    res._21 = a->_21 * b->_11 + a->_22 * b->_21;
    res._22 = a->_21 * b->_12 + a->_22 * b->_22;
    res._31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
    res._32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;

    ID2D1RenderTarget_SetTransform(target, &res);
}

void
d2d_setup_arc_segment(D2D1_ARC_SEGMENT* arc_seg, float cx, float cy, float r,
                      float base_angle, float sweep_angle)
{
    float sweep_rads = (base_angle + sweep_angle) * (WD_PI / 180.0f);

    arc_seg->point.x = cx + r * cosf(sweep_rads);
    arc_seg->point.y = cy + r * sinf(sweep_rads);
    arc_seg->size.width = r;
    arc_seg->size.height = r;
    arc_seg->rotationAngle = 0.0f;

    if(sweep_angle >= 0.0f)
        arc_seg->sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    else
        arc_seg->sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;

    if(sweep_angle >= 180.0f)
        arc_seg->arcSize = D2D1_ARC_SIZE_LARGE;
    else
        arc_seg->arcSize = D2D1_ARC_SIZE_SMALL;
}

ID2D1Geometry*
d2d_create_arc_geometry(float cx, float cy, float r,
                        float base_angle, float sweep_angle, BOOL pie)
{
    ID2D1PathGeometry* g = NULL;
    ID2D1GeometrySink* s;
    HRESULT hr;
    float base_rads = base_angle * (WD_PI / 180.0f);
    D2D1_POINT_2F pt;
    D2D1_ARC_SEGMENT arc_seg;

    wd_lock();
    hr = ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
    wd_unlock();
    if(FAILED(hr)) {
        WD_TRACE_HR("d2d_create_arc_geometry: "
                    "ID2D1Factory::CreatePathGeometry() failed.");
        return NULL;
    }
    hr = ID2D1PathGeometry_Open(g, &s);
    if(FAILED(hr)) {
        WD_TRACE_HR("d2d_create_arc_geometry: ID2D1PathGeometry::Open() failed.");
        ID2D1PathGeometry_Release(g);
        return NULL;
    }

    pt.x = cx + r * cosf(base_rads);
    pt.y = cy + r * sinf(base_rads);
    ID2D1GeometrySink_BeginFigure(s, pt, D2D1_FIGURE_BEGIN_FILLED);

    d2d_setup_arc_segment(&arc_seg, cx, cy, r, base_angle, sweep_angle);
    ID2D1GeometrySink_AddArc(s, &arc_seg);

    if(pie) {
        pt.x = cx;
        pt.y = cy;
        ID2D1GeometrySink_AddLine(s, pt);
        ID2D1GeometrySink_EndFigure(s, D2D1_FIGURE_END_CLOSED);
    } else {
        ID2D1GeometrySink_EndFigure(s, D2D1_FIGURE_END_OPEN);
    }

    ID2D1GeometrySink_Close(s);
    ID2D1GeometrySink_Release(s);

    return (ID2D1Geometry*) g;
}
