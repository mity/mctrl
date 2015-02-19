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
#include "generic.h"
#include "table.h"
#include "theme.h"


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

    WORD cache_hint[4];

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
grid_alloc_col_widths(grid_t* grid, WORD old_col_count, WORD new_col_count,
                      BOOL cannot_fail)
{
    WORD* col_widths;

    col_widths = realloc(grid->col_widths, new_col_count * sizeof(WORD));
    if(MC_ERR(col_widths == NULL)) {
        MC_TRACE("grid_alloc_col_widths: realloc() failed.");
        mc_send_notify(grid->notify_win, grid->win, NM_OUTOFMEMORY);

        if(cannot_fail  &&  grid->col_widths != NULL) {
            /* We need to be sync'ed with the underlying table, and if we
             * cannot have enough slots, we just fallback to the default
             * widths. */
            free(grid->col_widths);
            grid->col_widths = NULL;
        }

        return -1;
    }

    /* Set new columns to the default widths. */
    if(new_col_count > old_col_count) {
        memset(&col_widths[old_col_count], 0xff,
               (new_col_count - old_col_count) * sizeof(WORD));
    }

    grid->col_widths = col_widths;
    return 0;
}

static int
grid_alloc_row_heights(grid_t* grid, WORD old_row_count, WORD new_row_count,
                       BOOL cannot_fail)
{
    WORD* row_heights;

    row_heights = realloc(grid->row_heights, new_row_count * sizeof(WORD));
    if(MC_ERR(row_heights == NULL)) {
        MC_TRACE("grid_alloc_row_heights: realloc() failed.");
        mc_send_notify(grid->notify_win, grid->win, NM_OUTOFMEMORY);

        if(cannot_fail  &&  grid->row_heights != NULL) {
            /* We need to be sync'ed with the underlying table, and if we
             * cannot have enough slots, we just fallback to the default
             * heights. */
            free(grid->row_heights);
            grid->row_heights = NULL;
        }

        return -1;
    }

    /* Set new rows to the default heights. */
    if(new_row_count > old_row_count) {
        memset(&row_heights[old_row_count], 0xff,
               (new_row_count - old_row_count) * sizeof(WORD));
    }

    grid->row_heights = row_heights;
    return 0;
}

static int
grid_col_x2(grid_t* grid, WORD col0, int x0, WORD col)
{
    WORD i;
    int x = x0;

    if(grid->col_widths == NULL)
        return x0 + (col-col0) * grid->def_col_width;

    for(i = col0; i < col; i++)
        x += grid_col_width(grid, i);
    return x;
}

static inline int
grid_col_x(grid_t* grid, WORD col)
{
    return grid_col_x2(grid, 0, grid_header_width(grid) - grid->scroll_x, col);
}

static int
grid_row_y2(grid_t* grid, WORD row0, int y0, WORD row)
{
    WORD i;
    int y = y0;

    if(grid->row_heights == NULL)
        return y0 + (row-row0) * grid->def_row_height;

    for(i = row0; i < row; i++)
        y += grid_row_height(grid, i);
    return y;
}

static inline int
grid_row_y(grid_t* grid, WORD row)
{
    return grid_row_y2(grid, 0, grid_header_height(grid) - grid->scroll_y, row);
}

static void
grid_region_rect(grid_t* grid, WORD col0, WORD row0,
                 WORD col1, WORD row1, RECT* rect)
{
    int header_w, header_h;

    /* Note: Caller may never mix header and ordinary cells in one call,
     * because the latter is scrolled area, while the headers are not. Hence
     * it does not make any sense to mix theme together. */
    MC_ASSERT(col1 > col0  ||  col0 == MC_TABLE_HEADER);
    MC_ASSERT(row1 > row0  ||  row0 == MC_TABLE_HEADER);

    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    if(col0 == MC_TABLE_HEADER) {
        rect->left = 0;
        rect->right = header_w;
    } else {
        rect->left = grid_col_x(grid, col0);
        rect->right = grid_col_x2(grid, col0, rect->left, col1);
    }

    if(row0 == MC_TABLE_HEADER) {
        rect->top = 0;
        rect->bottom = header_h;
    } else {
        rect->top = grid_row_y(grid, row0);
        rect->bottom = grid_row_y2(grid, row0, rect->top, row1);
    }
}

static inline void
grid_cell_rect(grid_t* grid, WORD col, WORD row, RECT* rect)
{
    grid_region_rect(grid, col, row, col+1, row+1, rect);
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
        grid->scroll_x_max = grid_col_x2(grid, 0, 0, grid->col_count);
        grid->scroll_y_max = grid_row_y2(grid, 0, 0, grid->row_count);
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

typedef struct grid_dispinfo_tag grid_dispinfo_t;
struct grid_dispinfo_tag {
    MC_HVALUE value;
    TCHAR* text;
    DWORD flags;
};

static void
grid_get_dispinfo(grid_t* grid, WORD col, WORD row, table_cell_t* cell,
                  grid_dispinfo_t* di, DWORD mask)
{
    MC_NMGDISPINFO info;

    MC_ASSERT((mask & ~(MC_TCMF_TEXT | MC_TCMF_VALUE | MC_TCMF_FLAGS)) == 0);

    /* Use what can be taken from the cell. */
    if(cell != NULL) {
        if(cell->text != MC_LPSTR_TEXTCALLBACK) {
            di->text = (cell->is_value ? NULL : cell->text);
            mask &= ~MC_TCMF_TEXT;
        }

        di->value = (cell->is_value ? cell->value : NULL);
        di->flags = cell->flags;
        mask &= ~(MC_TCMF_VALUE | MC_TCMF_FLAGS);

        if(mask == 0)
            return;
    }

    /* For the rest data, fire MC_GN_GETDISPINFO notification. */
    info.hdr.hwndFrom = grid->win;
    info.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    info.hdr.code = (grid->unicode_notifications ? MC_GN_GETDISPINFOW : MC_GN_GETDISPINFOA);
    info.wColumn = col;
    info.wRow = row;
    info.cell.fMask = mask;
    /* Set info.cell members to meaningful values. lParam may be needed by the
     * app to find the requested data. Other members should be set to some
     * defaults to deal with broken apps which do not set the asked members. */
    if(cell != NULL) {
        info.cell.pszText = NULL;
        info.cell.hValue = NULL;
        info.cell.lParam = cell->lp;
        info.cell.dwFlags = cell->flags;
    } else {
        info.cell.pszText = NULL;
        info.cell.hValue = NULL;
        info.cell.lParam = 0;
        info.cell.dwFlags = 0;
    }
    MC_SEND(grid->notify_win, WM_NOTIFY, 0, &info);

    /* If needed, convert the text from parent to the expected format. */
    if(mask & MC_TCMF_TEXT) {
        if(grid->unicode_notifications == MC_IS_UNICODE)
            di->text = info.cell.pszText;
        else
            di->text = mc_str(info.cell.pszText, (grid->unicode_notifications ? MC_STRW : MC_STRA), MC_STRT);
    } else {
        /* Needed even when not asked for because of grid_free_dispinfo() */
        di->text = NULL;
    }

    /* Small optimization: We do not ask about the corresponding bits in the
     * mask for these. If not set, the assignment does no hurt and we save few
     * instructions. */
    di->value = info.cell.hValue;
    di->flags = info.cell.dwFlags;
}

static inline void
grid_free_dispinfo(grid_t* grid, table_cell_t* cell, grid_dispinfo_t* di)
{
    if(grid->unicode_notifications != MC_IS_UNICODE  &&
       (cell == NULL || di->text != cell->text)  &&  di->text != NULL)
        free(di->text);
}

static void
grid_paint_cell(grid_t* grid, WORD col, WORD row, table_cell_t* cell,
                HDC dc, RECT* rect)
{
    RECT content;
    UINT dt_flags = DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS;
    grid_dispinfo_t di;

    grid_get_dispinfo(grid, col, row, cell, &di,
                      MC_TCMF_TEXT | MC_TCMF_VALUE | MC_TCMF_FLAGS);

    /* Apply padding */
    content.left = rect->left + grid->padding_h;
    content.top = rect->top + grid->padding_v;
    content.right = rect->right - grid->padding_h;
    content.bottom = rect->bottom - grid->padding_v;

    /* Paint cell value or text. */
    if(di.value != NULL) {
        /* MC_TCF_ALIGNxxx flags are designed to match VALUE_PF_ALIGNxxx
         * corresponding ones. */
        value_paint(di.value, dc, &content, (di.flags & VALUE_PF_ALIGNMASK));
        return;
    } else if(di.text != NULL) {
        switch(di.flags & VALUE_PF_ALIGNMASKHORZ) {
            case VALUE_PF_ALIGNDEFAULT:   /* fall through */
            case VALUE_PF_ALIGNLEFT:      dt_flags |= DT_LEFT; break;
            case VALUE_PF_ALIGNCENTER:    dt_flags |= DT_CENTER; break;
            case VALUE_PF_ALIGNRIGHT:     dt_flags |= DT_RIGHT; break;
        }
        switch(di.flags & VALUE_PF_ALIGNMASKVERT) {
            case VALUE_PF_ALIGNTOP:       dt_flags |= DT_TOP; break;
            case VALUE_PF_ALIGNVDEFAULT:  /* fall through */
            case VALUE_PF_ALIGNVCENTER:   dt_flags |= DT_VCENTER; break;
            case VALUE_PF_ALIGNBOTTOM:    dt_flags |= DT_BOTTOM; break;
        }
        DrawText(dc, di.text, -1, &content, dt_flags);
    }

    grid_free_dispinfo(grid, cell, &di);
}

static void
grid_paint_header_cell(grid_t* grid, WORD col, WORD row, table_cell_t* cell,
                       HDC dc, RECT* rect, int index, DWORD style)
{
    table_cell_t tmp;
    table_cell_t* tmp_cell = &tmp;
    TCHAR buffer[16];
    DWORD fabricate;

    /* Paint header background. */
    if(grid->theme_header != NULL) {
        mcDrawThemeBackground(grid->theme_header, dc,
                              HP_HEADERITEM, HIS_NORMAL, rect, NULL);
    } else {
        DrawEdge(dc, rect, BDR_RAISEDINNER, BF_MIDDLE | BF_RECT);
    }

    /* The 'dead' cell cannot have any contents. */
    if(index < 0)
        return;

    /* Retrieve (or fabricate) cell to be painted. */
    fabricate = (style & (MC_GS_COLUMNHEADERMASK | MC_GS_ROWHEADERMASK));
    if(!fabricate) {
        /* Make copy so we can reset alignment flags below w/o side effects. */
        if(cell != NULL)
            memcpy(&tmp, cell, sizeof(table_cell_t));
        else
            tmp_cell = NULL;
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
        tmp.flags = (cell != NULL ? cell->flags : 0);
        tmp.is_value = FALSE;
    }

    /* If the header does not say explicitly otherwise, force centered
     * alignment for the header cells. */
    if((tmp.flags & VALUE_PF_ALIGNMASKHORZ) == VALUE_PF_ALIGNDEFAULT)
        tmp.flags |= VALUE_PF_ALIGNCENTER;
    if((tmp.flags & VALUE_PF_ALIGNMASKVERT) == VALUE_PF_ALIGNVDEFAULT)
        tmp.flags |= VALUE_PF_ALIGNVCENTER;

    /* Paint header contents. */
    grid_paint_cell(grid, col, row, tmp_cell, dc, rect);
}

static void
grid_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    grid_t* grid = (grid_t*) control;
    RECT client;
    RECT rect;
    int header_w, header_h;
    int col0, row0;
    int x0, y0;
    int col, row;
    int col_count = grid->col_count;
    int row_count = grid->row_count;
    table_t* table = grid->table;
    table_cell_t* cell;
    int old_mode;
    COLORREF old_text_color;
    HPEN old_pen;
    HFONT old_font;

    GRID_TRACE("grid_paint(%p, %d, %d, %d, %d)", grid,
               dirty->left, dirty->top, dirty->right, dirty->bottom);

    if(table == NULL  &&  !(grid->style & MC_GS_OWNERDATA))
        return;

    GetClientRect(grid->win, &client);
    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    old_mode = SetBkMode(dc, TRANSPARENT);
    old_text_color = SetTextColor(dc, RGB(0,0,0));
    old_pen = SelectObject(dc, GetStockObject(BLACK_PEN));
    old_font = (grid->font != NULL ? SelectObject(dc, grid->font) : NULL);

    /* Find 1st visible column */
    rect.left = header_w - grid->scroll_x;
    col0 = col_count;
    x0 = 0;
    for(col = 0; col < col_count; col++) {
        rect.right = rect.left + grid_col_width(grid, col);
        if(rect.right > header_w) {
            col0 = col;
            x0 = rect.left;
            break;
        }
        rect.left = rect.right;
    }

    /* Find 1st visible row */
    row0 = row_count;
    y0 = 0;
    rect.top = header_h - grid->scroll_y;
    for(row = 0; row < row_count; row++) {
        rect.bottom = rect.top + grid_row_height(grid, row);
        if(rect.bottom > header_h) {
            row0 = row;
            y0 = rect.top;
            break;
        }
        rect.top = rect.bottom;
    }

    /* If needed, send MC_GN_ODCACHEHINT */
    if(grid->style & MC_GS_OWNERDATA) {
        WORD col1, row1;

        rect.right = x0;
        for(col1 = col0; col1+1 < grid->col_count; col1++) {
            rect.right += grid_col_width(grid, col1);
            if(rect.right >= client.right)
                break;
        }

        rect.bottom = y0;
        for(row1 = row0; row1+1 < grid->row_count; row1++) {
            rect.bottom += grid_row_height(grid, row1);
            if(rect.bottom >= client.bottom)
                break;
        }

        if(col0 != grid->cache_hint[0] || row0 != grid->cache_hint[1] ||
           col1 != grid->cache_hint[2] || row1 != grid->cache_hint[3]) {
            MC_NMGCACHEHINT hint;

            hint.hdr.hwndFrom = grid->win;
            hint.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
            hint.hdr.code = MC_GN_ODCACHEHINT;
            hint.wColumnFrom = col0;
            hint.wRowFrom = row0;
            hint.wColumnTo = col1;
            hint.wRowTo = row1;
            GRID_TRACE("grid_paint: Sending MC_GN_ODCACHEHINT (%hu, %hu, %hu, %hu)",
                       col0, row0, col1, row1);
            MC_SEND(grid->notify_win, WM_NOTIFY, hint.hdr.idFrom, &hint);

            grid->cache_hint[0] = col0;
            grid->cache_hint[1] = row0;
            grid->cache_hint[2] = col1;
            grid->cache_hint[3] = row1;
        }
    }

    /* Paint the "dead" top left header cell */
    if(header_w > 0  &&  header_h > 0  &&
       dirty->left < header_w  &&  dirty->top < header_h)
    {
        mc_rect_set(&rect, 0, 0, grid->header_width, grid->header_height);
        mc_clip_set(dc, 0, 0, MC_MIN(header_w, client.right), MC_MIN(header_h, client.bottom));
        grid_paint_header_cell(grid, MC_TABLE_HEADER, MC_TABLE_HEADER, NULL, dc, &rect, -1, 0);
    }

    /* Paint column headers */
    if(header_h > 0  &&  dirty->top < header_h) {
        rect.left = x0;
        rect.top = 0;
        rect.bottom = header_h;

        for(col = col0; col < col_count; col++) {
            rect.right = rect.left + grid_col_width(grid, col);
            mc_clip_set(dc, MC_MAX(header_w, rect.left), rect.top,
                        MC_MIN(rect.right, client.right), MC_MIN(rect.bottom, client.bottom));
            grid_paint_header_cell(grid, col, MC_TABLE_HEADER, (table ? &table->cols[col] : NULL),
                                   dc, &rect, col, (grid->style & MC_GS_COLUMNHEADERMASK));
            rect.left = rect.right;
            if(rect.right >= client.right)
                break;
        }
    }

    /* Paint row headers */
    if(header_w > 0  &&  dirty->left <= header_w) {
        rect.left = 0;
        rect.top = y0;
        rect.right = header_w;

        for(row = row0; row < row_count; row++) {
            rect.bottom = rect.top + grid_row_height(grid, row);
            mc_clip_set(dc, rect.left, MC_MAX(header_h, rect.top),
                        MC_MIN(rect.right, client.right), MC_MIN(rect.bottom, client.bottom));
            grid_paint_header_cell(grid, MC_TABLE_HEADER, row, (table ? &table->rows[row] : NULL),
                                   dc, &rect, row, (grid->style & MC_GS_ROWHEADERMASK));
            rect.top = rect.bottom;
            if(rect.bottom >= client.bottom)
                break;
        }
    }

    /* Paint grid lines */
    if(!(grid->style & MC_GS_NOGRIDLINES)) {
        HPEN pen, old_pen;
        int x, y;

        mc_clip_set(dc, header_w, header_h, client.right, client.bottom);
        pen = CreatePen(PS_SOLID, 0, mcGetThemeSysColor(grid->theme_listview, COLOR_3DFACE));
        old_pen = SelectObject(dc, pen);

        x = x0;
        y = header_h + grid->scroll_y_max - grid->scroll_y;
        for(col = col0; col < col_count; col++) {
            x += grid_col_width(grid, col);
            MoveToEx(dc, x-1, header_h, NULL);
            LineTo(dc, x-1, y);
            if(x >= client.right)
                break;
        }

        x = header_w + grid->scroll_x_max - grid->scroll_x;
        y = y0;
        for(row = 0; row < row_count; row++) {
            y += grid_row_height(grid, row);
            MoveToEx(dc, header_w, y-1, NULL);
            LineTo(dc, x, y-1);
            if(y >= client.bottom)
                break;
        }

        SelectObject(dc, old_pen);
        DeleteObject(pen);
    }

    /* Paint grid cells */
    rect.top = y0;
    for(row = row0; row < row_count; row++) {
        rect.bottom = rect.top + grid_row_height(grid, row);
        rect.left = x0;
        for(col = col0; col < col_count; col++) {
            if(table != NULL)
                cell = table_cell(table, col, row);
            else
                cell = NULL;
            rect.right = rect.left + grid_col_width(grid, col);
            grid_paint_cell(grid, col, row, cell, dc, &rect);
            if(rect.right >= client.right)
                break;
            rect.left = rect.right;
        }
        if(rect.bottom >= client.bottom)
            break;
        rect.top = rect.bottom;
    }

    SetBkMode(dc, old_mode);
    SetTextColor(dc, old_text_color);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_font);
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
            if(grid->col_widths != NULL)
                grid_alloc_col_widths(grid, grid->col_count, rd->param[1], TRUE);
            grid->col_count = rd->param[1];
            grid_setup_scrollbars(grid, TRUE);
            if(!grid->no_redraw) {
                /* TODO: optimize by invalidating minimal rect (new cols) and
                         scrolling rightmost columns */
                InvalidateRect(grid->win, NULL, TRUE);
            }
            break;

        case TABLE_ROWCOUNT_CHANGED:
            if(grid->row_heights != NULL)
                grid_alloc_row_heights(grid, grid->row_count, rd->param[1], TRUE);
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
grid_redraw_cells(grid_t* grid, WORD col0, WORD row0, WORD col1, WORD row1)
{
    int header_w;
    int header_h;
    RECT rect;

    /* Note we usually use intervals [x0,x1] where x0 is included and x1
     * excluded. However here the region includes [col1, row1] to make the
     * public API consistent with LVM_REDRAWITEMS. */

    if((col0 != MC_TABLE_HEADER  &&  col0 > col1)  ||
       (row0 != MC_TABLE_HEADER  &&  row0 > row1)) {
        MC_TRACE("grid_redraw_cells: col0 > col1  ||  row0 > row1");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(grid->no_redraw)
        return 0;

    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    /* Invalidate row headers */
    if(col0 == MC_TABLE_HEADER) {
        rect.left = 0;
        rect.right = header_w;
        if(row0 != MC_TABLE_HEADER) {
            rect.top = grid_row_y(grid, row0);
            rect.bottom = grid_row_y2(grid, row0, rect.top, row1+1);
        } else {
            rect.top = header_h;
            if(row1 != MC_TABLE_HEADER)
                rect.bottom = grid_row_y(grid, row0);
            else
                rect.bottom = rect.top;
        }

        InvalidateRect(grid->win, &rect, TRUE);
    }

    /* Invalidate column headers */
    if(row0 == MC_TABLE_HEADER) {
        rect.top = 0;
        rect.bottom = header_h;
        if(col0 != MC_TABLE_HEADER) {
            rect.left = grid_col_x(grid, col0);
            rect.right = grid_col_x2(grid, col0, rect.left, col1+1);
        } else {
            rect.left = header_w;
            if(col1 != MC_TABLE_HEADER)
                rect.right = grid_col_x(grid, col0);
            else
                rect.right = rect.left;
        }

        InvalidateRect(grid->win, &rect, TRUE);
    }

    /* Invalidate ordinary cells */
    if(col1 == MC_TABLE_HEADER || row1 == MC_TABLE_HEADER) {
        /* Caller specified only some header cells. */
        return 0;
    }
    if(col0 == MC_TABLE_HEADER)
        col0 = 0;
    if(row0 == MC_TABLE_HEADER)
        row0 = 0;
    rect.left = grid_col_x(grid, col0);
    rect.top = grid_row_y(grid, row0);
    rect.right = grid_col_x2(grid, col0, rect.left, col1+1);
    rect.bottom = grid_row_y2(grid, row0, rect.top, row1+1);
    InvalidateRect(grid->win, &rect, TRUE);

    return 0;
}

static int
grid_set_col_width(grid_t* grid, WORD col, WORD width)
{
    if(MC_ERR(col >= grid->col_count)) {
        MC_TRACE("grid_set_col_width: column %hu our of range.", col);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(grid->col_widths == NULL) {
        if(width == GRID_DEFAULT_SIZE)
            return 0;

        if(MC_ERR(grid_alloc_col_widths(grid, 0, grid->col_count, FALSE) != 0)) {
            MC_TRACE("grid_set_col_width: grid_alloc_col_widths() failed.");
            return -1;
        }
    }

    grid->col_widths[col] = width;
    return 0;
}

static LONG
grid_get_col_width(grid_t* grid, WORD col)
{
    if(MC_ERR(col >= grid->col_count)) {
        MC_TRACE("grid_get_col_width: column %hu our of range.", col);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    return MAKELPARAM(grid_col_width(grid, col), 0);
}

static int
grid_set_row_height(grid_t* grid, WORD row, WORD height)
{
    if(MC_ERR(row >= grid->row_count)) {
        MC_TRACE("grid_set_row_height: row %hu our of range.", row);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(grid->row_heights == NULL) {
        if(height == GRID_DEFAULT_SIZE)
            return 0;

        if(MC_ERR(grid_alloc_row_heights(grid, 0, grid->row_count, FALSE) != 0)) {
            MC_TRACE("grid_set_row_height: grid_alloc_row_heights() failed.");
            return -1;
        }
    }

    grid->row_heights[row] = height;
    return 0;
}

static LONG
grid_get_row_height(grid_t* grid, WORD row)
{
    if(MC_ERR(row >= grid->row_count)) {
        MC_TRACE("grid_get_row_height: row %hu our of range.", row);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    return MAKELPARAM(grid_row_height(grid, row), 0);
}

static int
grid_set_table(grid_t* grid, table_t* table)
{
    if(table != NULL && table == grid->table)
        return 0;

    if(MC_ERR(table != NULL  &&  (grid->style & MC_GS_OWNERDATA))) {
        MC_TRACE("grid_set_table: Cannot install table while having style "
                 "MC_GS_OWNERDATA");
        SetLastError(ERROR_INVALID_STATE);
        return -1;
    }

    if(table != NULL) {
        table_ref(table);
    } else if(!(grid->style & (MC_GS_NOTABLECREATE | MC_GS_OWNERDATA))) {
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

    grid->cache_hint[0] = 0;
    grid->cache_hint[1] = 0;
    grid->cache_hint[2] = 0;
    grid->cache_hint[3] = 0;

    if(grid->col_widths != NULL) {
        free(grid->col_widths);
        grid->col_widths = NULL;
    }

    if(grid->row_heights != NULL) {
        free(grid->row_heights);
        grid->row_heights = NULL;
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
        if(grid->col_widths != NULL)
            grid_alloc_col_widths(grid, grid->col_count, col_count, TRUE);
        if(grid->row_heights)
            grid_alloc_row_heights(grid, grid->row_count, row_count, TRUE);

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

static void
grid_style_changed(grid_t* grid, STYLESTRUCT* ss)
{
    grid->style = ss->styleNew;

    if((ss->styleNew & MC_GS_OWNERDATA) != (ss->styleOld & MC_GS_OWNERDATA))
        grid_set_table(grid, NULL);

    if(!grid->no_redraw)
        InvalidateRect(grid->win, NULL, TRUE);
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

    doublebuffer_init();
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
    if(grid->table != NULL) {
        table_uninstall_view(grid->table, grid);
        table_unref(grid->table);
        grid->table = NULL;
    }

    grid_close_theme(grid);
}

static inline void
grid_ncdestroy(grid_t* grid)
{
    doublebuffer_fini();

    if(grid->col_widths)
        free(grid->col_widths);
    if(grid->row_heights)
        free(grid->row_heights);
    free(grid);
}

static LRESULT CALLBACK
grid_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    grid_t* grid = (grid_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            return generic_paint(win, grid->no_redraw,
                                 (grid->style & MC_GS_DOUBLEBUFFER),
                                 grid_paint, grid);

        case WM_PRINTCLIENT:
            return generic_printclient(win, (HDC) wp, grid_paint, grid);

        case WM_NCPAINT:
            return generic_ncpaint(win, grid->theme_listview, (HRGN) wp);

        case WM_ERASEBKGND:
            return generic_erasebkgnd(win, grid->theme_listview, (HDC) wp);

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

        case MC_GM_REDRAWCELLS:
            return (grid_redraw_cells(grid, LOWORD(wp), HIWORD(wp),
                                      LOWORD(lp), LOWORD(lp)) == 0 ? TRUE : FALSE);

        case MC_GM_SETCOLUMNWIDTH:
            return (grid_set_col_width(grid, wp, LOWORD(lp)) == 0 ? TRUE : FALSE);

        case MC_GM_GETCOLUMNWIDTH:
            return grid_get_col_width(grid, wp);

        case MC_GM_SETROWHEIGHT:
            return (grid_set_row_height(grid, wp, LOWORD(lp)) == 0 ? TRUE : FALSE);

        case MC_GM_GETROWHEIGHT:
            return grid_get_row_height(grid, wp);

        case WM_SETREDRAW:
            grid->no_redraw = !wp;
            if(!grid->no_redraw)
                InvalidateRect(win, NULL, TRUE);
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
            if(wp == GWL_STYLE)
                grid_style_changed(grid, (STYLESTRUCT*) lp);
            break;

        case WM_THEMECHANGED:
            grid_close_theme(grid);
            grid_open_theme(grid);
            if(!grid->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_SYSCOLORCHANGE:
            if(!grid->no_redraw)
                InvalidateRect(win, NULL, TRUE);
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
