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

#ifndef WD_BACKEND_D2D_H
#define WD_BACKEND_D2D_H

#include "misc.h"

#include <d2d1.h>


#define D2D_CANVASTYPE_BITMAP       0
#define D2D_CANVASTYPE_DC           1
#define D2D_CANVASTYPE_HWND         2

#define D2D_CANVASFLAG_RECTCLIP   0x1

typedef struct d2d_canvas_tag d2d_canvas_t;
struct d2d_canvas_tag {
    WORD type;
    WORD flags;
    union {
        ID2D1RenderTarget* target;
        ID2D1BitmapRenderTarget* bmp_target;
        ID2D1HwndRenderTarget* hwnd_target;
    };
    ID2D1GdiInteropRenderTarget* gdi_interop;
    ID2D1Layer* clip_layer;
};


extern const D2D1_MATRIX_3X2_F d2d_base_transform;

extern ID2D1Factory* d2d_factory;

static inline BOOL
d2d_enabled(void)
{
    return (d2d_factory != NULL);
}

static inline void
d2d_init_color(D2D_COLOR_F* c, WD_COLOR color)
{
    c->r = WD_RVALUE(color) / 255.0f;
    c->g = WD_GVALUE(color) / 255.0f;
    c->b = WD_BVALUE(color) / 255.0f;
    c->a = WD_AVALUE(color) / 255.0f;
}

int d2d_init(void);
void d2d_fini(void);

d2d_canvas_t* d2d_canvas_alloc(ID2D1RenderTarget* target, WORD type);

void d2d_reset_clip(d2d_canvas_t* c);

void d2d_reset_transform(ID2D1RenderTarget* target);
void d2d_apply_transform(ID2D1RenderTarget* target, D2D1_MATRIX_3X2_F* matrix);

void d2d_setup_arc_segment(D2D1_ARC_SEGMENT* arc_seg,
                    float cx, float cy, float r,
                    float base_angle, float sweep_angle);
ID2D1Geometry* d2d_create_arc_geometry(float cx, float cy, float r,
                    float base_angle, float sweep_angle, BOOL pie);


#endif  /* WD_BACKEND_D2D_H */
