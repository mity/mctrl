/*
 * Copyright (c) 2010-2016 Martin Mitas
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
#include "labeledit.h"
#include "mousedrag.h"
#include "mousewheel.h"
#include "rgn16.h"
#include "table.h"
#include "theme.h"


/* Uncomment this to have more verbose traces about MC_GRID control. */
/*#define GRID_DEBUG     1*/

#ifdef GRID_DEBUG
    #define GRID_TRACE  MC_TRACE
#else
    #define GRID_TRACE  MC_NOOP
#endif


static const TCHAR grid_wc[] = MC_WC_GRID;          /* window class name */

static const WCHAR grid_header_tc[] = L"HEADER";    /* theme classes */
static const WCHAR grid_listview_tc[] = L"LISTVIEW";


#define GRID_GGF_ALL                                                        \
        (MC_GGF_COLUMNHEADERHEIGHT | MC_GGF_ROWHEADERWIDTH |                \
         MC_GGF_DEFCOLUMNWIDTH | MC_GGF_DEFROWHEIGHT |                      \
         MC_GGF_PADDINGHORZ | MC_GGF_PADDINGVERT)

#define GRID_GS_SELMASK                                                     \
        (MC_GS_NOSEL | MC_GS_SINGLESEL | MC_GS_RECTSEL | MC_GS_COMPLEXSEL)

#define GRID_DEFAULT_SIZE               0xffff

#define CELL_DEF_PADDING_H              2
#define CELL_DEF_PADDING_V              1

#define DIVIDER_WIDTH                   10
#define SMALL_DIVIDER_WIDTH             4

#define COL_INVALID                     0xfffe   /* 0xffff is taken by MC_TABLE_HEADER */
#define ROW_INVALID                     0xfffe

/* Modes for selection dragging (how to apply marquee) */
#define DRAGSEL_NOOP                    0
#define DRAGSEL_SET                     1
#define DRAGSEL_UNION                   2
#define DRAGSEL_XOR                     3

/* Cursor for column and row resizing. */
#define CURSOR_DIVIDER_H                0
#define CURSOR_DIVIDER_V                1
#define CURSOR_DIVOPEN_H                2
#define CURSOR_DIVOPEN_V                3

static struct {
    int res_id;
    HCURSOR cur;
} grid_cursor[] = {
    { IDR_CURSOR_DIVIDER_H, NULL },
    { IDR_CURSOR_DIVIDER_V, NULL },
    { IDR_CURSOR_DIVOPEN_H, NULL },
    { IDR_CURSOR_DIVOPEN_V, NULL }
};


typedef struct grid_tag grid_t;
struct grid_tag {
    HWND win;
    HWND notify_win;
    HTHEME theme_header;
    HTHEME theme_listview;
    HFONT font;
    table_t* table;  /* may be NULL (MC_GS_OWNERDATA, MC_GS_NOTABLECREATE) */

    DWORD style                  : 16;
    DWORD no_redraw              :  1;
    DWORD unicode_notifications  :  1;
    DWORD rtl                    :  1;
    DWORD focus                  :  1;
    DWORD theme_listitem_defined :  1;
    DWORD tracking_leave         :  1;
    DWORD mouse_captured         :  1;
    DWORD colsizedrag_started    :  1;  /* Dragging column header divider. */
    DWORD rowsizedrag_started    :  1;  /* Dragging row header divider. */
    DWORD seldrag_considering    :  1;
    DWORD seldrag_started        :  1;  /* Dragging selection rectangle. */
    DWORD labeledit_considering  :  1;
    DWORD labeledit_started      :  1;  /* Editing of a label. */

    /* If MC_GS_OWNERDATA, we need it here locally. If not, it is a cached
     * value of table->col_count and table->row_count. */
    WORD col_count;
    WORD row_count;

    WORD cache_hint[4];

    /* Hot cell */
    WORD hot_col;
    WORD hot_row;

    /* Focused cell (or the cell with edit control if edit was started) */
    WORD focused_col;
    WORD focused_row;

    /* Selection */
    rgn16_t selection;
    WORD selmark_col;   /* selection mark for selecting with <SHIFT> key */
    WORD selmark_row;

    /* Cell geometry */
    WORD padding_h;
    WORD padding_v;
    WORD header_width;
    WORD header_height;
    WORD def_col_width;
    WORD def_row_height;
    WORD* col_widths;   /* alloc'ed lazily */
    WORD* row_heights;  /* alloc'ed lazily */

    /* Scrolling */
    int scroll_x;
    int scroll_x_max;   /* Sum of column widths (excluding header) */
    int scroll_y;
    int scroll_y_max;   /* Sum of row heights (excluding header) */
};


/* Forward declarations */
static void grid_mouse_move(grid_t* grid, int x, int y);
static void grid_end_label_edit(grid_t* grid, BOOL cancel);


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
grid_realloc_col_widths(grid_t* grid, WORD old_col_count, WORD new_col_count,
                        BOOL cannot_fail)
{
    WORD* col_widths;

    col_widths = realloc(grid->col_widths, new_col_count * sizeof(WORD));
    if(MC_ERR(col_widths == NULL)) {
        MC_TRACE("grid_realloc_col_widths: realloc() failed.");
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
grid_realloc_row_heights(grid_t* grid, WORD old_row_count, WORD new_row_count,
                         BOOL cannot_fail)
{
    WORD* row_heights;

    row_heights = realloc(grid->row_heights, new_row_count * sizeof(WORD));
    if(MC_ERR(row_heights == NULL)) {
        MC_TRACE("grid_realloc_row_heights: realloc() failed.");
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
grid_scroll_xy(grid_t* grid, int scroll_x, int scroll_y)
{
    SCROLLINFO sih, siv;
    RECT rect;
    int header_w = grid_header_width(grid);
    int header_h = grid_header_height(grid);
    int old_scroll_x = grid->scroll_x;
    int old_scroll_y = grid->scroll_y;

    sih.cbSize = sizeof(SCROLLINFO);
    sih.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(grid->win, SB_HORZ, &sih);

    siv.cbSize = sizeof(SCROLLINFO);
    siv.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(grid->win, SB_VERT, &siv);

    GetClientRect(grid->win, &rect);

    scroll_x = MC_MID3(scroll_x, 0, MC_MAX(0, sih.nMax - (int)sih.nPage));
    scroll_y = MC_MID3(scroll_y, 0, MC_MAX(0, siv.nMax - (int)siv.nPage));

    if(scroll_x == old_scroll_x  &&  scroll_y == old_scroll_y)
        return;

    /* Scrolling ends label edit. */
    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    /* Refresh */
    if(!grid->no_redraw) {
        if(scroll_x == old_scroll_x) {
            /* Optimization for purely vertical scrolling:
             * Column headers can be scrolled together with ordinary cells. */
            rect.top = header_h;
        } else if(scroll_y == old_scroll_y) {
            /* Optimization for purely horizontal scrolling:
             * Row headers can be scrolled together with ordinary cells. */
            rect.left = header_w;
        } else {
            /* Combined (both horizontal and vertical) scrolling. */
            RECT header_rect;

            if(header_h > 0) {
                /* Scroll column headers. */
                mc_rect_set(&header_rect, header_w, 0, rect.right, header_h);
                ScrollWindowEx(grid->win, old_scroll_x - scroll_x, 0,
                               &header_rect, &header_rect, NULL, NULL,
                               SW_ERASE | SW_INVALIDATE);
            }

            if(header_w > 0) {
                /* Scroll row headers. */
                mc_rect_set(&header_rect, 0, header_h, header_w, rect.bottom);
                ScrollWindowEx(grid->win, 0, old_scroll_y - scroll_y,
                               &header_rect, &header_rect, NULL, NULL,
                               SW_ERASE | SW_INVALIDATE);
            }

            rect.left = header_w;
            rect.top = header_h;
        }

        /* Scroll ordinary cells. */
        ScrollWindowEx(grid->win, old_scroll_x - scroll_x, old_scroll_y - scroll_y,
                       &rect, &rect, NULL, NULL, SW_ERASE | SW_INVALIDATE);

        /* Focus rect can overlap into headers, so the scrolling could leave
         * there an artifact. */
        if(grid->focus  &&  (grid->style & MC_GS_FOCUSEDCELL)  &&
           (header_w > 0 || header_h > 0))
        {
            RECT r;

            grid_cell_rect(grid, grid->focused_col, grid->focused_row, &r);
            mc_rect_inflate(&r, 1, 1);

            if(header_w > 0  &&  r.left < header_w  &&  r.right >= header_w) {
                RECT rr = { header_w-1, r.top, header_w, r.bottom };
                InvalidateRect(grid->win, &rr, TRUE);
            }
            if(header_h > 0  &&  r.top < header_h  &&  r.bottom >= header_h) {
                RECT rr = { r.left, header_h-1, r.right, header_h };
                InvalidateRect(grid->win, &rr, TRUE);
            }
        }
    }

    SetScrollPos(grid->win, SB_HORZ, scroll_x, TRUE);
    SetScrollPos(grid->win, SB_VERT, scroll_y, TRUE);
    grid->scroll_x = scroll_x;
    grid->scroll_y = scroll_y;
}

static void
grid_scroll(grid_t* grid, BOOL is_vertical, WORD opcode, int factor)
{
    SCROLLINFO si;
    int scroll_x = grid->scroll_x;
    int scroll_y = grid->scroll_y;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

    if(is_vertical) {
        GetScrollInfo(grid->win, SB_VERT, &si);
        switch(opcode) {
            case SB_BOTTOM:         scroll_y = si.nMax; break;
            case SB_LINEUP:         scroll_y -= factor * MC_MIN3(grid->def_row_height, 40, si.nPage); break;
            case SB_LINEDOWN:       scroll_y += factor * MC_MIN3(grid->def_row_height, 40, si.nPage); break;
            case SB_PAGEUP:         scroll_y -= si.nPage; break;
            case SB_PAGEDOWN:       scroll_y += si.nPage; break;
            case SB_THUMBPOSITION:  scroll_y = si.nPos; break;
            case SB_THUMBTRACK:     scroll_y = si.nTrackPos; break;
            case SB_TOP:            scroll_y = 0; break;
        }
    } else {
        GetScrollInfo(grid->win, SB_HORZ, &si);
        switch(opcode) {
            case SB_BOTTOM:         scroll_x = si.nMax; break;
            case SB_LINELEFT:       scroll_x -= factor * MC_MIN3(grid->def_col_width, 40, si.nPage); break;
            case SB_LINERIGHT:      scroll_x += factor * MC_MIN3(grid->def_col_width, 40, si.nPage); break;
            case SB_PAGELEFT:       scroll_x -= si.nPage; break;
            case SB_PAGERIGHT:      scroll_x += si.nPage; break;
            case SB_THUMBPOSITION:  scroll_x = si.nPos; break;
            case SB_THUMBTRACK:     scroll_x = si.nTrackPos; break;
            case SB_TOP:            scroll_x = 0; break;
        }
    }

    grid_scroll_xy(grid, scroll_x, scroll_y);
}

/* Called from WM_TIMER when in the marquee selection dragging mode. */
static void
grid_autoscroll(grid_t* grid)
{
    RECT client;
    int header_w, header_h;
    DWORD pos;
    POINT pt;
    int scroll_x, scroll_y;

    GetClientRect(grid->win, &client);
    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    pos = GetMessagePos();
    pt.x = GET_X_LPARAM(pos);
    pt.y = GET_Y_LPARAM(pos);
    ScreenToClient(grid->win, &pt);

    scroll_x = grid->scroll_x;
    if(pt.x < header_w)
        scroll_x -= header_w - pt.x;
    else if(pt.x >= client.right)
        scroll_x += pt.x - client.right + 1;

    scroll_y = grid->scroll_y;
    if(pt.y < header_h)
        scroll_y -= header_h - pt.y;
    else if(pt.y >= client.bottom)
        scroll_y += pt.y - client.bottom + 1;

    grid_scroll_xy(grid, scroll_x, scroll_y);

    /* Update the marquee accordingly. */
    grid_mouse_move(grid, pt.x, pt.y);
}

static void
grid_mouse_wheel(grid_t* grid, BOOL is_vertical, int wheel_delta)
{
    SCROLLINFO si;
    int line_delta;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    GetScrollInfo(grid->win, (is_vertical ? SB_VERT : SB_HORZ), &si);

    line_delta = mousewheel_scroll(grid->win, wheel_delta, si.nPage, is_vertical);
    if(line_delta != 0)
        grid_scroll(grid, is_vertical, SB_LINEDOWN, line_delta);
}

static void
grid_setup_scrollbars(grid_t* grid, BOOL recalc_max)
{
    SCROLLINFO si;
    RECT client;
    WORD header_w, header_h;

    GRID_TRACE("grid_setup_scrollbars(%p, %s)",
               grid, recalc_max ? "TRUE" : "FALSE");

    GetClientRect(grid->win, &client);
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
    si.nPage = mc_width(&client) - header_w;
    grid->scroll_x = SetScrollInfo(grid->win, SB_HORZ, &si, TRUE);

    /* SetScrollInfo() above could change client dimensions. */
    GetClientRect(grid->win, &client);

    /* Setup vertical scrollbar */
    si.nMax = grid->scroll_y_max;
    si.nPage = mc_height(&client) - header_h;
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
    TCHAR* text;
    DWORD flags;
    LPARAM lp;
};

static void
grid_get_dispinfo(grid_t* grid, WORD col, WORD row, table_cell_t* cell,
                  grid_dispinfo_t* di, DWORD mask)
{
    MC_NMGDISPINFO info;

    MC_ASSERT((mask & ~(MC_TCMF_TEXT | MC_TCMF_PARAM | MC_TCMF_FLAGS)) == 0);

    /* Use what can be taken from the cell. */
    if(cell != NULL) {
        if(cell->text != MC_LPSTR_TEXTCALLBACK) {
            di->text = cell->text;
            mask &= ~MC_TCMF_TEXT;
        }

        di->lp = cell->lp;
        mask &= ~(MC_TCMF_PARAM);

        di->flags = cell->flags;
        mask &= ~(MC_TCMF_FLAGS);

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
        info.cell.lParam = cell->lp;
        info.cell.dwFlags = cell->flags;
    } else {
        info.cell.pszText = NULL;
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
                HDC dc, RECT* rect, int control_cd_mode, MC_NMGCUSTOMDRAW* cd)
{
    RECT content;
    UINT dt_flags = DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS;
    grid_dispinfo_t di;
    int item_cd_mode = 0;
    BOOL is_selected;
    BOOL is_hot;
    int state;
    COLORREF text_color;
    COLORREF back_color;

    is_selected = rgn16_contains_xy(&grid->selection, col, row);

    /* If we are currently dragging a selection marquee, we want to display
     * selection state which would result from it if the users ends it right
     * now by WM_LBUTTONUP. */
    if(grid->seldrag_considering  ||  grid->seldrag_started) {
        if(mousedrag_lock(grid->win)) {
            int drag_start_x, drag_start_y;
            int drag_hotspot_x, drag_hotspot_y;
            int drag_mode;
            RECT marquee_rect;
            BOOL is_in_marquee;

            drag_start_x = mousedrag_start_x;
            drag_start_y = mousedrag_start_y;
            drag_hotspot_x = mousedrag_hotspot_x;
            drag_hotspot_y = mousedrag_hotspot_y;
            drag_mode = mousedrag_extra;
            mousedrag_unlock();

            marquee_rect.left = MC_MIN(drag_start_x, drag_hotspot_x) - grid->scroll_x;
            marquee_rect.top = MC_MIN(drag_start_y, drag_hotspot_y) - grid->scroll_y;
            marquee_rect.right = MC_MAX(drag_start_x, drag_hotspot_x) - grid->scroll_x + 1;
            marquee_rect.bottom = MC_MAX(drag_start_y, drag_hotspot_y) - grid->scroll_y + 1;
            is_in_marquee = mc_rect_overlaps_rect(rect, &marquee_rect);

            switch(drag_mode) {
                case DRAGSEL_SET:   is_selected = is_in_marquee; break;
                case DRAGSEL_UNION: is_selected = (is_selected || is_in_marquee); break;
                case DRAGSEL_XOR:   if(is_in_marquee) is_selected = !is_selected; break;
            }
        } else {
            MC_ASSERT(grid->seldrag_considering);
            MC_ASSERT(!grid->seldrag_started);
            grid->seldrag_considering = FALSE;
        }
    }

    is_hot = (col == grid->hot_col  &&  row == grid->hot_row  &&
              col < grid->col_count  &&  row < grid->row_count);  /* <-- avoid headers */

    if(col == MC_TABLE_HEADER  &&  row == MC_TABLE_HEADER) {
        grid_get_dispinfo(grid, col, row, cell, &di, MC_TCMF_FLAGS);
        di.text = NULL;
    } else {
        grid_get_dispinfo(grid, col, row, cell, &di, MC_TCMF_TEXT | MC_TCMF_FLAGS);
    }

    text_color = GetSysColor(COLOR_BTNTEXT);
    back_color = MC_CLR_NONE;
    if(is_selected) {
        if(grid->focus)
            back_color = RGB(209,232,255);
        else if(grid->style & MC_GS_SHOWSELALWAYS)
            back_color = GetSysColor(COLOR_BTNFACE);
    }

    /* Custom draw: Item pre-paint notification */
    if(control_cd_mode & CDRF_NOTIFYITEMDRAW) {
        cd->nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
        mc_rect_copy(&cd->nmcd.rc, rect);
        cd->nmcd.dwItemSpec = (DWORD)MAKELONG(col, row);
        cd->nmcd.uItemState = 0;
        if(is_selected)
            cd->nmcd.uItemState |= CDIS_SELECTED;
        if((grid->style & MC_GS_FOCUSEDCELL)  &&  grid->focus  &&
            col == grid->focused_col  &&  row == grid->focused_row)
            cd->nmcd.uItemState |= CDIS_FOCUS;
        if(is_hot)
            cd->nmcd.uItemState |= CDIS_HOT;
        cd->nmcd.lItemlParam = di.lp;
        cd->clrText = text_color;
        cd->clrTextBk = back_color;
        item_cd_mode = MC_SEND(grid->notify_win, WM_NOTIFY, cd->nmcd.hdr.idFrom, cd);
        if(item_cd_mode & (CDRF_SKIPDEFAULT | CDRF_DOERASE))
            return;
        text_color = cd->clrText;
        back_color = cd->clrTextBk;
    }

    /* Apply padding */
    content.left = rect->left + grid->padding_h;
    content.top = rect->top + grid->padding_v;
    content.right = rect->right - grid->padding_h;
    content.bottom = rect->bottom - grid->padding_v;

    /* Paint cell background */
    if(col != MC_TABLE_HEADER  &&  row != MC_TABLE_HEADER) {
        if(grid->theme_listitem_defined) {
            if(!IsWindowEnabled(grid->win)) {
                state = LISS_DISABLED;
            } else if(is_selected) {
                if(is_hot)
                    state = LISS_HOTSELECTED;
                else if(grid->focus)
                    state = LISS_SELECTED;
                else
                    state = LISS_SELECTEDNOTFOCUS;
            } else if(is_hot) {
                state = LISS_HOT;
            } else {
                state = LISS_NORMAL;
            }

            mcDrawThemeBackground(grid->theme_listview, dc, LVP_LISTITEM, state, rect, NULL);
        } else if(back_color != MC_CLR_NONE  &&  back_color != MC_CLR_DEFAULT) {
            SetBkColor(dc, back_color);
            ExtTextOut(dc, 0, 0, ETO_OPAQUE, rect, NULL, 0, NULL);
        }
    } else {
        if(grid->theme_header != NULL) {
            mcDrawThemeBackground(grid->theme_header, dc,
                                  HP_HEADERITEM, HIS_NORMAL, rect, NULL);
        } else {
            DrawEdge(dc, rect, BDR_RAISEDINNER, BF_MIDDLE | BF_RECT);
        }
    }

    /* Paint cell value or text. */
    if(di.text != NULL) {
        switch(di.flags & MC_TCF_ALIGNMASKHORZ) {
            case MC_TCF_ALIGNDEFAULT:   /* fall through */
            case MC_TCF_ALIGNLEFT:      dt_flags |= DT_LEFT; break;
            case MC_TCF_ALIGNCENTER:    dt_flags |= DT_CENTER; break;
            case MC_TCF_ALIGNRIGHT:     dt_flags |= DT_RIGHT; break;
        }
        switch(di.flags & MC_TCF_ALIGNMASKVERT) {
            case MC_TCF_ALIGNTOP:       dt_flags |= DT_TOP; break;
            case MC_TCF_ALIGNVDEFAULT:  /* fall through */
            case MC_TCF_ALIGNVCENTER:   dt_flags |= DT_VCENTER; break;
            case MC_TCF_ALIGNBOTTOM:    dt_flags |= DT_BOTTOM; break;
        }

        if(grid->theme_listitem_defined) {
            mcDrawThemeText(grid->theme_listview, dc, LVP_LISTITEM, state,
                            di.text, -1, dt_flags, 0, &content);
        } else {
            SetTextColor(dc, text_color);
            DrawText(dc, di.text, -1, &content, dt_flags);
        }
    }

    grid_free_dispinfo(grid, cell, &di);

    if(item_cd_mode & CDRF_NEWFONT)
        SelectObject(dc, grid->font);

    /* Custom draw: Item post-paint notification */
    if(item_cd_mode & CDRF_NOTIFYPOSTPAINT) {
        cd->nmcd.dwDrawStage = CDDS_POSTPAINT;
        MC_SEND(grid->notify_win, WM_NOTIFY, cd->nmcd.hdr.idFrom, cd);
    }
}

static void
grid_paint_header_cell(grid_t* grid, WORD col, WORD row, table_cell_t* cell,
                       HDC dc, RECT* rect, int index, DWORD style,
                       int control_cd_mode, MC_NMGCUSTOMDRAW* cd)
{
    table_cell_t tmp;
    table_cell_t* tmp_cell = &tmp;
    TCHAR buffer[16];
    DWORD fabricate;

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
    }

    /* If the header does not say explicitly otherwise, force centered
     * alignment for the header cells. */
    if((tmp.flags & MC_TCF_ALIGNMASKHORZ) == MC_TCF_ALIGNDEFAULT)
        tmp.flags |= MC_TCF_ALIGNCENTER;
    if((tmp.flags & MC_TCF_ALIGNMASKVERT) == MC_TCF_ALIGNVDEFAULT)
        tmp.flags |= MC_TCF_ALIGNVCENTER;

    /* Paint header contents. */
    grid_paint_cell(grid, col, row, tmp_cell, dc, rect, control_cd_mode, cd);
}

static inline void
grid_paint_rect(HDC dc, const RECT* r)
{
    POINT p[5] = {
        { r->left-1, r->top-1 },
        { r->right-1, r->top-1 },
        { r->right-1, r->bottom-1 },
        { r->left-1, r->bottom-1 },
        { r->left-1, r->top-1 }
    };

    Polyline(dc, p, 5);
}

static void
grid_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    grid_t* grid = (grid_t*) control;
    RECT client;
    RECT rect;
    int header_w, header_h;
    int gridline_w;
    int col0, row0;
    int x0, y0;
    int col, row;
    int col_count = grid->col_count;
    int row_count = grid->row_count;
    table_t* table = grid->table;
    table_cell_t* cell;
    MC_NMGCUSTOMDRAW cd = { { { 0 }, 0 }, 0 };
    int cd_mode;
    int old_mode;
    HPEN old_pen;
    HFONT old_font;
    HRGN old_clip;
    COLORREF old_text_color;
    COLORREF old_bk_color;

    GRID_TRACE("grid_paint(%p, %d, %d, %d, %d)", grid,
               dirty->left, dirty->top, dirty->right, dirty->bottom);

    if(table == NULL  &&  !(grid->style & MC_GS_OWNERDATA))
        return;

    GetClientRect(grid->win, &client);
    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);

    if(erase) {
        HBRUSH brush;

        brush = mcGetThemeSysColorBrush(grid->theme_listview, COLOR_WINDOW);
        FillRect(dc, &client, brush);
        DeleteObject(brush);
    }

    old_mode = SetBkMode(dc, TRANSPARENT);
    old_bk_color = GetBkColor(dc);
    old_text_color = GetTextColor(dc);
    old_pen = SelectObject(dc, GetStockObject(BLACK_PEN));
    old_font = SelectObject(dc, (grid->font != NULL ?
                            grid->font : GetStockObject(SYSTEM_FONT)));
    old_clip = mc_clip_get(dc);

    /* Custom draw: Control pre-paint notification */
    cd.nmcd.hdr.hwndFrom = grid->win;
    cd.nmcd.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    cd.nmcd.hdr.code = NM_CUSTOMDRAW;
    cd.nmcd.dwDrawStage = CDDS_PREPAINT;
    cd.nmcd.hdc = dc;
    cd.clrText = 0;
    cd.clrTextBk = 0;
    cd_mode = MC_SEND(grid->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
    if(cd_mode & (CDRF_SKIPDEFAULT | CDRF_DOERASE))
        goto skip_control_paint;

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
        grid_paint_header_cell(grid, MC_TABLE_HEADER, MC_TABLE_HEADER, NULL, dc,
                               &rect, -1, 0, cd_mode, &cd);
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
                                   dc, &rect, col, (grid->style & MC_GS_COLUMNHEADERMASK),
                                   cd_mode, &cd);
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
                                   dc, &rect, row, (grid->style & MC_GS_ROWHEADERMASK),
                                   cd_mode, &cd);
            rect.top = rect.bottom;
            if(rect.bottom >= client.bottom)
                break;
        }
    }

    /* Paint grid lines */
    if(!(grid->style & MC_GS_NOGRIDLINES)) {
        HPEN pen, old_pen;
        int max_x = header_w + grid->scroll_x_max - grid->scroll_x;
        int max_y = header_h + grid->scroll_y_max - grid->scroll_y;
        int x, y;

        /* Windows 10 is "too flat", we need at least divide headers from
         * the main body. */
        if(mc_win_version >= MC_WIN_10)
            mc_clip_set(dc, 0, 0, client.right, client.bottom);
        else
            mc_clip_set(dc, header_w, header_h, client.right, client.bottom);

        pen = CreatePen(PS_SOLID, 0, mcGetThemeSysColor(grid->theme_listview, COLOR_3DFACE));
        old_pen = SelectObject(dc, pen);

        x = x0 - 1;
        y = max_y;
        for(col = col0; col < col_count; col++) {
            x += grid_col_width(grid, col);
            MoveToEx(dc, x, 0, NULL);
            LineTo(dc, x, y);
            if(x >= client.right)
                break;
        }

        x = max_x;
        y = y0 - 1;
        for(row = row0; row < row_count; row++) {
            y += grid_row_height(grid, row);
            MoveToEx(dc, 0, y, NULL);
            LineTo(dc, x, y);
            if(y >= client.bottom)
                break;
        }

        /* Windows 10 is "too flat", we need at least divide headers from
         * the main body. */
        if(mc_win_version >= MC_WIN_10  &&  (header_w > 0  ||  header_h > 0)) {
            MoveToEx(dc, header_w - 1, 0, NULL);
            LineTo(dc, header_w - 1, max_y);

            MoveToEx(dc, 0, header_h - 1, NULL);
            LineTo(dc, max_x, header_h - 1);
        }

        SelectObject(dc, old_pen);
        DeleteObject(pen);

        gridline_w = 1;
    } else {
        gridline_w = 0;
    }

    /* Paint grid cells */
    mc_clip_set(dc, header_w, header_h, client.right, client.bottom);
    rect.top = y0;
    for(row = row0; row < row_count; row++) {
        rect.bottom = rect.top + grid_row_height(grid, row) - gridline_w;
        rect.left = x0;
        for(col = col0; col < col_count; col++) {
            if(table != NULL)
                cell = table_cell(table, col, row);
            else
                cell = NULL;
            rect.right = rect.left + grid_col_width(grid, col) - gridline_w;
            grid_paint_cell(grid, col, row, cell, dc, &rect, cd_mode, &cd);
            if(rect.right >= client.right)
                break;
            rect.left = rect.right + gridline_w;
        }
        if(rect.bottom >= client.bottom)
            break;
        rect.top = rect.bottom + gridline_w;
    }

    if(!(cd_mode & CDRF_SKIPPOSTPAINT)) {
        if(grid->seldrag_started) {
            /* Paint selection dragging marquee. */
            RECT r;

            r.left = MC_MIN(mousedrag_start_x, mousedrag_hotspot_x) - grid->scroll_x;
            r.top = MC_MIN(mousedrag_start_y, mousedrag_hotspot_y) - grid->scroll_y;
            r.right = MC_MAX(mousedrag_start_x, mousedrag_hotspot_x) - grid->scroll_x + 1;
            r.bottom = MC_MAX(mousedrag_start_y, mousedrag_hotspot_y) - grid->scroll_y + 1;

            DrawFocusRect(dc, &r);
        } else if(grid->focus  &&  (grid->style & MC_GS_FOCUSEDCELL)  &&
                  col_count > 0  &&  row_count > 0) {
            /* Paint focus cursor */
            RECT r;

            mc_clip_set(dc, MC_MAX(0, header_w-1), MC_MAX(0, header_h-1),
                            client.right, client.bottom);
            grid_cell_rect(grid, grid->focused_col, grid->focused_row, &r);
            r.right -= gridline_w;
            r.bottom -= gridline_w;
            grid_paint_rect(dc, &r);
            mc_rect_inflate(&r, -1, -1);
            grid_paint_rect(dc, &r);
        }
    }

    /* Custom draw: Control post-paint notification */
    if(cd_mode & CDRF_NOTIFYPOSTPAINT) {
        cd.nmcd.dwDrawStage = CDDS_POSTPAINT;
        MC_SEND(grid->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
    }

skip_control_paint:
    SetBkMode(dc, old_mode);
    SetBkColor(dc, old_bk_color);
    SetTextColor(dc, old_text_color);
    SelectObject(dc, old_pen);
    if(old_font != NULL)
        SelectObject(dc, old_font);
    mc_clip_reset(dc, old_clip);
}

static inline void
grid_invalidate_region(grid_t* grid, WORD col0, WORD row0, WORD col1, WORD row1)
{
    RECT r;

    grid_region_rect(grid, col0, row0, col1, row1, &r);
    InvalidateRect(grid->win, &r, TRUE);
}

static inline void
grid_invalidate_cell(grid_t* grid, WORD col, WORD row, BOOL extend_for_focus)
{
    RECT r;

    grid_cell_rect(grid, col, row, &r);
    if(extend_for_focus)
        mc_rect_inflate(&r, 1, 1);
    InvalidateRect(grid->win, &r, TRUE);
}

static inline void
grid_invalidate_selection(grid_t* grid)
{
    const rgn16_rect_t* ext = rgn16_extents(&grid->selection);
    if(ext != NULL)
        grid_invalidate_region(grid, ext->x0, ext->y0, ext->x1, ext->y1);
}

static void
grid_refresh(void* view, void* detail)
{
    grid_t* grid = (grid_t*) view;
    table_refresh_detail_t* rd = (table_refresh_detail_t*) detail;

    MC_ASSERT(rd != NULL);

    /* Any change in the table end label editing. */
    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    switch(rd->event) {
        case TABLE_CELL_CHANGED:
            if(!grid->no_redraw)
                grid_invalidate_cell(grid, rd->param[0], rd->param[1], FALSE);
            break;

        case TABLE_REGION_CHANGED:
            if(!grid->no_redraw)
                grid_invalidate_region(grid, rd->param[0], rd->param[1],
                                       rd->param[2], rd->param[3]);
            break;

        case TABLE_COLCOUNT_CHANGED:
            if(grid->col_widths != NULL)
                grid_realloc_col_widths(grid, grid->col_count, rd->param[1], TRUE);
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
                grid_realloc_row_heights(grid, grid->row_count, rd->param[1], TRUE);
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

static DWORD
grid_hit_test_ex(grid_t* grid, MC_GHITTESTINFO* info, RECT* cell_rect)
{
    int x = info->pt.x;
    int y = info->pt.y;
    RECT client;
    int header_w, header_h;
    WORD col, row;
    int x0, x1, x2, x3;
    int y0, y1, y2, y3;

    /* Check if outside client */
    GetClientRect(grid->win, &client);
    if(!mc_rect_contains_xy(&client, x, y)) {
        info->flags = 0;

        if(x < client.left)
            info->flags |= MC_GHT_TOLEFT;
        else if(x >= client.right)
            info->flags |= MC_GHT_TORIGHT;

        if(y < client.top)
            info->flags |= MC_GHT_ABOVE;
        else if(y >= client.bottom)
            info->flags |= MC_GHT_BELOW;

        info->wColumn = (WORD) -1;
        info->wRow = (WORD) -1;
        return (DWORD) -1;
    }

    /* Handle the "dead header cell" */
    header_w = grid_header_width(grid);
    header_h = grid_header_height(grid);
    if(x < header_w  &&  y < header_h) {
        info->flags = MC_GHT_ONCOLUMNHEADER | MC_GHT_ONROWHEADER;
        info->wColumn = MC_TABLE_HEADER;
        info->wRow = MC_TABLE_HEADER;
        if(cell_rect != NULL)
            mc_rect_set(cell_rect, 0, 0, header_w, header_h);
        return MAKELRESULT(MC_TABLE_HEADER, MC_TABLE_HEADER);
    }

    /* Handle column headers */
    if(y < header_h) {
        info->wRow = MC_TABLE_HEADER;

        x3 = header_w - grid->scroll_x;
        for(col = 0; col < grid->col_count; col++) {
            int divider_width;

            x0 = x3;
            x3 += grid_col_width(grid, col);

            if(x >= x3)
                continue;

            if(grid->style & MC_GS_RESIZABLECOLUMNS) {
                if(x3 - x0 > 2 * DIVIDER_WIDTH)
                    divider_width = DIVIDER_WIDTH;
                else
                    divider_width = SMALL_DIVIDER_WIDTH;
            } else {
                divider_width = 0;
            }

            x1 = x0 + divider_width / 2;
            x2 = x3 - divider_width / 2;

            if(x < x1  &&  col > 0) {
                if(grid_col_width(grid, col - 1) > 0)
                    info->flags = MC_GHT_ONCOLUMNDIVIDER;
                else
                    info->flags = MC_GHT_ONCOLUMNDIVOPEN;
                info->wColumn = col - 1;
            } else if(x >= x2) {
                info->flags = MC_GHT_ONCOLUMNDIVIDER;
                info->wColumn = col;
            } else {
                info->flags = MC_GHT_ONCOLUMNHEADER;
                info->wColumn = col;
            }

            if(cell_rect != NULL)
                grid_cell_rect(grid, info->wColumn, MC_TABLE_HEADER, cell_rect);

            return MAKELRESULT(info->wColumn, MC_TABLE_HEADER);
        }

        /* Treat a small area after the last column also as a part of the
         * column divider. */
        if((grid->style & MC_GS_RESIZABLECOLUMNS)  &&  grid->col_count > 0) {
            if(x < x3 + DIVIDER_WIDTH / 2) {
                if(grid_col_width(grid, grid->col_count - 1) > 0)
                    info->flags = MC_GHT_ONCOLUMNDIVIDER;
                else
                    info->flags = MC_GHT_ONCOLUMNDIVOPEN;
                info->wColumn = grid->col_count - 1;
                if(cell_rect != NULL)
                    mc_rect_set(cell_rect, x0, 0, x3, header_h);
                return MAKELRESULT(info->wColumn, MC_TABLE_HEADER);
            }
        }

        goto nowhere;
    }

    /* Handle row headers */
    if(x < header_w) {
        info->wColumn = MC_TABLE_HEADER;

        y3 = header_h - grid->scroll_y;
        for(row = 0; row < grid->row_count; row++) {
            int divider_width;

            y0 = y3;
            y3 += grid_row_height(grid, row);

            if(y >= y3)
                continue;

            if(grid->style & MC_GS_RESIZABLEROWS) {
                if(y3 - y0 > 2 * DIVIDER_WIDTH)
                    divider_width = DIVIDER_WIDTH;
                else
                    divider_width = SMALL_DIVIDER_WIDTH;
            } else {
                divider_width = 0;
            }

            y1 = y0 + divider_width / 2;
            y2 = y3 - divider_width / 2;

            if(y < y1  &&  row > 0) {
                if(grid_row_height(grid, row - 1) > 0)
                    info->flags = MC_GHT_ONROWDIVIDER;
                else
                    info->flags = MC_GHT_ONROWDIVOPEN;
                info->wRow = row - 1;
            } else if(y >= y2) {
                info->flags = MC_GHT_ONROWDIVIDER;
                info->wRow = row;
            } else {
                info->flags = MC_GHT_ONROWHEADER;
                info->wRow = row;
            }

            if(cell_rect != NULL)
                grid_cell_rect(grid, MC_TABLE_HEADER, info->wRow, cell_rect);

            return MAKELRESULT(MC_TABLE_HEADER, info->wRow);
        }

        /* Treat a small area after the last column also as a part of the
         * column divider. */
        if((grid->style & MC_GS_RESIZABLEROWS)  &&  grid->row_count > 0) {
            if(y < y3 + DIVIDER_WIDTH / 2) {
                if(grid_row_height(grid, grid->row_count - 1) > 0)
                    info->flags = MC_GHT_ONROWDIVIDER;
                else
                    info->flags = MC_GHT_ONROWDIVOPEN;
                info->wRow = grid->row_count - 1;
                if(cell_rect != NULL)
                    mc_rect_set(cell_rect, 0, y0, header_w, y3);
                return MAKELRESULT(MC_TABLE_HEADER, info->wRow);
            }
        }

        goto nowhere;
    }

    /* Handle ordinary cells */
    info->wColumn = (WORD) -1;
    x3 = header_w - grid->scroll_x;
    for(col = 0; col < grid->col_count; col++) {
        x0 = x3;
        x3 += grid_col_width(grid, col);
        if(x < x3) {
            info->wColumn = col;
            break;
        }
    }
    if(info->wColumn == (WORD) -1)
        goto nowhere;

    info->wRow = (WORD) -1;
    y3 = header_h - grid->scroll_y;
    for(row = 0; row < grid->row_count; row++) {
        y0 = y3;
        y3 += grid_row_height(grid, row);
        if(y < y3) {
            info->wRow = row;
            break;
        }
    }
    if(info->wRow == (WORD) -1)
        goto nowhere;

    info->flags = MC_GHT_ONNORMALCELL;
    if(cell_rect != NULL)
        mc_rect_set(cell_rect, x0, y0, x3, y3);
    return MAKELRESULT(info->wColumn, info->wRow);

    /* Nowhere. */
nowhere:
    info->flags = MC_GHT_NOWHERE;
    info->wColumn = (WORD) -1;
    info->wRow = (WORD) -1;
    return (DWORD) -1;
}

static inline DWORD
grid_hit_test(grid_t* grid, MC_GHITTESTINFO* info)
{
    return grid_hit_test_ex(grid, info, NULL);
}

static int
grid_set_focused_cell(grid_t* grid, WORD col, WORD row)
{
    WORD old_col = grid->focused_col;
    WORD old_row = grid->focused_row;
    MC_NMGFOCUSEDCELLCHANGE notif;

    if(MC_ERR(col >= grid->col_count  ||  row >= grid->row_count)) {
        MC_TRACE("grid_set_focused_cell: Cell [%hu, %hu] out of range.",
                 col, row);
        return -1;
    }

    if(col == grid->focused_col  &&  row == grid->focused_row)
        return 0;

    /* Fire notification MC_GN_FOCUSEDCELLCHANGING */
    notif.hdr.hwndFrom = grid->win;
    notif.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    notif.hdr.code = MC_GN_FOCUSEDCELLCHANGING;
    notif.wOldColumn = old_col;
    notif.wOldRow = old_row;
    notif.wNewColumn = col;
    notif.wNewRow = row;
    if(MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif)) {
        /* Application suppresses the default processing */
        GRID_TRACE("grid_set_focused_cell: "
                   "MC_GN_FOCUSEDCELLCHANGING suppresses the change");
        return -1;
    }

    /* Changing of the focus ends any cell editing. */
    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    /* Set new focus. */
    grid->focused_col = col;
    grid->focused_row = row;

    /* Fire notification MC_GN_FOCUSEDCELLCHANGED. */
    notif.hdr.code = MC_GN_FOCUSEDCELLCHANGED;
    MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif);

    /* Refresh */
    if(!grid->no_redraw  &&  grid->focus) {
        grid_invalidate_cell(grid, old_col, old_row, TRUE);
        grid_invalidate_cell(grid, col, row, TRUE);
    }

    return 0;
}

static void
grid_setup_MC_GSELECTION(MC_GSELECTION* gsel, rgn16_t* rgn)
{
    static const rgn16_rect_t empty_rc = { 0, 0, 0, 0 };
    const rgn16_rect_t* extents;
    const rgn16_rect_t* vec;
    WORD n;

    switch(rgn->n) {
        case 0:     extents = &empty_rc;      vec = NULL;           n = 0;          break;
        case 1:     extents = &rgn->s.rc;     vec = &rgn->s.rc;     n = 1;          break;
        default:    extents = &rgn->c.vec[0]; vec = &rgn->c.vec[1]; n = rgn->n - 1; break;
    }

    /* rgn16_rect_t and MC_GRECT are binary compatible. */
    memcpy(&gsel->rcExtents, extents, sizeof(rgn16_rect_t));
    gsel->uDataCount = n;
    gsel->rcData = (MC_GRECT*) vec;
}

/* Warning: This function always consumes the 'sel', even when it fails. */
static int
grid_install_selection(grid_t* grid, rgn16_t* sel)
{
    MC_NMGSELECTIONCHANGE notif;
    rgn16_t tmp;

    if(rgn16_equals_rgn(&grid->selection, sel)) {
        rgn16_fini(sel);
        return 0;
    }

    /* Fire notification MC_GN_SELECTIONCHANGING */
    notif.hdr.hwndFrom = grid->win;
    notif.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    notif.hdr.code = MC_GN_SELECTIONCHANGING;
    grid_setup_MC_GSELECTION(&notif.oldSelection, &grid->selection);
    grid_setup_MC_GSELECTION(&notif.newSelection, sel);

    if(MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif)) {
        /* Application suppresses the processing */
        GRID_TRACE("grid_install_selection: "
                   "MC_GN_SELECTIONCHANGING suppresses the change.");
        rgn16_fini(sel);
        return -1;
    }

    /* Install the new selection
     * (by swapping rgn16_t guts with the old selection). */
    memcpy(&tmp, &grid->selection, sizeof(rgn16_t));
    memcpy(&grid->selection, sel, sizeof(rgn16_t));
    memcpy(sel, &tmp, sizeof(rgn16_t));

    /* Refresh */
    if(!grid->no_redraw) {
        const rgn16_rect_t* ext;

        ext = rgn16_extents(&grid->selection);
        if(ext != NULL)
            grid_invalidate_region(grid, ext->x0, ext->y0, ext->x1, ext->y1);

        ext = rgn16_extents(sel);
        if(ext != NULL)
            grid_invalidate_region(grid, ext->x0, ext->y0, ext->x1, ext->y1);
    }

    /* Fire notification MC_GN_SELECTIONCHANGED */
    notif.hdr.code = MC_GN_SELECTIONCHANGED;
    MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif);

    /* Free the original selection */
    rgn16_fini(sel);

    return 0;
}

static int
grid_set_selection(grid_t* grid, MC_GSELECTION* gsel)
{
    UINT n = gsel->uDataCount;
    rgn16_t sel;

    if(n == 0) {
        rgn16_init(&sel);
    } else if(n == 1) {
        rgn16_init_with_rect(&sel, (rgn16_rect_t*) &gsel->rcData[0]);
    } else {
        /* We have to be careful. On input, application can provide rect
         * array which does not follow rgn16_t rules. Hence we create the
         * selection by iterative unioning all the rects.
         *
         * TODO: Optimize this. We should only call union for whole sequences
         *       of rects which follow the rgn16_t rules.
         */
        WORD i;

        rgn16_init(&sel);

        for(i = 0; i < gsel->uDataCount; i++) {
            rgn16_t rgn_rc;
            rgn16_t rgn_union;
            rgn16_rect_t r;

            r.x0 = gsel->rcData[i].wColumnFrom;
            r.y0 = gsel->rcData[i].wRowFrom;
            r.x1 = MC_MIN(gsel->rcData[i].wColumnTo, grid->col_count);
            r.y1 = MC_MIN(gsel->rcData[i].wRowTo, grid->row_count);

            if(r.x0 >= r.x1  ||  r.y0 >= r.y1) {
                /* Skip empty rect */
                continue;
            }

            rgn16_init_with_rect(&rgn_rc, &r);
            if(MC_ERR(rgn16_union(&rgn_union, &sel, &rgn_rc)) != 0) {
                MC_TRACE("grid_set_selection: rgn16_union() failed.");
                rgn16_fini(&sel);
                return -1;
            }
            rgn16_fini(&rgn_rc);

            rgn16_fini(&sel);
            memcpy(&sel, &rgn_union, sizeof(rgn16_t));
        }
    }

    /* Verify the selection corresponds to the control's style. */
    switch(grid->style & GRID_GS_SELMASK) {
        case MC_GS_NOSEL:
            if(sel.n > 0)
                goto err_invalid_sel;
            break;

        case MC_GS_SINGLESEL:
            if(sel.n != 0  &&
               (sel.s.n != 1 || sel.s.rc.x0+1 != sel.s.rc.x1 ||
                                sel.s.rc.y0+1 != sel.s.rc.y1)) {
                goto err_invalid_sel;
            }
            break;

        case MC_GS_RECTSEL:
            if(sel.n > 2)
                goto err_invalid_sel;
            break;

        case MC_GS_COMPLEXSEL:
            /* Noop. Any selection is ok. */
            break;
    }

    if(MC_ERR(grid_install_selection(grid, &sel) != 0)) {
        MC_TRACE("grid_set_selection: grid_install_selection() failed.");
        return -1;
    }

    return 0;

err_invalid_sel:
    MC_TRACE("grid_set_selection: Request selection refused due control style.");
    rgn16_fini(&sel);
    SetLastError(ERROR_INVALID_PARAMETER);
    return -1;
}

static UINT
grid_get_selection(grid_t* grid, MC_GSELECTION* gsel)
{
    static const rgn16_rect_t empty_rc = { 0, 0, 0, 0 };
    const rgn16_rect_t* extents;
    const rgn16_rect_t* vec;
    WORD n;
    rgn16_t* rgn = &grid->selection;

    switch(rgn->n) {
        case 0:     extents = &empty_rc;      vec = NULL;           n = 0;          break;
        case 1:     extents = &rgn->s.rc;     vec = &rgn->s.rc;     n = 1;          break;
        default:    extents = &rgn->c.vec[0]; vec = &rgn->c.vec[1]; n = rgn->n - 1; break;
    }

    if(gsel == NULL)
        return n;

    gsel->rcExtents.wColumnFrom = extents->x0;
    gsel->rcExtents.wRowFrom = extents->y0;
    gsel->rcExtents.wColumnTo = extents->x1;
    gsel->rcExtents.wRowTo = extents->y1;

    if(gsel->uDataCount == (UINT) -1) {
        gsel->uDataCount = n;
        gsel->rcData = (MC_GRECT*) vec;
    } else {
        gsel->uDataCount = MC_MIN(gsel->uDataCount, n);
        if(gsel->uDataCount > 0)
            memcpy(gsel->rcData, vec, gsel->uDataCount * sizeof(rgn16_rect_t));
    }

    return n;
}

static void
grid_change_focus(grid_t* grid, BOOL setfocus)
{
    if(!grid->no_redraw  &&  (grid->style & MC_GS_FOCUSEDCELL)  &&
       grid->col_count > 0  &&  grid->row_count > 0)
        grid_invalidate_cell(grid, grid->focused_col, grid->focused_row, TRUE);

    if(!grid->no_redraw)
        grid_invalidate_selection(grid);

    grid->focus = setfocus;
    mc_send_notify(grid->notify_win, grid->win,
                   (setfocus ? NM_SETFOCUS : NM_KILLFOCUS));
}

static BOOL
grid_set_cursor(grid_t* grid)
{
    MC_GHITTESTINFO info;

    GetCursorPos(&info.pt);
    ScreenToClient(grid->win, &info.pt);

    grid_hit_test(grid, &info);

    if(info.flags & (MC_GHT_ONCOLUMNDIVIDER | MC_GHT_ONCOLUMNDIVOPEN |
                     MC_GHT_ONROWDIVIDER | MC_GHT_ONROWDIVOPEN)) {
        int cur_id;

        if(info.flags & MC_GHT_ONCOLUMNDIVIDER)
            cur_id = CURSOR_DIVIDER_H;
        else if(info.flags & MC_GHT_ONCOLUMNDIVOPEN)
            cur_id = CURSOR_DIVOPEN_H;
        else if(info.flags & MC_GHT_ONROWDIVIDER)
            cur_id = CURSOR_DIVIDER_V;
        else if(info.flags & MC_GHT_ONROWDIVOPEN)
            cur_id = CURSOR_DIVOPEN_V;

        SetCursor(grid_cursor[cur_id].cur);
        return TRUE;
    }

    return FALSE;
}

static int
grid_set_geometry(grid_t* grid, MC_GGEOMETRY* geom, BOOL invalidate)
{
    GRID_TRACE("grid_set_geometry(%p, %p, %d)", grid, geom, (int)invalidate);

    /* Any change in the table end label editing. */
    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

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
    int old_width;
    MC_NMGCOLROWSIZECHANGE notif;

    GRID_TRACE("grid_set_col_width(%p, %hu, %hu)", grid, col, width);

    if(MC_ERR(col >= grid->col_count)) {
        MC_TRACE("grid_set_col_width: column %hu our of range.", col);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(grid->col_widths == NULL) {
        if(width == GRID_DEFAULT_SIZE)
            return 0;

        if(MC_ERR(grid_realloc_col_widths(grid, 0, grid->col_count, FALSE) != 0)) {
            MC_TRACE("grid_set_col_width: grid_realloc_col_widths() failed.");
            return -1;
        }
    }

    old_width = grid->col_widths[col];
    if(width == old_width)
        return 0;

    /* Fire notification MC_GN_COLUMNWIDTHCHANGING */
    notif.hdr.hwndFrom = grid->win;
    notif.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    notif.hdr.code = MC_GN_COLUMNWIDTHCHANGING;
    notif.wColumnOrRow = col;
    notif.wWidthOrHeight = width;
    if(MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif)) {
        /* Application suppresses the default processing */
        GRID_TRACE("grid_set_col_width: "
                   "MC_GN_COLUMNWIDTHCHANGING suppresses the change.");
        return -1;
    }

    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    grid->col_widths[col] = width;

    if(!grid->no_redraw) {
        RECT rect;
        int x0, x1;

        GetClientRect(grid->win, &rect);

        x0 = grid_col_x(grid, col);
        x1 = x0 + MC_MIN(old_width, width);

        rect.left = x1;
        ScrollWindowEx(grid->win, width - old_width, 0, &rect, &rect,
                       NULL, NULL, SW_INVALIDATE | SW_ERASE);

        rect.left = x0;
        rect.right = x1;
        InvalidateRect(grid->win, &rect, TRUE);
    }

    grid_setup_scrollbars(grid, TRUE);

    /* Fire notification MC_GN_COLUMNWIDTHCHANGED */
    notif.hdr.code = MC_GN_COLUMNWIDTHCHANGED;
    MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif);

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
    int old_height;
    MC_NMGCOLROWSIZECHANGE notif;

    GRID_TRACE("grid_set_row_height(%p, %hu, %hu)", grid, row, height);

    if(MC_ERR(row >= grid->row_count)) {
        MC_TRACE("grid_set_row_height: row %hu our of range.", row);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(grid->row_heights == NULL) {
        if(height == GRID_DEFAULT_SIZE)
            return 0;

        if(MC_ERR(grid_realloc_row_heights(grid, 0, grid->row_count, FALSE) != 0)) {
            MC_TRACE("grid_set_row_height: grid_realloc_row_heights() failed.");
            return -1;
        }
    }

    old_height = grid->row_heights[row];
    if(height == old_height)
        return 0;

    /* Fire notification MC_GN_ROWHEIGHTCHANGING */
    notif.hdr.hwndFrom = grid->win;
    notif.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    notif.hdr.code = MC_GN_ROWHEIGHTCHANGING;
    notif.wColumnOrRow = row;
    notif.wWidthOrHeight = height;
    if(MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif)) {
        /* Application suppresses the default processing */
        GRID_TRACE("grid_set_row_height: "
                   "MC_GN_ROWHEIGHTCHANGING suppresses the change.");
        return -1;
    }

    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    grid->row_heights[row] = height;

    if(!grid->no_redraw) {
        RECT rect;
        int y0, y1;

        GetClientRect(grid->win, &rect);

        y0 = grid_row_y(grid, row);
        y1 = y0 + MC_MIN(old_height, height);

        rect.top = y1;
        ScrollWindowEx(grid->win, 0, height - old_height, &rect, &rect,
                       NULL, NULL, SW_INVALIDATE | SW_ERASE);

        rect.top = y0;
        rect.bottom = y1;
        InvalidateRect(grid->win, &rect, TRUE);
    }

    grid_setup_scrollbars(grid, TRUE);

    /* Fire notification MC_GN_ROWHEIGHTCHANGED */
    notif.hdr.code = MC_GN_ROWHEIGHTCHANGED;
    MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif);

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

static void
grid_labeledit_callback(void* data, const TCHAR* text, BOOL save)
{
    grid_t* grid = (grid_t*) data;
    WORD col = grid->focused_col;
    WORD row = grid->focused_row;
    MC_NMGDISPINFO dispinfo;
    MC_NMGDISPINFO dispinfo2;
    table_cell_t* cell;
    void* converted_text;
    BOOL parent_maintains_text;

    GRID_TRACE("grid_labeledit_callback(%p, %S, %s)",
               grid, text, (save ? "save" : "cancel"));

    cell = (grid->table != NULL ? table_cell(grid->table, col, row) : NULL);
    parent_maintains_text = (cell == NULL  ||  cell->text == MC_LPSTR_TEXTCALLBACK);

    if(grid->unicode_notifications == MC_IS_UNICODE  ||  text == NULL)
        converted_text = (TCHAR*) text;
    else
        converted_text = mc_str(text, MC_STRT, (grid->unicode_notifications ? MC_STRW : MC_STRA));

    if(converted_text == NULL)
        save = FALSE;

    grid->labeledit_considering = FALSE;
    grid->labeledit_started = FALSE;

    /* Setup MC_NMGDISPINFO for MC_GN_ENDLABELEDIT. */
    dispinfo.hdr.hwndFrom = grid->win;
    dispinfo.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    dispinfo.hdr.code = (grid->unicode_notifications ? MC_GN_ENDLABELEDITW : MC_GN_ENDLABELEDITA);
    dispinfo.wColumn = col;
    dispinfo.wRow = row;
    dispinfo.cell.fMask = MC_TCMF_TEXT;
    dispinfo.cell.lParam = (cell != NULL ? cell->lp : 0);
    dispinfo.cell.pszText = converted_text;

    /* Remember copy of MC_NMGDISPINFO for MC_GN_SETDISPINFO below.
     * This prevents issues if parent changes contents of original dispinfo. */
    if(save  &&  parent_maintains_text) {
        dispinfo2.hdr.code = (grid->unicode_notifications ? MC_GN_SETDISPINFOW : MC_GN_SETDISPINFOA);
        memcpy(&dispinfo2, &dispinfo, sizeof(MC_NMGDISPINFO));
    }

    /* Fire MC_GN_ENDLABELEDIT. */
    if(MC_SEND(grid->notify_win, WM_NOTIFY, dispinfo.hdr.idFrom, &dispinfo) == 0) {
        GRID_TRACE("grid_labeledit_callback: MC_GN_ENDLABELEDIT suppresses "
                   "the text change.");
        save = FALSE;
    }

    /* Store the new text. */
    if(save) {
        if(parent_maintains_text) {
            MC_SEND(grid->notify_win, WM_NOTIFY, dispinfo2.hdr.idFrom, &dispinfo2);
        } else {
            if(MC_ERR(table_set_cell_data(grid->table, col, row, &dispinfo.cell,
                        grid->unicode_notifications) != 0)) {
                MC_TRACE("grid_labeledit_callback: table_set_cell_data() failed.");
            }
        }
    }

    if(converted_text != text  &&  converted_text != NULL)
        free(converted_text);
}

static HWND
grid_start_label_edit(grid_t* grid, WORD col, WORD row)
{
    RECT rect;
    MC_NMGDISPINFO dispinfo;
    table_cell_t* cell;
    grid_dispinfo_t di;
    HWND edit_win;

    GRID_TRACE("grid_start_label_edit(%p, %d, %d)", grid, col, row);

    if(col == COL_INVALID || row == ROW_INVALID)
        return NULL;

    grid->labeledit_considering = FALSE;
    grid_set_focused_cell(grid, col, row);

    if(grid->table != NULL)
        cell = table_cell(grid->table, col, row);
    else
        cell = NULL;

    grid_get_dispinfo(grid, col, row, cell, &di, MC_TCMF_TEXT | MC_TCMF_PARAM | MC_TCMF_FLAGS);
    edit_win = labeledit_start(grid->win, di.text, grid_labeledit_callback, grid);
    if(MC_ERR(edit_win == NULL)) {
        MC_TRACE("grid_start_label_edit: labeledit_start() failed.");
        goto out;
    }

    if(grid->font != NULL)
        MC_SEND(edit_win, WM_SETFONT, grid->font, FALSE);

    /* Set position and size of the edit box. */
    grid_cell_rect(grid, col, row, &rect);
    if(!(grid->style & MC_GS_NOGRIDLINES)) {
        rect.left -= 1;
        rect.top -= 1;
    }
    SetWindowPos(edit_win, NULL, rect.left, rect.top, mc_width(&rect),
                 mc_height(&rect), SWP_NOZORDER);

    /* Fire MC_GN_BEGINLABELEDIT. Note this is after the edit window is already
     * created as the application may want to get it through MC_GM_GETEDITCONTROL
     * and customize it. */
    dispinfo.hdr.hwndFrom = grid->win;
    dispinfo.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    dispinfo.hdr.code = (grid->unicode_notifications ? MC_GN_BEGINLABELEDITW : MC_GN_BEGINLABELEDITA);
    dispinfo.wColumn = col;
    dispinfo.wRow = row;
    dispinfo.cell.fMask = (MC_TCMF_TEXT | MC_TCMF_PARAM | MC_TCMF_FLAGS);
    if(grid->unicode_notifications == MC_IS_UNICODE)
        dispinfo.cell.pszText = di.text;
    else
        dispinfo.cell.pszText = mc_str(di.text, MC_STRT, (grid->unicode_notifications ? MC_STRW : MC_STRA));
    dispinfo.cell.lParam = di.lp;
    dispinfo.cell.dwFlags = di.flags;
    if(MC_SEND(grid->notify_win, WM_NOTIFY, dispinfo.hdr.idFrom, &dispinfo) != 0) {
        GRID_TRACE("grid_start_label_edit: MC_GN_BEGINLABELEDIT suppresses "
                   "the label editing.");
        labeledit_end(grid->win, FALSE);
        goto out;
    }

    /* Select whole edit box contents. We do this after MC_GN_BEGINLABELEDIT as
     * app. could change edit box contents. */
    MC_SEND(edit_win, EM_SETSEL, 0, -1);

    /* Show the edit box. */
    ShowWindow(edit_win, SW_SHOW);
    SetFocus(edit_win);

    grid->labeledit_started = TRUE;

out:
    if(dispinfo.cell.pszText != di.text  &&  dispinfo.cell.pszText != NULL)
        free(dispinfo.cell.pszText);
    grid_free_dispinfo(grid, cell, &di);

    return edit_win;
}

static void
grid_end_label_edit(grid_t* grid, BOOL cancel)
{
    GRID_TRACE("grid_end_label_edit(%p, %s)", grid, (cancel ? "cancel" : "save"));

    MC_ASSERT(!grid->labeledit_considering);
    MC_ASSERT(grid->labeledit_started);

    labeledit_end(grid->win, !cancel);
    grid->labeledit_started = FALSE;
}

static int
grid_reset_selection(grid_t* grid)
{
    rgn16_t sel;

    rgn16_init(&sel);
    if(MC_ERR(grid_install_selection(grid, &sel) != 0)) {
        MC_TRACE("grid_reset_selection: grid_install_selection() failed.");
        return -1;
    }

    return 0;
}

static int
grid_select_cell(grid_t* grid, WORD col, WORD row)
{
    rgn16_t sel;

    rgn16_init_with_xy(&sel, col, row);
    if(MC_ERR(grid_install_selection(grid, &sel) != 0)) {
        MC_TRACE("grid_select_cell: grid_install_selection() failed.");
        return -1;
    }

    return 0;
}

static int
grid_select_rect(grid_t* grid, WORD col0, WORD row0, WORD col1, WORD row1)
{
    rgn16_rect_t sel_rect = { col0, row0, col1, row1 };
    rgn16_t sel;

    rgn16_init_with_rect(&sel, &sel_rect);
    if(MC_ERR(grid_install_selection(grid, &sel) != 0)) {
        MC_TRACE("grid_select_rect: grid_install_selection() failed.");
        return -1;
    }

    return 0;
}

static int
grid_select_rect_UNION(grid_t* grid, WORD col0, WORD row0, WORD col1, WORD row1)
{
    rgn16_rect_t sel_rect = { col0, row0, col1, row1 };
    rgn16_t sel;
    rgn16_t sel_union;

    rgn16_init_with_rect(&sel, &sel_rect);

    if(MC_ERR(rgn16_union(&sel_union, &sel, &grid->selection) != 0)) {
        MC_TRACE("grid_select_rect_UNION: rgn16_union() failed.");
        return -1;
    }

    if(MC_ERR(grid_install_selection(grid, &sel_union) != 0)) {
        MC_TRACE("grid_select_rect_UNION: grid_install_selection() failed.");
        return -1;
    }

    return 0;
}

static int
grid_select_rect_XOR(grid_t* grid, WORD col0, WORD row0, WORD col1, WORD row1)
{
    rgn16_rect_t sel_rect = { col0, row0, col1, row1 };
    rgn16_t sel;
    rgn16_t sel_xor;

    rgn16_init_with_rect(&sel, &sel_rect);

    if(MC_ERR(rgn16_xor(&sel_xor, &sel, &grid->selection) != 0)) {
        MC_TRACE("grid_select_rect_XOR: rgn16_xor() failed.");
        return -1;
    }

    if(MC_ERR(grid_install_selection(grid, &sel_xor) != 0)) {
        MC_TRACE("grid_select_rect_XOR: grid_install_selection() failed.");
        return -1;
    }

    return 0;
}

static void
grid_end_sel_drag(grid_t* grid, BOOL cancel)
{
    WORD col_count = grid->col_count;
    WORD row_count = grid->row_count;

    MC_ASSERT(grid->seldrag_considering || grid->seldrag_started);

    /* Apply the selection marquee. */
    if(!cancel  &&  col_count > 0  &&  row_count > 0) {
        if(mousedrag_lock(grid->win)) {
            int drag_start_x, drag_start_y;
            int drag_hotspot_x, drag_hotspot_y;
            int drag_mode;
            int marquee_x0, marquee_y0, marquee_x1, marquee_y1;
            int x, y;
            int gridline_w = (grid->style & MC_GS_NOGRIDLINES) ? 0 : 1;
            WORD col0, row0, col1, row1;
            int err = -1;

            drag_start_x = mousedrag_start_x;
            drag_start_y = mousedrag_start_y;
            drag_hotspot_x = mousedrag_hotspot_x;
            drag_hotspot_y = mousedrag_hotspot_y;
            drag_mode = mousedrag_extra;

            mousedrag_unlock();

            /* Translate marquee into column and row rectangle. */
            marquee_x0 = MC_MIN(drag_start_x, drag_hotspot_x);
            marquee_y0 = MC_MIN(drag_start_y, drag_hotspot_y);
            marquee_x1 = MC_MAX(drag_start_x, drag_hotspot_x);
            marquee_y1 = MC_MAX(drag_start_y, drag_hotspot_y);
            x = grid_header_width(grid);
            for(col0 = 0; col0 < col_count; col0++) {
                x += grid_col_width(grid, col0);
                if(marquee_x0 < x + gridline_w)
                    break;
            }
            for(col1 = col0; col1 < col_count; col1++) {
                if(marquee_x1 < x + gridline_w)
                    break;
                if(col1+1 < col_count)
                    x += grid_col_width(grid, col1+1);
            }
            y = grid_header_height(grid);
            for(row0 = 0; row0 < row_count; row0++) {
                y += grid_row_height(grid, row0);
                if(marquee_y0 < y + gridline_w)
                    break;
            }
            for(row1 = row0; row1 < row_count; row1++) {
                if(marquee_y1 < y + gridline_w)
                    break;
                if(row1+1 < row_count)
                    y += grid_row_height(grid, row1+1);
            }

            GRID_TRACE("grid_end_sel_drag: %d %d %d %d",
                       (int) col0, (int) row0, (int) col1, (int) row1);

            /* Setup the selection. */
            switch(drag_mode) {
                case DRAGSEL_SET:
                    MC_ASSERT((grid->style & GRID_GS_SELMASK) == MC_GS_COMPLEXSEL ||
                              (grid->style & GRID_GS_SELMASK) == MC_GS_RECTSEL);
                    err = grid_select_rect(grid, col0, row0, col1 + 1, row1 + 1);
                    if(MC_ERR(err != 0))
                        MC_TRACE("grid_end_sel_drag: grid_select_rect() failed.");
                    break;

                case DRAGSEL_UNION:
                    MC_ASSERT((grid->style & GRID_GS_SELMASK) == MC_GS_COMPLEXSEL);
                    err = grid_select_rect_UNION(grid, col0, row0, col1 + 1, row1 + 1);
                    if(MC_ERR(err != 0))
                        MC_TRACE("grid_end_sel_drag: grid_select_rect_UNION() failed.");
                    break;

                case DRAGSEL_XOR:
                    MC_ASSERT((grid->style & GRID_GS_SELMASK) == MC_GS_COMPLEXSEL);
                    err = grid_select_rect_XOR(grid, col0, row0, col1 + 1, row1 + 1);
                    if(MC_ERR(err != 0))
                        MC_TRACE("grid_end_sel_drag: grid_select_rect_XOR() failed.");
                    break;
            }

            if(err == 0) {
                /* Remember the origin of the dragging as the selection mark. */
                grid->selmark_col = (drag_start_x < drag_hotspot_x) ? col0 : col1;
                grid->selmark_row = (drag_start_y < drag_hotspot_y) ? row0 : row1;

                /* Remember the target of the dragging as the focused cell. */
                if(grid->style & MC_GS_FOCUSEDCELL) {
                    WORD focused_col = (drag_start_x < drag_hotspot_x) ? col1 : col0;
                    WORD focused_row = (drag_start_y < drag_hotspot_y) ? row1 : row0;

                    grid_set_focused_cell(grid, focused_col, focused_row);
                }
            }
        } else {
            /* Noop: cancel */
        }

        KillTimer(grid->win, (UINT_PTR) grid_autoscroll);
    }

    if(grid->seldrag_started)
        mousedrag_stop(grid->win);
    grid->seldrag_considering = FALSE;
    grid->seldrag_started = FALSE;

    if(grid->mouse_captured)
        ReleaseCapture();
    grid->mouse_captured = FALSE;
    mc_send_notify(grid->notify_win, grid->win, NM_RELEASEDCAPTURE);

    InvalidateRect(grid->win, NULL, TRUE);
}

static void
grid_end_headersize_drag(grid_t* grid, BOOL cancel)
{
    MC_NMGCOLROWSIZECHANGE notif;

    MC_ASSERT(grid->colsizedrag_started  ||  grid->rowsizedrag_started);

    if(cancel) {
        if(grid->colsizedrag_started)
            grid_set_col_width(grid, mousedrag_index, mousedrag_extra);
        else
            grid_set_row_height(grid, mousedrag_index, mousedrag_extra);
    }

    mousedrag_stop(grid->win);
    grid->colsizedrag_started = FALSE;
    grid->rowsizedrag_started = FALSE;

    notif.hdr.hwndFrom = grid->win;
    notif.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
    if(grid->colsizedrag_started) {
        notif.hdr.code = MC_GN_ENDCOLUMNTRACK;
        notif.wColumnOrRow = mousedrag_index;
        notif.wWidthOrHeight = grid_col_width(grid, mousedrag_index);
    } else {
        notif.hdr.code = MC_GN_ENDROWTRACK;
        notif.wColumnOrRow = mousedrag_index;
        notif.wWidthOrHeight = grid_row_height(grid, mousedrag_index);
    }
    MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif);

    if(grid->mouse_captured)
        ReleaseCapture();
    grid->mouse_captured = FALSE;
    mc_send_notify(grid->notify_win, grid->win, NM_RELEASEDCAPTURE);
}

static void
grid_end_any_drag(grid_t* grid, BOOL cancel)
{
    if(grid->seldrag_considering || grid->seldrag_started)
        grid_end_sel_drag(grid, TRUE);
    else if(grid->colsizedrag_started || grid->rowsizedrag_started)
        grid_end_headersize_drag(grid, TRUE);
}

static void
grid_ensure_visible(grid_t* grid, WORD col, WORD row, BOOL partial)
{
    RECT viewport_rect;
    RECT cell_rect;
    int scroll_x = grid->scroll_x;
    int scroll_y = grid->scroll_y;

    GetClientRect(grid->win, &viewport_rect);
    viewport_rect.left = grid_header_width(grid);
    viewport_rect.top = grid_header_height(grid);

    grid_cell_rect(grid, col, row, &cell_rect);

    if(partial  &&  mc_rect_overlaps_rect(&viewport_rect, &cell_rect))
        return;
    if(mc_rect_contains_rect(&viewport_rect, &cell_rect))
        return;

    if(cell_rect.left < viewport_rect.left)
        scroll_x -= viewport_rect.left - cell_rect.left;
    else if(cell_rect.right > viewport_rect.right)
        scroll_x += cell_rect.right - viewport_rect.right;

    if(cell_rect.top < viewport_rect.top)
        scroll_y -= viewport_rect.top - cell_rect.top;
    else if(cell_rect.bottom > viewport_rect.bottom)
        scroll_y += cell_rect.bottom - viewport_rect.bottom;

    grid_scroll_xy(grid, scroll_x, scroll_y);
}

static void
grid_mouse_move(grid_t* grid, int x, int y)
{
    RECT cell_rect;
    WORD hot_col = COL_INVALID;
    WORD hot_row = ROW_INVALID;

    /* Updating selection dragging (marquee). */
    if(grid->seldrag_considering) {
        int drag_x = x - grid->scroll_x;
        int drag_y = y - grid->scroll_y;

        MC_ASSERT(!grid->seldrag_started);

        switch(mousedrag_consider_start(grid->win, drag_x, drag_y)) {
            case MOUSEDRAG_STARTED:
                grid->seldrag_considering = FALSE;
                grid->seldrag_started = TRUE;
                SetCapture(grid->win);
                grid->mouse_captured = TRUE;
                SetTimer(grid->win, (UINT_PTR) grid_autoscroll, 50, NULL);
                break;

            case MOUSEDRAG_CONSIDERING:
                /* noop */
                break;

            case MOUSEDRAG_CANCELED:
                grid->seldrag_considering = FALSE;
                break;
        }
    }

    if(grid->seldrag_started) {
        MC_ASSERT(!grid->seldrag_considering);

        /* Remember the moving corner of the marquee. */
        mousedrag_hotspot_x = x + grid->scroll_x;
        mousedrag_hotspot_y = y + grid->scroll_y;

        InvalidateRect(grid->win, NULL, TRUE);
        return;
    }

    /* Resizing column */
    if(grid->colsizedrag_started) {
        int right;

        grid_cell_rect(grid, mousedrag_index, MC_TABLE_HEADER, &cell_rect);
        right = MC_MAX(cell_rect.left, x - mousedrag_hotspot_x);
        if(right != cell_rect.right)
            grid_set_col_width(grid, mousedrag_index, right - cell_rect.left);

        return;
    }

    /* Resizing row */
    if(grid->rowsizedrag_started) {
        int bottom;

        grid_cell_rect(grid, MC_TABLE_HEADER, mousedrag_index, &cell_rect);
        bottom = MC_MAX(cell_rect.top, y - mousedrag_hotspot_y);
        if(bottom != cell_rect.bottom)
            grid_set_row_height(grid, mousedrag_index, bottom - cell_rect.top);

        return;
    }

    /* Hot tracking. */
    if(grid->theme_listview != NULL  &&  grid->theme_listitem_defined) {
        /* We paint hot item differently only with themes. */
        MC_GHITTESTINFO info;

        info.pt.x = x;
        info.pt.y = y;
        grid_hit_test(grid, &info);

        if(info.flags & MC_GHT_ONNORMALCELL) {
            hot_col = info.wColumn;
            hot_row = info.wRow;
        }
    }
    if(hot_col != grid->hot_col  ||  hot_row != grid->hot_row) {
        if(!grid->no_redraw) {
            if(grid->hot_col < grid->col_count  &&  grid->hot_row < grid->row_count)
                grid_invalidate_cell(grid, grid->hot_col, grid->hot_row, FALSE);

            if(hot_col < grid->col_count  &&  hot_row < grid->row_count)
                grid_invalidate_cell(grid, hot_col, hot_row, FALSE);
        }

        hot_col = grid->hot_col;
        hot_row = grid->hot_row;
    }

    /* Ask for WM_LEAVE */
    if(!grid->tracking_leave) {
        mc_track_mouse(grid->win, TME_LEAVE);
        grid->tracking_leave = TRUE;
    }
}

static void
grid_mouse_leave(grid_t* grid)
{
    if(grid->hot_col < grid->col_count  &&  grid->hot_row < grid->row_count)
        grid_invalidate_cell(grid, grid->hot_col, grid->hot_row, FALSE);

    grid->hot_col = COL_INVALID;
    grid->hot_row = ROW_INVALID;
    grid->tracking_leave = FALSE;
}

static int
grid_toggle_cell_selection(grid_t* grid, WORD col, WORD row)
{
    rgn16_t sel;

    if((grid->style & GRID_GS_SELMASK) == MC_GS_COMPLEXSEL) {
        rgn16_t tmp;
        int err;

        rgn16_init_with_xy(&tmp, col, row);
        err = rgn16_xor(&sel, &grid->selection, &tmp);
        rgn16_fini(&tmp);
        if(MC_ERR(err != 0)) {
            MC_TRACE("grid_toggle_cell_selection: rgn16_xor() failed.");
            return -1;
        }
    } else {
        /* In selection modes simpler then MC_GS_COMPLEXSEL we make the toggle
         * only work only within a single cell. */
        const rgn16_rect_t* ext = rgn16_extents(&grid->selection);
        if(ext != NULL  &&  col == ext->x0  &&  row == ext->y0  &&
                            col+1 == ext->x1  &&  row+1 == ext->y1)
            rgn16_init(&sel);
        else
            rgn16_init_with_xy(&sel, col, row);
    }

    if(MC_ERR(grid_install_selection(grid, &sel) != 0)) {
        MC_TRACE("grid_toggle_cell_selection: grid_install_selection() failed.");
        return -1;
    }

    return 0;
}

static void
grid_left_button_down(grid_t* grid, int x, int y)
{
    static const DWORD col_track_mask = MC_GHT_ONCOLUMNDIVIDER | MC_GHT_ONCOLUMNDIVOPEN;
    static const DWORD row_track_mask = MC_GHT_ONROWDIVIDER | MC_GHT_ONROWDIVOPEN;

    MC_GHITTESTINFO info;
    RECT cell_rect;
    BOOL control_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
    BOOL shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000);
    DWORD sel_mode;

    MC_ASSERT(!grid->mouse_captured);
    MC_ASSERT(!grid->colsizedrag_started);
    MC_ASSERT(!grid->rowsizedrag_started);

    if(mc_send_notify(grid->notify_win, grid->win, NM_CLICK)) {
        /* Application suppresses the default processing of the message */
        return;
    }

    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    info.pt.x = x;
    info.pt.y = y;
    grid_hit_test_ex(grid, &info, &cell_rect);

    /* Column/row divider? Consider dragging mode to resize the column/row. */
    if(info.flags & (col_track_mask | row_track_mask)) {
        MC_NMGCOLROWSIZECHANGE notif;

        /* Fire MC_GN_BEGINCOLUMNTRACK or MC_GN_BEGINROWTRACK */
        notif.hdr.hwndFrom = grid->win;
        notif.hdr.idFrom = GetWindowLong(grid->win, GWL_ID);
        if(info.flags & col_track_mask) {
            notif.hdr.code = MC_GN_BEGINCOLUMNTRACK;
            notif.wColumnOrRow = info.wColumn;
            notif.wWidthOrHeight = grid_col_width(grid, info.wColumn);
        } else {
            notif.hdr.code = MC_GN_BEGINROWTRACK;
            notif.wColumnOrRow = info.wRow;
            notif.wWidthOrHeight = grid_row_height(grid, info.wRow);
        }
        if(MC_SEND(grid->notify_win, WM_NOTIFY, notif.hdr.idFrom, &notif) != 0) {
            /* Application suppresses the dragging mode. */
            return;
        }

        /* Start the dragging. */
        if(mousedrag_start(grid->win, x, y) == MOUSEDRAG_STARTED) {
            if(info.flags & col_track_mask) {
                grid->colsizedrag_started = TRUE;
                mousedrag_index = info.wColumn;
                mousedrag_extra = grid_col_width(grid, info.wColumn);
                mousedrag_hotspot_x = x - cell_rect.right;
            } else {
                grid->rowsizedrag_started = TRUE;
                mousedrag_index = info.wRow;
                mousedrag_extra = grid_row_height(grid, info.wRow);
                mousedrag_hotspot_y = y - cell_rect.bottom;
            }
            SetCapture(grid->win);
            grid->mouse_captured = TRUE;
        }

        return;
    }

    /* If clicking into focused cell, we start considering about its editing. */
    if(grid->style & MC_GS_EDITLABELS) {
        if(info.wColumn == grid->focused_col && info.wRow == grid->focused_row) {
            MC_ASSERT(!grid->labeledit_started);
            MC_ASSERT(!grid->labeledit_considering);
            grid->labeledit_considering = TRUE;
            GRID_TRACE("grid_left_button_down: Starting consideration of label "
                       "edit for cell %d %d", grid->focused_col, grid->focused_row);
        }
    }

    /* Update selection. */
    sel_mode = (grid->style & GRID_GS_SELMASK);
    switch(sel_mode) {
        case MC_GS_COMPLEXSEL:
        case MC_GS_RECTSEL:
            /* In multi-select modes, we consider starting a mouse dragging
             * operation, assuming we are not on a column/row header. */
            if(!(info.flags & MC_GHT_ONHEADER)) {
                int start_x, start_y;
                UINT_PTR extra;

                /* We remember coordinates relatively, without the scrolling
                 * offsets, because we may need to scroll during the dragging. */
                start_x = x + grid->scroll_x;
                start_y = y + grid->scroll_y;

                /* We remember the mode how the rectangle shall be applied,
                 * given the special keys pressed. */
                if(sel_mode == MC_GS_COMPLEXSEL  &&  control_pressed)
                    extra = DRAGSEL_XOR;
                else if(sel_mode == MC_GS_COMPLEXSEL  &&  shift_pressed)
                    extra = DRAGSEL_UNION;
                else
                    extra = DRAGSEL_SET;

                if(mousedrag_set_candidate(grid->win, start_x, start_y, start_x, start_y, 0, extra)) {
                    grid->seldrag_considering = TRUE;
                    InvalidateRect(grid->win, NULL, TRUE);
                }
                break;
            } else {
                /* Fall through into MC_GS_NOSEL (indirectly through MC_GS_NOSEL). */
            }

        case MC_GS_SINGLESEL:
            /* Normal click sets the selection to the given cell. */
            if(info.flags & MC_GHT_ONNORMALCELL) {
                if(grid_select_cell(grid, info.wColumn, info.wRow) == 0) {
                    grid->selmark_col = info.wColumn;
                    grid->selmark_row = info.wRow;

                    if(grid->style & MC_GS_FOCUSEDCELL)
                        grid_set_focused_cell(grid, info.wColumn, info.wRow);
                } else {
                    MC_TRACE("grid_left_button_down: grid_select_cell() failed.");
                }
                break;
            } else {
                /* Fall through into MC_GS_NOSEL. */
            }

        case MC_GS_NOSEL:
            /* No selection. */
            grid_reset_selection(grid);
            grid->selmark_col = COL_INVALID;
            grid->selmark_row = ROW_INVALID;
            break;
    }
}

static void
grid_left_button_up(grid_t* grid, int x, int y)
{
    if(grid->seldrag_considering  ||  grid->seldrag_started)
        grid_end_sel_drag(grid, FALSE);
    else if(grid->colsizedrag_started  ||  grid->rowsizedrag_started)
        grid_end_headersize_drag(grid, FALSE);

    /* Consider label editing. */
    if(grid->labeledit_considering) {
        MC_GHITTESTINFO info;

        info.pt.x = x;
        info.pt.y = y;
        grid_hit_test(grid, &info);

        if(info.wColumn == grid->focused_col  &&  info.wRow == grid->focused_row) {
            /* Note we delay the start after double-click timeout to give
             * WM_LBUTTONDBLCLK a chance. If the WM_LBUTTONDBLCLK comes in the
             * mean time we cancel the timer. */
            GRID_TRACE("grid_left_button_up: Starting timer for label edit.");
            SetTimer(grid->win, (UINT_PTR) grid_start_label_edit,
                     GetDoubleClickTime(), NULL);
        }

        grid->labeledit_considering = FALSE;
    }
}

static void
grid_left_button_dblclick(grid_t* grid, int x, int y)
{
    /* If considering about label editing, stop it. */
    if(grid->labeledit_considering) {
        GRID_TRACE("grid_left_button_dblclick: Kill timer for label edit.");
        KillTimer(grid->win, (UINT_PTR) grid_start_label_edit);
        grid->labeledit_considering = FALSE;
    }

    mc_send_notify(grid->notify_win, grid->win, NM_DBLCLK);
}

static void
grid_right_button(grid_t* grid, int x, int y, BOOL dblclick)
{
    UINT notify_code = (dblclick ? NM_RDBLCLK : NM_RCLICK);
    POINT pt;

    if(mc_send_notify(grid->notify_win, grid->win, notify_code) != 0) {
        /* Application suppresses the default processing of the message */
        return;
    }

    /* Send WM_CONTEXTMENU */
    pt.x = x;
    pt.y = y;
    ClientToScreen(grid->win, &pt);
    MC_SEND(grid->notify_win, WM_CONTEXTMENU, grid->win, MAKELPARAM(pt.x, pt.y));
}

static WORD
grid_row_pgup_or_pgdn(grid_t* grid, WORD row, BOOL is_down)
{
    SCROLLINFO si;
    RECT rect;
    int y;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    GetScrollInfo(grid->win, SB_VERT, &si);

    grid_cell_rect(grid, 0, row, &rect);

    if(is_down) {
        y = rect.bottom;
        do {
            y += grid_row_height(grid, row);
            if(row >= grid->row_count)
                break;
            row++;
        } while(y < rect.top + (int)si.nPage);
    } else {
        y = rect.top;
        do {
            if(row == 0)
                break;
            row--;
            y -= grid_row_height(grid, row);
        } while(y > rect.bottom - (int)si.nPage);
    }

    return row;
}

static void
grid_move_focus(grid_t* grid, WORD col, WORD row)
{
    if(grid->col_count == 0  ||  grid->row_count == 0) {
        /* Empty table or no attached table. */
        MC_ASSERT(grid->focused_col == 0);
        MC_ASSERT(grid->focused_row == 0);
        return;
    }

    if(grid_set_focused_cell(grid, col, row) == 0)
        grid_ensure_visible(grid, col, row, FALSE);
}

static void
grid_key_down(grid_t* grid, int key)
{
    WORD old_focused_col = grid->focused_col;
    WORD old_focused_row = grid->focused_row;
    BOOL control_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
    BOOL shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000);
    int err;

    /* On <ESC>, cancel any dragging. */
    if(key == VK_ESCAPE) {
        grid_end_any_drag(grid, TRUE);
        return;
    }

    /* Swap meaning of VK_LEFT and VK_RIGHT if having right-to-left layout. */
    if(grid->rtl) {
        if(key == VK_LEFT)
            key = VK_RIGHT;
        else if(key == VK_RIGHT)
            key = VK_LEFT;
    }

    /* Move focused cell or scroll */
    switch(key) {
        case VK_LEFT:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                if(grid->focused_col > 0)
                    grid_move_focus(grid, grid->focused_col-1, grid->focused_row);
            } else {
                grid_scroll(grid, FALSE, SB_LINELEFT, 1);
            }
            break;

        case VK_RIGHT:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                if(grid->focused_col < grid->col_count-1)
                    grid_move_focus(grid, grid->focused_col+1, grid->focused_row);
            } else {
                grid_scroll(grid, FALSE, SB_LINERIGHT, 1);
            }
            break;

        case VK_UP:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                if(grid->focused_row > 0)
                    grid_move_focus(grid, grid->focused_col, grid->focused_row-1);
            } else {
                grid_scroll(grid, TRUE, SB_LINEUP, 1);
            }
            break;

        case VK_DOWN:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                if(grid->focused_row < grid->row_count-1)
                    grid_move_focus(grid, grid->focused_col, grid->focused_row+1);
            } else {
                grid_scroll(grid, TRUE, SB_LINEDOWN, 1);
            }
            break;

        case VK_HOME:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                if(control_pressed)
                    grid_move_focus(grid, grid->focused_col, 0);
                else
                    grid_move_focus(grid, 0, grid->focused_row);
            } else {
                grid_scroll(grid, control_pressed, SB_TOP, 1);
            }
            break;

        case VK_END:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                if(control_pressed)
                    grid_move_focus(grid, grid->focused_col, grid->row_count-1);
                else
                    grid_move_focus(grid, grid->col_count-1, grid->focused_row);
            } else {
                grid_scroll(grid, control_pressed, SB_BOTTOM, 1);
            }
            break;

        case VK_PRIOR:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                WORD row = grid_row_pgup_or_pgdn(grid, grid->focused_row, FALSE);
                grid_scroll(grid, TRUE, SB_PAGEUP, 1);
                grid_move_focus(grid, grid->focused_col, row);
            } else {
                grid_scroll(grid, TRUE, SB_PAGEUP, 1);
            }
            break;

        case VK_NEXT:
            if(grid->style & MC_GS_FOCUSEDCELL) {
                WORD row = grid_row_pgup_or_pgdn(grid, grid->focused_row, TRUE);
                grid_scroll(grid, TRUE, SB_PAGEDOWN, 1);
                grid_move_focus(grid, grid->focused_col, row);
            } else {
                grid_scroll(grid, TRUE, SB_PAGEDOWN, 1);
            }
            break;

        case VK_RETURN:
            if(grid->style & MC_GS_EDITLABELS) {
                grid_start_label_edit(grid, grid->focused_col, grid->focused_row);
                return;
            }
            break;
    }

    if((grid->style & GRID_GS_SELMASK) != MC_GS_NOSEL) {
        /* If the focused cell changed, we (likely) may also need to change
         * current selection. */
        if(!control_pressed  &&
           (grid->focused_col != old_focused_col || grid->focused_row != old_focused_row))
        {
            if(shift_pressed  &&  grid->selmark_col < grid->col_count  &&
                                  grid->selmark_row < grid->row_count) {
                err = grid_select_rect(grid,
                                MC_MIN(grid->selmark_col, grid->focused_col),
                                MC_MIN(grid->selmark_row, grid->focused_row),
                                MC_MAX(grid->selmark_col, grid->focused_col) + 1,
                                MC_MAX(grid->selmark_row, grid->focused_row) + 1);
                if(MC_ERR(err != 0))
                    MC_TRACE("grid_key_down: grid_select_rect() failed.");
            } else {
                err = grid_select_cell(grid, grid->focused_col, grid->focused_row);
                if(err == 0) {
                    grid->selmark_col = grid->focused_col;
                    grid->selmark_row = grid->focused_row;
                } else {
                    MC_TRACE("grid_key_down: grid_select_cell() failed.");
                }
            }
        }

        /* <CTRL>+<SPACE> toggles selection state of focused cell.
         * If (!MC_GS_COMPLEXSEL), the old selection is 1st reset if it involves
         * other any other cell. */
        if(control_pressed  &&  key == VK_SPACE) {
            err = grid_toggle_cell_selection(grid, grid->focused_col, grid->focused_row);
            if(err == 0) {
                MC_TRACE("grid_key_down: grid_toggle_cell_selection() failed.");
            } else {
                grid->selmark_col = grid->focused_col;
                grid->selmark_row = grid->focused_row;
            }
        }
    }
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

    grid_end_any_drag(grid, TRUE);
    if(grid->labeledit_started)
        grid_end_label_edit(grid, TRUE);

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

    grid->focused_col = 0;
    grid->focused_row = 0;

    rgn16_clear(&grid->selection);
    grid->selmark_col = COL_INVALID;
    grid->selmark_row = ROW_INVALID;

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

    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

    if(grid->table != NULL) {
        if(MC_ERR(table_resize(grid->table, col_count, row_count) != 0)) {
            MC_TRACE("grid_resize_table: table_resize() failed.");
            return -1;
        }
    } else {
        if(grid->col_widths != NULL)
            grid_realloc_col_widths(grid, grid->col_count, col_count, TRUE);
        if(grid->row_heights != NULL)
            grid_realloc_row_heights(grid, grid->row_count, row_count, TRUE);

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
    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

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

    if(grid->labeledit_started)
        grid_end_label_edit(grid, FALSE);

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

    grid->theme_listitem_defined = (grid->theme_listview != NULL  &&
                    mcIsThemePartDefined(grid->theme_listview, LVP_LISTITEM, 0));
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

    if((ss->styleNew & MC_GS_OWNERDATA) != (ss->styleOld & MC_GS_OWNERDATA)) {
        grid_set_table(grid, NULL);
    }

    if((ss->styleNew & GRID_GS_SELMASK) != (ss->styleOld & GRID_GS_SELMASK)) {
        grid_reset_selection(grid);
        if(grid->seldrag_considering || grid->seldrag_started)
            grid_end_sel_drag(grid, TRUE);
    }

    if((ss->styleNew & MC_GS_RESIZABLECOLUMNS) != (ss->styleOld & MC_GS_RESIZABLECOLUMNS)) {
        if(grid->colsizedrag_started)
            grid_end_headersize_drag(grid, TRUE);
    }

    if((ss->styleNew & MC_GS_RESIZABLEROWS) != (ss->styleOld & MC_GS_RESIZABLEROWS)) {
        if(grid->rowsizedrag_started)
            grid_end_headersize_drag(grid, TRUE);
    }

    if((ss->styleNew & MC_GS_EDITLABELS) != (ss->styleOld & MC_GS_EDITLABELS)) {
        grid->labeledit_considering = FALSE;
        if(grid->labeledit_started)
            grid_end_label_edit(grid, TRUE);
    }

    if(!grid->no_redraw)
        InvalidateRect(grid->win, NULL, TRUE);
}

static void
grid_exstyle_changed(grid_t* grid, STYLESTRUCT* ss)
{
    grid->rtl = mc_is_rtl_exstyle(ss->styleNew);

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
    grid->rtl = mc_is_rtl_exstyle(cs->dwExStyle);

    rgn16_init(&grid->selection);

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
    rgn16_fini(&grid->selection);
    free(grid);
}

static LRESULT CALLBACK
grid_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    grid_t* grid = (grid_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            /* Note that the selection marquee dragging implies double-buffering
             * because during that the control needs to be repainted quite often.
             */
            return generic_paint(win, grid->no_redraw,
                        ((grid->style & MC_GS_DOUBLEBUFFER) || grid->seldrag_started),
                        grid_paint, grid);

        case WM_PRINTCLIENT:
            return generic_printclient(win, (HDC) wp, grid_paint, grid);

        case WM_NCPAINT:
            return generic_ncpaint(win, grid->theme_listview, (HRGN) wp);

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

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

        case MC_GM_HITTEST:
            return (LRESULT) grid_hit_test(grid, (MC_GHITTESTINFO*) lp);

        case MC_GM_GETCELLRECT:
        {
            WORD col = LOWORD(wp);
            WORD row = HIWORD(wp);
            if(MC_ERR(col >= grid->col_count || row >= grid->row_count)) {
                MC_TRACE("MC_GM_GETCELLRECT: Column or row index out of range "
                         "(size: %ux%u; requested [%u,%u])",
                         (unsigned) grid->col_count, (unsigned) grid->row_count,
                         (unsigned) col, (unsigned) row);
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            grid_cell_rect(grid, col, row, (RECT*) lp);
            return TRUE;
        }

        case MC_GM_ENSUREVISIBLE:
        {
            WORD col = LOWORD(wp);
            WORD row = HIWORD(wp);
            if(MC_ERR(col >= grid->col_count || row >= grid->row_count)) {
                MC_TRACE("MC_GM_ENSUREVISIBLE: Column or row index out of range "
                         "(size: %ux%u; requested [%u,%u])",
                         (unsigned) grid->col_count, (unsigned) grid->row_count,
                         (unsigned) col, (unsigned) row);
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            grid_ensure_visible(grid, col, row, lp);
            return TRUE;
        }

        case MC_GM_SETFOCUSEDCELL:
            return (grid_set_focused_cell(grid, LOWORD(wp), HIWORD(wp)) == 0 ? TRUE : FALSE);

        case MC_GM_GETFOCUSEDCELL:
            return MAKELRESULT(grid->focused_col, grid->focused_row);

        case MC_GM_SETSELECTION:
            return (grid_set_selection(grid, (MC_GSELECTION*) lp) == 0 ? TRUE : FALSE);

        case MC_GM_GETSELECTION:
            return grid_get_selection(grid, (MC_GSELECTION*) lp);

        case MC_GM_GETEDITCONTROL:
            return (LRESULT) labeledit_win(win);

        case MC_GM_EDITLABEL:
        {
            WORD col = LOWORD(wp);
            WORD row = HIWORD(wp);
            if(MC_ERR(col >= grid->col_count || row >= grid->row_count)) {
                MC_TRACE("MC_GM_EDITLABEL: Column or row index out of range "
                         "(size: %ux%u; requested [%u,%u])",
                         (unsigned) grid->col_count, (unsigned) grid->row_count,
                         (unsigned) col, (unsigned) row);
                return (LRESULT) NULL;
            }

            return (LRESULT) grid_start_label_edit(grid, col, row);
        }

        case MC_GM_CANCELEDITLABEL:
            if(grid->labeledit_started)
                grid_end_label_edit(grid, TRUE);
            return 0;

        case WM_SETREDRAW:
            grid->no_redraw = !wp;
            if(!grid->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_VSCROLL:
        case WM_HSCROLL:
            grid_scroll(grid, (msg == WM_VSCROLL), LOWORD(wp), 1);
            return 0;

        case WM_MOUSEMOVE:
            grid_mouse_move(grid, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_MOUSELEAVE:
            grid_mouse_leave(grid);
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            grid_mouse_wheel(grid, (msg == WM_MOUSEWHEEL), (int)(SHORT)HIWORD(wp));
            return 0;

        case WM_LBUTTONDOWN:
            grid_left_button_down(grid, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_LBUTTONUP:
            grid_left_button_up(grid, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_LBUTTONDBLCLK:
            grid_left_button_dblclick(grid, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            grid_right_button(grid, GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                              (msg == WM_RBUTTONDBLCLK));
            return 0;

        case WM_KEYDOWN:
            grid_key_down(grid, wp);
            return 0;

        case WM_GETDLGCODE:
            if(wp == VK_ESCAPE)
                return DLGC_WANTMESSAGE;
            return DLGC_WANTARROWS;

        case WM_CAPTURECHANGED:
            grid_end_any_drag(grid, TRUE);
            grid->mouse_captured = FALSE;
            return 0;

        case WM_SIZE:
            /* Resizing of the control ends label edit. */
            if(grid->labeledit_started)
                grid_end_label_edit(grid, FALSE);

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

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            grid_change_focus(grid, (msg == WM_SETFOCUS));
            return 0;

        case WM_SETCURSOR:
            if(grid_set_cursor(grid))
                return TRUE;
            break;

        case WM_TIMER:
            if(wp == (UINT_PTR) grid_autoscroll) {
                grid_autoscroll(grid);
                return 0;
            } else if(wp == (UINT_PTR) grid_start_label_edit) {
                GRID_TRACE("grid_proc(WM_TIMER): Start label edit.");
                KillTimer(win, (UINT_PTR) grid_start_label_edit);
                grid_start_label_edit(grid, grid->focused_col, grid->focused_row);
                return 0;
            }
            break;

        case WM_STYLECHANGED:
            switch(wp) {
                case GWL_STYLE:
                    grid_style_changed(grid, (STYLESTRUCT*) lp);
                    break;
                case GWL_EXSTYLE:
                    grid_exstyle_changed(grid, (STYLESTRUCT*) lp);
                    break;
            }
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

        case WM_COMMAND:
            if(grid->labeledit_started  &&  HIWORD(wp) == EN_KILLFOCUS)
                grid_end_label_edit(grid, FALSE);
            break;

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
    int i;
    WNDCLASS wc = { 0 };

    for(i = 0; i < MC_SIZEOF_ARRAY(grid_cursor); i++) {
        grid_cursor[i].cur = LoadCursor(mc_instance,
                                        MAKEINTRESOURCE(grid_cursor[i].res_id));
        if(MC_ERR(grid_cursor[i].cur == NULL)) {
            MC_TRACE("grid_init_module: LoadCursor(%d) failed [%lu]",
                     grid_cursor[i].res_id, GetLastError());
            goto err_LoadCursor;
        }
    }

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_DBLCLKS;
    wc.lpfnWndProc = grid_proc;
    wc.cbWndExtra = sizeof(grid_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = grid_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("grid_init_module: RegisterClass() failed");
        goto err_RegisterClass;
    }

    return 0;

err_LoadCursor:
err_RegisterClass:
    while(--i >= 0)
        DestroyCursor(grid_cursor[i].cur);
    return -1;
}

void
grid_fini_module(void)
{
    int i;

    UnregisterClass(grid_wc, NULL);

    for(i = 0; i < MC_SIZEOF_ARRAY(grid_cursor); i++)
        DestroyCursor(grid_cursor[i].cur);
}
