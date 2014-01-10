/*
 * Copyright (c) 2010-2014 Martin Mitas
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

#include "grid.h"
#include "theme.h"
#include "table.h"


/* Uncomment this to have more verbose traces about MC_GRID control. */
/*#define GRID_DEBUG     1*/

#ifdef GRID_DEBUG
    #define GRID_TRACE         MC_TRACE
#else
    #define GRID_TRACE(...)    do {} while(0)
#endif


static const TCHAR grid_wc[] = MC_WC_GRID;          /* window class name */

static const WCHAR grid_header_tc[] = L"HEADER";    /* theme classes */
static const WCHAR grid_listview_tc[] = L"LISTVIEW";


#define GRID_GGF_ALL                                                        \
        (MC_GGF_COLUMNHEADERHEIGHT | MC_GGF_ROWHEADERWIDTH |                \
         MC_GGF_DEFCOLUMNWIDTH | MC_GGF_DEFROWHEIGHT |                      \
         MC_GGF_PADDINGHORZ | MC_GGF_PADDINGVERT)


#define GRID_DEFAULT_SIZE               0xffff


#define CELL_DEF_PADDING_H              2
#define CELL_DEF_PADDING_V              1



typedef struct grid_tag grid_t;
struct grid_tag {
    HWND win;
    HWND notify_win;
    HTHEME theme_header;
    HTHEME theme_listview;
    HFONT font;
    table_t* table;  /* may be NULL (MC_GS_OWNERDATA, MC_GS_NOTABLECREATE) */
    DWORD style                 : 16;
    DWORD no_redraw             :  1;
    DWORD unicode_notifications :  1;

    /* If MC_GS_OWNERDATA, we need it here locally. If not, it is a cached
     * value of table->col_count and table->row_count. */
    WORD col_count;
    WORD row_count;

    /* Cell geometry */
    WORD padding_h;
    WORD padding_v;
    WORD header_width;
    WORD header_height;
    WORD def_col_width;
    WORD def_row_height;
    WORD* col_widths;   /* alloc'ed lazily */
    WORD* row_heights;  /* alloc'ed lazily */

    int scroll_x;
    int scroll_x_max;   /* Sum of column widths (excluding header) */
    int scroll_y;
    int scroll_y_max;   /* Sum of row heights (excluding header) */
};


static inline WORD
grid_col_width(grid_t* grid, WORD col)
{
    WORD width;

    if(grid->col_widths != NULL)
        width = grid->col_widths[col];
    else
        width = GRID_DEFAULT_SIZE;

    if(width == GRID_DEFAULT_SIZE)
        width = grid->def_col_width;

    return width;
}

static inline  WORD
grid_row_height(grid_t* grid, WORD row)
{
    WORD height;

    if(grid->row_heights != NULL)
        height = grid->row_heights[row];
    else
        height = GRID_DEFAULT_SIZE;

    if(height == GRID_DEFAULT_SIZE)
        height = grid->def_row_height;

    return height;
}

static inline WORD
grid_header_height(grid_t* grid)
{
    if((grid->style & MC_GS_COLUMNHEADERMASK) == MC_GS_COLUMNHEADERNONE)
        return 0;
    else
        return grid->header_height;
}

static inline WORD
grid_header_width(grid_t* grid)
{
    if((grid->style & MC_GS_ROWHEADERMASK) == MC_GS_ROWHEADERNONE)
        return 0;
    else
        return grid->header_width;
}

static int
grid_col_left(grid_t* grid, WORD col)
{
    int left;
    WORD i;

    if(col == MC_TABLE_HEADER)
        return 0;

    left = grid_header_width(grid);

    if(grid->col_widths == NULL) {
        if(col > 0)
            left += (col-1) * grid->def_col_width;
    } else {
        for(i = 0; i < col; i++)
            left += grid_col_width(grid, col);
    }
    return left;
}

static int
grid_row_top(grid_t* grid, WORD row)
{
    int top;
    WORD i;

    if(row == MC_TABLE_HEADER)
        return 0;

    top = grid_header_height(grid);

    if(grid->row_heights == NULL) {
        if(row > 0)
            top += (row-1) * grid->def_row_height;
    } else {
        for(i = 0; i < row; i++)
            top += grid_row_height(grid, row);
    }
    return top;
}

static void
grid_region_rect(grid_t* grid, WORD col0, WORD row0,
                 WORD col1, WORD row1, RECT* rect)
{
    WORD i;

    MC_ASSERT(col1 > col0  ||  col0 == MC_TABLE_HEADER);
    MC_ASSERT(col1 != MC_TABLE_HEADER);
    MC_ASSERT(row1 > row0  ||  row0 == MC_TABLE_HEADER);
    MC_ASSERT(row1 != MC_TABLE_HEADER);

    rect->left = grid_col_left(grid, col0);
    rect->top = grid_row_top(grid, row0);

    if(grid->col_widths == NULL) {
        rect->right = grid_header_width(grid) + col1 * grid->def_col_width;
    } else {
        rect->right = rect->left + grid_col_width(grid, col0);
        for(i = (col0 != MC_TABLE_HEADER ? col0 + 1 : 0); i < col1; i++)
            rect->right += grid_col_width(grid, i);
    }

    if(grid->row_heights == NULL) {
        rect->bottom = grid_header_height(grid) + row1 * grid->def_row_height;
    } else {
        rect->bottom = rect->top + grid_row_height(grid, row0);
        for(i = (row0 != MC_TABLE_HEADER ? row0 + 1 : 0); i < row1; i++)
            rect->bottom += grid_row_height(grid, i);
    }
}

static void
grid_cell_rect(grid_t* grid, WORD col, WORD row, RECT* rect)
{
    rect->left = grid_col_left(grid, col);
    rect->top = grid_row_top(grid, row);
    rect->right = rect->left + grid_col_width(grid, col);
    rect->bottom = rect->top + grid_row_height(grid, row);
}

static void
grid_scroll(grid_t* grid, BOOL is_vertical, WORD opcode, int factor)
{
    SCROLLINFO si;
    int old_scroll_x = grid->scroll_x;
    int old_scroll_y = grid->scroll_y;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

    if(is_vertical) {
        GetScrollInfo(grid->win, SB_VERT, &si);
        switch(opcode) {
            case SB_BOTTOM: grid->scroll_y = si.nMax; break;
            case SB_LINEUP: grid->scroll_y -= factor * MC_MIN3(grid->def_row_height, 40, si.nPage); break;
            case SB_LINEDOWN: grid->scroll_y += factor * MC_MIN3(grid->def_row_height, 40, si.nPage); break;
            case SB_PAGEUP: grid->scroll_y -= si.nPage; break;
            case SB_PAGEDOWN: grid->scroll_y += si.nPage; break;
            case SB_THUMBPOSITION: grid->scroll_y = si.nPos; break;
            case SB_THUMBTRACK: grid->scroll_y = si.nTrackPos; break;
            case SB_TOP: grid->scroll_y = 0; break;
        }

        if(grid->scroll_y > si.nMax - (int)si.nPage)
            grid->scroll_y = si.nMax - (int)si.nPage;
        if(grid->scroll_y < si.nMin)
            grid->scroll_y = si.nMin;
        if(old_scroll_y == grid->scroll_y)
            return;

        SetScrollPos(grid->win, SB_VERT, grid->scroll_y, TRUE);
    } else {
        GetScrollInfo(grid->win, SB_HORZ, &si);
        switch(opcode) {
            case SB_BOTTOM: grid->scroll_x = si.nMax; break;
            case SB_LINELEFT: grid->scroll_x -= factor * MC_MIN3(grid->def_col_width, 40, si.nPage); break;
            case SB_LINERIGHT: grid->scroll_x += factor * MC_MIN3(grid->def_col_width, 40, si.nPage); break;
            case SB_PAGELEFT: grid->scroll_x -= si.nPage; break;
            case SB_PAGERIGHT: grid->scroll_x += si.nPage; break;
            case SB_THUMBPOSITION: grid->scroll_x = si.nPos; break;
            case SB_THUMBTRACK: grid->scroll_x = si.nTrackPos; break;
            case SB_TOP: grid->scroll_x = 0; break;
        }

        if(grid->scroll_x > si.nMax - (int)si.nPage)
            grid->scroll_x = si.nMax - (int)si.nPage;
        if(grid->scroll_x < si.nMin)
            grid->scroll_x = si.nMin;
        if(old_scroll_x == grid->scroll_x)
            return;

        SetScrollPos(grid->win, SB_HORZ, grid->scroll_x, TRUE);
    }

    if(!grid->no_redraw) {
        RECT rect;

        GetClientRect(grid->win, &rect);
        if(is_vertical)
            rect.top = grid_header_height(grid);
        else
            rect.left = grid_header_width(grid);

        ScrollWindowEx(grid->win, old_scroll_x - grid->scroll_x,
                       old_scroll_y - grid->scroll_y, &rect, &rect, NULL, NULL,
                       SW_ERASE | SW_INVALIDATE);
    }
}

static void
grid_mouse_wheel(grid_t* grid, BOOL is_vertical, int wheel_delta)
{
    SCROLLINFO si;
    int line_delta;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    GetScrollInfo(grid->win, (is_vertical ? SB_VERT : SB_HORZ), &si);

    line_delta = mc_wheel_scroll(grid->win, is_vertical, wheel_delta, si.nPage);
    if(line_delta != 0)
        grid_scroll(grid, is_vertical, SB_LINEDOWN, line_delta);
}

static void
grid_setup_scrollbars(grid_t* grid, BOOL recalc_max)
{
    SCROLLINFO si;
    RECT rect;
    WORD header_w, header_h;

    GetClientRect(grid->win, &rect);
    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    /* Recalculate max scroll values */
    if(recalc_max) {
        if(grid->col_widths != NULL) {
            WORD col;

            grid->scroll_x_max = 0;
            for(col = 0; col < grid->col_count; col++)
                grid->scroll_x_max += grid_col_width(grid, col);
        } else {
            grid->scroll_x_max = grid->col_count * grid->def_col_width;
        }

        if(grid->row_heights != NULL) {
            WORD row;

            grid->scroll_y_max = 0;
            for(row = 0; row < grid->row_count; row++)
                grid->scroll_y_max += grid_row_height(grid, row);
        } else {
            grid->scroll_y_max = grid->row_count * grid->def_row_height;
        }
    }

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;

    /* Setup horizontal scrollbar */
    si.nMax = grid->scroll_x_max;
    si.nPage = mc_width(&rect) - header_w;
    grid->scroll_x = SetScrollInfo(grid->win, SB_HORZ, &si, TRUE);

    /* Fixup for Win2000 - appearance of horizontal scrollbar sometimes
     * breaks calculation of vertical scrollbar properties and last row
     * can be "hidden" behind the horizontal scrollbar */
    GetClientRect(grid->win, &rect);

    /* Setup vertical scrollbar */
    si.nMax = grid->scroll_y_max;
    si.nPage = mc_height(&rect) - header_h;
    grid->scroll_y = SetScrollInfo(grid->win, SB_VERT, &si, TRUE);
}

static TCHAR*
grid_alphabetic_number(TCHAR buffer[16], WORD num)
{
    static const int digit_count = _T('Z') - _T('A');
    TCHAR* ptr;
    WORD digit;

    num++;
    buffer[15] = _T('\0');
    ptr = &buffer[15];

    while(num > 0) {
        ptr--;

        digit = num % digit_count;
        if(digit == 0) {
            digit = digit_count;
            num -= digit_count;
        }

        *ptr = _T('A')-1 + digit;
        num /= digit_count;
    }

    return ptr;
}

static void
grid_paint_cell(grid_t* grid, HDC dc, RECT* rect, table_cell_t* cell)
{
    RECT content;
    UINT dt_flags = DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS;

    if(cell->text == NULL)
        return;

    /* Apply padding */
    content.left = rect->left + grid->padding_h;
    content.top = rect->top + grid->padding_v;
    content.right = rect->right - grid->padding_h;
    content.bottom = rect->bottom - grid->padding_v;

    /* If the cell has a value, let it paint itself. */
    if(cell->is_value) {
        /* MC_TCF_ALIGNxxx flags are designed to match VALUE_PF_ALIGNxxx
         * corresponding ones. */
        value_paint(cell->value, dc, &content, (cell->flags & VALUE_PF_ALIGNMASK));
        return;
    }

    /* Paint cell's text */
    switch(cell->flags & VALUE_PF_ALIGNMASKHORZ) {
        case VALUE_PF_ALIGNDEFAULT:   /* fall through */
        case VALUE_PF_ALIGNLEFT:      dt_flags |= DT_LEFT; break;
        case VALUE_PF_ALIGNCENTER:    dt_flags |= DT_CENTER; break;
        case VALUE_PF_ALIGNRIGHT:     dt_flags |= DT_RIGHT; break;
    }
    switch(cell->flags & VALUE_PF_ALIGNMASKVERT) {
        case VALUE_PF_ALIGNTOP:       dt_flags |= DT_TOP; break;
        case VALUE_PF_ALIGNVDEFAULT:  /* fall through */
        case VALUE_PF_ALIGNVCENTER:   dt_flags |= DT_VCENTER; break;
        case VALUE_PF_ALIGNBOTTOM:    dt_flags |= DT_BOTTOM; break;
    }
    DrawText(dc, cell->text, -1, &content, dt_flags);
}

static void
grid_paint_header_cell(grid_t* grid, HDC dc, RECT* rect, table_cell_t* cell,
                       int index, DWORD style)
{
    table_cell_t tmp;
    TCHAR buffer[16];
    DWORD fabricate;

    /* Paint header background */
    if(grid->theme_header != NULL) {
        mcDrawThemeBackground(grid->theme_header, dc,
                              HP_HEADERITEM, HIS_NORMAL, rect, NULL);
    } else {
        DrawEdge(dc, rect, BDR_RAISEDINNER, BF_MIDDLE | BF_RECT);
    }

    if(index < 0)   /* the 'dead' cell cannot have any contents */
        return;

    /* Retrieve (or fabricate) cell to be painted */
    fabricate = (style & (MC_GS_COLUMNHEADERMASK | MC_GS_ROWHEADERMASK));
    if(!fabricate) {
        /* Make copy so we can reset alignment flags below w/o side effects. */
        memcpy(&tmp, cell, sizeof(table_cell_t));
    } else {
        if(fabricate == MC_GS_COLUMNHEADERNUMBERED  ||
           fabricate == MC_GS_ROWHEADERNUMBERED)
        {
            _stprintf(buffer, _T("%d"), index + 1);
            tmp.text = buffer;
        } else {
            MC_ASSERT(fabricate == MC_GS_COLUMNHEADERALPHABETIC  ||
                      fabricate == MC_GS_ROWHEADERALPHABETIC);
            tmp.text = grid_alphabetic_number(buffer, index);
        }
        tmp.flags = cell->flags;
        tmp.is_value = FALSE;
    }

    /* If the header does not say explicitly otherwise, force centered
     * alignment for the header cells */
    if((tmp.flags & VALUE_PF_ALIGNMASKHORZ) == VALUE_PF_ALIGNDEFAULT)
        tmp.flags |= VALUE_PF_ALIGNCENTER;
    if((tmp.flags & VALUE_PF_ALIGNMASKVERT) == VALUE_PF_ALIGNVDEFAULT)
        tmp.flags |= VALUE_PF_ALIGNVCENTER;

    /* Paint header contents */
    grid_paint_cell(grid, dc, rect, &tmp);
}

static void
grid_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    grid_t* grid = (grid_t*) control;
    table_t* table = grid->table;
    RECT client;
    RECT rect;
    int header_w, header_h;
    int old_dc_state;
    int col, row;
    int col_count = grid->col_count;
    int row_count = grid->row_count;

    GRID_TRACE("grid_paint(%p, %d, %d, %d, %d)", grid,
               dirty->left, dirty->top, dirty->right, dirty->bottom);

    if(table == NULL)
        return;

    old_dc_state = SaveDC(dc);
    GetClientRect(grid->win, &client);
    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    if(grid->font != NULL)
        SelectObject(dc, grid->font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0,0,0));
    SelectObject(dc, GetStockObject(BLACK_PEN));

    /* Paint the "dead" top left header cell */
    if(header_w > 0  &&  header_h > 0  &&
       dirty->left < header_w  &&  dirty->top < header_h)
    {
        mc_rect_set(&rect, 0, 0, grid->header_width, grid->header_height);
        mc_clip_set(dc, 0, 0, MC_MIN(header_w, client.right), MC_MIN(header_h, client.bottom));
        grid_paint_header_cell(grid, dc, &rect, NULL, -1, 0);
    }

    /* Paint column headers */
    if(header_h > 0  &&  dirty->top < header_h) {
        rect.left = header_w - grid->scroll_x;
        rect.top = 0;
        rect.bottom = header_h;

        for(col = 0; col < col_count; col++) {
            rect.right = rect.left + grid_col_width(grid, col);
            mc_clip_set(dc, MC_MAX(header_w, rect.left), rect.top,
                        MC_MIN(rect.right, client.right), MC_MIN(rect.bottom, client.bottom));
            grid_paint_header_cell(grid, dc, &rect,
                              (table ? &table->cols[col] : NULL),
                              col, (grid->style & MC_GS_COLUMNHEADERMASK));
            rect.left = rect.right;
        }
    }

    /* Paint row headers */
    if(header_w > 0  &&  dirty->left <= header_w) {
        rect.left = 0;
        rect.top = header_h - grid->scroll_y;
        rect.right = header_w;

        for(row = 0; row < row_count; row++) {
            rect.bottom = rect.top + grid_row_height(grid, row);
            mc_clip_set(dc, rect.left, MC_MAX(header_h, rect.top),
                        MC_MIN(rect.right, client.right), MC_MIN(rect.bottom, client.bottom));
            grid_paint_header_cell(grid, dc, &rect,
                              (table ? &table->rows[row] : NULL),
                              row, (grid->style & MC_GS_ROWHEADERMASK));
            rect.top = rect.bottom;
        }
    }

    /* Paint grid lines */
    if(!(grid->style & MC_GS_NOGRIDLINES)) {
        HPEN pen, old_pen;
        int x, y;

        mc_clip_set(dc, header_w, header_h, client.right, client.bottom);
        pen = CreatePen(PS_SOLID, 0, mcGetThemeSysColor(grid->theme_listview, COLOR_3DFACE));
        old_pen = SelectObject(dc, pen);

        x = header_w - grid->scroll_x;
        y = header_h + grid->scroll_y_max - grid->scroll_y;
        for(col = 0; col < col_count; col++) {
            x += grid_col_width(grid, col);
            MoveToEx(dc, x-1, header_h, NULL);
            LineTo(dc, x-1, y);
        }

        x = header_w + grid->scroll_x_max - grid->scroll_x;
        y = header_h - grid->scroll_y;
        for(row = 0; row < row_count; row++) {
            y += grid_row_height(grid, row);
            MoveToEx(dc, header_w, y-1, NULL);
            LineTo(dc, x, y-1);
        }

        SelectObject(dc, old_pen);
        DeleteObject(pen);
    }

    /* Paint grid cells */
    rect.top = header_h - grid->scroll_y;
    for(row = 0; row < row_count; row++) {
        rect.bottom = rect.top + grid_row_height(grid, row);
        rect.left = header_w - grid->scroll_x;
        for(col = 0; col < col_count; col++) {
            rect.right = rect.left + grid_col_width(grid, col);
            grid_paint_cell(grid, dc, &rect, table_cell(grid->table, col, row));
            rect.left = rect.right;
        }
        rect.top = rect.bottom;
    }

    RestoreDC(dc, old_dc_state);
}

static void
grid_refresh(void* view, void* detail)
{
    grid_t* grid = (grid_t*) view;
    table_refresh_detail_t* rd = (table_refresh_detail_t*) detail;
    RECT rect;

    MC_ASSERT(rd != NULL);

    switch(rd->event) {
        case TABLE_CELL_CHANGED:
            if(!grid->no_redraw) {
                grid_cell_rect(grid, rd->param[0], rd->param[1], &rect);
                InvalidateRect(grid->win, &rect, TRUE);
            }
            break;

        case TABLE_REGION_CHANGED:
            if(!grid->no_redraw) {
                grid_region_rect(grid, rd->param[0], rd->param[1],
                                 rd->param[2], rd->param[3], &rect);
                InvalidateRect(grid->win, &rect, TRUE);
            }
            break;

        case TABLE_COLCOUNT_CHANGED:
            grid->col_count = rd->param[1];
            grid_setup_scrollbars(grid, TRUE);
            if(!grid->no_redraw) {
                /* TODO: optimize by invalidating minimal rect (new cols) and
                         scrolling rightmost columns */
                InvalidateRect(grid->win, NULL, TRUE);
            }
            break;

        case TABLE_ROWCOUNT_CHANGED:
            grid->row_count = rd->param[1];
            grid_setup_scrollbars(grid, TRUE);
            if(!grid->no_redraw) {
                /* TODO: optimize by invalidating minimal rect (new rows) and
                         scrolling bottommost rows */
                InvalidateRect(grid->win, NULL, TRUE);
            }
            break;
    }
}

static LRESULT
grid_ncpaint(grid_t* grid, HRGN orig_clip)
{
    HWND win = grid->win;
    HTHEME theme = grid->theme_listview;
    HDC dc;
    int edge_h, edge_v;
    RECT rect;
    HRGN clip;
    HRGN tmp;
    LRESULT ret;

    if(theme == NULL)
        return DefWindowProc(win, WM_NCPAINT, (WPARAM)orig_clip, 0);

    edge_h = GetSystemMetrics(SM_CXEDGE);
    edge_v = GetSystemMetrics(SM_CYEDGE);
    GetWindowRect(win, &rect);

    /* Prepare the clip region for DefWindowProc() so that it does not repaint
     * what we do here. */
    if(orig_clip == (HRGN) 1)
        clip = CreateRectRgnIndirect(&rect);
    else
        clip = orig_clip;
    tmp = CreateRectRgn(rect.left + edge_h, rect.top + edge_v,
                        rect.right - edge_h, rect.bottom - edge_v);
    CombineRgn(clip, clip, tmp, RGN_AND);
    DeleteObject(tmp);

    mc_rect_offset(&rect, -rect.left, -rect.top);
    dc = GetWindowDC(win);
    ExcludeClipRect(dc, edge_h, edge_v, rect.right - 2*edge_h, rect.bottom - 2*edge_v);
    if(mcIsThemeBackgroundPartiallyTransparent(theme, 0, 0))
        mcDrawThemeParentBackground(win, dc, &rect);
    mcDrawThemeBackground(theme, dc, 0, 0, &rect, NULL);
    ReleaseDC(win, dc);

    /* Use DefWindowProc() to paint scrollbars */
    ret = DefWindowProc(win, WM_NCPAINT, (WPARAM)clip, 0);
    if(clip != orig_clip)
        DeleteObject(clip);
    return ret;
}

static void
grid_erasebkgnd(grid_t* grid, HDC dc)
{
    HWND win = grid->win;
    HTHEME theme = grid->theme_listview;
    RECT rect;
    HBRUSH brush;

    GetClientRect(win, &rect);
    brush = mcGetThemeSysColorBrush(theme, COLOR_WINDOW);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

static int
grid_set_geometry(grid_t* grid, MC_GGEOMETRY* geom, BOOL invalidate)
{
    GRID_TRACE("grid_set_geometry(%p, %p, %d)", grid, geom, (int)invalidate);

    if(geom != NULL) {
        if(geom->fMask & MC_GGF_COLUMNHEADERHEIGHT)
            grid->header_height = geom->wColumnHeaderHeight;
        if(geom->fMask & MC_GGF_ROWHEADERWIDTH)
            grid->header_width = geom->wRowHeaderWidth;
        if(geom->fMask & MC_GGF_DEFCOLUMNWIDTH)
            grid->def_col_width = geom->wDefColumnWidth;
        if(geom->fMask & MC_GGF_DEFROWHEIGHT)
            grid->def_row_height = geom->wDefRowHeight;
        if(geom->fMask & MC_GGF_PADDINGHORZ)
            grid->padding_h = geom->wPaddingHorz;
        if(geom->fMask & MC_GGF_PADDINGVERT)
            grid->padding_v = geom->wPaddingVert;
    } else {
        SIZE font_size;

        mc_font_size(NULL, &font_size, TRUE);
        grid->padding_h = CELL_DEF_PADDING_H;
        grid->padding_v = CELL_DEF_PADDING_V;
        grid->header_width = 6 * font_size.cx + 2 * grid->padding_h;
        grid->header_height = font_size.cy + 2 * grid->padding_v;
        grid->def_col_width = 8 * font_size.cx + 2 * grid->padding_h;
        grid->def_row_height = font_size.cy + 2 * grid->padding_v;
    }

    grid_setup_scrollbars(grid, TRUE);

    if(invalidate && !grid->no_redraw)
        InvalidateRect(grid->win, NULL, TRUE);

    return 0;
}

static int
grid_get_geometry(grid_t* grid, MC_GGEOMETRY* geom)
{
    GRID_TRACE("grid_get_geometry(%p, %p)", grid, geom);

    if(MC_ERR((geom->fMask & ~GRID_GGF_ALL) != 0)) {
        MC_TRACE("grid_get_geometry: fMask has some unsupported bit(s)");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(geom->fMask & MC_GGF_COLUMNHEADERHEIGHT)
        geom->wColumnHeaderHeight = grid->header_height;
    if(geom->fMask & MC_GGF_ROWHEADERWIDTH)
        geom->wRowHeaderWidth = grid->header_width;
    if(geom->fMask & MC_GGF_DEFCOLUMNWIDTH)
        geom->wDefColumnWidth = grid->def_col_width;
    if(geom->fMask & MC_GGF_DEFROWHEIGHT)
        geom->wDefRowHeight = grid->def_row_height;
    if(geom->fMask & MC_GGF_PADDINGHORZ)
        geom->wPaddingHorz = grid->padding_h;
    if(geom->fMask & MC_GGF_PADDINGVERT)
        geom->wPaddingVert = grid->padding_v;

    return 0;
}

static int
grid_set_table(grid_t* grid, table_t* table)
{
    if(table != NULL && table == grid->table)
        return 0;

    if(table != NULL) {
        table_ref(table);
    } else if(!(grid->style & MC_GS_NOTABLECREATE)) {
        table = table_create(0, 0);
        if(MC_ERR(table == NULL)) {
            MC_TRACE("grid_set_table: table_create() failed.");
            return -1;
        }
    }

    if(table != NULL) {
        if(MC_ERR(table_install_view(table, grid, grid_refresh)) != 0) {
            MC_TRACE("grid_set_table: table_install_view() failed.");
            table_unref(table);
            return -1;
        }
    }

    if(grid->table != NULL) {
        table_uninstall_view(grid->table, grid);
        table_unref(grid->table);
    }

    grid->table = table;

    if(table != NULL) {
        grid->col_count = table->col_count;
        grid->row_count = table->row_count;
    } else {
        grid->col_count = 0;
        grid->row_count = 0;
    }

    if(!grid->no_redraw) {
        InvalidateRect(grid->win, NULL, TRUE);
        grid_setup_scrollbars(grid, TRUE);
    }
    return 0;
}

static int
grid_resize_table(grid_t* grid, WORD col_count, WORD row_count)
{
    GRID_TRACE("grid_resize_table(%d, %d)", (int) col_count, (int) row_count);

    if(grid->table != 0) {
        if(MC_ERR(table_resize(grid->table, col_count, row_count) != 0)) {
            MC_TRACE("grid_resize_table: table_resize() failed.");
            return -1;
        }
    } else {
        grid->col_count = col_count;
        grid->row_count = row_count;

        if(!grid->no_redraw) {
            InvalidateRect(grid->win, NULL, TRUE);
            grid_setup_scrollbars(grid, TRUE);
        }
    }

    return 0;
}

static int
grid_clear(grid_t* grid, DWORD what)
{
    if(MC_ERR(grid->table == NULL)) {
        SetLastError(ERROR_NOT_SUPPORTED);
        MC_TRACE("grid_clear: No table installed.");
        return -1;
    }

    mcTable_Clear(grid->table, what);
    return 0;
}

static int
grid_set_cell(grid_t* grid, WORD col, WORD row, MC_TABLECELL* cell, BOOL unicode)
{
    if(MC_ERR(grid->table == NULL)) {
        SetLastError(ERROR_INVALID_HANDLE);
        MC_TRACE("grid_set_cell: No table installed.");
        return -1;
    }

    if(MC_ERR(table_set_cell_data(grid->table, col, row, cell, unicode) != 0)) {
        MC_TRACE("grid_set_cell: table_set_cell_data() failed.");
        return -1;
    }

    return 0;
}

static int
grid_get_cell(grid_t* grid, WORD col, WORD row, MC_TABLECELL* cell, BOOL unicode)
{
    if(MC_ERR(grid->table == NULL)) {
        SetLastError(ERROR_INVALID_HANDLE);
        MC_TRACE("grid_get_cell: No table installed.");
        return -1;
    }

    if(MC_ERR(table_get_cell_data(grid->table, col, row, cell, unicode) != 0)) {
        MC_TRACE("grid_get_cell: table_get_cell_data() failed.");
        return -1;
    }

    return 0;
}

static void
grid_notify_format(grid_t* grid)
{
    LRESULT lres;
    lres = MC_SEND(grid->notify_win, WM_NOTIFYFORMAT, grid->win, NF_QUERY);
    grid->unicode_notifications = (lres == NFR_UNICODE ? 1 : 0);
    GRID_TRACE("grid_notify_format: Will use %s notifications.",
               grid->unicode_notifications ? "Unicode" : "ANSI");
}

static void
grid_open_theme(grid_t* grid)
{
    /* Let only the list-view theme class associate with the window handle.
     * It is more significant then the header, so let GetWindowTheme()
     * return that one to the app. */
    grid->theme_header = mcOpenThemeData(NULL, grid_header_tc);
    grid->theme_listview = mcOpenThemeData(grid->win, grid_listview_tc);
}

static void
grid_close_theme(grid_t* grid)
{
    if(grid->theme_header) {
        mcCloseThemeData(grid->theme_header);
        grid->theme_header = NULL;
    }
    if(grid->theme_listview) {
        mcCloseThemeData(grid->theme_listview);
        grid->theme_listview = NULL;
    }
}

static grid_t*
grid_nccreate(HWND win, CREATESTRUCT* cs)
{
    grid_t* grid;

    grid = (grid_t*) malloc(sizeof(grid_t));
    if(MC_ERR(grid == NULL)) {
        MC_TRACE("grid_create: malloc() failed.");
        return NULL;
    }

    memset(grid, 0, sizeof(grid_t));
    grid->win = win;
    grid->notify_win = cs->hwndParent;
    grid->style = cs->style;

    grid_set_geometry(grid, NULL, FALSE);
    grid_notify_format(grid);

    mcBufferedPaintInit();
    return grid;
}

static int
grid_create(grid_t* grid)
{
    grid_open_theme(grid);

    if(MC_ERR(grid_set_table(grid, NULL) != 0)) {
        MC_TRACE("grid_create: grid_set_table() failed.");
        return -1;
    }

    return 0;
}

static void
grid_destroy(grid_t* grid)
{
    grid_close_theme(grid);
}

static inline void
grid_ncdestroy(grid_t* grid)
{
    mcBufferedPaintUnInit();
    free(grid);
}

static LRESULT CALLBACK
grid_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    grid_t* grid = (grid_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(win, &ps);
            if(!grid->no_redraw) {
                if(grid->style & MC_GS_DOUBLEBUFFER)
                    mc_doublebuffer(grid, &ps, grid_paint);
                else
                    grid_paint(grid, ps.hdc, &ps.rcPaint, ps.fErase);
            }
            EndPaint(win, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            RECT rect;
            GetClientRect(win, &rect);
            grid_paint(grid, (HDC) wp, &rect, TRUE);
            return 0;
        }

        case WM_NCPAINT:
            return grid_ncpaint(grid, (HRGN) wp);

        case WM_ERASEBKGND:
            grid_erasebkgnd(grid, (HDC) wp);
            return TRUE;

        case MC_GM_GETTABLE:
            return (LRESULT) grid->table;

        case MC_GM_SETTABLE:
            return (grid_set_table(grid, (table_t*) lp) == 0 ? TRUE : FALSE);

        case MC_GM_GETCOLUMNCOUNT:
            return grid->col_count;

        case MC_GM_GETROWCOUNT:
            return grid->row_count;

        case MC_GM_RESIZE:
            return (grid_resize_table(grid, LOWORD(wp), HIWORD(wp)) == 0 ? TRUE : FALSE);

        case MC_GM_CLEAR:
            return (grid_clear(grid, wp) == 0 ? TRUE : FALSE);

        case MC_GM_SETCELLW:
        case MC_GM_SETCELLA:
            return (grid_set_cell(grid, LOWORD(wp), HIWORD(wp), (MC_TABLECELL*)lp,
                                  (msg == MC_GM_SETCELLW)) == 0 ? TRUE : FALSE);

        case MC_GM_GETCELLW:
        case MC_GM_GETCELLA:
            return (grid_get_cell(grid, LOWORD(wp), HIWORD(wp), (MC_TABLECELL*)lp,
                                  (msg == MC_GM_GETCELLW)) == 0 ? TRUE : FALSE);

        case MC_GM_SETGEOMETRY:
            return (grid_set_geometry(grid, (MC_GGEOMETRY*)lp, TRUE) == 0 ? TRUE : FALSE);

        case MC_GM_GETGEOMETRY:
            return (grid_get_geometry(grid, (MC_GGEOMETRY*)lp) == 0 ? TRUE : FALSE);

        case WM_SETREDRAW:
            grid->no_redraw = !wp;
            return 0;

        case WM_VSCROLL:
        case WM_HSCROLL:
            grid_scroll(grid, (msg == WM_VSCROLL), LOWORD(wp), 1);
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            grid_mouse_wheel(grid, (msg == WM_MOUSEWHEEL), (int)(SHORT)HIWORD(wp));
            return 0;

        case WM_SIZE:
            if(!grid->no_redraw) {
                int old_scroll_x = grid->scroll_x;
                int old_scroll_y = grid->scroll_y;

                grid_setup_scrollbars(grid, FALSE);

                if(grid->scroll_x != old_scroll_x || grid->scroll_y != old_scroll_y)
                    InvalidateRect(win, NULL, TRUE);
            }
            return 0;

        case WM_GETFONT:
            return (LRESULT) grid->font;

        case WM_SETFONT:
            grid->font = (HFONT) wp;
            if((BOOL) lp  &&  !grid->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                grid->style = ss->styleNew;
                if(!grid->no_redraw)
                    RedrawWindow(win, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
            }
            break;

        case WM_THEMECHANGED:
            grid_close_theme(grid);
            grid_open_theme(grid);
            if(!grid->no_redraw)
                RedrawWindow(win, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
            return 0;

        case WM_SYSCOLORCHANGE:
            if(!grid->no_redraw)
                RedrawWindow(win, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
            return 0;

        case WM_NOTIFYFORMAT:
            if(lp == NF_REQUERY)
                grid_notify_format(grid);
            return (grid->unicode_notifications ? NFR_UNICODE : NFR_ANSI);

        case CCM_SETUNICODEFORMAT:
        {
            BOOL old = grid->unicode_notifications;
            grid->unicode_notifications = (wp != 0);
            return old;
        }

        case CCM_GETUNICODEFORMAT:
            return grid->unicode_notifications;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = grid->notify_win;
            grid->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case CCM_SETWINDOWTHEME:
            mcSetWindowTheme(win, (const WCHAR*) lp, NULL);
            return 0;

        case WM_NCCREATE:
            grid = grid_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(grid == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)grid);
            return TRUE;

        case WM_CREATE:
            return (grid_create(grid) == 0 ? 0 : -1);

        case WM_DESTROY:
            grid_destroy(grid);
            return 0;

        case WM_NCDESTROY:
            if(grid)
                grid_ncdestroy(grid);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}


int
grid_init_module(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_GLOBALCLASS | CS_PARENTDC;
    wc.lpfnWndProc = grid_proc;
    wc.cbWndExtra = sizeof(grid_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = grid_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("grid_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
grid_fini_module(void)
{
    UnregisterClass(grid_wc, NULL);
}
