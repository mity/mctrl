/*
 * Copyright (c) 2013 Martin Mitas
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
#include "dsa.h"
#include "gdix.h"
#include "color.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define CHART_DEBUG     1*/

#ifdef CHART_DEBUG
    #define CHART_TRACE       MC_TRACE
#else
    #define CHART_TRACE(...)  do {} while(0)
#endif


static const TCHAR chart_wc[] = MC_WC_CHART;    /* Window class name */


/* If we ever allow larger range for factor exponents, we may need to make
 * this larger, so the string representation of any values fits in. */
#define CHART_STR_VALUE_MAX_LEN       32


typedef struct chart_axis_tag chart_axis_t;
struct chart_axis_tag {
    int offset;
    BYTE factor_exp;
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
    HFONT font;
    DWORD style          : 16;
    DWORD no_redraw      : 1;
    DWORD mouse_tracked  : 1;
    DWORD tooltip_active : 1;
    chart_axis_t primary_axis;
    chart_axis_t secondary_axis;
    int min_visible_value;
    int max_visible_value;
    int hot_set_ix;
    int hot_i;
    dsa_t data;
};

typedef struct chart_layout_tag chart_layout_t;
struct chart_layout_tag {
    SIZE font_size;
    RECT title_rect;
    RECT body_rect;
    RECT legend_rect;
};

typedef struct chart_paint_tag chart_paint_t;
struct chart_paint_tag {
    chart_layout_t layout;
    GpGraphics* gfx;
    GpPen* pen;
    GpBrush* brush;
    GpStringFormat* format;
    GpFont* font;
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

static inline ARGB
chart_data_ARGB(chart_t* chart, int set_ix)
{
    return gdix_Color(chart_data_color(chart, set_ix));
}

static inline ARGB
chart_data_ARGB_with_alpha(chart_t* chart, int set_ix, BYTE alpha)
{
    return gdix_AColor(alpha, chart_data_color(chart, set_ix));
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
    SendMessage(chart->notify_win, WM_NOTIFY, info.hdr.idFrom, (LPARAM) &info);

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
    static int count = MC_ARRAY_SIZE(nice_numbers);
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
     * cases to have relatively small values, so in general the seqencial
     * search is probably more effective here. */
    i = 0;
    while(value > nice_numbers[i])
        i++;

    if(value == nice_numbers[i])
        return nice_numbers[i];

    return nice_numbers[up ? i : i-1];
}

static inline int
chart_text_width(const TCHAR* str, HFONT font)
{
    HDC dc;
    HFONT old_font;
    SIZE s;

    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    old_font = SelectObject(dc, font);
    GetTextExtentPoint32(dc, str, _tcslen(str), &s);
    SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);

    return s.cx;
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

    if(axis->factor_exp == 0) {
        _swprintf(buffer, L"%d", value);
    } else if(axis->factor_exp > 0) {
        int i, n;
        n = _swprintf(buffer, L"%d", value);
        for(i = n; i < n + axis->factor_exp; i++)
            buffer[n] = L'0';
        buffer[n + axis->factor_exp] = L'\0';
    } else {
        int factor = 10;
        int i;
        WCHAR dec_delim[4];
        int dec_delim_len;

        for(i = -(axis->factor_exp); i != 1; i--)
            factor *= 10;

        dec_delim_len = GetLocaleInfoW(GetThreadLocale(), LOCALE_SDECIMAL,
                                       dec_delim, MC_ARRAY_SIZE(dec_delim));
        if(MC_ERR(dec_delim_len == 0)) {
            dec_delim[0] = L'.';
            dec_delim_len = 1;
        }

        _swprintf(buffer, L"%d%.*s%0.*d", value / factor, dec_delim_len, dec_delim,
                  -(axis->factor_exp), MC_ABS(value) % factor);
    }
}

static inline REAL
chart_map_x(int x, int min_x, int max_x, RectF* rect)
{
    return rect->X + ((x - min_x) * rect->Width) / (REAL)(max_x - min_x);
}

static inline REAL
chart_unmap_x(int rx, int min_x, int max_x, RectF* rect)
{
    return min_x + (rx - rect->X) * (max_x - min_x) / rect->Width;
}

static inline REAL
chart_map_y(int y, int min_y, int max_y, RectF* rect)
{
    return rect->Y + ((max_y - y) * rect->Height) / (REAL)(max_y - min_y);
}

static void
chart_fixup_rect_v(RectF* rect, int min_y, int max_y, int step_y)
{
    REAL pps_old, pps_new;  /* pixels per step */
    REAL height;

    pps_old = chart_map_y(step_y, min_y, max_y, rect) - chart_map_y(2*step_y, min_y, max_y, rect);
    pps_new = floorf(pps_old);

    height = (rect->Height / pps_old) * pps_new;
    rect->Y += (rect->Height - height) / 2.0;
    rect->Height = height;
    rect->Y = roundf(rect->Y + rect->Height) - rect->Height;
}

static void
chart_fixup_rect_h(RectF* rect, int min_x, int max_x, int step_x)
{
    REAL pps_old, pps_new;  /* pixels per step */
    REAL width;

    pps_old = chart_map_x(2*step_x, min_x, max_x, rect) - chart_map_x(step_x, min_x, max_x, rect);
    pps_new = floorf(pps_old);

    width = (rect->Width / pps_old) * pps_new;
    rect->X += (rect->Width - width) / 2.0;
    rect->Width = width;
    rect->X = roundf(rect->X);
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


/***************
 *** Tooltip ***
 ***************/

static void
tooltip_create(chart_t* chart)
{
    TTTOOLINFO info = { 0 };
    NMTOOLTIPSCREATED ttc = { { 0 }, 0 };

    chart->tooltip_win = CreateWindow(TOOLTIPS_CLASS, NULL, WS_POPUP,
                                      0, 0, 0, 0, chart->win, 0, 0, 0);
    if(MC_ERR(chart->tooltip_win == NULL)) {
        MC_TRACE_ERR("tooltip_create: CreateWindow() failed");
        return;
    }

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
    info.hwnd = chart->win;
    SendMessage(chart->tooltip_win, TTM_ADDTOOL, 0, (LPARAM) &info);

    ttc.hdr.hwndFrom = chart->win;
    ttc.hdr.idFrom = GetWindowLong(chart->win, GWL_ID);
    ttc.hdr.code = NM_TOOLTIPSCREATED;
    ttc.hwndToolTips = chart->tooltip_win;
    SendMessage(chart->notify_win, WM_NOTIFY, ttc.hdr.idFrom, (LPARAM) &ttc);
}

static void
tooltip_activate(chart_t* chart, BOOL show)
{
    TTTOOLINFO info = { 0 };

    info.cbSize = sizeof(TTTOOLINFO);
    info.hwnd = chart->win;
    SendMessage(chart->tooltip_win, TTM_TRACKACTIVATE, show, (LPARAM) &info);

    chart->tooltip_active = show;
}

static void
tooltip_set_pos(chart_t* chart, int x, int y)
{
    TTTOOLINFO info = { 0 };
    DWORD size;
    POINT pt;

    info.cbSize = sizeof(TTTOOLINFO);
    info.hwnd = chart->win;

    size = SendMessage(chart->tooltip_win, TTM_GETBUBBLESIZE, 0, (LPARAM) &info);
    pt.x = x - LOWORD(size) / 2;
    pt.y = y - HIWORD(size) - 5;
    ClientToScreen(chart->win, &pt);
    SendMessage(chart->tooltip_win, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x, pt.y));
}

static void
tooltip_set_text(chart_t* chart, const TCHAR* str)
{
    TTTOOLINFO info = { 0 };

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
    info.hwnd = chart->win;
    info.lpszText = (TCHAR*) str;
    SendMessage(chart->tooltip_win, TTM_UPDATETIPTEXT, 0, (LPARAM) &info);
}

static void
tooltip_destroy(chart_t* chart)
{
    DestroyWindow(chart->tooltip_win);
    chart->tooltip_win = NULL;
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
            SendMessage(cache->chart->notify_win, WM_NOTIFY, info.hdr.idFrom, (LPARAM) &info);

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

static inline REAL
pie_normalize_angle(REAL angle)
{
    while(angle < 0.0)
        angle += 360.0;
    while(angle >= 360.0)
        angle -= 360.0;
    return angle;
}

static inline REAL
pie_vector_angle(REAL x0, REAL y0, REAL x1, REAL y1)
{
    return atan2f(y1-y0, x1-x0) * (180.0/M_PI);
}

static inline BOOL
pie_pt_in_sweep(REAL angle, REAL sweep, REAL x0, REAL y0, REAL x1, REAL y1)
{
    return (pie_normalize_angle(pie_vector_angle(x0,y0,x1,y1) - angle) < sweep);
}

static inline BOOL
pie_rect_in_sweep(REAL angle, REAL sweep, REAL x0, REAL y0, RectF* rect)
{
    return (pie_pt_in_sweep(angle, sweep, x0, y0, rect->X, rect->Y)  &&
            pie_pt_in_sweep(angle, sweep, x0, y0, rect->X + rect->Width, rect->Y)  &&
            pie_pt_in_sweep(angle, sweep, x0, y0, rect->X, rect->Y + rect->Height)  &&
            pie_pt_in_sweep(angle, sweep, x0, y0, rect->X + rect->Width, rect->Y + rect->Height));
}

static inline int
pie_value(chart_t* chart, int set_ix)
{
    return MC_ABS(chart_value(chart, set_ix, 0));
}

typedef struct pie_geometry_tag pie_geometry_t;
struct pie_geometry_tag {
    REAL x;
    REAL y;
    REAL r;
    REAL sum;
};

static void
pie_calc_geometry(chart_t* chart, chart_paint_t* ctx, pie_geometry_t* geom)
{
    int set_ix, n;

    geom->x = (ctx->layout.body_rect.left + ctx->layout.body_rect.right) / 2.0;
    geom->y = (ctx->layout.body_rect.top + ctx->layout.body_rect.bottom) / 2.0;
    geom->r = MC_MIN(mc_width(&ctx->layout.body_rect), mc_height(&ctx->layout.body_rect)) / 2.0 - 10.0;

    geom->sum = 0.0;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++)
        geom->sum += (REAL) pie_value(chart, set_ix);
}

static void
pie_paint(chart_t* chart, chart_paint_t* ctx)
{
    pie_geometry_t geom;
    int set_ix, n, val;
    REAL angle, sweep;
    REAL label_angle;  /* in rads */
    RectF label_rect;
    RectF label_bounds;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    pie_calc_geometry(chart, ctx, &geom);

    gdix_SetPenColor(ctx->pen, gdix_Color(GetSysColor(COLOR_WINDOW)));
    gdix_SetPenWidth(ctx->pen, 0.3);

    angle = -90.0;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        val = pie_value(chart, set_ix);
        sweep = (360.0 * (REAL) val) / geom.sum;

        /* Paint the pie */
        gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
        gdix_FillPie(ctx->gfx, ctx->brush, geom.x - geom.r, geom.y - geom.r,
                     2.0 * geom.r, 2.0 * geom.r, angle, sweep);

        /* Paint active aura */
        if(set_ix == chart->hot_set_ix) {
            GpStatus status;
            GpPath* path;

            status = gdix_CreatePath(FillModeAlternate, &path);
            if(MC_ERR(status != Ok)) {
                MC_TRACE("pie_paint: gdix_CreatePath() failed [%ld]", status);
            } else {
                COLORREF c = color_hint(chart_data_color(chart, set_ix));

                gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
                gdix_AddPathArc(path, geom.x - geom.r - 1.5, geom.y - geom.r - 1.5,
                                2.0 * geom.r + 3.0, 2.0 * geom.r + 3.0, angle, sweep);
                gdix_AddPathArc(path, geom.x - geom.r - 10, geom.y - geom.r - 10,
                                2.0 * geom.r + 20, 2.0 * geom.r + 20, angle+sweep, -sweep);
                gdix_FillPath(ctx->gfx, ctx->brush, path);
                gdix_DeletePath(path);
            }
        }

        /* Paint white borders */
        gdix_DrawPie(ctx->gfx, ctx->pen, geom.x - geom.r - 12, geom.y - geom.r - 12,
                     2.0 * geom.r + 24, 2.0 * geom.r + 24, angle, sweep);

        /* Paint label (if it fits in) */
        label_angle = (angle + sweep/2.0) * M_PI / 180.0;
        label_rect.X = geom.x + 0.75 * geom.r * cosf(label_angle);
        label_rect.Y = geom.y + 0.75 * geom.r * sinf(label_angle) - ctx->layout.font_size.cy / 2.0;
        label_rect.Width = 0;
        label_rect.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->primary_axis, val, buffer);
        gdix_MeasureString(ctx->gfx, buffer, -1, ctx->font, &label_rect,
                           ctx->format, &label_bounds, NULL, NULL);
        if(pie_rect_in_sweep(angle, sweep, geom.x, geom.y, &label_bounds)) {
            gdix_SetSolidFillColor(ctx->brush, gdix_RGB(255,255,255));
            gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &label_rect, ctx->format, ctx->brush);
        }

        angle += sweep;
    }
}

static void
pie_hit_test(chart_t* chart, chart_paint_t* ctx, int x, int y,
             int* p_set_ix, int* p_i)
{
    pie_geometry_t geom;
    int set_ix, n, val;
    REAL angle, sweep;
    REAL dx, dy;

    pie_calc_geometry(chart, ctx, &geom);

    dx = geom.x - x;
    dy = geom.y - y;
    if(dx * dx + dy * dy > geom.r * geom.r)
        return;

    angle = -90.0;
    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        val = pie_value(chart, set_ix);
        sweep = (360.0 * (REAL) val) / geom.sum;

        if(pie_pt_in_sweep(angle, sweep, geom.x, geom.y, x, y)) {
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
    chart_tooltip_text_common(chart, &chart->primary_axis, buffer, bufsize);
}


/*********************
 *** Scatter Chart ***
 *********************/

typedef struct scatter_geometry_tag scatter_geometry_t;
struct scatter_geometry_tag {
    RectF core_rect;

    int min_x;
    int max_x;
    int step_x;
    int min_step_x;

    int min_y;
    int max_y;
    int step_y;
    int min_step_y;
};

static inline REAL
scatter_map_y(int y, scatter_geometry_t* geom)
{
    return chart_map_y(y, geom->min_y, geom->max_y, &geom->core_rect);
}

static inline REAL
scatter_map_x(int x, scatter_geometry_t* geom)
{
    return chart_map_x(x, geom->min_x, geom->max_x, &geom->core_rect);
}

static void
scatter_calc_geometry(chart_t* chart, chart_layout_t* layout, cache_t* cache,
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

                if(x < geom->min_x)    geom->min_x = x;
                if(x > geom->max_x)    geom->max_x = x;
                if(y < geom->min_y)    geom->min_y = y;
                if(y > geom->max_y)    geom->max_y = y;
            }
        }

        /* We want the chart to include the axis */
        if(geom->min_x > 0)        geom->min_x = 0;
        else if(geom->max_x < 0)   geom->max_x = 0;

        if(geom->min_y > 0)        geom->min_y = 0;
        else if(geom->max_y < 0)   geom->max_y = 0;
    } else {
        geom->min_x = 0;
        geom->max_x = 0;
        geom->min_y = 0;
        geom->max_y = 0;
    }

    /* Avoid singularity */
    if(geom->min_x >= geom->max_x)   geom->max_x = geom->min_x + 1;
    if(geom->min_y >= geom->max_y)   geom->max_y = geom->min_y + 1;

    /* Round to nice values */
    geom->min_x = chart_round_value(geom->min_x + chart->primary_axis.offset, FALSE) - chart->primary_axis.offset;
    geom->max_x = chart_round_value(geom->max_x + chart->primary_axis.offset, TRUE) - chart->primary_axis.offset;
    geom->min_y = chart_round_value(geom->min_y + chart->secondary_axis.offset, FALSE) - chart->secondary_axis.offset;
    geom->max_y = chart_round_value(geom->max_y + chart->secondary_axis.offset, TRUE) - chart->secondary_axis.offset;

    /* Compute space for labels of horizontal axis */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->primary_axis, geom->max_x, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->primary_axis, geom->max_x, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of verical axis */
    label_y_w = 6 * layout->font_size.cx;
    chart_str_value(&chart->secondary_axis, geom->min_y, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->secondary_axis, geom->max_y, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute core area */
    geom->core_rect.X = layout->body_rect.left + label_y_w;
    geom->core_rect.Y = layout->body_rect.top + (label_y_h+1) / 2;
    geom->core_rect.Width = layout->body_rect.right - geom->core_rect.X;
    geom->core_rect.Height = layout->body_rect.bottom - label_x_h - geom->core_rect.Y;

    /* Steps for painting secondary lines */
    geom->step_x = chart_round_value(
            (geom->max_x - geom->min_x) * label_x_w / geom->core_rect.Width, TRUE);
    if(geom->step_x < 1)
        geom->step_x = 1;
    geom->step_y = chart_round_value(
            (geom->max_y - geom->min_y) * 3*label_y_h / (2*geom->core_rect.Height), TRUE);
    if(geom->step_y < 1)
        geom->step_y = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neiberhood pixels as it looks too ugly. */
    chart_fixup_rect_v(&geom->core_rect, geom->min_y, geom->max_y, geom->step_y);
    chart_fixup_rect_h(&geom->core_rect, geom->min_x, geom->max_x, geom->step_x);

    geom->min_step_x = ((geom->min_x + geom->step_x - 1) / geom->step_x) * geom->step_x;
    geom->min_step_y = ((geom->min_y + geom->step_y - 1) / geom->step_y) * geom->step_y;
}

static void
scatter_paint_grid(chart_t* chart, chart_paint_t* ctx, scatter_geometry_t* geom)
{
    REAL rx, ry;
    int x, y;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Secondary lines */
    gdix_SetPenColor(ctx->pen, gdix_RGB(191,191,191));
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        if(x == 0)
            continue;
        rx = scatter_map_x(x, geom);
        gdix_DrawLine(ctx->gfx, ctx->pen, rx, geom->core_rect.Y,
                      rx, geom->core_rect.Y + geom->core_rect.Height);
    }
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        if(y == 0)
            continue;
        ry = scatter_map_y(y, geom);
        gdix_DrawLine(ctx->gfx, ctx->pen, geom->core_rect.X, ry,
                      geom->core_rect.X + geom->core_rect.Width, ry);
    }

    /* Primary lines (axis) */
    gdix_SetPenColor(ctx->pen, gdix_RGB(0, 0, 0));
    rx = scatter_map_x(0, geom);
    gdix_DrawLine(ctx->gfx, ctx->pen, rx, geom->core_rect.Y,
                  rx, geom->core_rect.Y + geom->core_rect.Height);
    ry = scatter_map_y(0, geom);
    gdix_DrawLine(ctx->gfx, ctx->pen, geom->core_rect.X, ry,
                  geom->core_rect.X + geom->core_rect.Width, ry);

    /* Labels */
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        RectF rc;

        rc.X = scatter_map_x(x, geom);
        rc.Y = geom->core_rect.Y + geom->core_rect.Height + (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->primary_axis, x, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }
    gdix_SetStringFormatAlign(ctx->format, StringAlignmentFar);
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        RectF rc;

        rc.X = geom->core_rect.X - (ctx->layout.font_size.cx+1) / 2;
        rc.Y = scatter_map_y(y, geom) - (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->secondary_axis, y, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }
}

static void
scatter_paint(chart_t* chart, chart_paint_t* ctx)
{
    cache_t cache;
    scatter_geometry_t geom;
    int set_ix, n, i;
    chart_data_t* data;
    REAL rx, ry;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    scatter_calc_geometry(chart, &ctx->layout, &cache, &geom);
    scatter_paint_grid(chart, ctx, &geom);

    gdix_SetPenWidth(ctx->pen, 2.5);

    /* Paint hot aura */
    if(chart->hot_set_ix >= 0) {
        COLORREF c;
        int i0, i1;

        set_ix = chart->hot_set_ix;
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        c = color_hint(chart_data_color(chart, set_ix));
        gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));

        if(chart->hot_i >= 0) {
            i0 = chart->hot_i;
            i1 = chart->hot_i+1;
        } else {
            i0 = 0;
            i1 = data->count-1;
        }

        for(i = i0; i < i1; i += 2) {
            rx = scatter_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);
            ry = scatter_map_y(CACHE_VALUE(&cache, set_ix, i+1), &geom);
            gdix_FillEllipse(ctx->gfx, ctx->brush, rx-4.0, ry-4.0, 8.0, 8.0);
        }
    }

    /* Paint all data sets */
    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
        for(i = 0; i < data->count-1; i += 2) {  /* '-1' to protect if ->count is odd */
            rx = scatter_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);
            ry = scatter_map_y(CACHE_VALUE(&cache, set_ix, i+1), &geom);
            gdix_FillEllipse(ctx->gfx, ctx->brush, rx-2.0, ry-2.0, 4.0, 4.0);
        }
    }

    CACHE_FINI(&cache);
}

static void
scatter_hit_test(chart_t* chart, chart_paint_t* ctx, int x, int y,
                 int* p_set_ix, int* p_i)
{
    cache_t cache;
    scatter_geometry_t geom;
    REAL rx, ry;
    int set_ix, n, i;
    chart_data_t* data;
    REAL dx, dy;
    REAL dist2;

    rx = (REAL) x;
    ry = (REAL) y;
    n = dsa_size(&chart->data);
    dist2 = GetSystemMetrics(SM_CXDOUBLECLK) * GetSystemMetrics(SM_CYDOUBLECLK);

    CACHE_INIT(&cache, chart);
    scatter_calc_geometry(chart, &ctx->layout, &cache, &geom);

    for(set_ix = 0; set_ix < n; set_ix++) {
        data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        for(i = 0; i < data->count-1; i += 2) {
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

        chart_str_value(&chart->primary_axis, x, x_str);
        chart_str_value(&chart->secondary_axis, y, y_str);

        _sntprintf(buffer, bufsize, _T("%ls / %ls"), x_str, y_str);
        buffer[bufsize-1] = _T('\0');
    }
}


/*************************
 *** Line & Area Chart ***
 *************************/

typedef struct line_geometry_tag line_geometry_t;
struct line_geometry_tag {
    RectF core_rect;

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

static inline REAL
line_map_y(int y, line_geometry_t* geom)
{
    return chart_map_y(y, geom->min_y, geom->max_y, &geom->core_rect);
}

static inline REAL
line_map_x(int x, line_geometry_t* geom)
{
    return chart_map_x(x, geom->min_x, geom->max_x, &geom->core_rect);
}

static void
line_calc_geometry(chart_t* chart, BOOL stacked, chart_layout_t* layout,
                   cache_t* cache, line_geometry_t* geom)
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

                if(stacked)
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
        if(geom->min_y > 0)        geom->min_y = 0;
        else if(geom->max_y < 0)   geom->max_y = 0;
    } else {
        geom->min_count = 0;
        geom->min_y = 0;
        geom->max_y = 0;
    }

    geom->min_x = 0;
    geom->max_x = geom->min_count-1;

    /* Avoid singularity */
    if(geom->min_x >= geom->max_x)   geom->max_x = geom->min_x + 1;
    if(geom->min_y >= geom->max_y)   geom->max_y = geom->min_y + 1;

    /* Round to nice values */
    geom->min_y = chart_round_value(geom->min_y + chart->secondary_axis.offset, FALSE) - chart->secondary_axis.offset;
    geom->max_y = chart_round_value(geom->max_y + chart->secondary_axis.offset, TRUE) - chart->secondary_axis.offset;

    /* Compute space for labels of horizontal axis */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->primary_axis, 0, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->primary_axis, geom->min_count, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of verical axis */
    label_y_w = 6 * layout->font_size.cx;
    chart_str_value(&chart->secondary_axis, geom->min_y, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->secondary_axis, geom->max_y, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute core area */
    geom->core_rect.X = layout->body_rect.left + label_y_w;
    geom->core_rect.Y = layout->body_rect.top + (label_y_h+1) / 2;
    geom->core_rect.Width = layout->body_rect.right - geom->core_rect.X;
    geom->core_rect.Height = layout->body_rect.bottom - label_x_h - geom->core_rect.Y;

    /* Steps for painting secondary lines */
    geom->step_x = chart_round_value(
            (geom->max_x - geom->min_x) * label_x_w / geom->core_rect.Width, TRUE);
    if(geom->step_x < 1)
        geom->step_x = 1;
    geom->step_y = chart_round_value(
            (geom->max_y - geom->min_y) * label_y_h / geom->core_rect.Height, TRUE);
    if(geom->step_y < 1)
        geom->step_y = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neiberhood pixels as it looks too ugly. */
    chart_fixup_rect_h(&geom->core_rect, geom->min_x, geom->max_x, geom->step_x);
    chart_fixup_rect_v(&geom->core_rect, geom->min_y, geom->max_y, geom->step_y);

    geom->min_step_x = ((geom->min_x + geom->step_x - 1) / geom->step_x) * geom->step_x;
    geom->min_step_y = ((geom->min_y + geom->step_y - 1) / geom->step_y) * geom->step_y;
}

static void
line_paint_grid(chart_t* chart, chart_paint_t* ctx, line_geometry_t* geom)
{
    REAL rx, ry;
    int x, y;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Secondary lines */
    gdix_SetPenColor(ctx->pen, gdix_RGB(191,191,191));
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        if(x == 0)
            continue;
        rx = line_map_x(x, geom);
        gdix_DrawLine(ctx->gfx, ctx->pen, rx, geom->core_rect.Y,
                      rx, geom->core_rect.Y + geom->core_rect.Height);
    }
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        if(y == 0)
            continue;
        ry = line_map_y(y, geom);
        gdix_DrawLine(ctx->gfx, ctx->pen, geom->core_rect.X, ry,
                      geom->core_rect.X + geom->core_rect.Width, ry);
    }

    /* Primary lines (axis) */
    gdix_SetPenColor(ctx->pen, gdix_RGB(0, 0, 0));
    rx = line_map_x(0, geom);
    gdix_DrawLine(ctx->gfx, ctx->pen, rx, geom->core_rect.Y,
                  rx, geom->core_rect.Y + geom->core_rect.Height);
    ry = line_map_y(0, geom);
    gdix_DrawLine(ctx->gfx, ctx->pen, geom->core_rect.X, ry,
                  geom->core_rect.X + geom->core_rect.Width, ry);

    /* Labels */
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        RectF rc;

        rc.X = line_map_x(x, geom);
        rc.Y = geom->core_rect.Y + geom->core_rect.Height + (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->primary_axis, x, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }

    gdix_SetStringFormatAlign(ctx->format, StringAlignmentFar);
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        RectF rc;

        rc.X = geom->core_rect.X - (ctx->layout.font_size.cx+1) / 2;
        rc.Y = line_map_y(y, geom) - (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->secondary_axis, y, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }
}

static void
line_paint(chart_t* chart, BOOL area, BOOL stacked, chart_paint_t* ctx)
{
    cache_t cache;
    line_geometry_t geom;
    int set_ix, n, x, y;
    REAL rx, ry;
    REAL prev_rx = 0, prev_ry = 0;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    line_calc_geometry(chart, stacked, &ctx->layout, &cache, &geom);
    line_paint_grid(chart, ctx, &geom);

    /* Paint area */
    if(area  &&  geom.min_count > 1) {
        GpStatus status;
        GpPath* path;

        status = gdix_CreatePath(FillModeAlternate, &path);
        if(MC_ERR(status != Ok)) {
            MC_TRACE("line_paint: gdix_CreatePath() failed [%ld]", status);
        } else {
            int rx0, rx1, ry0;

            rx0 = line_map_x(0, &geom);
            rx1 = line_map_x(geom.min_count-1, &geom);
            ry0 = line_map_y(0, &geom);

            for(set_ix = 0; set_ix < n; set_ix++) {
                BYTE alpha;
                if(set_ix == chart->hot_set_ix  &&  chart->hot_i < 0)
                    alpha = 0x60;
                else
                    alpha = 0x3f;
                gdix_SetSolidFillColor(ctx->brush,
                            chart_data_ARGB_with_alpha(chart, set_ix, alpha));

                for(x = 0; x < geom.min_count; x++) {
                    if(stacked)
                        y = cache_stack(&cache, set_ix, x);
                    else
                        y = CACHE_VALUE(&cache, set_ix, x);
                    rx = line_map_x(x, &geom);
                    ry = line_map_y(y, &geom);
                    if(x > 0)
                        gdix_AddPathLine(path, prev_rx, prev_ry, rx, ry);
                    prev_rx = rx;
                    prev_ry = ry;
                }

                if(!stacked || set_ix == 0) {
                    gdix_AddPathLine(path, rx1, ry0, rx0, ry0);
                } else {
                    for(x = geom.min_count - 1; x >= 0; x--) {
                        y = cache_stack(&cache, set_ix-1, x);
                        rx = line_map_x(x, &geom);
                        ry = line_map_y(y, &geom);
                        gdix_AddPathLine(path, prev_rx, prev_ry, rx, ry);
                        prev_rx = rx;
                        prev_ry = ry;
                    }
                }

                gdix_FillPath(ctx->gfx, ctx->brush, path);
                gdix_ResetPath(path);
            }

            gdix_DeletePath(path);
        }
    }

    /* Paint hot aura */
    if(chart->hot_set_ix >= 0) {
        COLORREF c;
        int x0, x1;

        set_ix = chart->hot_set_ix;
        c = color_hint(chart_data_color(chart, set_ix));
        gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
        gdix_SetPenColor(ctx->pen, gdix_Color(c));
        gdix_SetPenWidth(ctx->pen, 2.5);

        if(chart->hot_i >= 0) {
            x0 = chart->hot_i;
            x1 = chart->hot_i+1;
        } else {
            x0 = 0;
            x1 = DSA_ITEM(&chart->data, set_ix, chart_data_t)->count;
        }

        for(x = x0; x < x1; x++) {
            if(stacked)
                y = cache_stack(&cache, set_ix, x);
            else
                y = CACHE_VALUE(&cache, set_ix, x);
            rx = line_map_x(x, &geom);
            ry = line_map_y(y, &geom);
            gdix_FillEllipse(ctx->gfx, ctx->brush, rx-4.0, ry-4.0, 8.0, 8.0);
            if(x > x0)
                gdix_DrawLine(ctx->gfx, ctx->pen, prev_rx, prev_ry, rx, ry);
            prev_rx = rx;
            prev_ry = ry;
        }

        gdix_SetPenWidth(ctx->pen, 1.0);
    }

    /* Paint all data sets */
    for(set_ix = 0; set_ix < n; set_ix++) {
        gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
        gdix_SetPenColor(ctx->pen, chart_data_ARGB(chart, set_ix));
        for(x = 0; x < geom.min_count; x++) {
            if(stacked)
                y = cache_stack(&cache, set_ix, x);
            else
                y = CACHE_VALUE(&cache, set_ix, x);
            rx = line_map_x(x, &geom);
            ry = line_map_y(y, &geom);
            gdix_FillEllipse(ctx->gfx, ctx->brush, rx-2.0, ry-2.0, 4.0, 4.0);
            if(x > 0)
                gdix_DrawLine(ctx->gfx, ctx->pen, prev_rx, prev_ry, rx, ry);
            prev_rx = rx;
            prev_ry = ry;
        }
    }

    CACHE_FINI(&cache);
}

static void
line_hit_test(chart_t* chart, BOOL stacked, chart_paint_t* ctx, int x, int y,
              int* p_set_ix, int* p_i)
{
    cache_t cache;
    line_geometry_t geom;
    REAL rx, ry;
    int max_dx, max_dy;
    int set_ix, n, i;
    REAL ri;
    int i0, i1;
    REAL dx, dy;
    REAL dist2;

    rx = (REAL) x;
    ry = (REAL) y;
    n = dsa_size(&chart->data);
    max_dx = GetSystemMetrics(SM_CXDOUBLECLK);
    max_dy = GetSystemMetrics(SM_CYDOUBLECLK);
    dist2 = max_dx * max_dy;

    CACHE_INIT(&cache, chart);
    line_calc_geometry(chart, stacked, &ctx->layout, &cache, &geom);

    /* Find index which maps horizontally close enough to the X mouse
     * coordinate. */
    ri = chart_unmap_x((REAL) x, geom.min_x, geom.max_x, &geom.core_rect);

    /* Any close hot point must be close of the ri */
    i0 = (int) (ri - MC_MAX(max_dx, max_dy) + 0.9999);
    i1 = (int) (ri + MC_MAX(max_dx, max_dy));

    for(set_ix = 0; set_ix < n; set_ix++) {
        for(i = i0; i <= i1; i++) {
            if(stacked)
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
    chart_tooltip_text_common(chart, &chart->secondary_axis, buffer, bufsize);
}


/********************
 *** Column Chart ***
 ********************/

typedef struct column_geometry_tag column_geometry_t;
struct column_geometry_tag {
    RectF core_rect;

    int min_count;

    int min_y;
    int max_y;
    int step_y;
    int min_step_y;

    REAL column_width;
    REAL column_padding;
    REAL group_padding;
};

static inline REAL
column_map_y(int y, column_geometry_t* geom)
{
    return chart_map_y(y, geom->min_y, geom->max_y, &geom->core_rect);
}

static void
column_calc_geometry(chart_t* chart, BOOL stacked, chart_layout_t* layout,
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

                if(stacked)
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
        if(geom->min_y > 0)        geom->min_y = 0;
        else if(geom->max_y < 0)   geom->max_y = 0;
    } else {
        geom->min_count = 0;
        geom->min_y = 0;
        geom->max_y = 0;
    }

    /* Avoid singularity */
    if(geom->min_y >= geom->max_y)   geom->max_y = geom->min_y + 1;

    /* Round to nice values */
    geom->min_y = chart_round_value(geom->min_y + chart->secondary_axis.offset, FALSE) - chart->secondary_axis.offset;
    geom->max_y = chart_round_value(geom->max_y + chart->secondary_axis.offset, TRUE) - chart->secondary_axis.offset;

    /* Compute space for labels of horizontal axis */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->primary_axis, 0, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->primary_axis, geom->min_count, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of verical axis */
    label_y_w = 6 * layout->font_size.cx;
    chart_str_value(&chart->secondary_axis, geom->min_y, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->secondary_axis, geom->max_y, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute core area */
    geom->core_rect.X = layout->body_rect.left + label_y_w;
    geom->core_rect.Y = layout->body_rect.top + (label_y_h+1) / 2;
    geom->core_rect.Width = layout->body_rect.right - geom->core_rect.X;
    geom->core_rect.Height = layout->body_rect.bottom - label_x_h - geom->core_rect.Y;

    /* Steps for painting secondary lines */
    geom->step_y = chart_round_value(
            (geom->max_y - geom->min_y) * label_y_h / geom->core_rect.Height, TRUE);
    if(geom->step_y < 1)
        geom->step_y = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neiberhood pixels as it looks too ugly. */
    chart_fixup_rect_v(&geom->core_rect, geom->min_y, geom->max_y, geom->step_y);
    geom->min_step_y = ((geom->min_y + geom->step_y - 1) / geom->step_y) * geom->step_y;

    if(stacked) {
        geom->column_width = geom->core_rect.Width / ((1+0.61) * geom->min_count);
        geom->group_padding = (0.61 * geom->column_width) / 2;
        geom->column_padding = 0;
    } else {
        if(n > 0) {
            geom->column_width = geom->core_rect.Width / ((n+0.5) * geom->min_count);
            geom->group_padding = (0.5 * geom->column_width) / 2;
            geom->column_padding = 0.15 * geom->column_width;
            geom->column_width -= 2 * geom->column_padding;
        } else {
            geom->column_width = 0;
            geom->column_padding = 0;
            geom->group_padding = 0;
        }
    }
}

static void
column_paint_grid(chart_t* chart, chart_paint_t* ctx, column_geometry_t* geom)
{
    REAL ry;
    int x, y;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Secondary lines */
    gdix_SetPenColor(ctx->pen, gdix_RGB(191,191,191));
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        if(y == 0)
            continue;
        ry = column_map_y(y, geom);
        gdix_DrawLine(ctx->gfx, ctx->pen, geom->core_rect.X, ry,
                      geom->core_rect.X + geom->core_rect.Width, ry);
    }

    /* Primary lines (axis) */
    gdix_SetPenColor(ctx->pen, gdix_RGB(0, 0, 0));
    ry = column_map_y(0, geom);
    gdix_DrawLine(ctx->gfx, ctx->pen, geom->core_rect.X, ry,
                  geom->core_rect.X + geom->core_rect.Width, ry);

    /* Labels */
    for(x = 0; x < geom->min_count; x++) {
        RectF rc;

        rc.X = geom->core_rect.X + (x + 0.5) * geom->core_rect.Width / geom->min_count;
        rc.Y = geom->core_rect.Y + geom->core_rect.Height + (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->primary_axis, x, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }

    gdix_SetStringFormatAlign(ctx->format, StringAlignmentFar);
    for(y = geom->min_step_y; y <= geom->max_y; y += geom->step_y) {
        RectF rc;

        rc.X = geom->core_rect.X - (ctx->layout.font_size.cx+1) / 2;
        rc.Y = column_map_y(y, geom) - (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->secondary_axis, y, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }
}

static void
column_paint(chart_t* chart, BOOL stacked, chart_paint_t* ctx)
{
    cache_t cache;
    column_geometry_t geom;
    int set_ix, n, i;
    REAL rx, ry0, ry1;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    column_calc_geometry(chart, stacked, &ctx->layout, &cache, &geom);
    column_paint_grid(chart, ctx, &geom);

    /* Paint all data sets */
    if(stacked) {
        rx = geom.core_rect.X;
        for(i = 0; i < geom.min_count; i++) {
            rx += geom.group_padding + geom.column_padding;
            ry0 = column_map_y(0, &geom);
            for(set_ix = 0; set_ix < n; set_ix++) {
                ry1 = column_map_y(cache_stack(&cache, set_ix, i), &geom);

                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    /* Paint aura */
                    COLORREF c = color_hint(chart_data_color(chart, set_ix));
                    gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
                    gdix_FillRectangle(ctx->gfx, ctx->brush, rx - 0.75*geom.group_padding, ry1,
                                       geom.column_width + 1.5*geom.group_padding, ry0-ry1);
                }

                gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
                gdix_FillRectangle(ctx->gfx, ctx->brush, rx, ry1, geom.column_width, ry0-ry1);

                ry0 = ry1;
            }
            rx += geom.column_width;
            rx += geom.group_padding + geom.column_padding;
        }
    } else {
        rx = geom.core_rect.X;
        ry0 = column_map_y(0, &geom);
        for(i = 0; i < geom.min_count; i++) {
            rx += geom.group_padding;
            for(set_ix = n-1; set_ix >= 0; set_ix--) {
                rx += geom.column_padding;
                ry1 = column_map_y(CACHE_VALUE(&cache, set_ix, i), &geom);

                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    /* Paint aura */
                    COLORREF c = color_hint(chart_data_color(chart, set_ix));
                    gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
                    gdix_FillRectangle(ctx->gfx, ctx->brush, rx - 0.8*geom.column_padding, ry1,
                                       geom.column_width + 1.6*geom.column_padding, ry0-ry1);
                }

                gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
                gdix_FillRectangle(ctx->gfx, ctx->brush, rx, ry1, geom.column_width, ry0-ry1);
                rx += geom.column_width + geom.column_padding;
            }
            rx += geom.group_padding;
        }
    }

    CACHE_FINI(&cache);
}

static void
column_hit_test(chart_t* chart, BOOL stacked, chart_paint_t* ctx, int x, int y,
             int* p_set_ix, int* p_i)
{
    cache_t cache;
    column_geometry_t geom;
    int set_ix, n, i;
    REAL rx, ry0, ry1;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);
    column_calc_geometry(chart, stacked, &ctx->layout, &cache, &geom);

    if(stacked) {
        rx = geom.core_rect.X;
        for(i = 0; i < geom.min_count; i++) {
            rx += geom.group_padding + geom.column_padding;
            ry0 = column_map_y(0, &geom);
            for(set_ix = 0; set_ix < n; set_ix++) {
                ry1 = column_map_y(cache_stack(&cache, set_ix, i), &geom);

                if(rx <= x  &&  x <= rx + geom.column_width  &&
                   ry1 <= y  &&  y <= ry0)
                {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }

                ry0 = ry1;
            }
            rx += geom.column_width;
            rx += geom.group_padding + geom.column_padding;
        }
    } else {
        rx = geom.core_rect.X;
        ry0 = column_map_y(0, &geom);
        for(i = 0; i < geom.min_count; i++) {
            rx += geom.group_padding;
            for(set_ix = n-1; set_ix >= 0; set_ix--) {
                rx += geom.column_padding;
                ry1 = column_map_y(CACHE_VALUE(&cache, set_ix, i), &geom);

                if(rx <= x  &&  x <= rx + geom.column_width  &&
                   ry1 <= y  &&  y <= ry0)
                {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }

                rx += geom.column_width + geom.column_padding;
            }
            rx += geom.group_padding;
        }
    }

done:
    CACHE_FINI(&cache);
}

static inline void
column_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->secondary_axis, buffer, bufsize);
}


/*****************
 *** Bar Chart ***
 *****************/

typedef struct bar_geometry_tag bar_geometry_t;
struct bar_geometry_tag {
    RectF core_rect;

    int min_count;

    int min_x;
    int max_x;
    int step_x;
    int min_step_x;

    REAL bar_height;
    REAL bar_padding;
    REAL group_padding;
};

static inline REAL
bar_map_x(int x, bar_geometry_t* geom)
{
    return chart_map_x(x, geom->min_x, geom->max_x, &geom->core_rect);
}

static void
bar_calc_geometry(chart_t* chart, BOOL stacked, chart_layout_t* layout,
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

                if(stacked)
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
        if(geom->min_x > 0)        geom->min_x = 0;
        else if(geom->max_x < 0)   geom->max_x = 0;
    } else {
        geom->min_count = 0;
        geom->min_x = 0;
        geom->max_x = 0;
    }

    /* Avoid singularity */
    if(geom->min_x >= geom->max_x)   geom->max_x = geom->min_x + 1;

    /* Round to nice values */
    geom->min_x = chart_round_value(geom->min_x + chart->secondary_axis.offset, FALSE) - chart->secondary_axis.offset;
    geom->max_x = chart_round_value(geom->max_x + chart->secondary_axis.offset, TRUE) - chart->secondary_axis.offset;

    /* Compute space for labels of horizontal axis
     * (note for BAR chart this is secondary!!!) */
    label_x_w = 3 * layout->font_size.cx;
    chart_str_value(&chart->secondary_axis, geom->min_x, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    chart_str_value(&chart->secondary_axis, geom->max_x, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_x_w = MC_MAX(label_x_w, tw + layout->font_size.cx);
    label_x_h = (3 * layout->font_size.cy + 1) / 2;

    /* Compute space for labels of verical axis */
    label_y_w = 6 * layout->font_size.cx;
    chart_str_value(&chart->primary_axis, 0, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx);
    chart_str_value(&chart->primary_axis, geom->min_count, buffer);
    tw = chart_text_width(buffer, chart->font);
    label_y_w = MC_MAX(label_y_w, tw + layout->font_size.cx) + (layout->font_size.cx + 1) / 2;
    label_y_h = layout->font_size.cy;

    /* Compute core area */
    geom->core_rect.X = layout->body_rect.left + label_y_w;
    geom->core_rect.Y = layout->body_rect.top + (label_y_h+1) / 2;
    geom->core_rect.Width = layout->body_rect.right - geom->core_rect.X;
    geom->core_rect.Height = layout->body_rect.bottom - label_x_h - geom->core_rect.Y;

    /* Steps for painting secondary lines */
    geom->step_x = chart_round_value(
            (geom->max_x - geom->min_x) * label_x_w / geom->core_rect.Width, TRUE);
    if(geom->step_x < 1)
        geom->step_x = 1;

    /* Fix-up the core rect so that painting secondary lines does mot lead
     * to anti-aliasing to neiberhood pixels as it looks too ugly. */
    chart_fixup_rect_h(&geom->core_rect, geom->min_x, geom->max_x, geom->step_x);
    geom->min_step_x = ((geom->min_x + geom->step_x - 1) / geom->step_x) * geom->step_x;

    if(stacked) {
        geom->bar_height = geom->core_rect.Height / ((1+0.61) * geom->min_count);
        geom->group_padding = (0.61 * geom->bar_height) / 2;
        geom->bar_padding = 0;
    } else {
        if(n > 0) {
            geom->bar_height = geom->core_rect.Height / ((n+0.5) * geom->min_count);
            geom->group_padding = (0.5 * geom->bar_height) / 2;
            geom->bar_padding = 0.15 * geom->bar_height;
            geom->bar_height -= 2 * geom->bar_padding;
        } else {
            geom->bar_height = 0;
            geom->bar_padding = 0;
            geom->group_padding = 0;
        }
    }
}

static void
bar_paint_grid(chart_t* chart, chart_paint_t* ctx, bar_geometry_t* geom)
{
    REAL rx;
    int x, y;
    WCHAR buffer[CHART_STR_VALUE_MAX_LEN];

    /* Secondary lines */
    gdix_SetPenColor(ctx->pen, gdix_RGB(191,191,191));
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        if(x == 0)
            continue;
        rx = bar_map_x(x, geom);
        gdix_DrawLine(ctx->gfx, ctx->pen, rx, geom->core_rect.Y,
                      rx, geom->core_rect.Y + geom->core_rect.Height);
    }

    /* Primary lines (axis) */
    gdix_SetPenColor(ctx->pen, gdix_RGB(0, 0, 0));
    rx = bar_map_x(0, geom);
    gdix_DrawLine(ctx->gfx, ctx->pen, rx, geom->core_rect.Y,
                  rx, geom->core_rect.Y + geom->core_rect.Height);

    /* Labels */
    for(x = geom->min_step_x; x <= geom->max_x; x += geom->step_x) {
        RectF rc;

        rc.X = bar_map_x(x, geom);
        rc.Y = geom->core_rect.Y + geom->core_rect.Height + (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->secondary_axis, x, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }

    gdix_SetStringFormatAlign(ctx->format, StringAlignmentFar);
    for(y = 0; y < geom->min_count; y++) {
        RectF rc;

        rc.X = geom->core_rect.X - (ctx->layout.font_size.cx+1) / 2;
        rc.Y = geom->core_rect.Y + geom->core_rect.Height -
                (y + 0.5) * geom->core_rect.Height / geom->min_count -
                (ctx->layout.font_size.cy+1) / 2;
        rc.Width = 0.0;
        rc.Height = ctx->layout.font_size.cy;
        chart_str_value(&chart->primary_axis, y, buffer);
        gdix_DrawString(ctx->gfx, buffer, -1, ctx->font, &rc, ctx->format, ctx->brush);
    }
}

static void
bar_paint(chart_t* chart, BOOL stacked, chart_paint_t* ctx)
{
    cache_t cache;
    bar_geometry_t geom;
    int set_ix, n, i;
    REAL rx0, rx1, ry;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);

    bar_calc_geometry(chart, stacked, &ctx->layout, &cache, &geom);
    bar_paint_grid(chart, ctx, &geom);

    /* Paint all data sets */
    if(stacked) {
        ry = geom.core_rect.Y + geom.core_rect.Height;
        for(i = 0; i < geom.min_count; i++) {
            rx0 = bar_map_x(0, &geom);
            ry -= geom.group_padding + geom.bar_padding;
            for(set_ix = 0; set_ix < n; set_ix++) {
                rx1 = bar_map_x(cache_stack(&cache, set_ix, i), &geom);

                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    /* Paint aura */
                    COLORREF c = color_hint(chart_data_color(chart, set_ix));
                    gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
                    gdix_FillRectangle(ctx->gfx, ctx->brush, rx0, ry-geom.bar_height - 0.75*geom.group_padding,
                                       rx1-rx0, geom.bar_height + 1.5*geom.group_padding);
                }

                gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
                gdix_FillRectangle(ctx->gfx, ctx->brush, rx0, ry-geom.bar_height,
                                   rx1-rx0, geom.bar_height);
                rx0 = rx1;
            }
            ry -= geom.bar_height;
            ry -= geom.group_padding + geom.bar_padding;
        }
    } else {
        rx0 = bar_map_x(0, &geom);
        ry = geom.core_rect.Y + geom.core_rect.Height;
        for(i = 0; i < geom.min_count; i++) {
            ry -= geom.group_padding;
            for(set_ix = n-1; set_ix >= 0; set_ix--) {
                ry -= geom.bar_padding;
                rx1 = bar_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);

                if(chart->hot_set_ix == set_ix  &&
                   (chart->hot_i == i  ||  chart->hot_i < 0))
                {
                    /* Paint aura */
                    COLORREF c = color_hint(chart_data_color(chart, set_ix));
                    gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
                    gdix_FillRectangle(ctx->gfx, ctx->brush, rx0, ry-geom.bar_height - 0.8*geom.bar_padding,
                                       rx1-rx0, geom.bar_height + 1.6*geom.bar_padding);
                }

                gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
                gdix_FillRectangle(ctx->gfx, ctx->brush, rx0, ry-geom.bar_height,
                                   rx1-rx0, geom.bar_height);
                ry -= geom.bar_height + geom.bar_padding;
            }
            ry -= geom.group_padding;
        }
    }

    CACHE_FINI(&cache);
}

static void
bar_hit_test(chart_t* chart, BOOL stacked, chart_paint_t* ctx, int x, int y,
             int* p_set_ix, int* p_i)
{
    cache_t cache;
    bar_geometry_t geom;
    int set_ix, n, i;
    REAL rx0, rx1, ry;

    n = dsa_size(&chart->data);

    CACHE_INIT(&cache, chart);
    bar_calc_geometry(chart, stacked, &ctx->layout, &cache, &geom);

    if(stacked) {
        ry = geom.core_rect.Y + geom.core_rect.Height;
        for(i = 0; i < geom.min_count; i++) {
            rx0 = bar_map_x(0, &geom);
            ry -= geom.group_padding + geom.bar_padding;
            for(set_ix = 0; set_ix < n; set_ix++) {
                rx1 = bar_map_x(cache_stack(&cache, set_ix, i), &geom);

                if(rx0 <= x  &&  x <= rx1  &&
                   ry-geom.bar_height <= y  &&  y <= ry)
                {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }

                rx0 = rx1;
            }
            ry -= geom.bar_height;
            ry -= geom.group_padding + geom.bar_padding;
        }
    } else {
        rx0 = bar_map_x(0, &geom);
        ry = geom.core_rect.Y + geom.core_rect.Height;
        for(i = 0; i < geom.min_count; i++) {
            ry -= geom.group_padding;
            for(set_ix = n-1; set_ix >= 0; set_ix--) {
                ry -= geom.bar_padding;
                rx1 = bar_map_x(CACHE_VALUE(&cache, set_ix, i), &geom);

                if(rx0 <= x  &&  x <= rx1  &&
                   ry-geom.bar_height <= y  &&  y <= ry)
                {
                    *p_set_ix = set_ix;
                    *p_i = i;
                    goto done;
                }

                ry -= geom.bar_height + geom.bar_padding;
            }
            ry -= geom.group_padding;
        }
    }

done:
    CACHE_FINI(&cache);
}

static inline void
bar_tooltip_text(chart_t* chart, TCHAR* buffer, UINT bufsize)
{
    chart_tooltip_text_common(chart, &chart->secondary_axis, buffer, bufsize);
}


/******************************
 *** Core Control Functions ***
 ******************************/

static void
chart_calc_layout(chart_t* chart, chart_layout_t* layout)
{
    TCHAR buf[2];
    int margin;
    RECT rect;

    GetClientRect(chart->win, &rect);
    mc_font_size(chart->font, &layout->font_size);

    margin = (layout->font_size.cy+1) / 2;

    GetWindowText(chart->win, buf, MC_ARRAY_SIZE(buf));
    if(buf[0] != _T('\0')) {
        layout->title_rect.left = rect.left + margin;
        layout->title_rect.top = rect.top + margin;
        layout->title_rect.right = rect.right - margin;
        layout->title_rect.bottom = layout->title_rect.top + layout->font_size.cy;
    } else {
        mc_rect_set(&layout->title_rect, 0, 0, 0, 0);
    }

    layout->legend_rect.left = rect.right - margin - 12 * layout->font_size.cx;
    layout->legend_rect.top = layout->title_rect.bottom + margin;
    layout->legend_rect.right = rect.right - margin;
    layout->legend_rect.bottom = rect.bottom - margin;

    layout->body_rect.left = rect.left + margin;
    layout->body_rect.top = layout->title_rect.bottom + margin;
    layout->body_rect.right = layout->legend_rect.left - margin;
    layout->body_rect.bottom = rect.bottom - margin;
}

static inline void
chart_legend_set_text_rect(chart_layout_t* layout, RectF* text_rect)
{
    text_rect->X = layout->legend_rect.left + layout->font_size.cy + 6.0;
    text_rect->Y = layout->legend_rect.top;
    text_rect->Width = layout->legend_rect.right - text_rect->X;
    text_rect->Height = layout->legend_rect.bottom - text_rect->Y;
}

static void
chart_paint_legend(chart_t* chart, chart_paint_t* ctx, HDC dc)
{
    TEXTMETRIC tm;
    REAL color_size, color_x, color_y;
    RectF text_rect;
    int set_ix, n;

    GetTextMetrics(dc, &tm);

    color_size = 0.70 * tm.tmAscent;
    color_x = ctx->layout.legend_rect.left + ctx->layout.font_size.cy - 0.90 * tm.tmAscent + 4.0;
    color_y = ctx->layout.legend_rect.top + tm.tmAscent - color_size;

    chart_legend_set_text_rect(&ctx->layout, &text_rect);

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        TCHAR buf[20];
        TCHAR* name;
        RectF bound;

        if(set_ix == chart->hot_set_ix) {
            GpStatus status;
            GpPath* path;

            status = gdix_CreatePath(FillModeAlternate, &path);
            if(MC_ERR(status != Ok)) {
                MC_TRACE("chart_paint_legend: gdix_CreatePath() failed [%ld]", status);
            } else {
                COLORREF c = color_hint(chart_data_color(chart, set_ix));

                gdix_SetSolidFillColor(ctx->brush, gdix_Color(c));
                gdix_AddPathRectangle(path, color_x - 1.5, color_y - 1.5, color_size + 3.0, color_size + 3.0);
                gdix_AddPathRectangle(path, color_x - 3.5, color_y - 3.5, color_size + 7.0, color_size + 7.0);
                gdix_FillPath(ctx->gfx, ctx->brush, path);
                gdix_DeletePath(path);
            }
        }

        gdix_SetSolidFillColor(ctx->brush, chart_data_ARGB(chart, set_ix));
        gdix_FillRectangle(ctx->gfx, ctx->brush, color_x, color_y, color_size, color_size);

        if(data->name != NULL) {
            name = data->name;
        } else {
            _stprintf(buf, _T("data-set-%d"), set_ix);
            name = buf;
        }

        gdix_SetSolidFillColor(ctx->brush, gdix_RGB(0,0,0));
        gdix_DrawString(ctx->gfx, name, -1, ctx->font, &text_rect, ctx->format, ctx->brush);
        gdix_MeasureString(ctx->gfx, name, -1, ctx->font, &text_rect, ctx->format, &bound, NULL, NULL);

        color_y += bound.Height;
        text_rect.Y += bound.Height;
        text_rect.Height -= bound.Height;
    }

    gdix_SetSolidFillColor(ctx->brush, gdix_RGB(0,0,0));
}

static int
chart_hit_test_legend(chart_t* chart, chart_paint_t* ctx, int x, int y)
{
    RectF text_rect;
    int set_ix, n;
    int result_set_ix = -1;

    chart_legend_set_text_rect(&ctx->layout, &text_rect);

    n = dsa_size(&chart->data);
    for(set_ix = 0; set_ix < n; set_ix++) {
        chart_data_t* data = DSA_ITEM(&chart->data, set_ix, chart_data_t);
        TCHAR buf[20];
        TCHAR* name;
        RectF bound;

        if(data->name != NULL) {
            name = data->name;
        } else {
            _stprintf(buf, _T("data-set-%d"), set_ix);
            name = buf;
        }

        gdix_MeasureString(ctx->gfx, name, -1, ctx->font, &text_rect,
                           ctx->format, &bound, NULL, NULL);

        if(bound.Y <= y  &&  y <= bound.Y + bound.Height) {
            result_set_ix = set_ix;
            break;
        }

        text_rect.Y += bound.Height;
        text_rect.Height -= bound.Height;
    }

    return result_set_ix;
}

static void
chart_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    chart_t* chart = (chart_t*) control;
    chart_paint_t ctx;
    GpStatus status;
    HFONT old_font = NULL;
    ARGB black = gdix_RGB(0,0,0);

    if(erase)
        FillRect(dc, dirty, GetSysColorBrush(COLOR_WINDOW));

    if(chart->font)
        old_font = SelectObject(dc, chart->font);

    chart_calc_layout(chart, &ctx.layout);
    status = gdix_CreateFromHDC(dc, &ctx.gfx);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_paint: gdix_CreateFromHDC() failed [%d]", (int)status);
        goto err_CreateFromHDC;
    }
    status = gdix_CreatePen1(black, 1.0, UnitWorld, &ctx.pen);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_paint: gdix_CreatePen1() failed [%d]", (int)status);
        goto err_CreatePen1;
    }
    status = gdix_CreateSolidFill(black, &ctx.brush);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_paint: gdix_CreateSolidFill() failed [%d]", (int)status);
        goto err_CreateSolidFill;
    }
    status = gdix_CreateStringFormat(0, LANG_NEUTRAL, &ctx.format);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_paint: gdix_CreateStringFormat() failed [%d]", (int)status);
        goto err_CreateStringFormat;
    }
    status = gdix_CreateFontFromDC(dc, &ctx.font);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_paint: gdix_CreateFontFromDC() failed [%d]", (int)status);
        goto err_CreateFontFromDC;
    }

    gdix_SetSmoothingMode(ctx.gfx, SmoothingModeHighQuality);

    if(mc_rect_overlaps_rect(dirty, &ctx.layout.legend_rect))
        chart_paint_legend(chart, &ctx, dc);

    gdix_SetStringFormatFlags(ctx.format, StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    gdix_SetStringFormatAlign(ctx.format, StringAlignmentCenter);

    if(!mc_rect_is_empty(&ctx.layout.title_rect)  &&
       mc_rect_overlaps_rect(dirty, &ctx.layout.title_rect))
    {
        WCHAR title[256];
        RectF rc;

        GetWindowTextW(chart->win, title, MC_ARRAY_SIZE(title));

        rc.X = ctx.layout.title_rect.left;
        rc.Y = ctx.layout.title_rect.top;
        rc.Width = mc_width(&ctx.layout.title_rect);
        rc.Height = mc_height(&ctx.layout.title_rect);

        gdix_DrawString(ctx.gfx, title, -1, ctx.font, &rc, ctx.format, ctx.brush);
    }

    if(mc_rect_overlaps_rect(dirty, &ctx.layout.body_rect)) {
        DWORD type = (chart->style & MC_CHS_TYPEMASK);
        switch(type) {
            case MC_CHS_PIE:
                pie_paint(chart, &ctx);
                break;

            case MC_CHS_SCATTER:
                scatter_paint(chart, &ctx);
                break;

            case MC_CHS_LINE:
            case MC_CHS_STACKEDLINE:
            case MC_CHS_AREA:
            case MC_CHS_STACKEDAREA:
            {
                BOOL stacked = (type == MC_CHS_STACKEDLINE  ||  type == MC_CHS_STACKEDAREA);
                BOOL area = (type == MC_CHS_AREA  ||  type == MC_CHS_STACKEDAREA);
                line_paint(chart, area, stacked, &ctx);
                break;
            }

            case MC_CHS_COLUMN:
            case MC_CHS_STACKEDCOLUMN:
            {
                BOOL stacked = (type == MC_CHS_STACKEDCOLUMN);
                column_paint(chart, stacked, &ctx);
                break;
            }

            case MC_CHS_BAR:
            case MC_CHS_STACKEDBAR:
            {
                BOOL stacked = (type == MC_CHS_STACKEDBAR);
                bar_paint(chart, stacked, &ctx);
                break;
            }
        }
    }

    gdix_DeleteFont(ctx.font);
err_CreateFontFromDC:
    gdix_DeleteStringFormat(ctx.format);
err_CreateStringFormat:
    gdix_DeleteBrush(ctx.brush);
err_CreateSolidFill:
    gdix_DeletePen(ctx.pen);
err_CreatePen1:
    gdix_DeleteGraphics(ctx.gfx);
err_CreateFromHDC:
    if(chart->font)
        SelectObject(dc, old_font);
}

static void
chart_hit_test(chart_t* chart, int x, int y, int* set_ix, int* i)
{
    chart_paint_t ctx;
    BOOL in_legend;
    BOOL in_body;
    HDC dc;
    HFONT old_font = NULL;
    GpStatus status;

    *set_ix = -1;
    *i = -1;

    chart_calc_layout(chart, &ctx.layout);
    in_legend = mc_rect_contains_xy(&ctx.layout.legend_rect, x, y);
    in_body = mc_rect_contains_xy(&ctx.layout.body_rect, x, y);

    if(!in_legend  &&  !in_body)
        return;

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    if(chart->font)
        old_font = SelectObject(dc, chart->font);

    status = gdix_CreateFromHDC(dc, &ctx.gfx);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_hit_test: gdix_CreateFromHDC() failed [%d]", (int)status);
        goto err_CreateFromHDC;
    }
    status = gdix_CreateStringFormat(0, LANG_NEUTRAL, &ctx.format);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_hit_test: gdix_CreateStringFormat() failed [%d]", (int)status);
        goto err_CreateStringFormat;
    }
    status = gdix_CreateFontFromDC(dc, &ctx.font);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("chart_hit_test: gdix_CreateFontFromDC() failed [%d]", (int)status);
        goto err_CreateFontFromDC;
    }
    ctx.pen = NULL;
    ctx.brush = NULL;

    gdix_SetSmoothingMode(ctx.gfx, SmoothingModeHighQuality);


    if(in_legend) {
        *set_ix = chart_hit_test_legend(chart, &ctx, x, y);
        *i = -1;
    } else if(in_body) {
        DWORD type = (chart->style & MC_CHS_TYPEMASK);
        switch(type) {
            case MC_CHS_PIE:
                pie_hit_test(chart, &ctx, x, y, set_ix, i);
                break;

            case MC_CHS_SCATTER:
                scatter_hit_test(chart, &ctx, x, y, set_ix, i);
                break;

            case MC_CHS_STACKEDLINE:
            case MC_CHS_STACKEDAREA:
            case MC_CHS_LINE:
            case MC_CHS_AREA:
            {
                BOOL stacked = (type == MC_CHS_STACKEDLINE || type == MC_CHS_STACKEDAREA);
                line_hit_test(chart, stacked, &ctx, x, y, set_ix, i);
                break;
            }

            case MC_CHS_STACKEDCOLUMN:
            case MC_CHS_COLUMN:
            {
                BOOL stacked = (type == MC_CHS_STACKEDCOLUMN);
                column_hit_test(chart, stacked, &ctx, x, y, set_ix, i);
                break;
            }

            case MC_CHS_STACKEDBAR:
            case MC_CHS_BAR:
            {
                BOOL stacked = (type == MC_CHS_STACKEDBAR);
                bar_hit_test(chart, stacked, &ctx, x, y, set_ix, i);
                break;
            }
        }
    }

err_CreateFontFromDC:
    gdix_DeleteStringFormat(ctx.format);
err_CreateStringFormat:
    gdix_DeleteGraphics(ctx.gfx);
err_CreateFromHDC:
    if(chart->font)
        SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);
}

static void
chart_update_tooltip(chart_t* chart)
{
    TCHAR buffer[256];

    if(chart->tooltip_win == NULL)
        return;

    if(chart->hot_set_ix < 0) {
        if(chart->tooltip_active)
            tooltip_activate(chart, FALSE);
        return;
    }

    buffer[0] = _T('\0');

    switch(chart->style & MC_CHS_TYPEMASK) {
        case MC_CHS_PIE:
            pie_tooltip_text(chart, buffer, MC_ARRAY_SIZE(buffer));
            break;

        case MC_CHS_SCATTER:
            scatter_tooltip_text(chart, buffer, MC_ARRAY_SIZE(buffer));
            break;

        case MC_CHS_LINE:
        case MC_CHS_STACKEDLINE:
        case MC_CHS_AREA:
        case MC_CHS_STACKEDAREA:
            line_tooltip_text(chart, buffer, MC_ARRAY_SIZE(buffer));
            break;

        case MC_CHS_COLUMN:
        case MC_CHS_STACKEDCOLUMN:
            column_tooltip_text(chart, buffer, MC_ARRAY_SIZE(buffer));
            break;

        case MC_CHS_BAR:
        case MC_CHS_STACKEDBAR:
            bar_tooltip_text(chart, buffer, MC_ARRAY_SIZE(buffer));
            break;
    }

    if(buffer[0] == _T('\0')) {
        if(chart->tooltip_active)
            tooltip_activate(chart, FALSE);
        return;
    }

    tooltip_set_text(chart, buffer);

    if(!chart->tooltip_active)
        tooltip_activate(chart, TRUE);
}

static void
chart_mouse_move(chart_t* chart, int x, int y)
{
    int set_ix, i;

    if(!IsWindowEnabled(chart->win))
        return;

    chart_hit_test(chart, x, y, &set_ix, &i);

    if(!chart->mouse_tracked  &&  set_ix >= 0) {
        TRACKMOUSEEVENT tme;

        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = chart->win;
        tme.dwHoverTime = HOVER_DEFAULT;

        TrackMouseEvent(&tme);
    }

    if(chart->hot_set_ix != set_ix  ||  chart->hot_i != i) {
        chart->hot_set_ix = set_ix;
        chart->hot_i = i;
        chart_update_tooltip(chart);

        if(!chart->no_redraw)
            InvalidateRect(chart->win, NULL, TRUE);
    }

    if(chart->tooltip_win != NULL  &&  chart->tooltip_active)
        tooltip_set_pos(chart, x, y);
}

static void
chart_mouse_leave(chart_t* chart)
{
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
            return chart->primary_axis.factor_exp;

        case 2:
            return chart->secondary_axis.factor_exp;

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
            chart->secondary_axis.factor_exp = exp;
            /* no break */

        case 1:
            chart->primary_axis.factor_exp = exp;
            break;

        case 2:
            chart->secondary_axis.factor_exp = exp;
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
            return chart->primary_axis.offset;

        case 2:
            return chart->secondary_axis.offset;

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
            chart->primary_axis.offset = offset;
            break;

        case 2:
            chart->secondary_axis.offset = offset;
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

static void
chart_style_changed(chart_t* chart, STYLESTRUCT* ss)
{
    if((chart->style & TVS_NOTOOLTIPS) != (ss->styleNew & TVS_NOTOOLTIPS)) {
        if(!(ss->styleNew & TVS_NOTOOLTIPS))
            tooltip_create(chart);
        else
            tooltip_destroy(chart);
    }

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

    return chart;
}

static int
chart_create(chart_t* chart)
{
    chart_setup_hot(chart);

    if(!(chart->style & MC_CHS_NOTOOLTIPS))
        tooltip_create(chart);

    return 0;
}

static void
chart_destroy(chart_t* chart)
{
    if(chart->tooltip_win != NULL  &&  !(chart->style & MC_CHS_NOTOOLTIPS))
        tooltip_destroy(chart);
}

static void
chart_ncdestroy(chart_t* chart)
{
    dsa_fini(&chart->data, chart_data_dtor);
    free(chart);
}

static LRESULT CALLBACK
chart_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    chart_t* chart = (chart_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            if(!chart->no_redraw) {
                PAINTSTRUCT ps;
                BeginPaint(win, &ps);
                if(chart->style & MC_CHS_DOUBLEBUFFER)
                    mc_doublebuffer(chart, &ps, chart_paint);
                else
                    chart_paint(chart, ps.hdc, &ps.rcPaint, ps.fErase);
                EndPaint(win, &ps);
            } else {
                ValidateRect(win, NULL);
            }
            return 0;

        case WM_PRINTCLIENT:
        {
            RECT rect;
            GetClientRect(win, &rect);
            chart_paint(chart, (HDC) wp, &rect, TRUE);
            return 0;
        }

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
            /* TODO */
            break;

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
        {
            HWND old_tooltip = chart->tooltip_win;
            chart->tooltip_win = (HWND) wp;
            return (LRESULT) old_tooltip;
        }

        case MC_CHM_GETTOOLTIPS:
            return (LRESULT) chart->tooltip_win;

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
            chart->font = (HFONT) wp;
            chart_setup_hot(chart);
            if((BOOL) lp  &&  !chart->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_SETREDRAW:
            chart->no_redraw = !wp;
            return 0;

        case WM_GETDLGCODE:
            return DLGC_STATIC;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                chart_style_changed(chart, (STYLESTRUCT*) lp);
                return 0;
            }
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
chart_init(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = chart_proc;
    wc.cbWndExtra = sizeof(chart_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = chart_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("chart_init: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
chart_fini(void)
{
    UnregisterClass(chart_wc, NULL);
}
