/*
 * Copyright (c) 2013-2016 Martin Mitas
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

#include <math.h>
#include <wdl.h>


/* Uncomment this to have more verbose traces from this module. */
/*#define CHART_DEBUG     1*/

#ifdef CHART_DEBUG
    #define CHART_TRACE     MC_TRACE
#else
    #define CHART_TRACE     MC_NOOP
#endif


static const TCHAR chart_wc[] = MC_WC_CHART;    /* Window class name */


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

typedef struct chart_paint_tag chart_paint_t;
struct chart_paint_tag {
    WD_HCANVAS canvas;
    WD_HBRUSH solid_brush;
    WD_HFONT font;
};

typedef struct chart_tag chart_t;
struct chart_tag {
    HWND win;
    HWND notify_win;
    HWND tooltip_win;
    HFONT font;
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
    chart_paint_t* paint_ctx;
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

static inline void
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

static inline float
chart_map_x(int x, int min_x, int max_x, const WD_RECT* rect)
{
    return rect->x0 + ((x - min_x) * (rect->x1 - rect->x0)) / (float)(max_x - min_x);
}


static inline float
chart_unmap_x(int rx, int min_x, int max_x, const WD_RECT* rect)
{
    return min_x + (rx - rect->x0) * (max_x - min_x) / (rect->x1 - rect->x0);
}

static inline float
chart_map_y(int y, int min_y, int max_y, const WD_RECT* rect)
{
    return rect->y0 + ((max_y - y) * (rect->y1 - rect->y0)) / (float)(max_y - min_y);
}

static void
chart_fixup_rect_h(WD_RECT* rect, int min_x, int max_x, int step_x)
{
    float pps_old, pps_new;  /* pixels per step */
    float width;

    pps_old = chart_map_x(2*step_x, min_x, max_x, rect) - chart_map_x(step_x, min_x, max_x, rect);
    pps_new = floorf(pps_old);

    width = ((rect->x1 - rect->x0) / pps_old) * pps_new;
    rect->x0 += (rect->x1 - rect->x0 - width) * 0.5f;
    rect->x0 = roundf(rect->x0);
    rect->x1 = rect->x0 + width;
}

static void
chart_fixup_rect_v(WD_RECT* rect, int min_y, int max_y, int step_y)
{
    float pps_old, pps_new;  /* pixels per step */
    float height;

    pps_old = chart_map_y(2*step_y, min_y, max_y, rect) - chart_map_y(step_y, min_y, max_y, rect);
    pps_new = floorf(pps_old);

    height = ((rect->y1 - rect->y0) / pps_old) * pps_new;
    rect->y0 += ((rect->y1 - rect->y0) - height) * 0.5f;
    rect->y0 = roundf(rect->y0);
    rect->y1 = rect->y0 + height;
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

static inline void
chart_draw_vert_string(WD_HCANVAS canvas, const WD_HFONT font,
                       const WD_RECT* rect, const TCHAR* str, int len,
                       const WD_HBRUSH brush)
{
    float cx = (rect->x0 + rect->x1) * 0.5f;
    float cy = (rect->y0 + rect->y1) * 0.5f;
    float w_half = (rect->x1 - rect->x0 + 1.0f) * 0.5f;
    float h_half = (rect->y1 - rect->y0 + 1.0f) * 0.5f;
    WD_RECT r = { cx - h_half, cy - w_half, cx + h_half, cy + w_half };

    wdRotateWorld(canvas, cx, cy, -90.0f);
    wdDrawString(canvas, font, &r, str, len, brush,
            WD_STR_NOCLIP | WD_STR_NOWRAP | WD_STR_CENTERALIGN);
    wdRotateWorld(canvas, cx, cy, +90.0f);
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
    ((cache)->values != NULL  ?  (cache)->values[(set_ix)][(i)]               \
                              :  cache_value_((cache), (set_ix), (i)))

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

static inline BOOL
pie_rect_in_sweep(float angle, float sweep, float x0, float y0, const WD_RECT* rect)
{
    return (pie_pt_in_sweep(angle, sweep, x0, y0, rect->x0, rect->y0)  &&
            pie_pt_in_sweep(angle, sweep, x0, y0, rect->x1, rect->y0)  &&
            pie_pt_in_sweep(angle, sweep, x0, y0, rect->x0, rect->y1)  &&
            pie_pt_in_sweep(angle, sweep, x0, y0, rect->x1, rect->y1));
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
pie_calc_geometry(chart_t* chart, chart_paint_t* ctx, const chart_layout_t* layout,
                  pie_geometry_t* geom)
{
    int set_ix, n;

    geom->cx = (layout->body_rect.left + layout->body_rect.right) / 2.0f;
    geom->cy = (layout->body_rect.top + layout->body_rect.bottom) / 2.0f;
    geom->r = MC_MIN(mc_width(&layout->body_rect), mc_height(&layout->body_rect)) / 2.0f - 10.0f;

    geom->sum = 0.0f;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++)
        geom->sum += (float) pie_value(chart, set_ix);
}

static void
pie_paint(chart_t* chart, chart_paint_t* ctx, const chart_layout_t* layout)
{
    pie_geometry_t geom;
    int set_ix, n, val;
    float angle, sweep;
    float label_angle_rads;
    WD_RECT label_rect;
    WD_RECT label_measure;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    pie_calc_geometry(chart, ctx, layout, &geom);

    angle = -90.0f;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        val = pie_value(chart, set_ix);
        sweep = (360.0f * (float) val) / geom.sum;

        /* Paint the pie */
        wdSetSolidBrushColor(ctx->solid_brush,
                WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
        wdFillPie(ctx->canvas, ctx->solid_brush,
                geom.cx, geom.cy, geom.r, angle, sweep);

        /* Paint active aura */
        if(set_ix == chart->hot_set_ix) {
            wdSetSolidBrushColor(ctx->solid_brush,
                    WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));
            wdDrawArc(ctx->canvas, ctx->solid_brush,
                    geom.cx, geom.cy, geom.r + 6.0f, angle, sweep, 8.0f);
        }

        /* Paint white borders */
        wdSetSolidBrushColor(ctx->solid_brush,
                WD_COLOR_FROM_GDI(GetSysColor(COLOR_WINDOW)));
        wdDrawPie(ctx->canvas, ctx->solid_brush,
                geom.cx, geom.cy, geom.r + 16.0f, angle, sweep, 1.0f);

        /* Paint label (if it fits in) */
        label_angle_rads = (angle + 0.5f * sweep) * (MC_PIf / 180.0f);
        label_rect.x0 = geom.cx + 0.75f * geom.r * cosf(label_angle_rads);
        label_rect.y0 = geom.cy + 0.75f * geom.r * sinf(label_angle_rads)
                        - layout->font_size.cy / 2.0f;
        label_rect.x1 = label_rect.x0;
        label_rect.y1 = label_rect.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis1, val, buffer);
        wdMeasureString(ctx->canvas, ctx->font, &label_rect, buffer, -1,
                    &label_measure, WD_STR_NOWRAP | WD_STR_CENTERALIGN);
        if(pie_rect_in_sweep(angle, sweep, geom.cx, geom.cy, &label_measure)) {
            wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(255,255,255));
            wdDrawString(ctx->canvas, ctx->font, &label_rect, buffer, -1, ctx->solid_brush,
                    WD_STR_NOCLIP | WD_STR_NOWRAP | WD_STR_CENTERALIGN);
        }

        angle += sweep;
    }
}

static void
pie_hit_test(chart_t* chart, chart_paint_t* ctx, const chart_layout_t* layout,
             int x, int y, int* p_set_ix, int* p_i)
{
    pie_geometry_t geom;
    int set_ix, n;
    float angle, sweep;
    float dx, dy;

    pie_calc_geometry(chart, ctx, layout, &geom);

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


/*********************
 *** Scatter Chart ***
 *********************/

typedef struct scatter_geometry_tag scatter_geometry_t;
struct scatter_geometry_tag {
    WD_RECT axis_x_name_rect;
    WD_RECT axis_y_name_rect;
    WD_RECT core_rect;

    int min_x;
    int max_x;
    int step_x;
    int min_step_x;

    int min_y;
    int max_y;
    int step_y;
    int min_step_y;
};

static inline float
scatter_map_x(int x, const scatter_geometry_t* geom)
{
    return chart_map_x(x, geom->min_x, geom->max_x, &geom->core_rect);
}

static inline float
scatter_map_y(int y, const scatter_geometry_t* geom)
{
    return chart_map_y(y, geom->min_y, geom->max_y, &geom->core_rect);
}

static void
scatter_calc_geometry(chart_t* chart, chart_paint_t* ctx,
                      const chart_layout_t* layout, cache_t* cache,
                      scatter_geometry_t* geom)
{
    int set_ix, n, i;
    chart_data_t* data;
    int label_x_w, label_x_h;
    int label_y_w, label_y_h;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
    int tw;

    n = dsa_size(&chart->data);

    if(n > 0) {
        /* Find extreme values */
        geom->min_x = INT32_MAX;
        geom->max_x = INT32_MIN;
        geom->min_y = INT32_MAX;
        geom->max_y = INT32_MIN;
        for(set_ix = 0; set_ix < n; set_ix++) {
            data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
            for(i = 0; i < data->count-1; i += 2) {  /* '-1' to protect if ->count is odd */
                int x, y;

                x = CACHE_VALUE(cache, set_ix, i);
                y = CACHE_VALUE(cache, set_ix, i+1);

                if(x < geom->min_x)
                    geom->min_x = x;
                if(x > geom->max_x)
                    geom->max_x = x;
                if(y < geom->min_y)
                    geom->min_y = y;
                if(y > geom->max_y)
                    geom->max_y = y;
            }
        }

        /* We want the chart to include the axis */
        if(geom->min_x > 0)
            geom->min_x = 0;
        else if(geom->max_x < 0)
            geom->max_x = 0;

        if(geom->min_y > 0)
            geom->min_y = 0;
        else if(geom->max_y < 0)
            geom->max_y = 0;
    } else {
        geom->min_x = 0;
        geom->max_x = 0;
        geom->min_y = 0;
        geom->max_y = 0;
    }

    /* Avoid singularity */
    if(geom->min_x >= geom->max_x)
        geom->max_x = geom->min_x + 1;
    if(geom->min_y >= geom->max_y)
        geom->max_y = geom->min_y + 1;

    /* Round to nice values */
    geom->min_x = chart_round_value(geom->min_x + chart->axis1.offset, FALSE) - chart->axis1.offset;
    geom->max_x = chart_round_value(geom->max_x + chart->axis1.offset, TRUE) - chart->axis1.offset;
    geom->min_y = chart_round_value(geom->min_y + chart->axis2.offset, FALSE) - chart->axis2.offset;
    geom->max_y = chart_round_value(geom->max_y + chart->axis2.offset, TRUE) - chart->axis2.offset;

    /* Compute space for labels of horizontal axis */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis1, geom->max_x, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis1, geom->max_x, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of vertical axis */
    label_y_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis2, geom->min_y, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis2, geom->max_y, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute axis name vertical coords */
    geom->axis_x_name_rect.y0 = layout->body_rect.bottom;
    geom->axis_x_name_rect.y1 = layout->body_rect.bottom;
    if(chart->axis1.name != NULL)
        geom->axis_x_name_rect.y0 -= layout->font_size.cy;

    geom->axis_y_name_rect.x0 = layout->body_rect.left;
    geom->axis_y_name_rect.x1 = layout->body_rect.left;
    if(chart->axis2.name != NULL)
        geom->axis_y_name_rect.x1 += layout->font_size.cy;

    /* Compute core area */
    geom->core_rect.x0 = layout->body_rect.left + label_y_w;
    geom->core_rect.y0 = layout->body_rect.top + (label_y_h+1) / 2.0f;
    geom->core_rect.x1 = layout->body_rect.right;
    geom->core_rect.y1 = layout->body_rect.bottom - label_x_h;
    if(geom->axis_x_name_rect.y1 > geom->axis_x_name_rect.y0) {
        geom->core_rect.y1 -= geom->axis_x_name_rect.y1 -
                    geom->axis_x_name_rect.y0 + layout->margin;
    }
    if(geom->axis_y_name_rect.x1 > geom->axis_y_name_rect.x0) {
        geom->core_rect.x0 += geom->axis_y_name_rect.x1 -
                     geom->axis_y_name_rect.x0 + layout->margin;
    }

    /* Steps for painting secondary lines */
    geom->step_x = chart_round_value((geom->max_x - geom->min_x) * label_x_w /
                        (geom->core_rect.x1 - geom->core_rect.x0), TRUE);
    if(geom->step_x < 1)
        geom->step_x = 1;
    geom->step_y = chart_round_value((geom->max_y - geom->min_y) * 3*label_y_h /
                        (2.0f * (geom->core_rect.y1 - geom->core_rect.y0)), TRUE);
    if(geom->step_y < 1)
        geom->step_y = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neighborhood pixels as it looks too ugly. */
    chart_fixup_rect_v(&geom->core_rect, geom->min_y, geom->max_y, geom->step_y);
    chart_fixup_rect_h(&geom->core_rect, geom->min_x, geom->max_x, geom->step_x);
    geom->min_step_x = ((geom->min_x + geom->step_x - 1) / geom->step_x) * geom->step_x;
    geom->min_step_y = ((geom->min_y + geom->step_y - 1) / geom->step_y) * geom->step_y;

    /* Compute axis name horizontal coords */
    geom->axis_x_name_rect.x0 = geom->core_rect.x0;
    geom->axis_x_name_rect.x1 = geom->core_rect.x1;
    geom->axis_y_name_rect.y0 = geom->core_rect.y0;
    geom->axis_y_name_rect.y1 = geom->core_rect.y1;
}

static void
scatter_paint_grid(chart_t* chart, chart_paint_t* ctx,
                   const chart_layout_t* layout, const scatter_geometry_t* geom)
{
    int x, y;
    float x0, y0, x1, y1;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Axis legends */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(95,95,95));
    if(chart->axis1.name != NULL) {
        wdDrawString(ctx->canvas, ctx->font, &geom->axis_x_name_rect,
                     chart->axis1.name, -1, ctx->solid_brush,
                     WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
    if(chart->axis2.name != NULL) {
        chart_draw_vert_string(ctx->canvas, ctx->font, &geom->axis_y_name_rect,
                               chart->axis2.name, -1, ctx->solid_brush);
    }

    /* Secondary lines */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(191,191,191));
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        if(x == 0)
            continue;

        x0 = scatter_map_x(x, geom);
        y0 = geom->core_rect.y0;
        x1 = x0;
        y1 = geom->core_rect.y1;
        wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    }
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        if(y == 0)
            continue;

        x0 = geom->core_rect.x0;
        y0 = scatter_map_y(y, geom);
        x1 = geom->core_rect.x1;
        y1 = y0;
        wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    }

    /* Primary lines (axis) */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(0,0,0));
    x0 = scatter_map_x(0, geom);
    y0 = geom->core_rect.y0;
    x1 = x0;
    y1 = geom->core_rect.y1;
    wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    x0 = geom->core_rect.x0;
    y0 = scatter_map_y(0, geom);
    x1 = geom->core_rect.x1;
    y1 = y0;
    wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);

    /* Labels */
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        WD_RECT rc;

        rc.x0 = scatter_map_x(x, geom);
        rc.y0 = geom->core_rect.y1 + (layout->font_size.cy+1) / 2.0f;
        rc.x1 = rc.x0;
        rc.y1 = rc.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis1, x, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rc, buffer, -1, ctx->solid_brush,
                          WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        WD_RECT rc;

        rc.x0 = geom->core_rect.x0 - (layout->font_size.cx+1) / 2.0f;
        rc.y0 = scatter_map_y(y, geom) - (layout->font_size.cy+1) / 2.0f;
        rc.x1 = rc.x0;
        rc.y1 = rc.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis2, y, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rc, buffer, -1, ctx->solid_brush,
                          WD_STR_RIGHTALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
}

static void
scatter_paint(chart_t* chart, chart_paint_t* ctx, const chart_layout_t* layout)
{
    cache_t cache;
    scatter_geometry_t geom;
    int set_ix, n, i;
    chart_data_t* data;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    scatter_calc_geometry(chart, ctx, layout, &cache, &geom);
    scatter_paint_grid(chart, ctx, layout, &geom);

    /* Paint hot aura */
    if(chart->hot_set_ix >= 0) {
        int i0, i1;

        set_ix = chart->hot_set_ix;
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        wdSetSolidBrushColor(ctx->solid_brush, WD_COLOR_FROM_GDI(
                        color_hint(chart_data_color(chart, set_ix))));

        if(chart->hot_i >= 0) {
            i0 = chart->hot_i;
            i1 = chart->hot_i+1;
        } else {
            i0 = 0;
            i1 = data->count-1;
        }

        for(i = i0; i < i1; i += 2) {
            wdFillCircle(ctx->canvas, ctx->solid_brush,
                    scatter_map_x(CACHE_VALUE(&cache, set_ix, i), &geom),
                    scatter_map_y(CACHE_VALUE(&cache, set_ix, i+1), &geom),
                    4.0f);
        }
    }

    /* Paint all data sets */
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
        for(i = 0; i < data->count-1; i += 2) {  /* '-1' to protect if ->count is odd */
            wdFillCircle(ctx->canvas, ctx->solid_brush,
                    scatter_map_x(CACHE_VALUE(&cache, set_ix, i), &geom),
                    scatter_map_y(CACHE_VALUE(&cache, set_ix, i+1), &geom),
                    2.0f);
        }
    }

    CACHE_FINI(&cache);
}

static void
scatter_hit_test(chart_t* chart, chart_paint_t* ctx, const chart_layout_t* layout,
                 int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    scatter_geometry_t geom;
    int set_ix, n, i;
    chart_data_t* data;
    float rx = (float) x;
    float ry = (float) y;
    float dist2;

    n = dsa_size(&chart->data);
    dist2 = GetSystemMetrics(SM_CXDOUBLECLK) * GetSystemMetrics(SM_CYDOUBLECLK);

    CACHE_INIT(&cache, chart);
    scatter_calc_geometry(chart, ctx, layout, &cache, &geom);

    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        for(i = 0; i < data->count-1; i += 2) {
            float dx, dy;

            dx = rx - scatter_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);
            dy = ry - scatter_map_y(CACHE_VALUE(&cache, set_ix, i+1), &geom);

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

typedef struct line_geometry_tag line_geometry_t;
struct line_geometry_tag {
    WD_RECT axis_x_name_rect;
    WD_RECT axis_y_name_rect;
    WD_RECT core_rect;

    int min_count;

    int min_x;
    int max_x;
    int step_x;
    int min_step_x;

    int min_y;
    int max_y;
    int step_y;
    int min_step_y;
};

static inline float
line_map_y(int y, const line_geometry_t* geom)
{
    return chart_map_y(y, geom->min_y, geom->max_y, &geom->core_rect);
}

static inline float
line_map_x(int x, const line_geometry_t* geom)
{
    return chart_map_x(x, geom->min_x, geom->max_x, &geom->core_rect);
}

static void
line_calc_geometry(chart_t* chart, BOOL is_stacked, chart_paint_t* ctx,
                   const chart_layout_t* layout, cache_t* cache,
                   line_geometry_t* geom)
{
    int set_ix, n, i;
    chart_data_t* data;
    int label_x_w, label_x_h;
    int label_y_w, label_y_h;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
    int tw;

    n = dsa_size(&chart->data);

    if(n > 0) {
        /* Find extreme values */
        geom->min_count = INT32_MAX;
        geom->min_y = INT32_MAX;
        geom->max_y = INT32_MIN;
        for(set_ix = 0; set_ix < n; set_ix++) {
            data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

            if(data->count < geom->min_count)
                geom->min_count = data->count;

            for(i = 0; i < data->count; i++) {
                int y;

                if(is_stacked)
                    y = cache_stack(cache, set_ix, i);
                else
                    y = CACHE_VALUE(cache, set_ix, i);

                if(y < geom->min_y)
                    geom->min_y = y;
                if(y > geom->max_y)
                    geom->max_y = y;
            }
        }

        /* We want the chart to include the axis */
        if(geom->min_y > 0)
            geom->min_y = 0;
        else if(geom->max_y < 0)
            geom->max_y = 0;
    } else {
        geom->min_count = 0;
        geom->min_y = 0;
        geom->max_y = 0;
    }

    geom->min_x = 0;
    geom->max_x = geom->min_count-1;

    /* Avoid singularity */
    if(geom->min_x >= geom->max_x)
        geom->max_x = geom->min_x + 1;
    if(geom->min_y >= geom->max_y)
        geom->max_y = geom->min_y + 1;

    /* Round to nice values */
    geom->min_y = chart_round_value(geom->min_y + chart->axis2.offset, FALSE) - chart->axis2.offset;
    geom->max_y = chart_round_value(geom->max_y + chart->axis2.offset, TRUE) - chart->axis2.offset;

    /* Compute space for labels of horizontal axis */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis1, 0, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis1, geom->min_count, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of vertical axis */
    label_y_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis2, geom->min_y, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis2, geom->max_y, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute axis name coords */
    geom->axis_x_name_rect.y0 = layout->body_rect.bottom;
    geom->axis_x_name_rect.y1 = layout->body_rect.bottom;
    if(chart->axis1.name != NULL)
        geom->axis_x_name_rect.y0 -= layout->font_size.cy;

    geom->axis_y_name_rect.x0 = layout->body_rect.left;
    geom->axis_y_name_rect.x1 = layout->body_rect.left;
    if(chart->axis2.name != NULL)
        geom->axis_y_name_rect.x1 += layout->font_size.cy;

    /* Compute core area */
    geom->core_rect.x0 = layout->body_rect.left + label_y_w;
    geom->core_rect.y0 = layout->body_rect.top + (label_y_h+1) / 2.0f;
    geom->core_rect.x1 = layout->body_rect.right;
    geom->core_rect.y1 = layout->body_rect.bottom - label_x_h;
    if(geom->axis_x_name_rect.y1 > geom->axis_x_name_rect.y0) {
        geom->core_rect.y1 -= geom->axis_x_name_rect.y1 -
                    geom->axis_x_name_rect.y0 + layout->margin;
    }
    if(geom->axis_y_name_rect.x1 > geom->axis_y_name_rect.x0) {
        geom->core_rect.x0 += geom->axis_y_name_rect.x1 -
                     geom->axis_y_name_rect.x0 + layout->margin;
    }

    /* Steps for painting secondary lines */
    geom->step_x = chart_round_value((geom->max_x - geom->min_x) * label_x_w /
                        (geom->core_rect.x1 - geom->core_rect.x0), TRUE);
    if(geom->step_x < 1)
        geom->step_x = 1;
    geom->step_y = chart_round_value((geom->max_y - geom->min_y) * 3*label_y_h /
                        (2.0f * (geom->core_rect.y1 - geom->core_rect.y0)), TRUE);
    if(geom->step_y < 1)
        geom->step_y = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neighborhood pixels as it looks too ugly. */
    chart_fixup_rect_v(&geom->core_rect, geom->min_y, geom->max_y, geom->step_y);
    chart_fixup_rect_h(&geom->core_rect, geom->min_x, geom->max_x, geom->step_x);
    geom->min_step_x = ((geom->min_x + geom->step_x - 1) / geom->step_x) * geom->step_x;
    geom->min_step_y = ((geom->min_y + geom->step_y - 1) / geom->step_y) * geom->step_y;

    /* Compute axis name horizontal coords */
    geom->axis_x_name_rect.x0 = geom->core_rect.x0;
    geom->axis_x_name_rect.x1 = geom->core_rect.x1;
    geom->axis_y_name_rect.y0 = geom->core_rect.y0;
    geom->axis_y_name_rect.y1 = geom->core_rect.y1;
}

static void
line_paint_grid(chart_t* chart, chart_paint_t* ctx,
                const chart_layout_t* layout, const line_geometry_t* geom)
{
    int x, y;
    float x0, y0, x1, y1;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Axis legends */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(95,95,95));
    if(chart->axis1.name != NULL) {
        wdDrawString(ctx->canvas, ctx->font, &geom->axis_x_name_rect,
                          chart->axis1.name, -1, ctx->solid_brush,
                          WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
    if(chart->axis2.name != NULL) {
        chart_draw_vert_string(ctx->canvas, ctx->font, &geom->axis_y_name_rect,
                               chart->axis2.name, -1, ctx->solid_brush);
    }

    /* Secondary lines */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(191,191,191));
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        if(x == 0)
            continue;

        x0 = line_map_x(x, geom);
        y0 = geom->core_rect.y0;
        x1 = x0;
        y1 = geom->core_rect.y1;
        wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    }
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        if(y == 0)
            continue;

        x0 = geom->core_rect.x0;
        y0 = line_map_y(y, geom);
        x1 = geom->core_rect.x1;
        y1 = y0;
        wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    }

    /* Primary lines (axis) */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(0,0,0));
    x0 = line_map_x(0, geom);
    y0 = geom->core_rect.y0;
    x1 = x0;
    y1 = geom->core_rect.y1;
    wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    x0 = geom->core_rect.x0;
    y0 = line_map_y(0, geom);
    x1 = geom->core_rect.x1;
    y1 = y0;
    wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);

    /* Labels */
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        WD_RECT rc;

        rc.x0 = line_map_x(x, geom);
        rc.y0 = geom->core_rect.y1 + (layout->font_size.cy+1) / 2.0f;
        rc.x1 = rc.x0;
        rc.y1 = rc.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis1, x, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rc, buffer, -1, ctx->solid_brush,
                          WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        WD_RECT rc;

        rc.x0 = geom->core_rect.x0 - (layout->font_size.cx+1) / 2.0f;
        rc.y0 = line_map_y(y, geom) - (layout->font_size.cy+1) / 2.0f;
        rc.x1 = rc.x0;
        rc.y1 = rc.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis2, y, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rc, buffer, -1, ctx->solid_brush,
                          WD_STR_RIGHTALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
}

static void
line_paint(chart_t* chart, BOOL is_area, BOOL is_stacked, chart_paint_t* ctx,
           const chart_layout_t* layout)
{
    cache_t cache;
    line_geometry_t geom;
    int set_ix, n;
    int x, y;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    line_calc_geometry(chart, is_stacked, ctx, layout, &cache, &geom);
    line_paint_grid(chart, ctx, layout, &geom);

    /* Paint area */
    if(is_area  &&  geom.min_count > 1) {
        for(set_ix = 0; set_ix < n; set_ix++) {
            WD_HPATH path;
            WD_PATHSINK sink;
            float fy0;
            BYTE alpha;

            path = wdCreatePath(ctx->canvas);
            if(MC_ERR(path == NULL)) {
                MC_TRACE("line_paint: wdCreatePath() failed.");
                continue;
            }

            if(MC_ERR(!wdOpenPathSink(&sink, path))) {
                MC_TRACE("line_paint: wdOpenPathSink() failed.");
                wdDestroyPath(path);
                continue;
            }

            fy0 = line_map_y(0, &geom);
            wdBeginFigure(&sink, line_map_x(0, &geom), fy0);

            for(x = 0; x < geom.min_count; x++) {
                if(is_stacked)
                    y = cache_stack(&cache, set_ix, x);
                else
                    y = CACHE_VALUE(&cache, set_ix, x);
                wdAddLine(&sink, line_map_x(x, &geom), line_map_y(y, &geom));
            }

            if(!is_stacked || set_ix == 0) {
                wdAddLine(&sink, line_map_x(geom.min_count - 1, &geom), fy0);
            } else {
                for(x = geom.min_count - 1; x >= 0; x--) {
                    y = cache_stack(&cache, set_ix-1, x);
                    wdAddLine(&sink, line_map_x(x, &geom), line_map_y(y, &geom));
                }
            }

            wdEndFigure(&sink, TRUE);
            wdClosePathSink(&sink);

            if(set_ix == chart->hot_set_ix  &&  chart->hot_i < 0)
                alpha = 0x60;
            else
                alpha = 0x2f;

            wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI_EX(alpha, chart_data_color(chart, set_ix)));
            wdFillPath(ctx->canvas, ctx->solid_brush, path);
            wdDestroyPath(path);
        }
    }

    /* Paint hot aura */
    if(chart->hot_set_ix >= 0) {
        int x0, x1;
        float fx, fy;
        float prev_fx, prev_fy;

        set_ix = chart->hot_set_ix;

        if(chart->hot_i >= 0) {
            x0 = chart->hot_i;
            x1 = chart->hot_i+1;
        } else {
            x0 = 0;
            x1 = DSA_ITEM(&chart->data, set_ix, chart_data_t)->count;
        }

        wdSetSolidBrushColor(ctx->solid_brush,
                WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));

        for(x = x0; x < x1; x++) {
            if(is_stacked)
                y = cache_stack(&cache, set_ix, x);
            else
                y = CACHE_VALUE(&cache, set_ix, x);

            fx = line_map_x(x, &geom);
            fy = line_map_y(y, &geom);
            wdFillCircle(ctx->canvas, ctx->solid_brush, fx, fy, 4.0f);

            if(x > x0)
                wdDrawLine(ctx->canvas, ctx->solid_brush, prev_fx, prev_fy, fx, fy, 3.5f);

            prev_fx = fx;
            prev_fy = fy;
        }
    }

    /* Paint all data sets */
    for(set_ix = 0; set_ix < n; set_ix++) {
        float fx, fy;
        float prev_fx, prev_fy;

        wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
        for(x = 0; x < geom.min_count; x++) {
            if(is_stacked)
                y = cache_stack(&cache, set_ix, x);
            else
                y = CACHE_VALUE(&cache, set_ix, x);

            fx = line_map_x(x, &geom);
            fy = line_map_y(y, &geom);
            wdFillCircle(ctx->canvas, ctx->solid_brush, fx, fy, 2.0f);

            if(x > 0)
                wdDrawLine(ctx->canvas, ctx->solid_brush, prev_fx, prev_fy, fx, fy, 1.0f);

            prev_fx = fx;
            prev_fy = fy;
        }
    }

    CACHE_FINI(&cache);
}

static void
line_hit_test(chart_t* chart, BOOL is_stacked, chart_paint_t* ctx,
              const chart_layout_t* layout, int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    line_geometry_t geom;
    int set_ix, n, i;
    int i0, i1;
    float ri;
    float rx = (float) x;
    float ry = (float) y;
    int max_dx, max_dy;
    float dist2;

    n = dsa_size(&chart->data);
    max_dx = GetSystemMetrics(SM_CXDOUBLECLK);
    max_dy = GetSystemMetrics(SM_CYDOUBLECLK);
    dist2 = max_dx * max_dy;

    CACHE_INIT(&cache, chart);
    line_calc_geometry(chart, is_stacked, ctx, layout, &cache, &geom);

    /* Find index which maps horizontally close enough to the X mouse
     * coordinate. */
    ri = chart_unmap_x((float) x, geom.min_x, geom.max_x, &geom.core_rect);

    /* Any close hot point must be close of the ri */
    i0 = (int) (ri - MC_MAX(max_dx, max_dy) + 0.9999f);
    i1 = (int) (ri + MC_MAX(max_dx, max_dy));

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

            dx = rx - line_map_x(i, &geom);
            dy = ry - line_map_y(y, &geom);

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

typedef struct column_geometry_tag column_geometry_t;
struct column_geometry_tag {
    WD_RECT axis_x_name_rect;
    WD_RECT axis_y_name_rect;
    WD_RECT core_rect;

    int min_count;

    int min_y;
    int max_y;
    int step_y;
    int min_step_y;

    float column_width;
    float column_padding;
    float group_padding;
};

static inline float
column_map_y(int y, const column_geometry_t* geom)
{
    return chart_map_y(y, geom->min_y, geom->max_y, &geom->core_rect);
}

static void
column_calc_geometry(chart_t* chart, BOOL is_stacked, const chart_layout_t* layout,
                     cache_t* cache, column_geometry_t* geom)
{
    int set_ix, n, i;
    chart_data_t* data;
    int label_x_w, label_x_h;
    int label_y_w, label_y_h;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
    int tw;

    n = dsa_size(&chart->data);

    if(n > 0) {
        /* Find extreme values */
        geom->min_count = INT32_MAX;
        geom->min_y = INT32_MAX;
        geom->max_y = INT32_MIN;
        for(set_ix = 0; set_ix < n; set_ix++) {
            data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

            if(data->count < geom->min_count)
                geom->min_count = data->count;

            for(i = 0; i < data->count; i++) {
                int y;

                if(is_stacked)
                    y = cache_stack(cache, set_ix, i);
                else
                    y = CACHE_VALUE(cache, set_ix, i);

                if(y < geom->min_y)
                    geom->min_y = y;
                if(y > geom->max_y)
                    geom->max_y = y;
            }
        }

        /* We want the chart to include the axis */
        if(geom->min_y > 0)
            geom->min_y = 0;
        else if(geom->max_y < 0)
            geom->max_y = 0;
    } else {
        geom->min_count = 0;
        geom->min_y = 0;
        geom->max_y = 0;
    }

    /* Avoid singularity */
    if(geom->min_y >= geom->max_y)
        geom->max_y = geom->min_y + 1;

    /* Round to nice values */
    geom->min_y = chart_round_value(geom->min_y + chart->axis2.offset, FALSE) - chart->axis2.offset;
    geom->max_y = chart_round_value(geom->max_y + chart->axis2.offset, TRUE) - chart->axis2.offset;

    /* Compute space for labels of horizontal axis */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis1, 0, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis1, geom->min_count, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of vertical axis */
    label_y_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis2, geom->min_y, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis2, geom->max_y, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute axis legend width/height */
    if(chart->axis2.name != NULL) {
        geom->axis_x_name_rect.y0 = layout->body_rect.bottom - layout->font_size.cy;
        geom->axis_x_name_rect.y1 = geom->axis_x_name_rect.y0 + layout->font_size.cy;
    } else {
        geom->axis_x_name_rect.y0 = layout->body_rect.bottom;
        geom->axis_x_name_rect.y1  = geom->axis_x_name_rect.y0;
    }
    if(chart->axis1.name != NULL) {
        geom->axis_y_name_rect.x0 = layout->body_rect.left;
        geom->axis_y_name_rect.x1 = geom->axis_y_name_rect.x0 + layout->font_size.cy;
    } else {
        geom->axis_y_name_rect.x0 = layout->body_rect.left;
        geom->axis_y_name_rect.x1 = geom->axis_y_name_rect.x0;
    }

    /* Compute core area */
    geom->core_rect.x0 = layout->body_rect.left + label_y_w;
    geom->core_rect.y0 = layout->body_rect.top + (label_y_h+1) / 2.0f;
    geom->core_rect.x1 = layout->body_rect.right;
    geom->core_rect.y1 = layout->body_rect.bottom - label_x_h;
    if(geom->axis_x_name_rect.y1 > geom->axis_x_name_rect.y0)
        geom->core_rect.y1 -= geom->axis_x_name_rect.y1 - geom->axis_x_name_rect.y0 + layout->margin;
    if(geom->axis_y_name_rect.x1 > geom->axis_y_name_rect.x0)
        geom->core_rect.x0 += geom->axis_y_name_rect.x1 - geom->axis_y_name_rect.x0 + layout->margin;

    /* Steps for painting secondary lines */
    geom->step_y = chart_round_value((geom->max_y - geom->min_y) * label_y_h / (geom->core_rect.y1 - geom->core_rect.y0), TRUE);
    if(geom->step_y < 1)
        geom->step_y = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neighborhood pixels as it looks too ugly. */
    chart_fixup_rect_v(&geom->core_rect, geom->min_y, geom->max_y, geom->step_y);
    geom->min_step_y = ((geom->min_y + geom->step_y - 1) / geom->step_y) * geom->step_y;

    if(is_stacked) {
        geom->column_width = (geom->core_rect.x1 - geom->core_rect.x0) / (1.61f * geom->min_count);
        geom->group_padding = 0.5f * 0.61f * geom->column_width;
        geom->column_padding = 0.0f;
    } else {
        if(n > 0) {
            geom->column_width = (geom->core_rect.x1 - geom->core_rect.x0) / ((n+0.5f) * geom->min_count);
            geom->group_padding = 0.25f * geom->column_width;
            geom->column_padding = 0.15f * geom->column_width;
            geom->column_width -= 2.0f * geom->column_padding;
        } else {
            geom->column_width = 0.0f;
            geom->column_padding = 0.0f;
            geom->group_padding = 0.0f;
        }
    }

    /* Refresh axis legend areas after the fix-up */
    geom->axis_x_name_rect.x0 = geom->core_rect.x0;
    geom->axis_x_name_rect.x1 = geom->core_rect.x1;
    geom->axis_y_name_rect.y0 = geom->core_rect.y0;
    geom->axis_y_name_rect.y1 = geom->core_rect.y1;
}

static void
column_paint_grid(chart_t* chart, chart_paint_t* ctx,
                  const chart_layout_t* layout, const column_geometry_t* geom)
{
    float x0, y0, x1, y1;
    WD_RECT rect;
    int x, y;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Axis legends */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(95,95,95));
    if(chart->axis1.name != NULL) {
        wdDrawString(ctx->canvas, ctx->font, &geom->axis_x_name_rect,
                chart->axis1.name, -1, ctx->solid_brush,
                WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
    if(chart->axis2.name != NULL) {
        chart_draw_vert_string(ctx->canvas, ctx->font, &geom->axis_y_name_rect,
                chart->axis2.name, -1, ctx->solid_brush);
    }

    /* Horizontal grid lines */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(191,191,191));
    x0 = geom->core_rect.x0;
    x1 = geom->core_rect.x1;
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        if(y == 0)
            continue;

        y1 = y0 = column_map_y(y, geom);
        wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    }
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(0,0,0));
    y1 = y0 = column_map_y(0, geom);
    wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);

    /* Labels */
    for(x = 0; x < geom->min_count; x++) {
        rect.x0 = geom->core_rect.x0 + (x + 0.5f) *
                (geom->core_rect.x1 - geom->core_rect.x0) / geom->min_count;
        rect.y0 = geom->core_rect.y1 + (layout->font_size.cy+1) / 2.0f;
        rect.x1 = rect.x0;
        rect.y1 = rect.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis1, x, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rect, buffer, -1,
                ctx->solid_brush, WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }

    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        rect.x0 = geom->core_rect.x0 - (layout->font_size.cx+1) / 2.0f;
        rect.y0 = column_map_y(y, geom) - (layout->font_size.cy+1) / 2.0f;
        rect.x1 = rect.x0;
        rect.y1 = rect.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis2, y, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rect, buffer, -1,
                ctx->solid_brush, WD_STR_RIGHTALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
}

static void
column_paint(chart_t* chart, BOOL is_stacked, chart_paint_t* ctx,
             const chart_layout_t* layout)
{
    cache_t cache;
    column_geometry_t geom;
    int set_ix, n, i;
    WD_RECT column_rect;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    column_calc_geometry(chart, is_stacked, layout, &cache, &geom);
    column_paint_grid(chart, ctx, layout, &geom);

    /* Paint all data sets */
    if(is_stacked) {
        column_rect.x0 = geom.core_rect.x0;
        for(i = 0; i < geom.min_count; i++) {
            column_rect.x0 += geom.group_padding + geom.column_padding;
            column_rect.x1 = column_rect.x0 + geom.column_width;
            column_rect.y1 = column_map_y(0, &geom);

            for(set_ix = 0; set_ix < n; set_ix++) {
                column_rect.y0 = column_map_y(cache_stack(&cache, set_ix, i), &geom);

                /* Paint aura for hot bar */
                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    float aura_inlate = 0.75f * geom.group_padding;

                    wdSetSolidBrushColor(ctx->solid_brush,
                            WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));
                    wdFillRect(ctx->canvas, ctx->solid_brush,
                            column_rect.x0 - aura_inlate, column_rect.y0,
                            column_rect.x1 + aura_inlate, column_rect.y1);
                }

                /* Paint bar */
                wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
                wdFillRect(ctx->canvas, ctx->solid_brush,
                        column_rect.x0, column_rect.y0, column_rect.x1, column_rect.y1);
                column_rect.y1 = column_rect.y0;
            }

            column_rect.x0 = column_rect.x1 + geom.group_padding + geom.column_padding;
        }
    } else {
        column_rect.x0 = geom.core_rect.x0;
        column_rect.y1 = column_map_y(0, &geom);
        for(i = 0; i < geom.min_count; i++) {
            column_rect.x0 += geom.group_padding;

            for(set_ix = 0; set_ix < n; set_ix++) {
                column_rect.x0 += geom.column_padding;
                column_rect.x1 = column_rect.x0 + geom.column_width;
                column_rect.y0 = column_map_y(CACHE_VALUE(&cache, set_ix, i), &geom);

                /* Paint aura for hot bar */
                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    float aura_inlate = 0.75f * geom.group_padding;

                    wdSetSolidBrushColor(ctx->solid_brush,
                            WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));
                    wdFillRect(ctx->canvas, ctx->solid_brush,
                            column_rect.x0 - aura_inlate, column_rect.y0,
                            column_rect.x1 + aura_inlate, column_rect.y1);
                }

                /* Paint bar */
                wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
                wdFillRect(ctx->canvas, ctx->solid_brush,
                        column_rect.x0, column_rect.y0, column_rect.x1, column_rect.y1);
                column_rect.x0 = column_rect.x1 + geom.column_padding;
            }

            column_rect.x0 += geom.group_padding;
        }
    }

    CACHE_FINI(&cache);
}

static void
column_hit_test(chart_t* chart, BOOL is_stacked, chart_paint_t* ctx,
             chart_layout_t* layout, int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    column_geometry_t geom;
    int set_ix, n, i;
    WD_RECT column_rect;

    n = dsa_size(&chart->data);
    CACHE_INIT(&cache, chart);
    column_calc_geometry(chart, is_stacked, layout, &cache, &geom);

    /* Paint all data sets */
    if(is_stacked) {
        column_rect.x0 = geom.core_rect.x0;
        for(i = 0; i < geom.min_count; i++) {
            column_rect.x0 += geom.group_padding + geom.column_padding;
            column_rect.x1 = column_rect.x0 + geom.column_width;
            column_rect.y1 = column_map_y(0, &geom);

            for(set_ix = 0; set_ix < n; set_ix++) {
                column_rect.y0 = column_map_y(cache_stack(&cache, set_ix, i), &geom);

                if(column_rect.x0 <= x  &&  x < column_rect.x1  &&
                   column_rect.y0 <= y  &&  y < column_rect.y1) {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }

                column_rect.y1 = column_rect.y0;
            }

            column_rect.x0 = column_rect.x1 + geom.group_padding + geom.column_padding;
        }
    } else {
        column_rect.x0 = geom.core_rect.x0;
        column_rect.y1 = column_map_y(0, &geom);
        for(i = 0; i < geom.min_count; i++) {
            column_rect.x0 += geom.group_padding;

            for(set_ix = 0; set_ix < n; set_ix++) {
                column_rect.x0 += geom.column_padding;
                column_rect.x1 = column_rect.x0 + geom.column_width;
                column_rect.y0 = column_map_y(CACHE_VALUE(&cache, set_ix, i), &geom);

                if(column_rect.x0 <= x  &&  x < column_rect.x1  &&
                   column_rect.y0 <= y  &&  y < column_rect.y1) {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }

                column_rect.x0 = column_rect.x1 + geom.column_padding;
            }

            column_rect.x0 += geom.group_padding;
        }
    }

done:
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

typedef struct bar_geometry_tag bar_geometry_t;
struct bar_geometry_tag {
    WD_RECT axis_x_name_rect;
    WD_RECT axis_y_name_rect;
    WD_RECT core_rect;

    int min_count;

    int min_x;
    int max_x;
    int step_x;
    int min_step_x;

    float bar_height;
    float bar_padding;
    float group_padding;
};

static inline float
bar_map_x(int x, const bar_geometry_t* geom)
{
    return chart_map_x(x, geom->min_x, geom->max_x, &geom->core_rect);
}

static void
bar_calc_geometry(chart_t* chart, BOOL is_stacked, const chart_layout_t* layout,
                  cache_t* cache, bar_geometry_t* geom)
{
    int set_ix, n, i;
    chart_data_t* data;
    int label_x_w, label_x_h;
    int label_y_w, label_y_h;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];
    int tw;

    n = dsa_size(&chart->data);

    if(n > 0) {
        /* Find extreme values */
        geom->min_count = INT32_MAX;
        geom->min_x = INT32_MAX;
        geom->max_x = INT32_MIN;
        for(set_ix = 0; set_ix < n; set_ix++) {
            data = DSA_ITEM(&chart->data, set_ix, chart_data_t);

            if(data->count < geom->min_count)
                geom->min_count = data->count;

            for(i = 0; i < data->count; i++) {
                int x;

                if(is_stacked)
                    x = cache_stack(cache, set_ix, i);
                else
                    x = CACHE_VALUE(cache, set_ix, i);

                if(x < geom->min_x)
                    geom->min_x = x;
                if(x > geom->max_x)
                    geom->max_x = x;
            }
        }

        /* We want the chart to include the axis */
        if(geom->min_x > 0)
            geom->min_x = 0;
        else if(geom->max_x < 0)
            geom->max_x = 0;
    } else {
        geom->min_count = 0;
        geom->min_x = 0;
        geom->max_x = 0;
    }

    /* Avoid singularity */
    if(geom->min_x >= geom->max_x)
        geom->max_x = geom->min_x + 1;

    /* Round to nice values */
    geom->min_x = chart_round_value(geom->min_x + chart->axis2.offset, FALSE) - chart->axis2.offset;
    geom->max_x = chart_round_value(geom->max_x + chart->axis2.offset, TRUE) - chart->axis2.offset;

    /* Compute space for labels of horizontal axis
     * (note for BAR chart this is secondary!!!) */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis2, geom->min_x, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis2, geom->max_x, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of vertical axis */
    label_y_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->axis1, 0, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->axis1, geom->min_count, buffer);
    tw = mc_string_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute axis legend width/height */
    if(chart->axis2.name != NULL) {
        geom->axis_x_name_rect.y0 = layout->body_rect.bottom - layout->font_size.cy;
        geom->axis_x_name_rect.y1 = geom->axis_x_name_rect.y0 + layout->font_size.cy;
    } else {
        geom->axis_x_name_rect.y0 = layout->body_rect.bottom;
        geom->axis_x_name_rect.y1  = geom->axis_x_name_rect.y0;
    }
    if(chart->axis1.name != NULL) {
        geom->axis_y_name_rect.x0 = layout->body_rect.left;
        geom->axis_y_name_rect.x1 = geom->axis_y_name_rect.x0 + layout->font_size.cy;
    } else {
        geom->axis_y_name_rect.x0 = layout->body_rect.left;
        geom->axis_y_name_rect.x1 = geom->axis_y_name_rect.x0;
    }

    /* Compute core area */
    geom->core_rect.x0 = layout->body_rect.left + label_y_w;
    geom->core_rect.y0 = layout->body_rect.top + (label_y_h+1) / 2.0f;
    geom->core_rect.x1 = layout->body_rect.right;
    geom->core_rect.y1 = layout->body_rect.bottom - label_x_h;
    if(geom->axis_x_name_rect.y1 > geom->axis_x_name_rect.y0)
        geom->core_rect.y1 -= geom->axis_x_name_rect.y1 - geom->axis_x_name_rect.y0 + layout->margin;
    if(geom->axis_y_name_rect.x1 > geom->axis_y_name_rect.x0)
        geom->core_rect.x0 += geom->axis_y_name_rect.x1 - geom->axis_y_name_rect.x0 + layout->margin;

    /* Steps for painting secondary lines */
    geom->step_x = chart_round_value((geom->max_x - geom->min_x) * label_x_w / (geom->core_rect.x1 - geom->core_rect.x0), TRUE);
    if(geom->step_x < 1)
        geom->step_x = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neighborhood pixels as it looks too ugly. */
    chart_fixup_rect_h(&geom->core_rect, geom->min_x, geom->max_x, geom->step_x);
    geom->min_step_x = ((geom->min_x + geom->step_x - 1) / geom->step_x) * geom->step_x;

    if(is_stacked) {
        geom->bar_height = (geom->core_rect.y1 - geom->core_rect.y0) / (1.61f * geom->min_count);
        geom->group_padding = (0.61f * geom->bar_height) / 2;
        geom->bar_padding = 0.0f;
    } else {
        if(n > 0) {
            geom->bar_height = (geom->core_rect.y1 - geom->core_rect.y0) / ((n+0.5f) * geom->min_count);
            geom->group_padding = 0.25f * geom->bar_height;
            geom->bar_padding = 0.15f * geom->bar_height;
            geom->bar_height -= 2.0f * geom->bar_padding;
        } else {
            geom->bar_height = 0.0f;
            geom->bar_padding = 0.0f;
            geom->group_padding = 0.0f;
        }
    }

    /* Refresh axis legend areas after the fix-up */
    geom->axis_x_name_rect.x0 = geom->core_rect.x0;
    geom->axis_x_name_rect.x1 = geom->core_rect.x1;
    geom->axis_y_name_rect.y0 = geom->core_rect.y0;
    geom->axis_y_name_rect.y1 = geom->core_rect.y1;
}

static void
bar_paint_grid(chart_t* chart, chart_paint_t* ctx,
               const chart_layout_t* layout, const bar_geometry_t* geom)
{
    int x, y;
    float x0, y0, x1, y1;
    WD_RECT rect;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Axis legends */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(95,95,95));
    if(chart->axis2.name != NULL) {
        wdDrawString(ctx->canvas, ctx->font, &geom->axis_x_name_rect,
                chart->axis2.name, -1, ctx->solid_brush,
                WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
    if(chart->axis1.name != NULL) {
        chart_draw_vert_string(ctx->canvas, ctx->font, &geom->axis_y_name_rect,
                chart->axis1.name, -1, ctx->solid_brush);
    }

    /* Vertical grid lines */
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(191,191,191));
    y0 = geom->core_rect.y0;
    y1 = geom->core_rect.y1;
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        if(x == 0)
            continue;
        x1 = x0 = bar_map_x(x, geom);
        wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);
    }
    wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(0,0,0));
    x1 = x0 = bar_map_x(0, geom);
    wdDrawLine(ctx->canvas, ctx->solid_brush, x0, y0, x1, y1, 1.0f);

    /* Labels */
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        rect.x0 = bar_map_x(x, geom);
        rect.y0 = geom->core_rect.y1 + (layout->font_size.cy+1) / 2.0f;
        rect.x1 = rect.x0;
        rect.y1 = rect.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis2, x, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rect, buffer, -1, ctx->solid_brush,
                WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }

    for(y = 0; y < geom->min_count; y++) {
        rect.x0 = geom->core_rect.x0 - (layout->font_size.cx+1) / 2.0f;
        rect.y0 = geom->core_rect.y1 - (y + 0.5f) *
                (geom->core_rect.y1 - geom->core_rect.y0) / geom->min_count -
                (layout->font_size.cy+1) / 2.0f;
        rect.x1 = rect.x0;
        rect.y1 = rect.y0 + layout->font_size.cy;
        chart_str_value(&chart->axis1, y, buffer);
        wdDrawString(ctx->canvas, ctx->font, &rect, buffer, -1, ctx->solid_brush,
                WD_STR_RIGHTALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }
}

static void
bar_paint(chart_t* chart, BOOL is_stacked, chart_paint_t* ctx,
          const chart_layout_t* layout)
{
    cache_t cache;
    bar_geometry_t geom;
    int set_ix, n, i;
    WD_RECT bar_rect;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    bar_calc_geometry(chart, is_stacked, layout, &cache, &geom);
    bar_paint_grid(chart, ctx, layout, &geom);

    /* Paint all data sets */
    if(is_stacked) {
        bar_rect.y1 = geom.core_rect.y1;
        for(i = 0; i < geom.min_count; i++) {
            bar_rect.y1 -= geom.group_padding + geom.bar_padding;
            bar_rect.y0 = bar_rect.y1 - geom.bar_height;
            bar_rect.x0 = bar_map_x(0, &geom);

            for(set_ix = 0; set_ix < n; set_ix++) {
                bar_rect.x1 = bar_map_x(cache_stack(&cache, set_ix, i), &geom);

                /* Paint aura for hot bar */
                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    float aura_inlate = 0.75f * geom.group_padding;

                    wdSetSolidBrushColor(ctx->solid_brush,
                            WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));
                    wdFillRect(ctx->canvas, ctx->solid_brush,
                            bar_rect.x0, bar_rect.y0 - aura_inlate,
                            bar_rect.x1, bar_rect.y1 + aura_inlate);
                }

                /* Paint bar */
                wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
                wdFillRect(ctx->canvas, ctx->solid_brush,
                        bar_rect.x0, bar_rect.y0, bar_rect.x1, bar_rect.y1);
                bar_rect.x0 = bar_rect.x1;
            }

            bar_rect.y1 = bar_rect.y0 - geom.group_padding - geom.bar_padding;
        }
    } else {
        bar_rect.x0 = bar_map_x(0, &geom);
        bar_rect.y1 = geom.core_rect.y1;
        for(i = 0; i < geom.min_count; i++) {
            bar_rect.y1 -= geom.group_padding;
            for(set_ix = n-1; set_ix >= 0; set_ix--) {
                bar_rect.y1 -= geom.bar_padding;
                bar_rect.y0 = bar_rect.y1 - geom.bar_height;
                bar_rect.x1 = bar_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);

                /* Paint aura for hot bar */
                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    float aura_inlate = 0.8f * geom.bar_padding;

                    wdSetSolidBrushColor(ctx->solid_brush,
                            WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));
                    wdFillRect(ctx->canvas, ctx->solid_brush,
                            bar_rect.x0, bar_rect.y0 - aura_inlate,
                            bar_rect.x1, bar_rect.y1 + aura_inlate);
                }

                /* Paint bar */
                wdSetSolidBrushColor(ctx->solid_brush,
                        WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
                wdFillRect(ctx->canvas, ctx->solid_brush,
                        bar_rect.x0, bar_rect.y0, bar_rect.x1, bar_rect.y1);
                bar_rect.y1 = bar_rect.y0 - geom.bar_padding;
            }

            bar_rect.y1 -= geom.group_padding;
        }
    }

    CACHE_FINI(&cache);
}

static void
bar_hit_test(chart_t* chart, BOOL is_stacked, chart_paint_t* ctx,
             const chart_layout_t* layout, int x, int y, int* p_set_ix, int* p_i)
{
    cache_t cache;
    bar_geometry_t geom;
    int set_ix, n, i;
    WD_RECT bar_rect;

    n = dsa_size(&chart->data);
    CACHE_INIT(&cache, chart);
    bar_calc_geometry(chart, is_stacked, layout, &cache, &geom);

    /* Paint all data sets */
    if(is_stacked) {
        bar_rect.y1 = geom.core_rect.y1;
        for(i = 0; i < geom.min_count; i++) {
            bar_rect.y1 -= geom.group_padding + geom.bar_padding;
            bar_rect.y0 = bar_rect.y1 - geom.bar_height;
            bar_rect.x0 = bar_map_x(0, &geom);

            for(set_ix = 0; set_ix < n; set_ix++) {
                bar_rect.x1 = bar_map_x(cache_stack(&cache, set_ix, i), &geom);

                if(bar_rect.x0 <= x  &&  x < bar_rect.x1  &&
                   bar_rect.y0 <= y  &&  y < bar_rect.y1) {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }
            }

            bar_rect.y1 = bar_rect.y0 - geom.group_padding + geom.bar_padding;
        }
    } else {
        bar_rect.x0 = bar_map_x(0, &geom);
        bar_rect.y1 = geom.core_rect.y1;
        for(i = 0; i < geom.min_count; i++) {
            bar_rect.y1 -= geom.group_padding;
            for(set_ix = n-1; set_ix >= 0; set_ix--) {
                bar_rect.y1 -= geom.bar_padding;
                bar_rect.y0 = bar_rect.y1 - geom.bar_height;
                bar_rect.x1 = bar_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);

                if(bar_rect.x0 <= x  &&  x < bar_rect.x1  &&
                   bar_rect.y0 <= y  &&  y < bar_rect.y1) {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }
                bar_rect.y1 = bar_rect.y0 - geom.bar_padding;
            }

            bar_rect.y1 -= geom.group_padding;
        }
    }

done:
    CACHE_FINI(&cache);
}

static inline void
bar_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->axis2, buffer, bufsize);
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
    mc_font_size(chart->font, &layout->font_size, FALSE);

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
chart_paint_legend(chart_t* chart, chart_paint_t* ctx,
                   const chart_layout_t* layout)
{
    WD_FONTMETRICS fm;
    float color_size;
    int set_ix, n;
    WD_RECT color_rect;
    WD_RECT text_rect;

    if(MC_ERR(ctx->font == NULL)) {
        MC_TRACE("chart_paint_legend: ctx->font == NULL");
        return;
    }

    wdFontMetrics(ctx->font, &fm);
    color_size = fm.fEmHeight;

    color_rect.x0 = layout->legend_rect.left;
    color_rect.y0 = layout->legend_rect.top + (fm.fLeading - fm.fEmHeight) / 2.0f;
    color_rect.x1 = color_rect.x0 + color_size;
    color_rect.y1 = color_rect.y0 + color_size;

    /* Fix into the pixel grid (anti-aliasing would do ugly effects here). */
    color_rect.x0 = color_rect.x0 + 0.5f;
    color_rect.x1 = ceilf(color_rect.x1) + 0.5f;

    text_rect.x0 = color_rect.x1 + ceilf(0.2f * color_size);
    text_rect.y0 = layout->legend_rect.top;
    text_rect.x1 = layout->legend_rect.right;
    text_rect.y1 = layout->legend_rect.bottom;

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        TCHAR buf[20];
        TCHAR* name;
        WD_RECT measure;
        float text_height;

        /* Paint active aura */
        if(set_ix == chart->hot_set_ix) {
            wdSetSolidBrushColor(ctx->solid_brush,
                    WD_COLOR_FROM_GDI(color_hint(chart_data_color(chart, set_ix))));
            wdDrawRect(ctx->canvas, ctx->solid_brush,
                    color_rect.x0 - 2.5f, color_rect.y0 - 2.5f,
                    color_rect.x1 + 2.5f, color_rect.y1 + 2.5f, 2.0);
        }

        /* Legend color */
        wdSetSolidBrushColor(ctx->solid_brush,
                WD_COLOR_FROM_GDI(chart_data_color(chart, set_ix)));
        wdFillRect(ctx->canvas, ctx->solid_brush,
                color_rect.x0, color_rect.y0, color_rect.x1, color_rect.y1);

        /* Legend text */
        if(data->name != NULL) {
            name = data->name;
        } else {
            _stprintf(buf, _T("data-set-%d"), set_ix);
            name = buf;
        }
        wdMeasureString(ctx->canvas, ctx->font, &text_rect, name, -1, &measure, 0);
        wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(0,0,0));
        wdDrawString(ctx->canvas, ctx->font, &text_rect, name, -1, ctx->solid_brush, WD_STR_NOCLIP);

        /* Recalculate for next data series */
        text_height = ceilf(measure.y1 - measure.y0);
        color_rect.y0 += text_height;
        color_rect.y1 += text_height;
        text_rect.y0 += text_height;
        if(text_rect.y0 >= text_rect.y1)
            break;
    }
}

static int
chart_hit_test_legend(chart_t* chart, chart_paint_t* ctx,
                      chart_layout_t* layout, int x, int y)
{
    WD_FONTMETRICS fm;
    float color_size;
    WD_RECT color_rect;
    WD_RECT text_rect;
    int set_ix, n;

    if(MC_ERR(ctx->font == NULL)) {
        MC_TRACE("chart_hit_test_legend: ctx->font == NULL");
        return -1;
    }

    wdFontMetrics(ctx->font, &fm);
    color_size = fm.fEmHeight;

    color_rect.x0 = layout->legend_rect.left;
    color_rect.y0 = layout->legend_rect.top + (fm.fLeading - fm.fEmHeight) / 2.0f;
    color_rect.x1 = color_rect.x0 + color_size;
    color_rect.y1 = color_rect.y0 + color_size;

    /* Fix into the pixel grid (anti-aliasing would do ugly effects here). */
    color_rect.x0 = color_rect.x0 + 0.5f;
    color_rect.x1 = ceilf(color_rect.x1) + 0.5f;

    text_rect.x0 = color_rect.x1 + ceilf(0.2f * color_size);
    text_rect.y0 = layout->legend_rect.top;
    text_rect.x1 = layout->legend_rect.right;
    text_rect.y1 = layout->legend_rect.bottom;

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        TCHAR buf[20];
        TCHAR* name;
        WD_RECT measure;
        float text_height;

        /* Legend text */
        if(data->name != NULL) {
            name = data->name;
        } else {
            _stprintf(buf, _T("data-set-%d"), set_ix);
            name = buf;
        }

        wdMeasureString(ctx->canvas, ctx->font, &text_rect, name, -1, &measure, 0);
        if(measure.y0 <= y  &&  y <= measure.y1)
            return set_ix;
        text_height = ceilf(measure.y1 - measure.y0);
        text_rect.y0 += text_height;
    }

    return -1;
}

static void
chart_paint_ctx_init(chart_paint_t* ctx, WD_HCANVAS canvas, HFONT font)
{
    ctx->canvas = canvas;
    ctx->solid_brush = wdCreateSolidBrush(canvas, 0);
    ctx->font = wdCreateFontWithGdiHandle(font);
}

static void
chart_paint_ctx_fini(chart_paint_t* ctx)
{
    if(ctx->font != NULL)
        wdDestroyFont(ctx->font);
    if(ctx->solid_brush != NULL)
        wdDestroyBrush(ctx->solid_brush);
    if(ctx->canvas != NULL)
        wdDestroyCanvas(ctx->canvas);
}

static void
chart_free_cached_paint_ctx(chart_t* chart)
{
    if(chart->paint_ctx != NULL) {
        chart_paint_ctx_fini(chart->paint_ctx);
        free(chart->paint_ctx);
        chart->paint_ctx = NULL;
    }
}

static BOOL
chart_paint_with_ctx(chart_t* chart, chart_paint_t* ctx, RECT* dirty, BOOL erase)
{
    chart_layout_t layout;
	DWORD type;

    wdBeginPaint(ctx->canvas);

    if(erase)
        wdClear(ctx->canvas, WD_COLOR_FROM_GDI(GetSysColor(COLOR_WINDOW)));

    chart_calc_layout(chart, &layout);

    /* Paint title */
    if(!mc_rect_is_empty(&layout.title_rect)) {
        WCHAR title[256];
        WD_RECT rc = { layout.title_rect.left, layout.title_rect.top,
                            layout.title_rect.right, layout.title_rect.bottom };

        GetWindowTextW(chart->win, title, MC_SIZEOF_ARRAY(title));
        wdSetSolidBrushColor(ctx->solid_brush, WD_RGB(0,0,0));
        wdDrawString(ctx->canvas, ctx->font, &rc, title, -1, ctx->solid_brush,
                          WD_STR_CENTERALIGN | WD_STR_NOWRAP | WD_STR_NOCLIP);
    }

    /* Paint legend */
    chart_paint_legend(chart, ctx, &layout);

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
            BOOL is_stacked = (type == MC_CHS_STACKEDLINE  ||  type == MC_CHS_STACKEDAREA);
            BOOL is_area = (type == MC_CHS_AREA  ||  type == MC_CHS_STACKEDAREA);
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

    return wdEndPaint(ctx->canvas);
}

static void
chart_paint(chart_t* chart)
{
    PAINTSTRUCT ps;
    chart_paint_t* ctx;
    chart_paint_t tmp_ctx;

    BeginPaint(chart->win, &ps);
    if(chart->no_redraw)
        goto no_paint;

    if(chart->paint_ctx != NULL) {
        /* Use the cached paint context. */
        ctx = chart->paint_ctx;
    } else {
        /* Initialize new context. */
        WD_HCANVAS c = wdCreateCanvasWithPaintStruct(chart->win, &ps,
                (chart->style & MC_CHS_DOUBLEBUFFER) ? WD_CANVAS_DOUBLEBUFFER : 0);
        if(MC_ERR(c == NULL))
            goto no_paint;
        chart_paint_ctx_init(&tmp_ctx, c, chart->font);
        ctx = &tmp_ctx;
    }

    /* Do the painting itself. */
    if(chart_paint_with_ctx(chart, ctx, &ps.rcPaint, ps.fErase)) {
        /* We are allowed to cache the context for reuse. */
        if(ctx == &tmp_ctx) {
            ctx = (chart_paint_t*) malloc(sizeof(chart_paint_t));
            if(ctx != NULL) {
                memcpy(ctx, &tmp_ctx, sizeof(chart_paint_t));
                chart->paint_ctx = ctx;
            } else {
                chart_paint_ctx_fini(&tmp_ctx);
            }
        }
    } else {
        /* We are instructed to destroy the context. */
        chart_paint_ctx_fini(ctx);
        if(chart->paint_ctx != NULL) {
            free(chart->paint_ctx);
            chart->paint_ctx = NULL;
        }
    }

no_paint:
    EndPaint(chart->win, &ps);
}

static void
chart_printclient(chart_t* chart, HDC dc)
{
    chart_paint_t ctx;
    WD_HCANVAS c;
    RECT rect;

    GetClientRect(chart->win, &rect);

    c = wdCreateCanvasWithHDC(dc, &rect, 0);
    if(c == NULL)
        return;

    chart_paint_ctx_init(&ctx, c, chart->font);
    chart_paint_with_ctx(chart, &ctx, &rect, TRUE);
    chart_paint_ctx_fini(&ctx);
}

static void
chart_hit_test_with_ctx(chart_t* chart, chart_paint_t* ctx, int x, int y,
                        int* set_ix, int* i)
{
    chart_layout_t layout;

    chart_calc_layout(chart, &layout);

    *set_ix = -1;
    *i = -1;

    if(mc_rect_contains_xy(&layout.legend_rect, x, y)) {
        *set_ix = chart_hit_test_legend(chart, ctx, &layout, x, y);
    } else if(mc_rect_contains_xy(&layout.body_rect, x, y)) {
        DWORD type = (chart->style & MC_CHS_TYPEMASK);
        switch(type) {
            case MC_CHS_PIE:
                pie_hit_test(chart, ctx, &layout, x, y, set_ix, i);
                return;

            case MC_CHS_SCATTER:
                scatter_hit_test(chart, ctx, &layout, x, y, set_ix, i);
                return;

            case MC_CHS_STACKEDLINE:
            case MC_CHS_STACKEDAREA:
            case MC_CHS_LINE:
            case MC_CHS_AREA:
            {
                BOOL is_stacked = (type == MC_CHS_STACKEDLINE || type == MC_CHS_STACKEDAREA);
                line_hit_test(chart, is_stacked, ctx, &layout, x, y, set_ix, i);
                return;
            }

            case MC_CHS_STACKEDCOLUMN:
            case MC_CHS_COLUMN:
            {
                BOOL is_stacked = (type == MC_CHS_STACKEDCOLUMN);
                column_hit_test(chart, is_stacked, ctx, &layout, x, y, set_ix, i);
                return;
            }

            case MC_CHS_STACKEDBAR:
            case MC_CHS_BAR:
            {
                BOOL is_stacked = (type == MC_CHS_STACKEDBAR);
                bar_hit_test(chart, is_stacked, ctx, &layout, x, y, set_ix, i);
                return;
            }
        }
    }
}

static void
chart_hit_test(chart_t* chart, int x, int y, int* set_ix, int* i)
{
    chart_paint_t* ctx;
    chart_paint_t tmp_ctx;
    HDC dc = NULL;

    if(chart->paint_ctx != NULL) {
        /* Use the cached paint context. */
        ctx = chart->paint_ctx;
    } else {
        /* Initialize new context. */
        RECT rect;
        WD_HCANVAS c;

        dc = GetDCEx(NULL, NULL, DCX_CACHE);
        GetClientRect(chart->win, &rect);
        c = wdCreateCanvasWithHDC(dc, &rect, 0);
        if(MC_ERR(c == NULL)) {
            MC_TRACE("chart_hit_test: wdCreateCanvasWithHDC() failed.");
            *set_ix = -1;
            *i = -1;
            return;
        }
        chart_paint_ctx_init(&tmp_ctx, c, chart->font);
        ctx = &tmp_ctx;
    }

    if(MC_ERR(ctx->canvas == NULL))
        return;

    chart_hit_test_with_ctx(chart, ctx, x, y, set_ix, i);

    if(ctx == &tmp_ctx)
        chart_paint_ctx_fini(ctx);
    if(dc != NULL)
        ReleaseDC(NULL, dc);
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
            InvalidateRect(chart->win, NULL, TRUE);
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
            InvalidateRect(chart->win, NULL, TRUE);
    }
}

static void
chart_setup_hot(chart_t* chart)
{
    if(IsWindowEnabled(chart->win)) {
        DWORD pos;
        int set_ix, i;

        pos = GetMessagePos();
        chart_hit_test(chart, GET_X_LPARAM(pos), GET_Y_LPARAM(pos), &set_ix, &i);
        chart->hot_set_ix = set_ix;
        chart->hot_i = i;
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
        InvalidateRect(chart->win, NULL, TRUE);

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
        InvalidateRect(chart->win, NULL, TRUE);

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
        InvalidateRect(chart->win, NULL, TRUE);
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
        InvalidateRect(chart->win, NULL, TRUE);

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
        InvalidateRect(chart->win, NULL, TRUE);

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
        InvalidateRect(chart->win, NULL, TRUE);

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
        InvalidateRect(chart->win, NULL, TRUE);

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
        InvalidateRect(chart->win, NULL, TRUE);

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

    if((chart->style & MC_CHS_DOUBLEBUFFER) != (ss->styleNew & MC_CHS_DOUBLEBUFFER))
        chart_free_cached_paint_ctx(chart);

    chart->style = ss->styleNew;

    chart_setup_hot(chart);
    if(!chart->no_redraw)
        InvalidateRect(chart->win, NULL, TRUE);
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

    doublebuffer_init();

    return chart;
}

static int
chart_create(chart_t* chart)
{
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
}

static void
chart_ncdestroy(chart_t* chart)
{
    doublebuffer_fini();
    dsa_fini(&chart->data, chart_data_dtor);

    if(chart->axis1.name != NULL)
        free(chart->axis1.name);

    if(chart->axis2.name != NULL)
        free(chart->axis2.name);

    chart_free_cached_paint_ctx(chart);
    free(chart);
}

static LRESULT CALLBACK
chart_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    chart_t* chart = (chart_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            chart_paint(chart);
            return 0;

        case WM_PRINTCLIENT:
            chart_printclient(chart, (HDC) wp);
            return 0;

        case WM_DISPLAYCHANGE:
            chart_free_cached_paint_ctx(chart);
            InvalidateRect(win, NULL, FALSE);
            break;

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

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
                InvalidateRect(win, NULL, TRUE);
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
            if(chart->paint_ctx != NULL)
                wdResizeCanvas(chart->paint_ctx->canvas, LOWORD(lp), HIWORD(lp));
            break;

        case WM_SETTEXT:
        {
            LRESULT res;
            res = DefWindowProc(win, msg, wp, lp);
            chart_setup_hot(chart);
            if(!chart->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return res;
        }

        case WM_GETFONT:
            return (LRESULT) chart->font;

        case WM_SETFONT:
            if(chart->font != (HFONT) wp) {
                chart->font = (HFONT) wp;
                chart_free_cached_paint_ctx(chart);
                chart_setup_hot(chart);
            }
            if((BOOL) lp  &&  !chart->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_SETREDRAW:
            chart->no_redraw = !wp;
            if(!chart->no_redraw)
                InvalidateRect(win, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_STATIC;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE)
                chart_style_changed(chart, (STYLESTRUCT*) lp);
            break;

        case WM_SYSCOLORCHANGE:
            if(!chart->no_redraw)
                InvalidateRect(win, NULL, TRUE);
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

    if(MC_ERR(!wdInitialize(WD_INIT_STRINGAPI))) {
        MC_TRACE("chart_init_module: wdInitialize() failed.");
        return -1;
    }

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = chart_proc;
    wc.cbWndExtra = sizeof(chart_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = chart_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("chart_init_module: RegisterClass() failed");
        wdTerminate(WD_INIT_STRINGAPI);
        return -1;
    }

    return 0;
}

void
chart_fini_module(void)
{
    UnregisterClass(chart_wc, NULL);

    wdTerminate(WD_INIT_STRINGAPI);
}
