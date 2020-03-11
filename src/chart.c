/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2013-2020 Martin Mitas
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

#include "chart.h"
#include "generic.h"
#include "dsa.h"
#include "color.h"
#include "tooltip.h"
#include "xd2d.h"
#include "xdwrite.h"

#include <float.h>
#include <math.h>


/* Uncomment this to have more verbose traces from this module. */
/*#define CHART_DEBUG     1*/

#ifdef CHART_DEBUG
    #define CHART_TRACE     MC_TRACE
#else
    #define CHART_TRACE     MC_NOOP
#endif


static const TCHAR chart_wc[] = MC_WC_CHART;    /* Window class name */


#define CHART_XD2D_CACHE_TIMER_ID    1


/* If we ever allow larger range for factor exponents, we may need to make
 * this larger, so the string representation of any values fits in. */
#define CHART_STR_VALUE_MAX_LEN       32


typedef struct chart_axis_tag chart_axis_t;
struct chart_axis_tag {
    TCHAR* name;
    int offset;
    CHAR factor_exp;
};

typedef struct chart_data_tag chart_data_t;
struct chart_data_tag {
    TCHAR* name;
    COLORREF color;
    UINT count;
    int* values;
};

typedef struct chart_tag chart_t;
struct chart_tag {
    HWND win;
    HWND notify_win;
    HWND tooltip_win;
    HFONT gdi_font;
    c_IDWriteTextFormat* text_fmt;
    UINT16 font_units_per_height;
    UINT16 font_cap_height;
    xd2d_cache_t xd2d_cache;
    DWORD style          : 16;
    DWORD no_redraw      :  1;
    DWORD tracking_leave :  1;
    DWORD tooltip_active :  1;
    chart_axis_t axis1;
    chart_axis_t axis2;
    int min_visible_value;
    int max_visible_value;
    int hot_set_ix;
    int hot_i;
    dsa_t data;
};

typedef struct chart_layout_tag chart_layout_t;
struct chart_layout_tag {
    SIZE font_size;
    int margin;
    RECT title_rect;
    RECT body_rect;
    RECT legend_rect;
};

typedef struct chart_xd2d_ctx_tag chart_xd2d_ctx_t;
struct chart_xd2d_ctx_tag {
    xd2d_ctx_t ctx;
    c_ID2D1SolidColorBrush* solid_brush;
};


/*******************
 *** xd2d vtable ***
 *******************/

static int
chart_xd2d_init(xd2d_ctx_t* raw_ctx)
{
    chart_xd2d_ctx_t* ctx = (chart_xd2d_ctx_t*) raw_ctx;
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    c_D2D1_COLOR_F c = { 0 };
    HRESULT hr;

    hr = c_ID2D1RenderTarget_CreateSolidColorBrush(rt, &c, NULL, &ctx->solid_brush);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("chart_xd2d_init: ID2D1RenderTarget::CreateSolidColorBrush() failed.");
        return -1;
    }

    return 0;
}

static void
chart_xd2d_fini(xd2d_ctx_t* raw_ctx)
{
    chart_xd2d_ctx_t* ctx = (chart_xd2d_ctx_t*) raw_ctx;
    c_ID2D1RenderTarget_Release(ctx->solid_brush);
}

static void chart_paint(void* ctrl, xd2d_ctx_t* ctx);

static xd2d_vtable_t chart_xd2d_vtable = {
    sizeof(chart_xd2d_ctx_t),
    chart_xd2d_init,
    chart_xd2d_fini,
    chart_paint
};


/*****************
 *** Utilities ***
 *****************/

static void
chart_data_dtor(dsa_t* dsa, void* item)
{
    chart_data_t* data = (chart_data_t*) item;

    if(data->name)
        free(data->name);
    if(data->values)
        free(data->values);
}

static inline COLORREF
chart_data_color(chart_t* chart, int set_ix)
{
    chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
    if(data->color == MC_CLR_DEFAULT)
        return color_seq(set_ix);
    else
        return data->color;
}

static int
chart_value_from_parent(chart_t* chart, int set_ix, int i)
{
    MC_NMCHDISPINFO info;
    int value;

    info.hdr.hwndFrom = chart->win;
    info.hdr.idFrom = GetWindowLong(chart->win, GWL_ID);
    info.hdr.code = MC_CHN_GETDISPINFO;
    info.fMask = MC_CHDIM_VALUES;
    info.iDataSet = set_ix;
    info.iValueFirst = i;
    info.iValueLast = i;
    info.piValues = &value;
    MC_SEND(chart->notify_win, WM_NOTIFY, info.hdr.idFrom, &info);

    return value;
}

static int
chart_round_value(int value, BOOL up)
{
    static const int nice_numbers[] = {
                 1,                                  2,                      5,
                10,         12,         15,         20,         30,         50,         60,         80,
               100,        120,        150,        200,        300,        500,        600,        800,
              1000,       1200,       1500,       2000,       3000,       5000,       6000,       8000,
             10000,      12000,      15000,      20000,      30000,      50000,      60000,      80000,
            100000,     120000,     150000,     200000,     300000,     500000,     600000,     800000,
           1000000,    1200000,    1500000,    2000000,    3000000,    5000000,    6000000,    8000000,
          10000000,   12000000,   15000000,   20000000,   30000000,   50000000,   60000000,   80000000,
         100000000,  120000000,  150000000,  200000000,  300000000,  500000000,  600000000,  800000000,
        1000000000, 1200000000, 1500000000, 2000000000
    };
    static int count = MC_SIZEOF_ARRAY(nice_numbers);
    int i;

    if(value == 0)
        return 0;

    if(value < 0) {
        if(value < -nice_numbers[count-1])  /* INT32_MIN != -INT32_MAX */
            return (up ? -nice_numbers[count-1] : INT32_MIN);
        else
            return -chart_round_value(-value, !up);
    }

    if(value > nice_numbers[count-1])
        return (up ? INT32_MAX : nice_numbers[count-1]);

    /* Although we could use binary search here, I expect vast majority of use
     * cases to have relatively small values, so in general the sequential
     * search is probably more effective here. */
    i = 0;
    while(value > nice_numbers[i])
        i++;

    if(value == nice_numbers[i])
        return nice_numbers[i];

    return nice_numbers[up ? i : i-1];
}

static inline int
chart_value(chart_t* chart, int set_ix, int i)
{
    chart_data_t* data;

    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
    if(data->values != NULL)
        return data->values[i];
    else
        return chart_value_from_parent(chart, set_ix, i);
}

static void
chart_str_value(chart_axis_t* axis, int value, WCHAR buffer[CHART_STR_VALUE_MAX_LEN])
{
    value += axis->offset;

    if(axis->factor_exp == 0  ||  value == 0) {
        _swprintf(buffer, L"%d", value);
    } else if(axis->factor_exp > 0) {
        int i, n;
        n = _swprintf(buffer, L"%d", value);
        for(i = n; i < n + axis->factor_exp; i++)
            buffer[i] = L'0';
        buffer[n + axis->factor_exp] = L'\0';
    } else {
        int factor = 10;
        int i;
        WCHAR dec_delim[4];
        int dec_delim_len;

        for(i = -(axis->factor_exp); i != 1; i--)
            factor *= 10;

        dec_delim_len = GetLocaleInfoW(GetThreadLocale(), LOCALE_SDECIMAL,
                                       dec_delim, MC_SIZEOF_ARRAY(dec_delim));
        if(MC_ERR(dec_delim_len == 0)) {
            dec_delim[0] = L'.';
            dec_delim_len = 1;
        }

        _swprintf(buffer, L"%d%.*s%0.*d", value / factor, dec_delim_len, dec_delim,
                  -(axis->factor_exp), MC_ABS(value) % factor);
    }
}

static void
chart_tooltip_text_common(chart_t* chart, chart_axis_t* axis,
                          TCHAR* buffer, UINT bufsize)
{
    if(chart->hot_set_ix >= 0  &&  chart->hot_i >= 0) {
        WCHAR val_str[CHART_STR_VALUE_MAX_LEN];
        int val;

        val = chart_value(chart, chart->hot_set_ix, chart->hot_i);
        chart_str_value(axis, val, val_str);
        mc_str_inbuf(val_str, MC_STRW, buffer, MC_STRT, bufsize);
    }
}


/*******************
 *** Value Cache ***
 *******************/

typedef struct cache_tag cache_t;
struct cache_tag {
    chart_t* chart;
    int** values;
};

static void
cache_init_(cache_t* cache, int n)
{
    int* values;
    int set_ix;
    chart_data_t* data;

    values = (int*) &cache->values[n];

    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&cache->chart->data, set_ix, chart_data_t);
        if(data->values != NULL) {
            cache->values[set_ix] = data->values;
        } else {
            MC_NMCHDISPINFO info;

            info.hdr.hwndFrom = cache->chart->win;
            info.hdr.idFrom = GetWindowLong(info.hdr.hwndFrom, GWL_ID);
            info.hdr.code = MC_CHN_GETDISPINFO;
            info.fMask = MC_CHDIM_VALUES;
            info.iDataSet = set_ix;
            info.iValueFirst = 0;
            info.iValueLast = data->count - 1;
            info.piValues = values;
            MC_SEND(cache->chart->notify_win, WM_NOTIFY, info.hdr.idFrom, &info);

            cache->values[set_ix] = values;
            values += data->count;
        }
    }
}

/* These must be macros because we create it on the stack of the caller. */

#define CACHE_INIT(cache, chart)                                              \
    do {                                                                      \
        int set_ix, n;                                                        \
        int cache_size = 0;                                                   \
        chart_data_t* data;                                                   \
                                                                              \
        n = dsa_size(&(chart)->data);                                         \
        for(set_ix = 0; set_ix < n; set_ix++) {                               \
            data = DSA_ITEM(&(chart)->data, set_ix, chart_data_t);            \
            if(data->values == NULL)                                          \
                cache_size += data->count;                                    \
        }                                                                     \
                                                                              \
        (cache)->chart = (chart);                                             \
        (cache)->values = (int**) _malloca(n * sizeof(int*) +                 \
                                           cache_size * sizeof(int));         \
        if(MC_ERR((cache)->values == NULL))                                   \
            MC_TRACE("CACHE_INIT: _malloca() failed.");                       \
        else                                                                  \
            cache_init_((cache), n);                                          \
    } while(0)

#define CACHE_FINI(cache)                                                     \
    do {                                                                      \
        if((cache)->values != NULL)                                           \
            _freea((cache)->values);                                          \
    } while(0)


static int
cache_value_(cache_t* cache, int set_ix, int i)
{
    chart_data_t* data;

    data = DSA_ITEM(&cache->chart->data, set_ix, chart_data_t);
    if(data->values != NULL)
        return data->values[i];
    else
        return chart_value_from_parent(cache->chart, set_ix, i);
}

#define CACHE_VALUE(cache, set_ix, i)                                         \
    ((i < DSA_ITEM(&((cache)->chart)->data, set_ix, chart_data_t)->count)     \
            ? ((cache)->values != NULL  ?  (cache)->values[(set_ix)][(i)]     \
                                    :  cache_value_((cache), (set_ix), (i)))  \
            : 0)

static inline int
cache_stack(cache_t* cache, int set_ix, int i)
{
    int sum = 0;

    while(set_ix >= 0) {
        sum += CACHE_VALUE(cache, set_ix, i);
        set_ix--;
    }

    return sum;
}


/*****************
 *** Pie Chart ***
 *****************/

static inline float
pie_normalize_angle(float angle)
{
    while(angle < 0.0f)
        angle += 360.0f;
    while(angle >= 360.0f)
        angle -= 360.0f;
    return angle;
}

static inline float
pie_vector_angle(float x0, float y0, float x1, float y1)
{
    return atan2f(y1-y0, x1-x0) * (180.0f / MC_PIf);
}

static inline BOOL
pie_pt_in_sweep(float angle, float sweep, float x0, float y0, float x1, float y1)
{
    return (pie_normalize_angle(pie_vector_angle(x0, y0, x1, y1) - angle) < sweep);
}

static inline int
pie_value(chart_t* chart, int set_ix)
{
    return MC_ABS(chart_value(chart, set_ix, 0));
}

typedef struct pie_geometry_tag pie_geometry_t;
struct pie_geometry_tag {
    float cx;
    float cy;
    float r;
    float sum;
};

static void
pie_calc_geometry(chart_t* chart, const chart_layout_t* layout, pie_geometry_t* geom)
{
    int set_ix, n;

    geom->cx = (layout->body_rect.left + layout->body_rect.right) / 2.0f;
    geom->cy = (layout->body_rect.top + layout->body_rect.bottom) / 2.0f;
    geom->r = MC_MIN(mc_width(&layout->body_rect), mc_height(&layout->body_rect)) / 2.0f - 6.0f;

    geom->sum = 0.0f;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++)
        geom->sum += (float) pie_value(chart, set_ix);
}

static void
pie_paint(chart_t* chart, chart_xd2d_ctx_t* ctx, const chart_layout_t* layout)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    pie_geometry_t geom;
    int set_ix, n, val;
    float angle, sweep;

    pie_calc_geometry(chart, layout, &geom);

    angle = -90.0f;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        c_ID2D1PathGeometry* path;
        c_ID2D1GeometrySink* path_sink;
        COLORREF cref;
        c_D2D1_ARC_SEGMENT arc;
        c_D2D1_COLOR_F c;
        float angle_cos, angle_sin;
        float sweep_cos, sweep_sin;

        cref = chart_data_color(chart, set_ix);
        val = pie_value(chart, set_ix);
        sweep = (360.0f * (float) val) / geom.sum;

        angle_cos = cosf(angle * (MC_PIf / 180.0f));
        angle_sin = sinf(angle * (MC_PIf / 180.0f));
        sweep_cos = cosf((angle+sweep) * (MC_PIf / 180.0f));
        sweep_sin = sinf((angle+sweep) * (MC_PIf / 180.0f));

        arc.rotationAngle = 0.0f;
        arc.sweepDirection = c_D2D1_SWEEP_DIRECTION_CLOCKWISE;
        arc.arcSize = (sweep < 180.0f ? c_D2D1_ARC_SIZE_SMALL : c_D2D1_ARC_SIZE_LARGE);

        /* Paint the pie */
        path = xd2d_CreatePathGeometry(&path_sink);
        if(path != NULL) {
            c_D2D1_POINT_2F pt;

            pt.x = geom.cx + geom.r * angle_cos;
            pt.y = geom.cy + geom.r * angle_sin;
            c_ID2D1GeometrySink_BeginFigure(path_sink, pt, c_D2D1_FIGURE_BEGIN_FILLED);
            arc.point.x = geom.cx + geom.r * sweep_cos;
            arc.point.y = geom.cy + geom.r * sweep_sin;
            arc.size.width = geom.r;
            arc.size.height = geom.r;
            c_ID2D1GeometrySink_AddArc(path_sink, &arc);
            pt.x = geom.cx;
            pt.y = geom.cy;
            c_ID2D1GeometrySink_AddLine(path_sink, pt);
            c_ID2D1GeometrySink_EndFigure(path_sink, c_D2D1_FIGURE_END_CLOSED);
            c_ID2D1GeometrySink_Close(path_sink);
            c_ID2D1GeometrySink_Release(path_sink);

            xd2d_color_set_cref(&c, cref);
            c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

            c_ID2D1RenderTarget_FillGeometry(rt, (c_ID2D1Geometry*) path,
                        (c_ID2D1Brush*) ctx->solid_brush, NULL);
            c_ID2D1PathGeometry_Release(path);
        }

        /* Paint active aura */
        if(set_ix == chart->hot_set_ix) {
            c_D2D1_POINT_2F pt;

            path = xd2d_CreatePathGeometry(&path_sink);
            if(path != NULL) {
                pt.x = geom.cx + (geom.r+4.0f) * angle_cos;
                pt.y = geom.cy + (geom.r+4.0f) * angle_sin;
                c_ID2D1GeometrySink_BeginFigure(path_sink, pt, c_D2D1_FIGURE_BEGIN_HOLLOW);
                arc.point.x = geom.cx + (geom.r+4.0f) * sweep_cos;
                arc.point.y = geom.cy + (geom.r+4.0f) * sweep_sin;
                arc.size.width = (geom.r+4.0f);
                arc.size.height = (geom.r+4.0f);
                c_ID2D1GeometrySink_AddArc(path_sink, &arc);
                c_ID2D1GeometrySink_EndFigure(path_sink, c_D2D1_FIGURE_END_OPEN);
                c_ID2D1GeometrySink_Close(path_sink);
                c_ID2D1GeometrySink_Release(path_sink);

                xd2d_color_set_cref(&c, color_hint(cref));
                c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

                c_ID2D1RenderTarget_DrawGeometry(rt, (c_ID2D1Geometry*) path,
                                (c_ID2D1Brush*) ctx->solid_brush, 4.0f, NULL);
                c_ID2D1PathGeometry_Release(path);
            }
        }

        /* Paint white borders */
        {
            c_D2D1_POINT_2F pt0, pt1;

            xd2d_color_set_cref(&c, GetSysColor(COLOR_WINDOW));
            c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

            pt0.x = geom.cx;
            pt0.y = geom.cy;
            pt1.x = geom.cx + (geom.r+7.0f) * angle_cos;
            pt1.y = geom.cy + (geom.r+7.0f) * angle_sin;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1,
                        (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
            pt1.x = geom.cx + (geom.r+7.0f) * sweep_cos;
            pt1.y = geom.cy + (geom.r+7.0f) * sweep_sin;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1,
                        (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
        }

        /* Paint label */
        {
            WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
            c_IDWriteTextLayout* text_layout;

            chart_str_value(&chart->axis1, val, buffer);
            text_layout = xdwrite_create_text_layout(buffer, wcslen(buffer),
                        chart->text_fmt, 0.0f, 0.0f,
                        XDWRITE_ALIGN_CENTER | XDWRITE_VALIGN_CENTER | XDWRITE_NOWRAP);
            if(text_layout != NULL) {
                float rads = (angle + 0.5f * sweep) * (MC_PIf / 180.0f);
                c_D2D1_POINT_2F pt;
                c_DWRITE_TEXT_METRICS metrics;
                float dw;
                float dh;

                pt.x = geom.cx + 0.75f * geom.r * cosf(rads);
                pt.y = geom.cy + 0.75f * geom.r * sinf(rads);

                c_IDWriteTextLayout_GetMetrics(text_layout, &metrics);
                dw = 0.5f * metrics.width;
                dh = 0.5f * metrics.height;

                /* Paint only if it can fit inside the pie. */
                if(pie_pt_in_sweep(angle, sweep, geom.cx, geom.cy, pt.x - dw, pt.y - dh)  &&
                   pie_pt_in_sweep(angle, sweep, geom.cx, geom.cy, pt.x - dw, pt.y + dh)  &&
                   pie_pt_in_sweep(angle, sweep, geom.cx, geom.cy, pt.x + dw, pt.y - dh)  &&
                   pie_pt_in_sweep(angle, sweep, geom.cx, geom.cy, pt.x + dw, pt.y + dh))
                {
                    xd2d_color_set_rgb(&c, 255, 255, 255);
                    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

                    c_ID2D1RenderTarget_DrawTextLayout(rt, pt, text_layout,
                                (c_ID2D1Brush*) ctx->solid_brush, 0);
                }

                c_IDWriteTextLayout_Release(text_layout);
            }
        }

        angle += sweep;
    }
}

static void
pie_hit_test(chart_t* chart, const chart_layout_t* layout,
             int x, int y, int* p_set_ix, int* p_i)
{
    pie_geometry_t geom;
    int set_ix, n;
    float angle, sweep;
    float dx, dy;

    pie_calc_geometry(chart, layout, &geom);

    dx = geom.cx - x;
    dy = geom.cy - y;
    if(dx * dx + dy * dy > geom.r * geom.r)
        return;

    angle = -90.0f;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        sweep = (360.0f * (float)pie_value(chart, set_ix)) / geom.sum;

        if(pie_pt_in_sweep(angle, sweep, geom.cx, geom.cy, x, y)) {
            *p_set_ix = set_ix;
            *p_i = 0;
            break;
        }

        angle += sweep;
    }
}

static inline void
pie_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->axis1, buffer, bufsize);
}


/*******************
 *** Grid Layout ***
 *******************/

/* This is shared by most chart types (all but pie chart); to compute where to
 * place axis, their labels etc. */

typedef enum grid_axis_type_tag grid_axis_type_t;
enum grid_axis_type_tag {
    GRID_AXIS_CONTINUITY = 0,
    GRID_AXIS_DISCRETE,
    GRID_AXIS_CATEGORY
};

typedef struct grid_axis_tag grid_axis_t;
struct grid_axis_tag {
    grid_axis_type_t type;
    int direction;
    c_D2D1_POINT_2F name_pos;   /* Center of the axis name label. */
    int min_value;              /* Min. value marked on the axis. */
    int max_value;              /* Max. value marked on the axis. */
    int grid_base;              /* First orthogonal line. */
    int grid_delta;             /* Difference between two orthogonal lines. */
    float coord0;               /* Coordinate corresponding to min_value. */
    float coord1;               /* Coordinate corresponding to max_value. */
    float coord_labels;
    float coord_delta;
};

typedef struct grid_layout_tag grid_layout_t;
struct grid_layout_tag {
    float label_font_size;
    grid_axis_t x_axis;
    grid_axis_t y_axis;
};

static inline float
grid_map(int value, const grid_axis_t* a)
{
    float f;

    f = ((float)(value - a->min_value) * (a->coord1 - a->coord0))
       / (float)(a->max_value - a->min_value);

    if(a->direction > 0)
        return a->coord0 + f;
    else
        return a->coord1 - f;
}

static inline float
grid_map_x(int value, const grid_layout_t* gl)
{
    return grid_map(value, &gl->x_axis);
}

static inline float
grid_map_y(int value, const grid_layout_t* gl)
{
    return grid_map(value, &gl->y_axis);
}

static inline float
grid_unmap_x(float coord, const grid_layout_t* gl)
{
    const grid_axis_t* a = &gl->x_axis;
    return a->min_value + ((coord - a->coord0) * (float)(a->max_value - a->min_value))
                        / (a->coord1 - a->coord0);
}

static void
grid_fix_axis_scale(grid_axis_t* axis, chart_axis_t* axis_params)
{
    /* Make sure the orthogonal axis is visible. */
    if(axis->type == GRID_AXIS_CONTINUITY) {
        axis->min_value = MC_MIN(0, axis->min_value);
        axis->max_value = MC_MAX(0, axis->max_value);
    }

    /* Avoid singularity. */
    if(axis->type != GRID_AXIS_CATEGORY) {
        if(axis->min_value >= axis->max_value)
            axis->max_value = axis->min_value + 1;
    }

    /* Round the scale to beautiful rounded values. */
    if(axis->type == GRID_AXIS_CONTINUITY) {
        axis->min_value = chart_round_value(axis->min_value + axis_params->offset, FALSE) - axis_params->offset;
        axis->max_value = chart_round_value(axis->max_value + axis_params->offset, TRUE) - axis_params->offset;
    }
}

static void
grid_calc_base_and_delta(grid_axis_t* axis, int min_pixels)
{
    if(axis->type != GRID_AXIS_CONTINUITY) {
        axis->grid_base = 0;
        axis->grid_delta = 1;
        return;
    }

    /* Difference of two orthogonal lines in neighborhood. */
    axis->grid_delta = chart_round_value(
                (min_pixels * (axis->max_value - axis->min_value))
                / (int)(axis->coord1 - axis->coord0), TRUE);
    axis->grid_delta = MC_MAX(axis->grid_delta, 1);

    /* Value corresponding to the 1st visible lines. We choose it so that
     * (1) BASE >= min_value_x; and
     * (2) BASE + K * DELTA == 0 (for some integer K) */
    axis->grid_base = (axis->min_value / axis->grid_delta) * axis->grid_delta;
}

static void
grid_fix_coords(grid_axis_t* axis)
{
    float old_width, new_width;

    old_width = axis->coord1 - axis->coord0;
    if(axis->type == GRID_AXIS_CATEGORY) {
        float n = (float)(axis->max_value - axis->min_value + 1);
        axis->coord_delta = floorf(old_width / n);
        new_width = axis->coord_delta * n;
    } else {
        float old_ppgd, new_ppgd; /* pixels per grid_delta */

        old_ppgd = grid_map(axis->grid_delta, axis) - grid_map(0, axis);
        old_ppgd = MC_ABS(old_ppgd);
        new_ppgd = floorf(old_ppgd);
        new_width = old_width * new_ppgd / old_ppgd;
        axis->coord_delta = new_ppgd;
    }

    axis->coord0 = floorf(axis->coord0 + 0.5f * (old_width - new_width));
    axis->coord1 = axis->coord0 + new_width;
}

static void
grid_calc_layout(chart_t* chart, const chart_layout_t* chart_layout,
                 cache_t* cache, grid_layout_t* gl)
{
    DWORD type = (chart->style & MC_CHS_TYPEMASK);
    chart_axis_t* xa = &chart->axis1;
    chart_axis_t* ya = &chart->axis2;
    float font_height;
    int i, n, set_ix;
    chart_data_t* data;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
    size_t len;
    c_IDWriteTextLayout* text_layout;
    float x_label_width, x_label_height;
    float y_label_width, y_label_height, y_label_padding;

    /* What axis we need. Note we initially treat the bar chart the same as
     * column chart, later we simply swap the axis params. */
    switch(type) {
        case MC_CHS_SCATTER:
            gl->x_axis.type = GRID_AXIS_CONTINUITY;
            gl->y_axis.type = GRID_AXIS_CONTINUITY;
            break;

        case MC_CHS_LINE:
        case MC_CHS_STACKEDLINE:
        case MC_CHS_AREA:
        case MC_CHS_STACKEDAREA:
            gl->x_axis.type = GRID_AXIS_DISCRETE;
            gl->y_axis.type = GRID_AXIS_CONTINUITY;
            break;

        case MC_CHS_COLUMN:
        case MC_CHS_STACKEDCOLUMN:
        case MC_CHS_BAR:
        case MC_CHS_STACKEDBAR:
            gl->x_axis.type = GRID_AXIS_CATEGORY;
            gl->y_axis.type = GRID_AXIS_CONTINUITY;
            break;
    }

    /* Find extreme values. */
    n = dsa_size(&chart->data);
    if(n > 0) {
        gl->x_axis.min_value = INT32_MAX;
        gl->x_axis.max_value = INT32_MIN;
        gl->y_axis.min_value = INT32_MAX;
        gl->y_axis.max_value = INT32_MIN;
        switch(type) {
            case MC_CHS_SCATTER:
                for(set_ix = 0; set_ix < n; set_ix++) {
                    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
                    for(i = 0; i < data->count-1; i += 2) {  /* '-1' to protect if ->count is odd */
                        int x = CACHE_VALUE(cache, set_ix, i);
                        int y = CACHE_VALUE(cache, set_ix, i+1);
                        gl->x_axis.min_value = MC_MIN(gl->x_axis.min_value, x);
                        gl->x_axis.max_value = MC_MAX(gl->x_axis.max_value, x);
                        gl->y_axis.min_value = MC_MIN(gl->y_axis.min_value, y);
                        gl->y_axis.max_value = MC_MAX(gl->y_axis.max_value, y);
                    }
                }
                break;

            case MC_CHS_LINE:
            case MC_CHS_AREA:
            case MC_CHS_COLUMN:
            case MC_CHS_BAR:
                gl->x_axis.min_value = 0;
                gl->x_axis.max_value = 0;
                for(set_ix = 0; set_ix < n; set_ix++) {
                    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
                    for(i = 0; i < data->count; i++) {
                        int y = CACHE_VALUE(cache, set_ix, i);
                        gl->y_axis.min_value = MC_MIN(gl->y_axis.min_value, y);
                        gl->y_axis.max_value = MC_MAX(gl->y_axis.max_value, y);
                    }
                    gl->x_axis.max_value = MC_MAX(gl->x_axis.max_value, data->count-1);
                }
                break;

            case MC_CHS_STACKEDLINE:
            case MC_CHS_STACKEDAREA:
            case MC_CHS_STACKEDCOLUMN:
            case MC_CHS_STACKEDBAR:
                gl->x_axis.min_value = 0;
                gl->x_axis.max_value = 0;
                for(set_ix = 0; set_ix < n; set_ix++) {
                    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
                    for(i = 0; i < data->count; i++) {
                        int y = cache_stack(cache, set_ix, i);
                        gl->y_axis.min_value = MC_MIN(gl->y_axis.min_value, y);
                        gl->y_axis.max_value = MC_MAX(gl->y_axis.max_value, y);
                    }
                    gl->x_axis.max_value = MC_MAX(gl->x_axis.max_value, data->count-1);
                }
                break;
        }
    } else {
        gl->x_axis.min_value = 0;
        gl->x_axis.max_value = 0;
        gl->y_axis.min_value = 0;
        gl->y_axis.max_value = 0;
    }

    /* Swap the axis for the bar chart. */
    if(type == MC_CHS_BAR || type == MC_CHS_STACKEDBAR) {
        grid_axis_t tmp;

        memcpy(&tmp, &gl->x_axis, sizeof(grid_axis_t));
        memcpy(&gl->x_axis, &gl->y_axis, sizeof(grid_axis_t));
        memcpy(&gl->y_axis, &tmp, sizeof(grid_axis_t));
        xa = &chart->axis2;
        ya = &chart->axis1;
    }

    if(chart->text_fmt != NULL)
        font_height = ceilf(c_IDWriteTextFormat_GetFontSize(chart->text_fmt));
    else
        font_height = 0.0f;

    gl->label_font_size = 0.8f * font_height;

    gl->x_axis.direction = +1;
    gl->y_axis.direction = -1;

    /* Artificially expand the axis so we can paint the chart properly. */
    grid_fix_axis_scale(&gl->x_axis, xa);
    grid_fix_axis_scale(&gl->y_axis, ya);

    /* Compute space reserved for all X-axis value labels. */
    chart_str_value(xa, gl->x_axis.max_value, buffer);
    len = wcslen(buffer);
    text_layout = xdwrite_create_text_layout(buffer, len,
                chart->text_fmt, 0.0f, 0.0f, XDWRITE_NOWRAP);
    x_label_width = 0.0f;
    x_label_height = 0.0f;
    if(text_layout != NULL) {
        c_DWRITE_TEXT_METRICS text_metrics;
        c_DWRITE_TEXT_RANGE range = { 0, len };
        c_IDWriteTextLayout_SetFontSize(text_layout, gl->label_font_size, range);
        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
        x_label_width = MC_MAX(x_label_width, text_metrics.width);
        x_label_height = MC_MAX(x_label_height, text_metrics.height);
        c_IDWriteTextLayout_Release(text_layout);
    }
    if(gl->x_axis.min_value < 0) {
        chart_str_value(xa, gl->x_axis.min_value, buffer);
        len = wcslen(buffer);
        text_layout = xdwrite_create_text_layout(buffer, len,
                    chart->text_fmt, 0.0f, 0.0f, XDWRITE_NOWRAP);
        if(text_layout != NULL) {
            c_DWRITE_TEXT_METRICS text_metrics;
            c_DWRITE_TEXT_RANGE range = { 0, len };
            c_IDWriteTextLayout_SetFontSize(text_layout, gl->label_font_size, range);
            c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
            x_label_width = MC_MAX(x_label_width, text_metrics.width);
            x_label_height = MC_MAX(x_label_height, text_metrics.height);
            c_IDWriteTextLayout_Release(text_layout);
        }
    }
    x_label_width = MC_MAX(1.4f * x_label_width, 20.0f);
    x_label_height = 1.5f * x_label_height;

    /* Compute space reserved for all Y-axis value labels. */
    chart_str_value(ya, gl->y_axis.max_value, buffer);
    len = wcslen(buffer);
    text_layout = xdwrite_create_text_layout(buffer, len,
                chart->text_fmt, 0.0f, 0.0f, XDWRITE_NOWRAP);
    y_label_width = 0.0f;
    y_label_height = 0.0f;
    y_label_padding = 0.0f;
    if(text_layout != NULL) {
        c_DWRITE_TEXT_METRICS text_metrics;
        c_DWRITE_TEXT_RANGE range = { 0, len };
        c_IDWriteTextLayout_SetFontSize(text_layout, font_height * 0.8f, range);
        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
        y_label_width = MC_MAX(y_label_width, text_metrics.width);
        y_label_height = MC_MAX(y_label_height, text_metrics.height);
        y_label_padding = MC_MAX(y_label_padding, text_metrics.width / len);
        c_IDWriteTextLayout_Release(text_layout);
    }
    if(gl->y_axis.min_value < 0) {
        chart_str_value(ya, gl->y_axis.min_value, buffer);
        len = wcslen(buffer);
        text_layout = xdwrite_create_text_layout(buffer, len,
                    chart->text_fmt, 0.0f, 0.0f, XDWRITE_NOWRAP);
        if(text_layout != NULL) {
            c_DWRITE_TEXT_METRICS text_metrics;
            c_DWRITE_TEXT_RANGE range = { 0, len };
            c_IDWriteTextLayout_SetFontSize(text_layout, font_height * 0.8f, range);
            c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
            y_label_width = MC_MAX(y_label_width, text_metrics.width);
            y_label_height = MC_MAX(y_label_height, text_metrics.height);
            y_label_padding = MC_MAX(y_label_padding, text_metrics.height / len);
            c_IDWriteTextLayout_Release(text_layout);
        }
    }
    y_label_width = y_label_width + 0.5f * y_label_height;
    y_label_height = MC_MAX(1.5f * y_label_height, 20.0f);

    /* Compute grid area. */
    gl->x_axis.coord0 = (float) chart_layout->body_rect.left;
    gl->y_axis.coord0 = (float) chart_layout->body_rect.top;
    gl->x_axis.coord1 = (float) chart_layout->body_rect.right;
    gl->y_axis.coord1 = (float) chart_layout->body_rect.bottom;
    if(xa->name != NULL)
        gl->y_axis.coord1 -= 1.3f * font_height;
    if(ya->name != NULL)
        gl->x_axis.coord0 += 1.5f * font_height;
    gl->x_axis.coord0 += MC_MAX(y_label_width, 0.5f * x_label_width);
    gl->y_axis.coord0 += 0.5f * y_label_height;
    gl->y_axis.coord1 -= x_label_height;

    /* Compute axis base and delta. */
    grid_calc_base_and_delta(&gl->x_axis, x_label_width);
    grid_calc_base_and_delta(&gl->y_axis, y_label_height);

    /* Resize the grid_rect to make all grid lines fit into pixel matrix
     * without unwanted anti-aliasing artifacts. */
    grid_fix_coords(&gl->x_axis);
    grid_fix_coords(&gl->y_axis);

    /* Compute axis name positions. */
    gl->x_axis.name_pos.x = 0.5f * (gl->x_axis.coord0 + gl->x_axis.coord1 - 1.0f);
    gl->x_axis.name_pos.y = (float) chart_layout->body_rect.bottom - 0.5f * font_height;
    gl->y_axis.name_pos.x = (float) chart_layout->body_rect.left + 0.5f * font_height;
    gl->y_axis.name_pos.y = 0.5f * (gl->y_axis.coord0 + gl->y_axis.coord1 - 1.0f);

    /* Compute axis label positions. */
    gl->x_axis.coord_labels = gl->y_axis.coord1 + x_label_height;
    gl->y_axis.coord_labels = gl->x_axis.coord0 - y_label_padding;
}

static void
grid_paint(chart_t* chart, chart_xd2d_ctx_t* ctx, const grid_layout_t* gl)
{
    DWORD type = (chart->style & MC_CHS_TYPEMASK);
    chart_axis_t* xa;
    chart_axis_t* ya;
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    c_IDWriteTextLayout* text_layout;
    c_D2D1_COLOR_F c;
    c_D2D1_POINT_2F pt0, pt1;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
    size_t len;
    int v;

    if(type == MC_CHS_BAR || type == MC_CHS_STACKEDBAR) {
        xa = &chart->axis2;
        ya = &chart->axis1;
    } else {
        xa = &chart->axis1;
        ya = &chart->axis2;
    }

    xd2d_color_set_rgb(&c, 95, 95, 95);
    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

    /* Paint X-axis name. */
    text_layout = xdwrite_create_text_layout(xa->name, _tcslen(xa->name),
                chart->text_fmt, 0.0f, 0.0f,
                XDWRITE_ALIGN_CENTER | XDWRITE_VALIGN_CENTER | XDWRITE_NOWRAP);
    if(text_layout != NULL) {
        c_ID2D1RenderTarget_DrawTextLayout(rt, gl->x_axis.name_pos,
                    text_layout, (c_ID2D1Brush*) ctx->solid_brush, 0);
        c_IDWriteTextLayout_Release(text_layout);
    }

    /* Paint Y-axis name. */
    text_layout = xdwrite_create_text_layout(ya->name, _tcslen(ya->name),
                chart->text_fmt, 0.0f, 0.0f,
                XDWRITE_ALIGN_CENTER | XDWRITE_VALIGN_CENTER | XDWRITE_NOWRAP);
    if(text_layout != NULL) {
        c_D2D1_MATRIX_3X2_F old_matrix;
        c_D2D1_MATRIX_3X2_F matrix;
        float cx = gl->y_axis.name_pos.x + 0.5f;
        float cy = gl->y_axis.name_pos.y + 0.5f;

        c_ID2D1RenderTarget_GetTransform(rt, &old_matrix);
        matrix._11 = 0.0f;      matrix._12 = -1.0f;
        matrix._21 = 1.0f;      matrix._22 = 0.0f;
        matrix._31 = -cy + cx;  matrix._32 = cx + cy;
        c_ID2D1RenderTarget_SetTransform(rt, &matrix);
        c_ID2D1RenderTarget_DrawTextLayout(rt, gl->y_axis.name_pos,
                    text_layout, (c_ID2D1Brush*) ctx->solid_brush, 0);
        c_IDWriteTextLayout_Release(text_layout);
        c_ID2D1RenderTarget_SetTransform(rt, &old_matrix);
    }

    /* Paint grid lines. */
    xd2d_color_set_rgb(&c, 191, 191, 191);
    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
    if(gl->x_axis.type != GRID_AXIS_CATEGORY) {
        for(v = gl->x_axis.grid_base; v <= gl->x_axis.max_value; v += gl->x_axis.grid_delta) {
            pt0.x = grid_map_x(v, gl);
            pt0.y = gl->y_axis.coord0;
            pt1.x = pt0.x;
            pt1.y = gl->y_axis.coord1;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
        }
    }
    if(gl->y_axis.type != GRID_AXIS_CATEGORY) {
        for(v = gl->y_axis.grid_base; v <= gl->y_axis.max_value; v += gl->y_axis.grid_delta) {
            pt0.x = gl->x_axis.coord0;
            pt0.y = grid_map_y(v, gl);
            pt1.x = gl->x_axis.coord1;
            pt1.y = pt0.y;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
        }
    }

    /* Paint primary lines (axis) */
    xd2d_color_set_rgb(&c, 0, 0, 0);
    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
    if(gl->x_axis.type != GRID_AXIS_CATEGORY) {
        if(gl->x_axis.min_value <= -xa->offset  &&  -xa->offset <= gl->x_axis.max_value) {
            pt0.x = grid_map_x(-xa->offset, gl);
            pt0.y = gl->y_axis.coord0;
            pt1.x = pt0.x;
            pt1.y = gl->y_axis.coord1;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
        }
    }
    if(gl->y_axis.type != GRID_AXIS_CATEGORY) {
        if(gl->y_axis.min_value <= -ya->offset  &&  -ya->offset <= gl->y_axis.max_value) {
            pt0.x = gl->x_axis.coord0;
            pt0.y = grid_map_y(-ya->offset, gl);
            pt1.x = gl->x_axis.coord1;
            pt1.y = pt0.y;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
        }
    }

    /* Paint X-axis labels */
    if(gl->x_axis.type != GRID_AXIS_CATEGORY)
        pt0.x = gl->x_axis.coord0;
    else
        pt0.x = gl->x_axis.coord0 + 0.5f * gl->x_axis.coord_delta;
    pt0.y = gl->x_axis.coord_labels;
    for(v = gl->x_axis.grid_base; v <= gl->x_axis.max_value; v += gl->x_axis.grid_delta) {
        chart_str_value(xa, v, buffer);
        len = wcslen(buffer);
        text_layout = xdwrite_create_text_layout(buffer, len, chart->text_fmt, 0.0f, 0.0f,
                    XDWRITE_NOWRAP | XDWRITE_ALIGN_CENTER | XDWRITE_VALIGN_BOTTOM);
        if(text_layout != NULL) {
            c_DWRITE_TEXT_RANGE range = { 0, len };
            c_IDWriteTextLayout_SetFontSize(text_layout, gl->label_font_size, range);
            c_ID2D1RenderTarget_DrawTextLayout(rt, pt0, text_layout,
                        (c_ID2D1Brush*) ctx->solid_brush, 0);
            c_IDWriteTextLayout_Release(text_layout);
        }

        pt0.x += gl->x_axis.coord_delta;
    }

    /* Paint Y-axis labels */
    pt0.x = gl->y_axis.coord_labels;
    if(gl->y_axis.type != GRID_AXIS_CATEGORY)
        pt0.y = gl->y_axis.coord1;
    else
        pt0.y = gl->y_axis.coord1 - 0.5f * gl->y_axis.coord_delta;
    for(v = gl->y_axis.grid_base; v <= gl->y_axis.max_value; v += gl->y_axis.grid_delta) {
        chart_str_value(ya, v, buffer);
        len = wcslen(buffer);
        text_layout = xdwrite_create_text_layout(buffer, len, chart->text_fmt, 0.0f, 0.0f,
                    XDWRITE_NOWRAP | XDWRITE_ALIGN_RIGHT | XDWRITE_VALIGN_CENTER);
        if(text_layout != NULL) {
            c_DWRITE_TEXT_RANGE range = { 0, len };
            c_IDWriteTextLayout_SetFontSize(text_layout, gl->label_font_size, range);
            c_ID2D1RenderTarget_DrawTextLayout(rt, pt0, text_layout,
                        (c_ID2D1Brush*) ctx->solid_brush, 0);
            c_IDWriteTextLayout_Release(text_layout);
        }

        pt0.y -= gl->y_axis.coord_delta;
    }
}


/*********************
 *** Scatter Chart ***
 *********************/

static void
scatter_paint(chart_t* chart, chart_xd2d_ctx_t* ctx, const chart_layout_t* layout)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    cache_t cache;
    grid_layout_t gl;
    int set_ix, n;
    int i;
    chart_data_t* data;
    c_D2D1_COLOR_F c;
    c_D2D1_ELLIPSE e;

    CACHE_INIT(&cache, chart);

    grid_calc_layout(chart, layout, &cache, &gl);
    grid_paint(chart, ctx, &gl);

    /* Paint aura for hot value(s). */
    if(chart->hot_set_ix >= 0) {
        int i0, i1;

        set_ix = chart->hot_set_ix;
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

        xd2d_color_set_cref(&c, color_hint(chart_data_color(chart, set_ix)));
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

        if(chart->hot_i >= 0) {
            i0 = chart->hot_i;
            i1 = chart->hot_i+1;
        } else {
            i0 = 0;
            i1 = data->count-1;
        }

        e.radiusX = 4.0f;
        e.radiusY = 4.0f;
        for(i = i0; i < i1; i += 2) {
            e.point.x = grid_map_x(CACHE_VALUE(&cache, set_ix, i), &gl);
            e.point.y = grid_map_y(CACHE_VALUE(&cache, set_ix, i+1), &gl);
            c_ID2D1RenderTarget_FillEllipse(rt, &e, (c_ID2D1Brush*) ctx->solid_brush);
        }
    }

    /* Paint all data sets. */
    n = dsa_size(&chart->data);
    e.radiusX = 2.0f;
    e.radiusY = 2.0f;
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

        xd2d_color_set_cref(&c, chart_data_color(chart, set_ix));
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

        for(i = 0; i < data->count-1; i += 2) {  /* '-1' to protect if ->count is odd */
            e.point.x = grid_map_x(CACHE_VALUE(&cache, set_ix, i), &gl);
            e.point.y = grid_map_y(CACHE_VALUE(&cache, set_ix, i+1), &gl);
            c_ID2D1RenderTarget_FillEllipse(rt, &e, (c_ID2D1Brush*) ctx->solid_brush);
        }
    }

    CACHE_FINI(&cache);
}

static void
scatter_hit_test(chart_t* chart, const chart_layout_t* layout,
                 int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    grid_layout_t gl;
    int set_ix, n, i;
    chart_data_t* data;
    float rx = (float) x;
    float ry = (float) y;
    float dist2;

    n = dsa_size(&chart->data);
    dist2 = GetSystemMetrics(SM_CXDOUBLECLK) * GetSystemMetrics(SM_CYDOUBLECLK);

    CACHE_INIT(&cache, chart);
    grid_calc_layout(chart, layout, &cache, &gl);

    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        for(i = 0; i < data->count-1; i += 2) {
            float dx, dy;

            dx = rx - grid_map_x(CACHE_VALUE(&cache, set_ix, i), &gl);
            dy = ry - grid_map_y(CACHE_VALUE(&cache, set_ix, i+1), &gl);

            if(dx * dx + dy * dy < dist2) {
                *p_set_ix = set_ix;
                *p_i = i;
                dist2 = dx * dx + dy * dy;
            }
        }
    }

    CACHE_FINI(&cache);
}

static void
scatter_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    if(chart->hot_set_ix >= 0  &&  chart->hot_i >= 0) {
        int x, y;
        WCHAR x_str[CHART_STR_VALUE_MAX_LEN];
        WCHAR y_str[CHART_STR_VALUE_MAX_LEN];

        x = chart_value(chart, chart->hot_set_ix, chart->hot_i);
        y = chart_value(chart, chart->hot_set_ix, chart->hot_i+1);

        chart_str_value(&chart->axis1, x, x_str);
        chart_str_value(&chart->axis2, y, y_str);

        _sntprintf(buffer, bufsize, _T("%ls / %ls"), x_str, y_str);
        buffer[bufsize-1] = _T('\0');
    }
}


/*************************
 *** Line & Area Chart ***
 *************************/

static void
line_paint_area(chart_t* chart, cache_t* cache, int set_ix, BOOL is_stacked,
                chart_xd2d_ctx_t* ctx, const grid_layout_t* gl)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    c_ID2D1PathGeometry* path;
    c_ID2D1GeometrySink* sink;
    c_D2D1_POINT_2F pt;
    BYTE alpha;
    c_D2D1_COLOR_F c;
    int x0, x1, x, y;

    x0 = 0;
    x1 = DSA_ITEM(&chart->data, set_ix, chart_data_t)->count;
    if(x1 < 1)
        return;

    if(set_ix == chart->hot_set_ix)
        alpha = 63;
    else
        alpha = 31;
    xd2d_color_set_crefa(&c, chart_data_color(chart, set_ix), alpha);
    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

    path = xd2d_CreatePathGeometry(&sink);
    if(MC_ERR(path == NULL)) {
        MC_TRACE("line_paint_area: xd2d_CreatePathGeometry() failed.");
        return;
    }

    pt.x = gl->x_axis.coord0;
    pt.y = gl->y_axis.coord1;
    c_ID2D1GeometrySink_BeginFigure(sink, pt, c_D2D1_FIGURE_BEGIN_FILLED);
    for(x = x0; x < x1; x++) {
        if(is_stacked)
            y = cache_stack(cache, set_ix, x);
        else
            y = CACHE_VALUE(cache, set_ix, x);

        pt.x = grid_map_x(x, gl);
        pt.y = grid_map_y(y, gl);
        c_ID2D1GeometrySink_AddLine(sink, pt);
    }
    pt.y = gl->y_axis.coord1;
    c_ID2D1GeometrySink_AddLine(sink, pt);
    if(is_stacked  &&  set_ix > 0) {
        for(x = x1-1; x >= x0; x--) {
            y = cache_stack(cache, set_ix-1, x);

            pt.x = grid_map_x(x, gl);
            pt.y = grid_map_y(y, gl);
            c_ID2D1GeometrySink_AddLine(sink, pt);
        }
    }
    c_ID2D1GeometrySink_EndFigure(sink, c_D2D1_FIGURE_END_CLOSED);

    c_ID2D1GeometrySink_Close(sink);
    c_ID2D1GeometrySink_Release(sink);

    c_ID2D1RenderTarget_FillGeometry(rt, (c_ID2D1Geometry*) path,
                (c_ID2D1Brush*) ctx->solid_brush, NULL);
    c_ID2D1PathGeometry_Release(path);
}

static void
line_paint_lines(chart_t* chart, cache_t* cache, int set_ix, BOOL is_stacked,
                 chart_xd2d_ctx_t* ctx, const grid_layout_t* gl, BOOL is_aura)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    int x0, x1, x, y;
    float line_width;
    c_D2D1_POINT_2F pt0, pt1;
    c_D2D1_COLOR_F c;
    c_D2D1_ELLIPSE e;

    if(is_aura) {
        if(chart->hot_i >= 0) {
            x0 = chart->hot_i;
            x1 = chart->hot_i+1;
        } else {
            x0 = 0;
            x1 = DSA_ITEM(&chart->data, set_ix, chart_data_t)->count;
        }
        xd2d_color_set_cref(&c, color_hint(chart_data_color(chart, set_ix)));
        e.radiusX = 4.0f;
        e.radiusY = 4.0f;
        line_width = 3.5f;
    } else {
        x0 = 0;
        x1 = DSA_ITEM(&chart->data, set_ix, chart_data_t)->count;
        xd2d_color_set_cref(&c, chart_data_color(chart, set_ix));
        e.radiusX = 2.0f;
        e.radiusY = 2.0f;
        line_width = 1.0f;
    }

    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);

    for(x = x0; x < x1; x++) {
        if(is_stacked)
            y = cache_stack(cache, set_ix, x);
        else
            y = CACHE_VALUE(cache, set_ix, x);

        pt1.x = grid_map_x(x, gl);
        pt1.y = grid_map_y(y, gl);
        if(x > x0) {
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1,
                    (c_ID2D1Brush*) ctx->solid_brush, line_width, NULL);
        }

        e.point.x = pt1.x;
        e.point.y = pt1.y;
        c_ID2D1RenderTarget_FillEllipse(rt, &e, (c_ID2D1Brush*) ctx->solid_brush);

        pt0.x = pt1.x;
        pt0.y = pt1.y;
    }
}

static void
line_paint(chart_t* chart, BOOL is_area, BOOL is_stacked, chart_xd2d_ctx_t* ctx,
           const chart_layout_t* layout)
{
    cache_t cache;
    grid_layout_t gl;
    int set_ix, n;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    grid_calc_layout(chart, layout, &cache, &gl);
    grid_paint(chart, ctx, &gl);

    /* Paint areas */
    if(is_area) {
        for(set_ix = 0; set_ix < n; set_ix++)
            line_paint_area(chart, &cache, set_ix, is_stacked, ctx, &gl);
    }

    /* Paint aura */
    if(chart->hot_set_ix >= 0)
        line_paint_lines(chart, &cache, chart->hot_set_ix, is_stacked, ctx, &gl, TRUE);

    /* Paint all data sets */
    for(set_ix = 0; set_ix < n; set_ix++)
        line_paint_lines(chart, &cache, set_ix, is_stacked, ctx, &gl, FALSE);

    CACHE_FINI(&cache);
}

static void
line_hit_test(chart_t* chart, BOOL is_stacked, const chart_layout_t* layout,
              int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    grid_layout_t gl;
    int set_ix, n, i;
    float ri;
    int i0, i1;
    float rx = (float) x;
    float ry = (float) y;
    int max_dx, max_dy;
    float dist2;

    n = dsa_size(&chart->data);
    max_dx = GetSystemMetrics(SM_CXDOUBLECLK);
    max_dy = GetSystemMetrics(SM_CYDOUBLECLK);
    dist2 = max_dx * max_dy;

    CACHE_INIT(&cache, chart);
    grid_calc_layout(chart, layout, &cache, &gl);

    /* Find index which maps horizontally close enough to the X mouse
     * coordinate. */
    ri = grid_unmap_x(rx, &gl);

    /* Any close hot point must be close of the ri */
    i0 = (int) floorf(ri - MC_MAX(max_dx, max_dy));
    i1 = (int) ceilf(ri + MC_MAX(max_dx, max_dy));

    for(set_ix = 0; set_ix < n; set_ix++) {
        int ii0 = MC_MAX(i0, 0);
        int ii1 = MC_MIN(i1, DSA_ITEM(&chart->data, set_ix, chart_data_t)->count);

        for(i = ii0; i <= ii1; i++) {
            int y;
            float dx, dy;

            if(is_stacked)
                y = cache_stack(&cache, set_ix, i);
            else
                y = CACHE_VALUE(&cache, set_ix, i);

            dx = rx - grid_map_x(i, &gl);
            dy = ry - grid_map_y(y, &gl);

            if(dx * dx + dy * dy < dist2) {
                *p_set_ix = set_ix;
                *p_i = i;
                dist2 = dx * dx + dy * dy;
            }
        }
    }
    CACHE_FINI(&cache);
}

static inline void
line_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->axis2, buffer, bufsize);
}


/********************
 *** Column Chart ***
 ********************/

typedef struct column_layout_tag column_layout_t;
struct column_layout_tag {
    float group_delta;
    float col_width;
    float col_delta;
    float col0_offset;
};

static void
column_calc_layout(chart_t* chart, BOOL is_stacked, const grid_axis_t* axis,
                   column_layout_t* col_layout)
{
    int n = (is_stacked ? 1 : dsa_size(&(chart)->data));
    float group_width;

    col_layout->group_delta = axis->coord_delta;
    group_width = col_layout->group_delta / MC_PHIf;
    col_layout->col_width = group_width / (n + (n-1)/MC_PHIf);
    col_layout->col_delta = col_layout->col_width * (1.0f + 1.0f/MC_PHIf);
    col_layout->col0_offset = floorf(0.5f * (col_layout->group_delta - group_width));
    if(col_layout->col_width > 3.0f)
        col_layout->col_width = floorf(col_layout->col_width);
    if(col_layout->col_delta > 3.0f)
        col_layout->col_delta = ceilf(col_layout->col_delta);
}

static void
column_paint(chart_t* chart, BOOL is_stacked, chart_xd2d_ctx_t* ctx,
             const chart_layout_t* layout)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    cache_t cache;
    grid_layout_t gl;
    column_layout_t cl;
    int set_ix, i, n;
    chart_data_t* data;
    c_D2D1_RECT_F r;
    c_D2D1_POINT_2F pt0, pt1;
    c_D2D1_COLOR_F c, c_full;

    CACHE_INIT(&cache, chart);

    grid_calc_layout(chart, layout, &cache, &gl);
    grid_paint(chart, ctx, &gl);

    column_calc_layout(chart, is_stacked, &gl.x_axis, &cl);

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        xd2d_color_set_cref(&c_full, chart_data_color(chart, set_ix));
        xd2d_color_set_crefa(&c, chart_data_color(chart, set_ix), 175);

        r.left = gl.x_axis.coord0 + cl.col0_offset;
        if(!is_stacked)
            r.left += set_ix * cl.col_delta;
        for(i = 0; i < data->count; i++) {
            if(set_ix == chart->hot_set_ix  &&  (i == chart->hot_i || chart->hot_i < 0))
                c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c_full);
            else
                c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
            r.right = r.left + cl.col_width;
            if(!is_stacked  ||  set_ix == 0) {
                r.top = grid_map_y(CACHE_VALUE(&cache, set_ix, i), &gl);
                r.bottom = grid_map_y(0, &gl);
            } else {
                r.top = grid_map_y(cache_stack(&cache, set_ix, i), &gl);
                r.bottom = grid_map_y(cache_stack(&cache, set_ix-1, i), &gl);
            }
            c_ID2D1RenderTarget_FillRectangle(rt, &r, (c_ID2D1Brush*) ctx->solid_brush);

            pt0.x = r.left;
            pt0.y = r.bottom - 0.5f;
            pt1.x = r.left;
            pt1.y = r.top;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
            pt0.x = r.right;
            pt1.x = r.right;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
            pt0.x = r.left;
            pt0.y = r.top;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);

            r.left += cl.group_delta;
        }
    }

    CACHE_FINI(&cache);
}

static void
column_hit_test(chart_t* chart, BOOL is_stacked, chart_layout_t* layout,
                int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    grid_layout_t gl;
    column_layout_t cl;
    int set_ix, i, n;
    chart_data_t* data;
    c_D2D1_RECT_F r;

    CACHE_INIT(&cache, chart);

    grid_calc_layout(chart, layout, &cache, &gl);
    column_calc_layout(chart, is_stacked, &gl.x_axis, &cl);

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

        r.left = gl.x_axis.coord0 + cl.col0_offset;
        if(!is_stacked)
            r.left += set_ix * cl.col_delta;
        for(i = 0; i < data->count; i++) {
            r.right = r.left + cl.col_width;
            if(!is_stacked  ||  set_ix == 0) {
                r.top = grid_map_y(CACHE_VALUE(&cache, set_ix, i), &gl);
                r.bottom = grid_map_y(0, &gl);
            } else {
                r.top = grid_map_y(cache_stack(&cache, set_ix, i), &gl);
                r.bottom = grid_map_y(cache_stack(&cache, set_ix-1, i), &gl);
            }

            if(r.left <= x  &&  x <= r.right  &&  r.top <= y  &&  y <= r.bottom) {
                *p_set_ix = set_ix;
                *p_i = i;
                goto out;
            }

            r.left += cl.group_delta;
        }
    }

out:
    CACHE_FINI(&cache);
}

static inline void
column_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->axis2, buffer, bufsize);
}


/*****************
 *** Bar Chart ***
 *****************/

typedef column_layout_t bar_layout_t;

static inline void
bar_calc_layout(chart_t* chart, BOOL is_stacked, const grid_axis_t* axis,
                bar_layout_t* bar_layout)
{
    column_calc_layout(chart, is_stacked, axis, bar_layout);
}

static void
bar_paint(chart_t* chart, BOOL is_stacked, chart_xd2d_ctx_t* ctx,
          const chart_layout_t* layout)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    cache_t cache;
    grid_layout_t gl;
    bar_layout_t bl;
    int set_ix, i, n;
    chart_data_t* data;
    c_D2D1_RECT_F r;
    c_D2D1_POINT_2F pt0, pt1;
    c_D2D1_COLOR_F c, c_full;

    CACHE_INIT(&cache, chart);

    grid_calc_layout(chart, layout, &cache, &gl);
    grid_paint(chart, ctx, &gl);

    bar_calc_layout(chart, is_stacked, &gl.y_axis, &bl);

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        xd2d_color_set_cref(&c_full, chart_data_color(chart, set_ix));
        xd2d_color_set_crefa(&c, chart_data_color(chart, set_ix), 175);

        r.bottom = gl.y_axis.coord1 - bl.col0_offset;
        if(!is_stacked)
            r.bottom -= (n-1 - set_ix) * bl.col_delta;
        for(i = 0; i < data->count; i++) {
            if(set_ix == chart->hot_set_ix  &&  (i == chart->hot_i || chart->hot_i < 0))
                c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c_full);
            else
                c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
            r.top = r.bottom - bl.col_width;
            if(!is_stacked  ||  set_ix == 0) {
                r.left = grid_map_x(0, &gl);
                r.right = grid_map_x(CACHE_VALUE(&cache, set_ix, i), &gl);
            } else {
                r.left = grid_map_x(cache_stack(&cache, set_ix-1, i), &gl);
                r.right = grid_map_x(cache_stack(&cache, set_ix, i), &gl);
            }
            c_ID2D1RenderTarget_FillRectangle(rt, &r, (c_ID2D1Brush*) ctx->solid_brush);

            pt0.x = r.left + 0.5f;
            pt0.y = r.bottom;
            pt1.x = r.right;
            pt1.y = r.bottom;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
            pt0.y = r.top;
            pt1.y = r.top;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);
            pt0.x = r.right;
            pt0.y = r.bottom;
            c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);

            r.bottom -= bl.group_delta;
        }
    }

    CACHE_FINI(&cache);
}

static void
bar_hit_test(chart_t* chart, BOOL is_stacked, chart_layout_t* layout,
             int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    grid_layout_t gl;
    column_layout_t bl;
    int set_ix, i, n;
    chart_data_t* data;
    c_D2D1_RECT_F r;

    CACHE_INIT(&cache, chart);

    grid_calc_layout(chart, layout, &cache, &gl);
    bar_calc_layout(chart, is_stacked, &gl.y_axis, &bl);

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

        r.bottom = gl.y_axis.coord1 - bl.col0_offset;
        if(!is_stacked)
            r.bottom -= (n-1 - set_ix) * bl.col_delta;
        for(i = 0; i < data->count; i++) {
            r.top = r.bottom - bl.col_width;
            if(!is_stacked  ||  set_ix == 0) {
                r.left = grid_map_x(0, &gl);
                r.right = grid_map_x(CACHE_VALUE(&cache, set_ix, i), &gl);
            } else {
                r.left = grid_map_x(cache_stack(&cache, set_ix-1, i), &gl);
                r.right = grid_map_x(cache_stack(&cache, set_ix, i), &gl);
            }

            if(r.left <= x  &&  x <= r.right  &&  r.top <= y  &&  y <= r.bottom) {
                *p_set_ix = set_ix;
                *p_i = i;
                goto out;
            }

            r.bottom -= bl.group_delta;
        }
    }

out:
    CACHE_FINI(&cache);
}

static inline void
bar_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->axis2, buffer, bufsize);
}


/******************************
 *** Chart Legend Functions ***
 ******************************/

static void
legend_paint(chart_t* chart, chart_xd2d_ctx_t* ctx, const chart_layout_t* layout)
{
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    float font_height;
    float color_size;
    c_D2D1_RECT_F label_rect;
    c_D2D1_RECT_F color_rect;
    int set_ix, n;

    if(chart->text_fmt == NULL)
        return;

    font_height = c_IDWriteTextFormat_GetFontSize(chart->text_fmt);
    color_size = roundf(((float)chart->font_cap_height * font_height) / (float)chart->font_units_per_height);

    label_rect.left = layout->legend_rect.left + color_size + MC_MIN(4.0f, ceilf(0.3f * font_height));
    label_rect.top = layout->legend_rect.top;
    label_rect.right = layout->legend_rect.right;
    label_rect.bottom = layout->legend_rect.bottom;

    color_rect.left = layout->legend_rect.left;
    color_rect.right = color_rect.left + color_size;

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        c_D2D1_COLOR_F c;
        TCHAR buf[20];
        TCHAR* name;
        c_IDWriteTextLayout* text_layout;
        c_D2D1_POINT_2F pt;
        c_DWRITE_TEXT_METRICS text_metrics;
        c_DWRITE_LINE_METRICS line_metrics[4];
        UINT32 dummy;

        if(data->name != NULL) {
            name = data->name;
        } else {
            _stprintf(buf, _T("data-set-%d"), set_ix);
            name = buf;
        }

        /* Paint legend label */
        text_layout = xdwrite_create_text_layout(name, _tcslen(name), chart->text_fmt,
                    label_rect.right - label_rect.left, label_rect.bottom - label_rect.top,
                    XDWRITE_ALIGN_LEFT | XDWRITE_VALIGN_TOP);
        if(text_layout == NULL)
            continue;
        xd2d_color_set_rgb(&c, 0, 0, 0);
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
        pt.x = label_rect.left;
        pt.y = label_rect.top;
        c_ID2D1RenderTarget_DrawTextLayout(rt, pt, text_layout, (c_ID2D1Brush*) ctx->solid_brush, 0);
        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
        c_IDWriteTextLayout_GetLineMetrics(text_layout, line_metrics, text_metrics.lineCount, &dummy);
        c_IDWriteTextLayout_Release(text_layout);

        /* Paint legend color */
        color_rect.bottom = floorf(label_rect.top + line_metrics[0].baseline);
        color_rect.top = color_rect.bottom - color_size;
        xd2d_color_set_cref(&c, chart_data_color(chart, set_ix));
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
        c_ID2D1RenderTarget_FillRectangle(rt, &color_rect, (c_ID2D1Brush*) ctx->solid_brush);
        c_ID2D1RenderTarget_DrawRectangle(rt, &color_rect, (c_ID2D1Brush*) ctx->solid_brush, 1.0f, NULL);

        /* Paint active aura */
        if(set_ix == chart->hot_set_ix) {
            c_D2D1_RECT_F aura_rect = { color_rect.left - 2.5f, color_rect.top - 2.5f,
                                        color_rect.right + 2.5f, color_rect.bottom + 2.5f };
            xd2d_color_set_cref(&c, color_hint(chart_data_color(chart, set_ix)));
            c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
            c_ID2D1RenderTarget_DrawRectangle(rt, &aura_rect, (c_ID2D1Brush*) ctx->solid_brush, 2.0f, NULL);
        }

        /* Recalculate coordinates for the next data series */
        label_rect.top += text_metrics.height;
        if(label_rect.top >= label_rect.bottom)
            break;
    }
}

static int
legend_hit_test(chart_t* chart, chart_layout_t* layout, int x, int y)
{
    float font_height;
    float color_size;
    c_D2D1_RECT_F label_rect;
    int set_ix, n;

    if(MC_ERR(chart->text_fmt == NULL))
        return -1;

    font_height = c_IDWriteTextFormat_GetFontSize(chart->text_fmt);
    color_size = roundf(((float)chart->font_cap_height * font_height) / (float)chart->font_units_per_height);

    label_rect.left = layout->legend_rect.left + color_size + MC_MIN(4.0f, ceilf(0.3f * font_height));
    label_rect.top = layout->legend_rect.top;
    label_rect.right = layout->legend_rect.right;
    label_rect.bottom = layout->legend_rect.bottom;

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        TCHAR buf[20];
        TCHAR* name;
        c_IDWriteTextLayout* text_layout;
        c_DWRITE_TEXT_METRICS text_metrics;
        BOOL is_inside = FALSE;

        if(data->name != NULL) {
            name = data->name;
        } else {
            _stprintf(buf, _T("data-set-%d"), set_ix);
            name = buf;
        }
        text_layout = xdwrite_create_text_layout(name, _tcslen(name), chart->text_fmt,
                    label_rect.right - label_rect.left, label_rect.bottom - label_rect.top,
                    XDWRITE_ALIGN_LEFT | XDWRITE_VALIGN_TOP);
        if(text_layout == NULL)
            continue;

        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);

        if(label_rect.top <= y  &&  y < label_rect.top + text_metrics.height) {
            if(x < label_rect.left)
                return set_ix;

            if(label_rect.left <= x  &&  x < label_rect.left + text_metrics.width) {
                c_DWRITE_HIT_TEST_METRICS ht_metrics;
                BOOL is_trailing_hit;

                c_IDWriteTextLayout_HitTestPoint(text_layout,
                            x - label_rect.left, y - label_rect.top,
                            &is_trailing_hit, &is_inside, &ht_metrics);
            }
        }

        c_IDWriteTextLayout_Release(text_layout);

        if(is_inside)
            return set_ix;

        /* Recalculate coordinates for the next data series */
        label_rect.top += text_metrics.height;
        if(label_rect.top >= label_rect.bottom)
            break;
    }

    return -1;
}


/******************************
 *** Core Control Functions ***
 ******************************/

static void
chart_calc_layout(chart_t* chart, chart_layout_t* layout)
{
    TCHAR buf[2];
    RECT rect;

    GetClientRect(chart->win, &rect);
    // FIXME: we should use DirectWrite metrics here.
    mc_font_size(chart->gdi_font, &layout->font_size, FALSE);

    layout->margin = (layout->font_size.cy+1) / 2;

    GetWindowText(chart->win, buf, MC_SIZEOF_ARRAY(buf));
    if(buf[0] != _T('\0')) {
        layout->title_rect.left = rect.left + layout->margin;
        layout->title_rect.top = rect.top + layout->margin;
        layout->title_rect.right = rect.right - layout->margin;
        layout->title_rect.bottom = layout->title_rect.top + layout->font_size.cy;
    } else {
        mc_rect_set(&layout->title_rect, 0, 0, 0, 0);
    }

    layout->legend_rect.left = rect.right - layout->margin - 12 * layout->font_size.cx;
    layout->legend_rect.top = layout->title_rect.bottom + layout->margin;
    layout->legend_rect.right = rect.right - layout->margin;
    layout->legend_rect.bottom = rect.bottom - layout->margin;

    layout->body_rect.left = rect.left + layout->margin;
    layout->body_rect.top = layout->title_rect.bottom + layout->margin;
    layout->body_rect.right = layout->legend_rect.left - layout->margin;
    layout->body_rect.bottom = rect.bottom - layout->margin;
}

static void
chart_paint(void* ctrl, xd2d_ctx_t* raw_ctx)
{
    chart_t* chart = (chart_t*) ctrl;
    chart_xd2d_ctx_t* ctx = (chart_xd2d_ctx_t*) raw_ctx;
    c_ID2D1RenderTarget* rt = ctx->ctx.rt;
    chart_layout_t layout;
    c_D2D1_MATRIX_3X2_F matrix;
	DWORD type;

    if(ctx->ctx.erase) {
        c_D2D1_COLOR_F c = XD2D_COLOR_CREF(GetSysColor(COLOR_WINDOW));
        c_ID2D1RenderTarget_Clear(rt, &c);
    }

    /* Transform to make [0,0] to fit in the pixel matrix. */
    matrix._11 = 1.0f;  matrix._12 = 0.0f;
    matrix._21 = 0.0f;  matrix._22 = 1.0f;
    matrix._31 = 0.5f;  matrix._32 = 0.5f;
    c_ID2D1RenderTarget_SetTransform(rt, &matrix);

    chart_calc_layout(chart, &layout);

    /* Paint title */
    if(!mc_rect_is_empty(&layout.title_rect)) {
        TCHAR title[256];
        c_IDWriteTextLayout* text_layout;
        int len;

        len = GetWindowText(chart->win, title, MC_SIZEOF_ARRAY(title));
        text_layout = xdwrite_create_text_layout(title, len, chart->text_fmt,
                    mc_width(&layout.title_rect), mc_height(&layout.title_rect),
                    XDWRITE_ALIGN_CENTER | XDWRITE_NOWRAP | XDWRITE_ELLIPSIS_END);
        if(text_layout != NULL) {
            c_D2D1_COLOR_F c = XD2D_COLOR_RGB(0,0,0);
            c_D2D1_POINT_2F pt = { layout.title_rect.left, layout.title_rect.top };
            c_DWRITE_TEXT_RANGE range = { 0, len };

            c_IDWriteTextLayout_SetFontWeight(text_layout, FW_MEDIUM, range);
            c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &c);
            c_ID2D1RenderTarget_DrawTextLayout(rt, pt, text_layout, (c_ID2D1Brush*) ctx->solid_brush, 0);
            c_IDWriteTextLayout_Release(text_layout);
        }
    }

    /* Paint legend */
    legend_paint(chart, ctx, &layout);

    /* Paint the chart body */
    type = (chart->style & MC_CHS_TYPEMASK);
    switch(type) {
        case MC_CHS_PIE:
            pie_paint(chart, ctx, &layout);
            break;

        case MC_CHS_SCATTER:
            scatter_paint(chart, ctx, &layout);
            break;

        case MC_CHS_LINE:
        case MC_CHS_STACKEDLINE:
        case MC_CHS_AREA:
        case MC_CHS_STACKEDAREA:
        {
            BOOL is_stacked = (type == MC_CHS_STACKEDLINE || type == MC_CHS_STACKEDAREA);
            BOOL is_area = (type == MC_CHS_AREA || type == MC_CHS_STACKEDAREA);
            line_paint(chart, is_area, is_stacked, ctx, &layout);
            break;
        }

        case MC_CHS_COLUMN:
        case MC_CHS_STACKEDCOLUMN:
        {
            BOOL is_stacked = (type == MC_CHS_STACKEDCOLUMN);
            column_paint(chart, is_stacked, ctx, &layout);
            break;
        }

        case MC_CHS_BAR:
        case MC_CHS_STACKEDBAR:
        {
            BOOL is_stacked = (type == MC_CHS_STACKEDBAR);
            bar_paint(chart, is_stacked, ctx, &layout);
            break;
        }
    }
}

static void
chart_hit_test(chart_t* chart, int x, int y, int* set_ix, int* i)
{
    chart_layout_t layout;

    chart_calc_layout(chart, &layout);

    *set_ix = -1;
    *i = -1;

    if(mc_rect_contains_xy(&layout.legend_rect, x, y)) {
        *set_ix = legend_hit_test(chart, &layout, x, y);
    } else if(mc_rect_contains_xy(&layout.body_rect, x, y)) {
        DWORD type = (chart->style & MC_CHS_TYPEMASK);
        switch(type) {
            case MC_CHS_PIE:
                pie_hit_test(chart, &layout, x, y, set_ix, i);
                return;

            case MC_CHS_SCATTER:
                scatter_hit_test(chart, &layout, x, y, set_ix, i);
                return;

            case MC_CHS_STACKEDLINE:
            case MC_CHS_STACKEDAREA:
            case MC_CHS_LINE:
            case MC_CHS_AREA:
            {
                BOOL is_stacked = (type == MC_CHS_STACKEDLINE || type == MC_CHS_STACKEDAREA);
                line_hit_test(chart, is_stacked, &layout, x, y, set_ix, i);
                return;
            }

            case MC_CHS_STACKEDCOLUMN:
            case MC_CHS_COLUMN:
            {
                BOOL is_stacked = (type == MC_CHS_STACKEDCOLUMN);
                column_hit_test(chart, is_stacked, &layout, x, y, set_ix, i);
                return;
            }

            case MC_CHS_STACKEDBAR:
            case MC_CHS_BAR:
            {
                BOOL is_stacked = (type == MC_CHS_STACKEDBAR);
                bar_hit_test(chart, is_stacked, &layout, x, y, set_ix, i);
                return;
            }
        }
    }
}

static void
chart_update_tooltip(chart_t* chart)
{
    TCHAR buffer[256];

    if(chart->tooltip_win == NULL)
        return;

    buffer[0] = _T('\0');

    if(chart->hot_set_ix >= 0) {
        switch(chart->style & MC_CHS_TYPEMASK) {
            case MC_CHS_PIE:
                pie_tooltip_text(chart, buffer, MC_SIZEOF_ARRAY(buffer));
                break;

            case MC_CHS_SCATTER:
                scatter_tooltip_text(chart, buffer, MC_SIZEOF_ARRAY(buffer));
                break;

            case MC_CHS_LINE:
            case MC_CHS_STACKEDLINE:
            case MC_CHS_AREA:
            case MC_CHS_STACKEDAREA:
                line_tooltip_text(chart, buffer, MC_SIZEOF_ARRAY(buffer));
                break;

            case MC_CHS_COLUMN:
            case MC_CHS_STACKEDCOLUMN:
                column_tooltip_text(chart, buffer, MC_SIZEOF_ARRAY(buffer));
                break;

            case MC_CHS_BAR:
            case MC_CHS_STACKEDBAR:
                bar_tooltip_text(chart, buffer, MC_SIZEOF_ARRAY(buffer));
                break;
        }
    }

    if(buffer[0] != _T('\0')) {
        tooltip_update_text(chart->tooltip_win, chart->win, buffer);

        if(!chart->tooltip_active) {
            tooltip_show_tracking(chart->tooltip_win, chart->win, TRUE);
            chart->tooltip_active = TRUE;
        }
    } else {
        if(chart->tooltip_active) {
            tooltip_show_tracking(chart->tooltip_win, chart->win, FALSE);
            chart->tooltip_active = FALSE;
        }
    }
}

static void
chart_mouse_move(chart_t* chart, int x, int y)
{
    int set_ix, i;

    if(!IsWindowEnabled(chart->win))
        return;

    chart_hit_test(chart, x, y, &set_ix, &i);

    if(!chart->tracking_leave  &&  set_ix >= 0) {
        mc_track_mouse(chart->win, TME_LEAVE);
        chart->tracking_leave = TRUE;
    }

    if(chart->hot_set_ix != set_ix  ||  chart->hot_i != i) {
        chart->hot_set_ix = set_ix;
        chart->hot_i = i;
        chart_update_tooltip(chart);

        if(!chart->no_redraw)
            xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
    }

    if(chart->tooltip_win != NULL  &&  chart->tooltip_active) {
        SIZE tip_size;

        tooltip_size(chart->tooltip_win, &tip_size);
        tooltip_move_tracking(chart->tooltip_win, chart->win,
                    x - tip_size.cx / 2, y - tip_size.cy - 5);
    }
}

static void
chart_mouse_leave(chart_t* chart)
{
    chart->tracking_leave = FALSE;

    if(!IsWindowEnabled(chart->win))
        return;

    if(chart->hot_set_ix != -1  ||  chart->hot_i != -1) {
        chart->hot_set_ix = -1;
        chart->hot_i = -1;
        chart_update_tooltip(chart);

        if(!chart->no_redraw)
            xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
    }
}

static void
chart_setup_hot(chart_t* chart)
{
    if(IsWindowEnabled(chart->win)) {
        DWORD pos = GetMessagePos();
        chart_hit_test(chart, GET_X_LPARAM(pos), GET_Y_LPARAM(pos),
                        &chart->hot_set_ix, &chart->hot_i);
    } else {
        chart->hot_set_ix = -1;
        chart->hot_i = -1;
    }

    chart_update_tooltip(chart);
}

static int
chart_insert_dataset(chart_t* chart, int set_ix, MC_CHDATASET* dataset)
{
    int* values;
    chart_data_t* data;

    if(MC_ERR(set_ix < 0)) {
        MC_TRACE("chart_insert_dataset: Invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    if(MC_ERR(dataset->dwCount == 0)) {
        MC_TRACE("chart_insert_dataset: Data set cannot be empty");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    if(set_ix > dsa_size(&chart->data))
        set_ix = dsa_size(&chart->data);

    if(dataset->piValues != NULL) {
        values = (int*) malloc(sizeof(int) * dataset->dwCount);
        if(MC_ERR(values == NULL)) {
            MC_TRACE("chart_insert_dataset: malloc() failed.");
            mc_send_notify(chart->notify_win, chart->win, NM_OUTOFMEMORY);
            return -1;
        }
        memcpy(values, dataset->piValues, sizeof(int) * dataset->dwCount);
    } else {
        values = NULL;
    }

    data = (chart_data_t*) dsa_insert_raw(&chart->data, set_ix);
    if(MC_ERR(data == NULL)) {
        MC_TRACE("chart_insert_dataset: dsa_insert_raw() failed.");
        mc_send_notify(chart->notify_win, chart->win, NM_OUTOFMEMORY);
        if(values)
            free(values);
        return -1;
    }

    data->name = NULL;
    data->color = MC_CLR_DEFAULT;
    data->count = dataset->dwCount;
    data->values = values;

    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return set_ix;
}

static BOOL
chart_delete_dataset(chart_t* chart, int set_ix)
{
    if(set_ix < 0  ||  set_ix >= dsa_size(&chart->data)) {
        MC_TRACE("chart_delete_dataset: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    dsa_remove(&chart->data, set_ix, chart_data_dtor);
    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return TRUE;
}

static int
chart_get_dataset(chart_t* chart, int set_ix, MC_CHDATASET* dataset)
{
    chart_data_t* data;

    if(set_ix < 0  ||  set_ix >= dsa_size(&chart->data)) {
        MC_TRACE("chart_get_dataset: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

    if(dataset != NULL) {
        if(data->values != NULL) {
            int n = MC_MIN(dataset->dwCount, data->count);
            memcpy(dataset->piValues, data->values, n * sizeof(int));
            dataset->dwCount = n;
        } else {
            MC_TRACE("chart_get_dataset: Dataset has only virtual data.");
            dataset->dwCount = 0;
        }
    }

    return data->count;
}

static BOOL
chart_set_dataset(chart_t* chart, int set_ix, MC_CHDATASET* dataset)
{
    chart_data_t* data;
    int* values;

    if(MC_ERR(set_ix < 0  ||  set_ix >= dsa_size(&chart->data))) {
        MC_TRACE("chart_set_dataset: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if(MC_ERR(dataset->dwCount == 0)) {
        MC_TRACE("chart_set_dataset: Data set cannot be empty.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

    if(data->values != NULL  &&  dataset->piValues != NULL  &&  data->count == dataset->dwCount) {
        memcpy(data->values, dataset->piValues, data->count * sizeof(int));
        goto fast_code_path;
    }

    if(dataset->piValues != NULL) {
        values = (int*) malloc(sizeof(int) * dataset->dwCount);
        if(MC_ERR(values == NULL)) {
            MC_TRACE("chart_set_dataset: malloc() failed.");
            mc_send_notify(chart->notify_win, chart->win, NM_OUTOFMEMORY);
            return FALSE;
        }
        memcpy(values, dataset->piValues, sizeof(int) * dataset->dwCount);
    } else {
        values = NULL;
    }

    if(data->values != NULL)
        free(data->values);
    data->count = dataset->dwCount;
    data->values = values;

fast_code_path:
    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
    return TRUE;
}

static COLORREF
chart_get_dataset_color(chart_t* chart, int set_ix)
{
    if(MC_ERR(set_ix < 0  ||  set_ix >= dsa_size(&chart->data))) {
        MC_TRACE("chart_get_dataset_color: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return (COLORREF) -1;
    }

    return DSA_ITEM(&chart->data, set_ix, chart_data_t)->color;
}

static BOOL
chart_set_dataset_color(chart_t* chart, int set_ix, COLORREF color)
{
    if(MC_ERR(set_ix < 0  ||  set_ix >= dsa_size(&chart->data))) {
        MC_TRACE("chart_set_dataset_color: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DSA_ITEM(&chart->data, set_ix, chart_data_t)->color = color;

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return TRUE;
}

static BOOL
chart_get_dataset_legend(chart_t* chart, int set_ix, UINT buf_size,
                         void* buffer, BOOL unicode)
{
    chart_data_t* data;

    if(MC_ERR(set_ix < 0  ||  set_ix >= dsa_size(&chart->data))) {
        MC_TRACE("chart_get_dataset_legend: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

    mc_str_inbuf(data->name, MC_STRT,
                 buffer, (unicode ? MC_STRW : MC_STRA), buf_size);
    return TRUE;
}

static BOOL
chart_set_dataset_legend(chart_t* chart, int set_ix, void* text, BOOL unicode)
{
    TCHAR* str;
    chart_data_t* data;

    if(MC_ERR(set_ix < 0  ||  set_ix >= dsa_size(&chart->data))) {
        MC_TRACE("chart_set_dataset_legend: invalid data set index (%d)", set_ix);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(text != NULL) {
        str = (TCHAR*) mc_str(text, unicode ? MC_STRW : MC_STRA, MC_STRT);
        if(MC_ERR(str == NULL)) {
            MC_TRACE("chart_set_dataset_legend: mc_str() failed.");
            return FALSE;
        }
    } else {
        str = NULL;
    }

    data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
    if(data->name != NULL)
        free(data->name);
    data->name = str;

    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return TRUE;
}

static int
chart_get_factor_exponent(chart_t* chart, int axis_id)
{
    switch(axis_id) {
        case 1:
            return chart->axis1.factor_exp;

        case 2:
            return chart->axis2.factor_exp;

        default:
            MC_TRACE("chart_get_factor_exponent: Invalid axis %d", axis_id);
            SetLastError(ERROR_INVALID_PARAMETER);
            return -666;
    }
}

static BOOL
chart_set_factor_exponent(chart_t* chart, int axis_id, int exp)
{
    if(MC_ERR(exp < -9  ||  +9 < exp)) {
        MC_TRACE("chart_set_factor_exponent: Invalid factor exponent %d", exp);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    switch(axis_id) {
        case 0:
            chart->axis2.factor_exp = exp;
            /* no break */

        case 1:
            chart->axis1.factor_exp = exp;
            break;

        case 2:
            chart->axis2.factor_exp = exp;
            break;

        default:
            MC_TRACE("chart_set_factor_exponent: Invalid axis %d", axis_id);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return TRUE;
}

static int
chart_get_axis_offset(chart_t* chart, int axis_id)
{
    switch(axis_id) {
        case 1:
            return chart->axis1.offset;

        case 2:
            return chart->axis2.offset;

        default:
            MC_TRACE("chart_get_axis_offset: Invalid axis %d", axis_id);
            SetLastError(ERROR_INVALID_PARAMETER);
            return -666;
    }
}

static BOOL
chart_set_axis_offset(chart_t* chart, int axis_id, int offset)
{
    switch(axis_id) {
        case 1:
            chart->axis1.offset = offset;
            break;

        case 2:
            chart->axis2.offset = offset;
            break;

        default:
            MC_TRACE("chart_set_axis_offset: Invalid axis %d", axis_id);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return TRUE;
}

static BOOL
chart_get_axis_legend(chart_t* chart, int axis_id, UINT buf_size, void* buffer,
                      BOOL unicode)
{
    chart_axis_t* axis;

    switch(axis_id) {
        case 1:  axis = &chart->axis1; break;
        case 2:  axis = &chart->axis2; break;
        default:
            MC_TRACE("chart_get_axis_legend: Invalid axis %d", axis_id);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    mc_str_inbuf(axis->name, MC_STRT,
                 buffer, (unicode ? MC_STRW : MC_STRA), buf_size);
    return TRUE;
}

static BOOL
chart_set_axis_legend(chart_t* chart, int axis_id, void* text, BOOL unicode)
{
    TCHAR* str;
    chart_axis_t* axis;

    switch(axis_id) {
        case 1:  axis = &chart->axis1; break;
        case 2:  axis = &chart->axis2; break;
        default:
            MC_TRACE("chart_set_axis_legend: Invalid axis %d", axis_id);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    if(text != NULL) {
        str = (TCHAR*) mc_str(text, unicode ? MC_STRW : MC_STRA, MC_STRT);
        if(MC_ERR(str == NULL)) {
            MC_TRACE("chart_set_axis_legend: mc_str() failed.");
            return FALSE;
        }
    } else {
        str = NULL;
    }

    if(axis->name != NULL)
        free(axis->name);
    axis->name = str;

    chart_setup_hot(chart);

    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);

    return TRUE;
}

static void
chart_style_changed(chart_t* chart, STYLESTRUCT* ss)
{
    if((chart->style & MC_CHS_NOTOOLTIPS) != (ss->styleNew & MC_CHS_NOTOOLTIPS)) {

        if(!(ss->styleNew & MC_CHS_NOTOOLTIPS)) {
            chart->tooltip_win = tooltip_create(chart->win, chart->notify_win, TRUE);
        } else {
            tooltip_destroy(chart->tooltip_win);
            chart->tooltip_win = NULL;
            chart->tooltip_active = FALSE;
        }
    }

    chart->style = ss->styleNew;

    chart_setup_hot(chart);
    if(!chart->no_redraw)
        xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
}

static chart_t*
chart_nccreate(HWND win, CREATESTRUCT* cs)
{
    chart_t* chart;

    chart = (chart_t*) malloc(sizeof(chart_t));
    if(MC_ERR(chart == NULL)) {
        MC_TRACE("chart_nccreate: malloc() failed.");
        return NULL;
    }

    memset(chart, 0, sizeof(chart_t));
    chart->win = win;
    chart->notify_win = cs->hwndParent;
    chart->style = cs->style;
    chart->hot_set_ix = -1;
    chart->hot_i = -1;
    dsa_init(&chart->data, sizeof(chart_data_t));

    return chart;
}

static void
chart_setup_text_fmt(chart_t* chart)
{
    c_DWRITE_FONT_METRICS font_metrics;

    if(chart->text_fmt != NULL)
        c_IDWriteTextFormat_Release(chart->text_fmt);

    chart->text_fmt = xdwrite_create_text_format(chart->gdi_font, &font_metrics);
    chart->font_units_per_height = font_metrics.designUnitsPerEm;
    chart->font_cap_height = font_metrics.capHeight;
}

static int
chart_create(chart_t* chart)
{
    chart_setup_text_fmt(chart);
    chart_setup_hot(chart);

    if(!(chart->style & MC_CHS_NOTOOLTIPS))
        chart->tooltip_win = tooltip_create(chart->win, chart->notify_win, TRUE);

    return 0;
}

static void
chart_destroy(chart_t* chart)
{
    if(chart->tooltip_win != NULL) {
        if(!(chart->style & MC_CHS_NOTOOLTIPS))
            tooltip_destroy(chart->tooltip_win);
        else
            tooltip_uninstall(chart->tooltip_win, chart->win);
    }

    if(chart->text_fmt != NULL) {
        c_IDWriteTextFormat_Release(chart->text_fmt);
        chart->text_fmt = NULL;
    }
}

static void
chart_ncdestroy(chart_t* chart)
{
    dsa_fini(&chart->data, chart_data_dtor);

    if(chart->axis1.name != NULL)
        free(chart->axis1.name);

    if(chart->axis2.name != NULL)
        free(chart->axis2.name);

    xd2d_free_cache(&chart->xd2d_cache);
    free(chart);
}

static LRESULT CALLBACK
chart_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    chart_t* chart = (chart_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            xd2d_paint(win, chart->no_redraw, 0,
                    &chart_xd2d_vtable, (void*) chart, &chart->xd2d_cache);
            if(chart->xd2d_cache != NULL)
                SetTimer(win, CHART_XD2D_CACHE_TIMER_ID, 30 * 1000, NULL);
            return 0;

        case WM_PRINTCLIENT:
            xd2d_printclient(win, (HDC) wp, 0, &chart_xd2d_vtable, (void*) chart);
            return 0;

        case WM_DISPLAYCHANGE:
            xd2d_free_cache(&chart->xd2d_cache);
            xd2d_invalidate(win, NULL, TRUE, &chart->xd2d_cache);
            break;

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

        case WM_TIMER:
            if(wp == CHART_XD2D_CACHE_TIMER_ID) {
                xd2d_free_cache(&chart->xd2d_cache);
                KillTimer(win, CHART_XD2D_CACHE_TIMER_ID);
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            chart_mouse_move(chart, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_MOUSELEAVE:
            chart_mouse_leave(chart);
            return 0;

        case MC_CHM_GETDATASETCOUNT:
            return (LRESULT) dsa_size(&chart->data);

        case MC_CHM_DELETEALLDATASETS:
            dsa_clear(&chart->data, chart_data_dtor);
            if(!chart->no_redraw)
                xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
            return TRUE;

        case MC_CHM_INSERTDATASET:
            return chart_insert_dataset(chart, wp, (MC_CHDATASET*) lp);

        case MC_CHM_DELETEDATASET:
            return chart_delete_dataset(chart, wp);

        case MC_CHM_GETDATASET:
            return chart_get_dataset(chart, wp, (MC_CHDATASET*) lp);

        case MC_CHM_SETDATASET:
            return chart_set_dataset(chart, wp, (MC_CHDATASET*) lp);

        case MC_CHM_GETDATASETCOLOR:
            return chart_get_dataset_color(chart, wp);

        case MC_CHM_SETDATASETCOLOR:
            return chart_set_dataset_color(chart, wp, (COLORREF) lp);

        case MC_CHM_GETDATASETLEGENDW:
        case MC_CHM_GETDATASETLEGENDA:
            return chart_get_dataset_legend(chart, LOWORD(wp), HIWORD(wp),
                                (void*) lp, (msg == MC_CHM_GETDATASETLEGENDW));

        case MC_CHM_SETDATASETLEGENDW:
        case MC_CHM_SETDATASETLEGENDA:
            return chart_set_dataset_legend(chart, wp, (void*)lp,
                                            (msg == MC_CHM_SETDATASETLEGENDW));

        case MC_CHM_GETFACTOREXPONENT:
            return chart_get_factor_exponent(chart, wp);

        case MC_CHM_SETFACTOREXPONENT:
            return chart_set_factor_exponent(chart, wp, lp);

        case MC_CHM_GETAXISOFFSET:
            return chart_get_axis_offset(chart, wp);

        case MC_CHM_SETAXISOFFSET:
            return chart_set_axis_offset(chart, wp, lp);

        case MC_CHM_SETTOOLTIPS:
            return generic_settooltips(win, &chart->tooltip_win, (HWND) wp, TRUE);

        case MC_CHM_GETTOOLTIPS:
            return (LRESULT) chart->tooltip_win;

        case MC_CHM_GETAXISLEGENDW:
        case MC_CHM_GETAXISLEGENDA:
            return chart_get_axis_legend(chart, LOWORD(wp), HIWORD(wp),
                                (void*)lp, (msg == MC_CHM_GETAXISLEGENDW));

        case MC_CHM_SETAXISLEGENDW:
        case MC_CHM_SETAXISLEGENDA:
            return chart_set_axis_legend(chart, wp, (void*)lp,
                                         (msg == MC_CHM_SETAXISLEGENDW));

        case WM_SIZE:
            if(chart->xd2d_cache != NULL) {
                c_D2D1_SIZE_U size = { LOWORD(lp), HIWORD(lp) };
                c_ID2D1HwndRenderTarget_Resize((c_ID2D1HwndRenderTarget*) chart->xd2d_cache->rt, &size);
            }
            break;

        case WM_SETTEXT:
        {
            LRESULT res;
            res = DefWindowProc(win, msg, wp, lp);
            chart_setup_hot(chart);
            if(!chart->no_redraw)
                xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
            return res;
        }

        case WM_GETFONT:
            return (LRESULT) chart->gdi_font;

        case WM_SETFONT:
            if(chart->gdi_font != (HFONT) wp) {
                chart->gdi_font = (HFONT) wp;
                chart_setup_text_fmt(chart);
                chart_setup_hot(chart);
                if((BOOL) lp  &&  !chart->no_redraw)
                    xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
            }
            return 0;

        case WM_SETREDRAW:
            chart->no_redraw = !wp;
            if(!chart->no_redraw)
                xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_STATIC;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE)
                chart_style_changed(chart, (STYLESTRUCT*) lp);
            break;

        case WM_SYSCOLORCHANGE:
            if(!chart->no_redraw)
                xd2d_invalidate(chart->win, NULL, TRUE, &chart->xd2d_cache);
            break;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = chart->notify_win;
            chart->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case WM_NCCREATE:
            chart = (chart_t*) chart_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(chart == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)chart);
            break;

        case WM_CREATE:
            return (chart_create(chart) == 0 ? 0 : -1);

        case WM_DESTROY:
            chart_destroy(chart);
            break;

        case WM_NCDESTROY:
            if(chart)
                chart_ncdestroy(chart);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}

int
chart_init_module(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = chart_proc;
    wc.cbWndExtra = sizeof(chart_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = chart_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("chart_init_module: RegisterClass() failed.");
        return -1;
    }

    return 0;
}

void
chart_fini_module(void)
{
    UnregisterClass(chart_wc, NULL);
}
