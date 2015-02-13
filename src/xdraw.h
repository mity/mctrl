/*
 * Copyright (c) 2015 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MC_XDRAW_H
#define MC_XDRAW_H

#include "misc.h"


/* This module provides an abstraction of "high quality drawing" with support
 * for anti-aliasing and alpha channel for situations where plain GDI does not
 * suffice.
 *
 * It is actually a light-weighted wrapper of Direct2D API (D2D1.DLL) and
 * Direct Write API (DWRITE.DLL) for Windows Vista SP2 and newer, or GDI+
 * (GDIPLUS.DLL) for older Windows versions.
 *
 * Note that Windows 2000 do not include GDIPLUS.DLL as part of system. This
 * module shall work on Win2K only if application deploys redistributable
 * version of GDIPLUS.DLL, which can be obtained from Microsoft.
 */

/* GDI+ limitations:
 *  -- Only true-type fonts are supported.
 *  -- When callers asks incompatible font, xdraw.c falls back to a "default"
 *     font as specified by MS user interface guidelines (Segoe UI or Tahoma,
 *     depending on Windows version).
 */

/* Miscellaneous notes:
 *  -- All coordinates and sizes are measured in pixels (unless caller installs
 *     a transformation matrix which scales).
 *  -- Whole numbers correspond to pixels (e.g. [0,0] is left-top pixel)
 *  -- Coordinates with fractions cause anti-aliasing: pixels in neighborhood
 *     are affected.
 *  -- Angles are measured in degrees (360 degrees form a full circle).
 *  -- Alpha channel: 0x00 = transparent, 0xff = opaque.
 */


/**************************
 ***  Color Management  ***
 **************************/

typedef DWORD xdraw_color_t;

#define XDRAW_ARGB(a,r,g,b)                                                  \
    ((xdraw_color_t) ((((a) & 0xff) << 24) | (((r) & 0xff) << 16) |          \
                      (((g) & 0xff) <<  8) | (((b) & 0xff) <<  0)))
#define XDRAW_RGBA(r,g,b,a)       XDRAW_ARGB((a), (r), (g), (b))
#define XDRAW_RGB(r,g,b)          XDRAW_ARGB(0xff, (r), (g), (b))
#define XDRAW_COLORREF(rgb)       XDRAW_ARGB(0xff, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb))
#define XDRAW_ACOLORREF(a,rgb)    XDRAW_ARGB((a), GetRValue(rgb), GetGValue(rgb), GetBValue(rgb))
#define XDRAW_COLORREFA(rgb,a)    XDRAW_ARGB((a), GetRValue(rgb), GetGValue(rgb), GetBValue(rgb))

#define XDRAW_ALPHAVALUE(color)   ((((xdraw_color_t)(color)) & 0xff000000) >> 24)
#define XDRAW_REDVALUE(color)     ((((xdraw_color_t)(color)) & 0x00ff0000) >> 16)
#define XDRAW_GREENVALUE(color)   ((((xdraw_color_t)(color)) & 0x0000ff00) >> 8)
#define XDRAW_BLUEVALUE(color)    ((((xdraw_color_t)(color)) & 0x000000ff) >> 0)


/**********************
 ***  Opaque Types  ***
 **********************/

typedef struct xdraw_canvas_tag xdraw_canvas_t;
typedef struct xdraw_brush_tag xdraw_brush_t;
typedef struct xdraw_font_tag xdraw_font_t;
typedef struct xdraw_image_tag xdraw_image_t;
typedef struct xdraw_path_tag xdraw_path_t;
typedef struct xdraw_path_sink_tag xdraw_path_sink_t;


/*************************
 ***  Geometric Types  ***
 *************************/

typedef struct xdraw_point_tag xdraw_point_t;
struct xdraw_point_tag {
    float x;
    float y;
};

typedef struct xdraw_rect_tag xdraw_line_t;
struct xdraw_line_tag {
    float x0;
    float y0;
    float x1;
    float y1;
};

typedef struct xdraw_rect_tag xdraw_rect_t;
struct xdraw_rect_tag {
    float x0;
    float y0;
    float x1;
    float y1;
};

typedef struct xdraw_circle_tag xdraw_circle_t;
struct xdraw_circle_tag {
    float x;
    float y;
    float r;
};


/***************************
 ***  Canvas Management  ***
 ***************************/

/* All drawing happens on a "canvas", an abstraction that represents any plain
 * which can be painted.
 *
 * When implementing standard control painting (WM_PAINT), the canvas can be
 * cached for reuse by subsequent WM_PAINT messages, if the implementation
 * fulfills all these conditions:
 *   (1) The canvas has to be created with xdraw_canvas_create_with_paintstruct()
 *       and it may be used strictly for the purposes of WM_PAINT.
 *   (2) The cached canvas has to be destroyed/recreated whenever function
 *       xdraw_canvas_end_paint() returns FALSE.
 *   (3) The canvas has to be resized with xdraw_canvas_resize() whenever the
 *       window receives message WM_SIZE.
 *   (4) The cached canvas has to be destroyed/recreated whenever the window
 *       caching the canvas gets message WM_DISPLAYCHANGE.
 */

#define XDRAW_CANVAS_DOUBLEBUFER     0x0001
#define XDRAW_CANVAS_GDICOMPAT       0x0002

xdraw_canvas_t* xdraw_canvas_create_with_paintstruct(HWND win, PAINTSTRUCT* ps, DWORD flags);
xdraw_canvas_t* xdraw_canvas_create_with_dc(HDC dc, const RECT* rect, DWORD flags);
void xdraw_canvas_destroy(xdraw_canvas_t* canvas);

/* Only for canvas created with xdraw_canvas_create_with_paintstruct(). */
int xdraw_canvas_resize(xdraw_canvas_t* canvas, UINT width, UINT height);

/* The drawing code must be enclosed between these functions.
 * When xdraw_canvas_end_paint() return FALSE, the canvas must not be cached
 * for subsequent reuse. */
void xdraw_canvas_begin_paint(xdraw_canvas_t* canvas);
BOOL xdraw_canvas_end_paint(xdraw_canvas_t* canvas);

/* For interoperability with GDI. Note the canvas has to be created with
 * the flag XDRAW_CANVAS_GDICOMPAT for this to work. Painting should use only
 * GDI functions to paint the DC between acquire and release call. */
HDC xdraw_canvas_acquire_dc(xdraw_canvas_t* canvas, BOOL retain_contents);
void xdraw_canvas_release_dc(xdraw_canvas_t* canvas, HDC dc);

void xdraw_canvas_transform_with_rotation(xdraw_canvas_t* canvas, float angle);
void xdraw_canvas_transform_with_translation(xdraw_canvas_t* canvas, float dx, float dy);
void xdraw_canvas_transform_reset(xdraw_canvas_t* canvas);


/**************************
 ***  Brush Management  ***
 **************************/

xdraw_brush_t* xdraw_brush_solid_create(xdraw_canvas_t* canvas, xdraw_color_t color);
void xdraw_brush_destroy(xdraw_brush_t* brush);

void xdraw_brush_solid_set_color(xdraw_brush_t* solidbrush, xdraw_color_t color);


/*************************
 ***  Path Management  ***
 *************************/

xdraw_path_t* xdraw_path_create(xdraw_canvas_t* canvas);
void xdraw_path_destroy(xdraw_path_t* path);

xdraw_path_sink_t* xdraw_path_open_sink(xdraw_path_t* path);
void xdraw_path_close_sink(xdraw_path_sink_t* sink);

void xdraw_path_begin_figure(xdraw_path_sink_t* sink, const xdraw_point_t* start_point);
void xdraw_path_end_figure(xdraw_path_sink_t* sink, BOOL closed_end);

void xdraw_path_add_line(xdraw_path_sink_t* sink, const xdraw_point_t* end_point);


/*************************
 ***  Font Management  ***
 *************************/

xdraw_font_t* xdraw_font_create_with_LOGFONT(xdraw_canvas_t* canvas, const LOGFONT* logfont);
xdraw_font_t* xdraw_font_create_with_HFONT(xdraw_canvas_t* canvas, HFONT font_handle);
void xdraw_font_destroy(xdraw_font_t* font);

typedef struct xdraw_font_metrics_tag xdraw_font_metrics_t;
struct xdraw_font_metrics_tag {
    float em_height;
    float cell_ascent;   /* Distance between top of char cell and base line. */
    float cell_descent;  /* Distance between bottom of char cell and base line. */
    float line_spacing;  /* Distance between two base lines. */
};

void xdraw_font_get_metrics(const xdraw_font_t* font, xdraw_font_metrics_t* metrics);


/**************************
 ***  Image Management  ***
 **************************/

xdraw_image_t* xdraw_image_load_from_file(const TCHAR* path);
xdraw_image_t* xdraw_image_load_from_stream(IStream* stream);
void xdraw_image_destroy(xdraw_image_t* image);

void xdraw_image_get_size(xdraw_image_t* image, float* w, float* h);


/**********************************
 ***  Draw and fill operations  ***
 **********************************/

void xdraw_clear(xdraw_canvas_t* canvas, xdraw_color_t color);

void xdraw_draw_arc(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                    const xdraw_circle_t* circle, float base_angle,
                    float sweep_angle, float stroke_width);
void xdraw_draw_image(xdraw_canvas_t* canvas, const xdraw_image_t* image,
                      const xdraw_rect_t* dst, const xdraw_rect_t* src);
void xdraw_draw_line(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                     const xdraw_line_t* line, float stroke_width);
void xdraw_draw_pie(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                    const xdraw_circle_t* circle, float base_angle,
                    float sweep_angle, float stroke_width);
void xdraw_draw_rect(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                     const xdraw_rect_t* rect, float stroke_width);

void xdraw_fill_circle(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                       const xdraw_circle_t* circle);
void xdraw_fill_path(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                     const xdraw_path_t* path);
void xdraw_fill_pie(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                    const xdraw_circle_t* circle, float base_angle,
                    float sweep_angle);
void xdraw_fill_rect(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                     const xdraw_rect_t* rect);


/***********************
 ***  String output  ***
 ***********************/

#define XDRAW_STRING_LEFT         0x00
#define XDRAW_STRING_CENTER       0x01
#define XDRAW_STRING_RIGHT        0x02
#define XDRAW_STRING_CLIP         0x04
#define XDRAW_STRING_NOWRAP       0x08

void xdraw_draw_string(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                       const xdraw_rect_t* rect, const TCHAR* str, int len,
                       const xdraw_brush_t* brush, DWORD flags);


typedef struct xdraw_string_measure_tag xdraw_string_measure_t;
struct xdraw_string_measure_tag {
    xdraw_rect_t bound;
};

void xdraw_measure_string(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                          const xdraw_rect_t* rect, const TCHAR* str, int len,
                          xdraw_string_measure_t* measure, DWORD flags);


#endif  /* MC_XDRAW_H */
