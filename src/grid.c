/*
 * Copyright (c) 2010-2011 Martin Mitas
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


#define PADDING_H     3
#define PADDING_V     2


#define MC_GS_COLUMNHEADERMASK                                             \
            (MC_GS_COLUMNHEADERNONE | MC_GS_COLUMNHEADERNUMBERED |         \
             MC_GS_COLUMNHEADERALPHABETIC | MC_GS_COLUMNHEADERCUSTOM)

#define MC_GS_ROWHEADERMASK                                                \
            (MC_GS_ROWHEADERNONE | MC_GS_ROWHEADERNUMBERED |               \
             MC_GS_ROWHEADERALPHABETIC | MC_GS_ROWHEADERCUSTOM)


static const TCHAR grid_wc[] = MC_WC_GRID;    /* window class name */
static const WCHAR grid_tc[] = L"HEADER";     /* theme class */


typedef struct grid_tag grid_t;
struct grid_tag {
    HWND win;
    HTHEME theme;
    HFONT font;
    table_t* table;
    UINT style            : 30;
    UINT no_redraw        : 1;
    UINT dirty_scrollbars : 1;
    WORD header_width;
    WORD header_height;
    WORD cell_width;
    WORD cell_height;
    WORD cell_padding_horz;
    WORD cell_padding_vert;
    int scroll_x;
    int scroll_y;
};

static TCHAR*
grid_num_to_alpha(TCHAR buffer[16], WORD num)
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

typedef struct grid_layout_tag grid_layout_t;
struct grid_layout_tag {
    BOOL display_col_headers;  /* whether columns have headers */
    BOOL display_row_headers;  /* ditto for rows */
    WORD display_col0;         /* index of first column for grid contents (not headers) */
    WORD display_row0;         /* ditto for rows */
    WORD display_col_count;    /* count of columns for grid contents (not headers) */
    WORD display_row_count;    /* ditto for rows */
};

static void
grid_calc_layout(grid_t* grid, grid_layout_t* layout)
{
    WORD col_count;
    WORD row_count;

    if(!grid->table) {
        memset(layout, 0, sizeof(grid_layout_t));
        return;
    }

    col_count = table_col_count(grid->table);
    row_count = table_row_count(grid->table);

    layout->display_col_headers = (row_count > 0  &&  (grid->style & MC_GS_COLUMNHEADERMASK) != MC_GS_COLUMNHEADERNONE);
    layout->display_row_headers = (col_count > 0  &&  (grid->style & MC_GS_ROWHEADERMASK) != MC_GS_ROWHEADERNONE);
    layout->display_col0 = (col_count > 0  &&  (grid->style & MC_GS_ROWHEADERMASK) == MC_GS_ROWHEADERCUSTOM) ? 1 : 0;
    layout->display_row0 = (row_count > 0  &&  (grid->style & MC_GS_COLUMNHEADERMASK) == MC_GS_COLUMNHEADERCUSTOM) ? 1 : 0;
    layout->display_col_count = col_count - layout->display_col0;
    layout->display_row_count = row_count - layout->display_row0;
}

static void
grid_paint(grid_t* grid, HDC dc, RECT* dirty)
{
    grid_layout_t layout;
    WORD headerw, headerh;
    WORD col0, row0;
    WORD col1, row1;
    WORD col, row;
    RECT rect;
    HRGN clip;
    int old_dc_state, cell_dc_state;

    GRID_TRACE("grid_paint(%d, %d, %d, %d)",
               dirty->left, dirty->top, dirty->right, dirty->bottom);

    grid_calc_layout(grid, &layout);
    headerw = (layout.display_row_headers ? grid->header_width : 0);
    headerh = (layout.display_col_headers ? grid->header_height : 0);

    /* Calculate range of cells in dirty rect
     * ([col0,row0] inclusive; [col1,row1] exclusive) */
    col0 = (grid->scroll_x + MC_MAX(0, dirty->left - headerw)) / grid->cell_width;
    row0 = (grid->scroll_y + MC_MAX(0, dirty->top - headerh)) / grid->cell_height;
    col1 = MC_MIN(layout.display_col_count, (grid->scroll_x + dirty->right - headerw) / grid->cell_width + 1);
    row1 = MC_MIN(layout.display_row_count, (grid->scroll_y + dirty->bottom - headerh) / grid->cell_height + 1);

    GRID_TRACE("grid_paint: cell region [%d, %d] - [%d, %d]", col0, row0, col1, row1);

    old_dc_state = SaveDC(dc);

    SelectObject(dc, grid->font ? grid->font : GetStockObject(SYSTEM_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0,0,0));
    SelectObject(dc, GetStockObject(BLACK_PEN));

    clip = CreateRectRgn(0, 0, 1, 1);
    if(GetClipRgn(dc, clip) <= 0) {
        DeleteObject(clip);
        clip = NULL;
    }

    /* Paint "dead" top left cell */
    if(headerw > 0 && headerh > 0 && dirty->left <= headerw && dirty->top <= headerh) {
        rect.left = 0;
        rect.top = 0;
        rect.right = headerw;
        rect.bottom = headerh;

        if(grid->theme) {
            theme_DrawThemeBackground(grid->theme, dc, HP_HEADERITEM,
                                      HIS_NORMAL, &rect, NULL);
        } else {
            DrawEdge(dc, &rect, BDR_RAISEDINNER, BF_MIDDLE | BF_RECT);
        }
    }

    /* Paint column headers */
    if(headerh > 0 && dirty->top <= headerh) {
        TCHAR buffer[16];

        rect.left = headerw + col0 * grid->cell_width - grid->scroll_x;
        rect.top = 0;
        rect.right = rect.left + grid->cell_width;
        rect.bottom = headerh;

        for(col = col0; col < col1; col++) {
            IntersectClipRect(dc, MC_MAX(headerw, rect.left), rect.top,
                                  rect.right, rect.bottom);

            if(grid->theme) {
                theme_DrawThemeBackground(grid->theme, dc, HP_HEADERITEM,
                                          HIS_NORMAL, &rect, NULL);
            } else {
                DrawEdge(dc, &rect, BDR_RAISEDINNER, BF_MIDDLE | BF_RECT);
            }

            switch(grid->style & MC_GS_COLUMNHEADERMASK) {
                case MC_GS_COLUMNHEADERNUMBERED:
                    _stprintf(buffer, _T("%d"), col + 1);
                    DrawText(dc, buffer, -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    break;
                case MC_GS_COLUMNHEADERALPHABETIC:
                    DrawText(dc, grid_num_to_alpha(buffer, col),
                             -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    break;
                case MC_GS_COLUMNHEADERCUSTOM:
                {
                    RECT r;
                    r.left = rect.left + grid->cell_padding_horz;
                    r.right = rect.right - 2 * grid->cell_padding_horz - 1;
                    r.top = rect.top + grid->cell_padding_vert;
                    r.bottom = rect.bottom - 2 * grid->cell_padding_vert - 1;
                    cell_dc_state = SaveDC(dc);
                    table_paint_cell(grid->table, col + layout.display_col0, 0, dc, &r);
                    RestoreDC(dc, cell_dc_state);
                    break;
                }
            }

            rect.left += grid->cell_width;
            rect.right += grid->cell_width;

            SelectClipRgn(dc, clip);
        }
    }

    /* Paint row headers */
    if(headerw > 0 && dirty->left <= headerw) {
        TCHAR buffer[16];

        rect.left = 0;
        rect.top = headerh + row0 * grid->cell_height - grid->scroll_y;
        rect.right = headerw;
        rect.bottom = rect.top + grid->cell_height;

        for(row = row0; row < row1; row++) {
            IntersectClipRect(dc, rect.left, MC_MAX(headerh, rect.top),
                                  rect.right, rect.bottom);

            if(grid->theme) {
                rect.bottom++;  /* damn: Aero is ugly w/o this */
                theme_DrawThemeBackground(grid->theme, dc, HP_HEADERITEM,
                                          HIS_NORMAL, &rect, NULL);
                rect.bottom--;  /* damn: Aero is ugly w/o this */
            } else {
                DrawEdge(dc, &rect, BDR_RAISEDINNER, BF_MIDDLE | BF_RECT);
            }

            switch(grid->style & MC_GS_ROWHEADERMASK) {
                case MC_GS_ROWHEADERNUMBERED:
                    _stprintf(buffer, _T("%d"), row+1);
                    DrawText(dc, buffer, -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    break;
                case MC_GS_ROWHEADERALPHABETIC:
                    DrawText(dc, grid_num_to_alpha(buffer, row),
                             -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    break;
                case MC_GS_ROWHEADERCUSTOM:
                {
                    RECT r;
                    r.left = rect.left + grid->cell_padding_horz;
                    r.right = rect.right - 2 * grid->cell_padding_horz - 1;
                    r.top = rect.top + grid->cell_padding_vert;
                    r.bottom = rect.bottom - 2 * grid->cell_padding_vert - 1;
                    cell_dc_state = SaveDC(dc);
                    table_paint_cell(grid->table, 0, row + layout.display_row0, dc, &r);
                    RestoreDC(dc, cell_dc_state);
                    break;
                }
            }

            rect.top += grid->cell_height;
            rect.bottom += grid->cell_height;

            SelectClipRgn(dc, clip);
        }
    }

    /* Paint grid lines */
    if(!(grid->style & MC_GS_NOGRIDLINES)) {
        int x;
        int y;
        HPEN pen, old_pen;

        pen = CreatePen(PS_SOLID, 0, RGB(223,223,223));
        old_pen = SelectObject(dc, pen);

        x = headerw + (col0+1) * grid->cell_width - grid->scroll_x - 1;
        y = headerh + row1 * grid->cell_height - grid->scroll_y - 1;
        for(col = col0; col < col1; col++) {
            MoveToEx(dc, x, headerh, NULL);
            LineTo(dc, x, y);
            x += grid->cell_width;
        }

        x = headerw + col1 * grid->cell_width - grid->scroll_x - 1;
        y = headerh + (row0+1) * grid->cell_height - grid->scroll_y - 1;
        for(row = row0; row < row1; row++) {
            MoveToEx(dc, headerw, y, NULL);
            LineTo(dc, x, y);
            y += grid->cell_height;
        }

        SelectObject(dc, old_pen);
        DeleteObject(pen);
    }

    /* Paint grid cells */
    rect.top = headerh + row0 * grid->cell_height - grid->scroll_y + grid->cell_padding_vert;
    for(row = layout.display_row0 + row0; row < layout.display_row0 + row1; row++) {
        rect.left = headerw + col0 * grid->cell_width - grid->scroll_x + grid->cell_padding_horz;
        for(col = layout.display_col0 + col0; col < layout.display_col0 + col1; col++) {
            rect.right = rect.left + grid->cell_width - 2*grid->cell_padding_horz - 1;
            rect.bottom = rect.top + grid->cell_height - 2*grid->cell_padding_vert - 1;

            IntersectClipRect(dc, MC_MAX(headerw, rect.left), MC_MAX(headerh, rect.top),
                                  rect.right, rect.bottom);
            cell_dc_state = SaveDC(dc);
            table_paint_cell(grid->table, col, row, dc, &rect);
            RestoreDC(dc, cell_dc_state);
            SelectClipRgn(dc, clip);

            rect.left += grid->cell_width;
        }
        rect.top += grid->cell_height;
    }

    RestoreDC(dc, old_dc_state);
}

static void
grid_refresh(void* view, void* detail)
{
    table_region_t* region = (table_region_t*) detail;
    grid_t* grid = (grid_t*) view;
    grid_layout_t layout;
    WORD headerw, headerh;
    RECT rect;

    if(region == NULL) {
        InvalidateRect(grid->win, NULL, TRUE);
        return;
    }

    grid_calc_layout(grid, &layout);
    headerw = (layout.display_row_headers ? grid->header_width : 0);
    headerh = (layout.display_col_headers ? grid->header_height : 0);

    /* Refresh affected row header */
    if(region->col0 < layout.display_col0) {
        rect.left = 0;
        rect.top = headerh + MC_MAX(0, (region->row0 - layout.display_row0) * grid->cell_height - grid->scroll_y);
        rect.right = headerw;
        rect.bottom = rect.top + (region->row1 - region->row0) * grid->cell_height;
        InvalidateRect(grid->win, &rect, TRUE);

        region->col0 = layout.display_col0;
    }

    /* Refresh affected column header */
    if(region->row0 < layout.display_row0) {
        rect.left = headerw + MC_MAX(0, (region->col0 - layout.display_col0) * grid->cell_width - grid->scroll_x);
        rect.top = 0;
        rect.right = rect.left + (region->col1 - region->col0) * grid->cell_width;
        rect.bottom = headerh;
        InvalidateRect(grid->win, &rect, TRUE);

        region->row0 = layout.display_row0;
    }

    /* Refresh affected contents */
    rect.left = headerw + MC_MAX(0, (region->col0 - layout.display_col0) * grid->cell_width - grid->scroll_x);
    rect.top = headerh + MC_MAX(0, (region->row0 - layout.display_row0) * grid->cell_height - grid->scroll_y);
    rect.right = rect.left + (region->col1 - region->col0) * grid->cell_width;
    rect.bottom = rect.top + (region->row1 - region->row0) * grid->cell_height;
    InvalidateRect(grid->win, &rect, TRUE);
}

static void
grid_scroll(grid_t* grid, WORD opcode, int factor, BOOL is_vertical)
{
    grid_layout_t layout;
    RECT rect;
    SCROLLINFO si;
    WORD headerw, headerh;
    int old_scroll_x = grid->scroll_x;
    int old_scroll_y = grid->scroll_y;

    grid_calc_layout(grid, &layout);
    headerw = (layout.display_row_headers ? grid->header_width : 0);
    headerh = (layout.display_col_headers ? grid->header_height : 0);
    GetClientRect(grid->win, &rect);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

    if(is_vertical) {
        GetScrollInfo(grid->win, SB_VERT, &si);
        switch(opcode) {
            case SB_BOTTOM:        grid->scroll_y = si.nMax; break;
            case SB_LINEUP:        grid->scroll_y -= factor * MC_MIN(MC_MIN(grid->cell_height, 40), si.nPage); break;
            case SB_LINEDOWN:      grid->scroll_y += factor * MC_MIN(MC_MIN(grid->cell_height, 40), si.nPage); break;
            case SB_PAGEUP:        grid->scroll_y -= si.nPage; break;
            case SB_PAGEDOWN:      grid->scroll_y += si.nPage; break;
            case SB_THUMBPOSITION: grid->scroll_y = si.nPos; break;
            case SB_THUMBTRACK:    grid->scroll_y = si.nTrackPos; break;
            case SB_TOP:           grid->scroll_y = 0; break;
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
            case SB_BOTTOM:        grid->scroll_x = si.nMax; break;
            case SB_LINELEFT:      grid->scroll_x -= factor * MC_MIN(MC_MIN(grid->cell_width, 40), si.nPage); break;
            case SB_LINERIGHT:     grid->scroll_x += factor * MC_MIN(MC_MIN(grid->cell_width, 40), si.nPage); break;
            case SB_PAGELEFT:      grid->scroll_x -= si.nPage; break;
            case SB_PAGERIGHT:     grid->scroll_x += si.nPage; break;
            case SB_THUMBPOSITION: grid->scroll_x = si.nPos; break;
            case SB_THUMBTRACK:    grid->scroll_x = si.nTrackPos; break;
            case SB_TOP:           grid->scroll_x = 0; break;
        }

        if(grid->scroll_x > si.nMax - (int)si.nPage)
            grid->scroll_x = si.nMax - (int)si.nPage;
        if(grid->scroll_x < si.nMin)
            grid->scroll_x = si.nMin;
        if(old_scroll_x == grid->scroll_x)
            return;

        SetScrollPos(grid->win, SB_HORZ, grid->scroll_x, TRUE);
    }

    if(is_vertical)
        rect.top = headerh;
    else
        rect.left = headerw;

    ScrollWindowEx(grid->win, old_scroll_x - grid->scroll_x,
                   old_scroll_y - grid->scroll_y, &rect, &rect, NULL, NULL,
                   SW_ERASE | SW_INVALIDATE);
}

static void
grid_setup_scrollbars(grid_t* grid)
{
    grid_layout_t layout;
    RECT rect;
    SCROLLINFO si;
    WORD headerw, headerh;
    
    if(grid->no_redraw) {
        grid->dirty_scrollbars = 1;
        return;
    }

    grid_calc_layout(grid, &layout);
    headerw = (layout.display_row_headers ? grid->header_width : 0);
    headerh = (layout.display_col_headers ? grid->header_height : 0);
    GetClientRect(grid->win, &rect);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;

    /* Setup horizontal scrollbar */
    si.nMax = layout.display_col_count * grid->cell_width;
    si.nPage = rect.right - rect.left - headerw;
    grid->scroll_x = SetScrollInfo(grid->win, SB_HORZ, &si, TRUE);

    /* Fixup for Win2000 - appearance of horizontal toolbar sometimes
     * breaks calculation of vertical scrollbar properties and last row
     * can be "hidden" behind the horizontal scrollbar */
    GetClientRect(grid->win, &rect);

    /* Setup vertical scrollbar */
    si.nMax = layout.display_row_count * grid->cell_height;
    si.nPage = rect.bottom - rect.top - headerh;
    grid->scroll_y = SetScrollInfo(grid->win, SB_VERT, &si, TRUE);
}

static int
grid_set_table(grid_t* grid, table_t* table)
{
    if(table != NULL  &&  table == grid->table)
        return 0;

    if(table != NULL) {
        table_ref(table);
    } else if(!(grid->style & MC_GS_NOTABLECREATE)) {
        table = table_create(0, 0, NULL, 0);
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

    InvalidateRect(grid->win, NULL, TRUE);
    grid_setup_scrollbars(grid);
    return 0;
}

#define GRID_GGF_SUPPORTED_MASK                                         \
        (MC_GGF_COLUMNHEADERHEIGHT | MC_GGF_ROWHEADERWIDTH |            \
         MC_GGF_COLUMNWIDTH | MC_GGF_ROWHEIGHT |                        \
         MC_GGF_PADDINGHORZ | MC_GGF_PADDINGVERT)

static int
grid_set_geometry(grid_t* grid, MC_GGEOMETRY* geom, BOOL invalidate)
{
    GRID_TRACE("grid_set_geometry(%p, %p, %d)", grid, geom, (int)invalidate);

    if(geom != NULL) {
        if(MC_ERR((geom->fMask & ~GRID_GGF_SUPPORTED_MASK) != 0)) {
            MC_TRACE("grid_set_geometry: fMask has some unsupported bit(s)");
            SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        if(geom->fMask & MC_GGF_COLUMNHEADERHEIGHT)
            grid->header_height = geom->wColumnHeaderHeight;
        if(geom->fMask & MC_GGF_ROWHEADERWIDTH)
            grid->header_width = geom->wRowHeaderWidth;
        if(geom->fMask & MC_GGF_COLUMNWIDTH)
            grid->cell_width = geom->wColumnWidth;
        if(geom->fMask & MC_GGF_ROWHEIGHT)
            grid->cell_height = geom->wRowHeight;
        if(geom->fMask & MC_GGF_PADDINGHORZ)
            grid->cell_padding_horz = geom->wPaddingHorz;
        if(geom->fMask & MC_GGF_PADDINGVERT)
            grid->cell_padding_vert = geom->wPaddingVert;
    } else {
        /* Set the geometry according to reasonable default values. */
        SIZE size;
        mc_font_size(grid->font, &size);

        grid->header_width = 4 * size.cx + 2 * PADDING_H + 1;
        grid->header_height = size.cy + 2 * PADDING_V + 1;
        grid->cell_width = 8 * size.cx + 2 * PADDING_H + 1;
        grid->cell_height = size.cy + 2 * PADDING_V + 1;
        grid->cell_padding_horz = PADDING_H;
        grid->cell_padding_vert = PADDING_V;
    }

    grid_setup_scrollbars(grid);

    if(invalidate)
        InvalidateRect(grid->win, NULL, TRUE);
    return 0;
}

static int
grid_get_geometry(grid_t* grid, MC_GGEOMETRY* geom)
{
    GRID_TRACE("grid_get_geometry(%p, %p)", grid, geom);

    if(MC_ERR((geom->fMask & ~GRID_GGF_SUPPORTED_MASK) != 0)) {
        MC_TRACE("grid_get_geometry: fMask has some unsupported bit(s)");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(geom->fMask & MC_GGF_COLUMNHEADERHEIGHT)
        geom->wColumnHeaderHeight = grid->header_height;
    if(geom->fMask & MC_GGF_ROWHEADERWIDTH)
        geom->wRowHeaderWidth = grid->header_width;
    if(geom->fMask & MC_GGF_COLUMNWIDTH)
        geom->wColumnWidth = grid->cell_width;
    if(geom->fMask & MC_GGF_ROWHEIGHT)
        geom->wRowHeight = grid->cell_height;
    if(geom->fMask & MC_GGF_PADDINGHORZ)
        geom->wPaddingHorz = grid->cell_padding_horz;
    if(geom->fMask & MC_GGF_PADDINGVERT)
        geom->wPaddingVert = grid->cell_padding_vert;

    return 0;
}

static void
grid_style_changed(grid_t* grid, int style_type, STYLESTRUCT* ss)
{
    if(style_type == GWL_STYLE)
        grid->style = ss->styleNew;

    grid_setup_scrollbars(grid);
    RedrawWindow(grid->win, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
}

static void
grid_theme_changed(grid_t* grid)
{
    if(grid->theme)
        theme_CloseThemeData(grid->theme);
    grid->theme = theme_OpenThemeData(grid->win, grid_tc);

    grid_setup_scrollbars(grid);
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

    grid->win = win;
    grid->theme = NULL;
    grid->font = NULL;
    grid->table = NULL;
    grid->style = cs->style;
    grid->no_redraw = 0;
    grid->dirty_scrollbars = 0;
    grid->scroll_x = 0;
    grid->scroll_y = 0;

    grid_set_geometry(grid, NULL, FALSE);

    return grid;
}

static int
grid_create(grid_t* grid)
{
    grid->theme = theme_OpenThemeData(grid->win, grid_tc);

    if(MC_ERR(grid_set_table(grid, NULL) != 0)) {
        MC_TRACE("grid_create: grid_set_table() failed.");
        return -1;
    }

    return 0;
}

static void
grid_destroy(grid_t* grid)
{
    if(grid->theme) {
        theme_CloseThemeData(grid->theme);
        grid->theme = NULL;
    }

    if(grid->table) {
        table_uninstall_view(grid->table, grid);
        table_unref(grid->table);
        grid->table = NULL;
    }
}

static inline void
grid_ncdestroy(grid_t* grid)
{
    free(grid);
}

static LRESULT CALLBACK
grid_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    grid_t* grid = (grid_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            if(grid->no_redraw)
                return 0;
            /* no break */
        case WM_PRINTCLIENT:
            {
                HDC dc = (HDC)wp;
                PAINTSTRUCT ps;

                if(wp == 0)
                    dc = BeginPaint(win, &ps);
                else
                    GetClientRect(grid->win, &ps.rcPaint);
                grid_paint(grid, dc, &ps.rcPaint);
                if(wp == 0)
                    EndPaint(win, &ps);
            }
            return 0;

        case WM_SETREDRAW:
            grid->no_redraw = !wp;
            if(!grid->no_redraw)
                grid_setup_scrollbars(grid);
            return 0;

        case MC_GM_GETTABLE:
            return (LRESULT) grid->table;

        case MC_GM_SETTABLE:
            return (grid_set_table(grid, (table_t*) lp) == 0 ? TRUE : FALSE);

        case MC_GM_GETCOLUMNCOUNT:
            return mcTable_ColumnCount(grid->table);

        case MC_GM_GETROWCOUNT:
            return mcTable_RowCount(grid->table);

        case MC_GM_RESIZE:
            return mcTable_Resize(grid->table, LOWORD(wp), HIWORD(wp));

        case MC_GM_CLEAR:
            mcTable_Clear(grid->table);
            return 0;

        case MC_GM_SETCELL:
        {
            MC_GCELL* cell = (MC_GCELL*) lp;
            return mcTable_SetCell(grid->table, cell->wCol, cell->wRow,
                                                cell->hType, cell->hValue);
        }

        case MC_GM_GETCELL:
        {
            MC_GCELL* cell = (MC_GCELL*) lp;
            return mcTable_GetCell(grid->table, cell->wCol, cell->wRow,
                                   &cell->hType, &cell->hValue);
        }

        case MC_GM_SETGEOMETRY:
            return (grid_set_geometry(grid, (MC_GGEOMETRY*)lp, TRUE) == 0 ? TRUE : FALSE);

        case MC_GM_GETGEOMETRY:
            return (grid_get_geometry(grid, (MC_GGEOMETRY*)lp) == 0 ? TRUE : FALSE);

        case WM_VSCROLL:
        case WM_HSCROLL:
            grid_scroll(grid, LOWORD(wp), 1, (msg == WM_VSCROLL));
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        {
            int delta;
            delta = mc_wheel_scroll(win, (msg == WM_MOUSEWHEEL), (SHORT)HIWORD(wp));
            if(delta != 0)
                grid_scroll(grid, SB_LINEDOWN, delta, (msg == WM_MOUSEWHEEL));
            return 0;
        }

        case WM_SIZE:
            grid_setup_scrollbars(grid);
            return 0;

        case WM_GETFONT:
            return (LRESULT) grid->font;

        case WM_SETFONT:
            grid->font = (HFONT) wp;
            if((BOOL) lp)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_STYLECHANGED:
            grid_style_changed(grid, wp, (STYLESTRUCT*) lp);
            return 0;

        case WM_THEMECHANGED:
            grid_theme_changed(grid);
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
grid_init(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = grid_proc;
    wc.cbWndExtra = sizeof(grid_t*);
    wc.hInstance = mc_instance_exe;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = grid_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE("grid_init: RegisterClass() failed [%lu]", GetLastError());
        return -1;
    }

    return 0;
}

void
grid_fini(void)
{
    UnregisterClass(grid_wc, mc_instance_exe);
}
