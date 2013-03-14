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

#include "treelist.h"
#include "theme.h"


/* TODO:
 *  -- Incremental search.
 *  -- Tooltips and style MC_TLS_NOTOOLTIPS.
 *  -- Hot tracking (needed when EXPLORER-themed).
 *  -- Style MC_TLS_GRIDLINES.
 *  -- Handling state and style MC_TLS_CHECKBOXES.
 *  -- Support editing labels and style MC_TLS_EDITLABELS and related
 *     notifications.
 */


/* Uncomment this to have more verbose traces from this module. */
/*#define TREELIST_DEBUG     1*/

#ifdef TREELIST_DEBUG
    #define TREELIST_TRACE       MC_TRACE
#else
    #define TREELIST_TRACE(...)  do {} while(0)
#endif


/* Note:
 *   In comments and function names in this module, the term "displayed item"
 *   means the item is not hidden due any collapsed item on its parent chain
 *   upward to the root. It does NOT mean the item must be scrolled into the
 *   current viewport.
 */


static const TCHAR treelist_wc[] = MC_WC_TREELIST;  /* Window class name */
static const WCHAR treelist_tc[] = L"TREEVIEW";     /* Theming identifier */


typedef struct treelist_item_tag treelist_item_t;
struct treelist_item_tag {
    treelist_item_t* parent;
    treelist_item_t* sibling_prev;
    treelist_item_t* sibling_next;
    treelist_item_t* child_head;
    treelist_item_t* child_tail;

    TCHAR* text;
    TCHAR** subitems;
    LPARAM lp;
    SHORT img;
    SHORT img_selected;
    SHORT img_expanded;
    WORD state     : 15;
    WORD children  : 1;
};

/* Iterator over ALL items of the control */
static treelist_item_t*
item_next_ex(treelist_item_t* item, treelist_item_t* stopper)
{
    if(item->child_head != NULL)
        return item->child_head;

    do {
        if(item->sibling_next != NULL)
            return item->sibling_next;
        item = item->parent;
    } while(item != NULL  &&  item != stopper);

    return NULL;
}

static inline treelist_item_t*
item_next(treelist_item_t* item)
{
    return item_next_ex(item, NULL);
}

/* Iteraror over items displayed (i.e. not hidden by collpased parent) below
 * the specified 'item', but does not step over the parent 'stopper' */
static treelist_item_t*
item_next_displayed_ex(treelist_item_t* item, treelist_item_t* stopper, int* level)
{
    if(item->child_head != NULL  &&  (item->state & MC_TLIS_EXPANDED)) {
        (*level)++;
        return item->child_head;
    }

    do {
        if(item->sibling_next != NULL)
            return item->sibling_next;
        (*level)--;
        item = item->parent;
    } while(item != stopper  &&  item != NULL);

    return NULL;
}

static inline treelist_item_t*
item_next_displayed(treelist_item_t* item, int* level)
{
    return item_next_displayed_ex(item, NULL, level);
}

static treelist_item_t*
item_prev_displayed(treelist_item_t* item)
{
    if(item->sibling_prev != NULL) {
        item = item->sibling_prev;
        while(item->child_tail != NULL  &&  (item->state & MC_TLIS_EXPANDED))
            item = item->child_tail;
        return item;
    }

    return item->parent;
}

static BOOL
item_is_displayed(treelist_item_t* item)
{
    while(item->parent != NULL) {
        item = item->parent;
        if(!(item->state & MC_TLIS_EXPANDED))
            return FALSE;
    }

    return TRUE;
}

static inline BOOL
item_has_children(treelist_item_t* item)
{
    return (item->child_head != NULL  ||  item->children != 0);
}

typedef struct treelist_tag treelist_t;
struct treelist_tag {
    HWND win;
    HWND header_win;
    HWND tooltip_win;
    HWND notify_win;
    HTHEME theme;
    HFONT font;
    HIMAGELIST imglist_normal;
    treelist_item_t* root_head;
    treelist_item_t* root_tail;
    treelist_item_t* scrolled_item;   /* can be NULL if not known */
    treelist_item_t* selected_item;
    int scrolled_level;               /* level of the scrolled_item */
    DWORD style                 : 16;
    DWORD no_redraw             :  1;
    DWORD unicode_notifications :  1;
    DWORD dirty_scrollbars      :  1;
    DWORD item_height_set       :  1;
    DWORD focus                 :  1;
    DWORD displayed_items;
    WORD col_count;
    WORD item_height;
    WORD item_indent;
    WORD scroll_y;                    /* in rows */
    int scroll_x;                     /* in pixels */
    int scroll_x_max;
};


#define MC_TLCF_ALL     (MC_TLCF_FORMAT | MC_TLCF_WIDTH | MC_TLCF_TEXT |      \
                         MC_TLCF_IMAGE | MC_TLCF_ORDER)

#define MC_TLIF_ALL     (MC_TLIF_STATE | MC_TLIF_TEXT | MC_TLIF_LPARAM |      \
                         MC_TLIF_IMAGE | MC_TLIF_SELECTEDIMAGE |              \
                         MC_TLIF_EXPANDEDIMAGE | MC_TLIF_CHILDREN)

#define MC_TLSIF_ALL    (MC_TLSIF_TEXT)


#define SCROLL_H_UNIT              5
#define EMPTY_SELECT_WIDTH        80
#define ITEM_INDENT_MIN           19  /* minimal item indent (per level) */
#define ITEM_HEIGHT_MIN           16  /* minimal item height */
#define ITEM_HEIGHT_FONT_MARGIN_V  3  /* margin of item height derived from font size */
#define ITEM_PAINT_MARGIN_H        1  /* we follow listview in that the (sub)items are offseted to right in compatision to the header */
#define ITEM_PADDING_H             2  /* inner padding of the item */
#define ITEM_PADDING_V             1  /* inner padding of the item */
#define ITEM_PADDING_H_THEMEEXTRA  2  /* when using theme, use extra more horiz. padding. */
#define ITEM_DTFLAGS              (DT_EDITCONTROL | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER | DT_END_ELLIPSIS)


static void
treelist_layout_header(treelist_t* tl)
{
    RECT rect;
    WINDOWPOS header_pos;
    HD_LAYOUT header_layout = { &rect, &header_pos };

    GetClientRect(tl->win, &rect);

    MC_MSG(tl->header_win, HDM_LAYOUT, 0, &header_layout);
    SetWindowPos(tl->header_win, header_pos.hwndInsertAfter,
                 header_pos.x - tl->scroll_x, header_pos.y,
                 header_pos.cx + tl->scroll_x, header_pos.cy,
                 header_pos.flags);
}

static inline void
treelist_do_hscroll(treelist_t* tl, SCROLLINFO* si, int scroll_x)
{
    if(scroll_x > si->nMax - (int)si->nPage + 1)
        scroll_x = si->nMax - (int)si->nPage + 1;
    if(scroll_x < si->nMin)
        scroll_x = si->nMin;

    if(scroll_x == tl->scroll_x)
        return;

    SetScrollPos(tl->win, SB_HORZ, scroll_x, TRUE);
    if(!tl->no_redraw) {
        RECT header_rect;
        RECT client_rect;
        RECT scroll_rect;

        GetWindowRect(tl->header_win, &header_rect);
        MapWindowPoints(NULL, tl->win, (POINT*) &header_rect, 2);
        GetClientRect(tl->win, &client_rect);
        mc_rect_set(&scroll_rect, 0, mc_height(&header_rect),
                    client_rect.right, client_rect.bottom);
        ScrollWindowEx(tl->win, (tl->scroll_x - scroll_x), 0,
                    &scroll_rect, &scroll_rect, NULL, NULL, SW_ERASE | SW_INVALIDATE);
    }
    tl->scroll_x = scroll_x;
    treelist_layout_header(tl);
}

static void
treelist_hscroll_rel(treelist_t* tl, int delta)
{
    SCROLLINFO si;

    TREELIST_TRACE("treelist_hscroll_rel(%p, %d)", tl, (int)delta);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(tl->win, SB_HORZ, &si);

    treelist_do_hscroll(tl, &si, tl->scroll_x + delta);
}

static void
treelist_hscroll(treelist_t* tl, WORD opcode)
{
    SCROLLINFO si;
    int scroll_x = tl->scroll_x;

    TREELIST_TRACE("treelist_hscroll(%p, %d)", tl, (int)opcode);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;
    GetScrollInfo(tl->win, SB_HORZ, &si);
    switch(opcode) {
        case SB_BOTTOM:          scroll_x = si.nMax; break;
        case SB_LINEUP:          scroll_x -= SCROLL_H_UNIT; break;
        case SB_LINEDOWN:        scroll_x += SCROLL_H_UNIT; break;
        case SB_PAGEUP:          scroll_x -= si.nPage; break;
        case SB_PAGEDOWN:        scroll_x += si.nPage; break;
        case SB_THUMBPOSITION:   scroll_x = si.nPos; break;
        case SB_THUMBTRACK:      scroll_x = si.nTrackPos; break;
        case SB_TOP:             scroll_x = 0; break;
    }

    treelist_do_hscroll(tl, &si, scroll_x);
}

static void
treelist_do_vscroll(treelist_t* tl, SCROLLINFO* si, int scroll_y)
{
    if(scroll_y > si->nMax - (int)si->nPage + 1)
        scroll_y = si->nMax - (int)si->nPage + 1;
    if(scroll_y < si->nMin)
        scroll_y = si->nMin;

    if(scroll_y == tl->scroll_y)
        return;

    SetScrollPos(tl->win, SB_VERT, scroll_y, TRUE);
    if(!tl->no_redraw) {
        RECT header_rect;
        RECT client_rect;
        RECT scroll_rect;

        GetWindowRect(tl->header_win, &header_rect);
        MapWindowPoints(NULL, tl->win, (POINT*) &header_rect, 2);
        GetClientRect(tl->win, &client_rect);
        mc_rect_set(&scroll_rect, 0, mc_height(&header_rect),
                    client_rect.right, client_rect.bottom);
        ScrollWindowEx(tl->win, 0, (tl->scroll_y - scroll_y) * tl->item_height,
                    &scroll_rect, &scroll_rect, NULL, NULL, SW_ERASE | SW_INVALIDATE);
    }
    tl->scroll_y = scroll_y;
    tl->scrolled_item = NULL;
}

static void
treelist_vscroll_rel(treelist_t* tl, int row_delta)
{
    SCROLLINFO si;

    TREELIST_TRACE("treelist_vscroll_rel(%p, %d)", tl, (int)row_delta);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(tl->win, SB_VERT, &si);

    treelist_do_vscroll(tl, &si, tl->scroll_y + row_delta);
}

static void
treelist_vscroll(treelist_t* tl, WORD opcode)
{
    SCROLLINFO si;
    int scroll_y = tl->scroll_y;

    TREELIST_TRACE("treelist_vscroll(%p, %d)", tl, (int)opcode);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;
    GetScrollInfo(tl->win, SB_VERT, &si);
    switch(opcode) {
        case SB_BOTTOM:          scroll_y = si.nMax; break;
        case SB_LINEUP:          scroll_y -= 1; break;
        case SB_LINEDOWN:        scroll_y += 1; break;
        case SB_PAGEUP:          scroll_y -= si.nPage; break;
        case SB_PAGEDOWN:        scroll_y += si.nPage; break;
        case SB_THUMBPOSITION:   scroll_y = si.nPos; break;
        case SB_THUMBTRACK:      scroll_y = si.nTrackPos; break;
        case SB_TOP:             scroll_y = 0; break;
    }

    treelist_do_vscroll(tl, &si, scroll_y);
}

static void
treelist_mouse_wheel(treelist_t* tl, BOOL vertical, int wheel_delta)
{
    SCROLLINFO si;
    int line_delta;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    GetScrollInfo(tl->win, (vertical ? SB_VERT : SB_HORZ), &si);

    line_delta = mc_wheel_scroll(tl->win, vertical, wheel_delta, si.nPage);
    if(line_delta != 0) {
        if(vertical)
            treelist_vscroll_rel(tl, line_delta);
        else
            treelist_hscroll_rel(tl, line_delta);
    }
}

static int
treelist_items_per_page(treelist_t* tl)
{
    RECT header_rect;
    RECT client_rect;

    GetWindowRect(tl->header_win, &header_rect);
    GetClientRect(tl->win, &client_rect);
    return ((mc_height(&client_rect) - mc_height(&header_rect)) / tl->item_height);
}

static void
treelist_setup_scrollbars(treelist_t* tl)
{
    RECT rect;
    SCROLLINFO si;
    int scroll_x, scroll_y;

    if(tl->no_redraw) {
        tl->dirty_scrollbars = 1;
        return;
    }

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;

    /* Setup vertical scrollbar */
    si.nMax = tl->displayed_items-1;
    si.nPage = treelist_items_per_page(tl);
    if(si.nPage < 1)
        si.nPage = 1;
    scroll_y = SetScrollInfo(tl->win, SB_VERT, &si, TRUE);
    treelist_do_vscroll(tl, &si, scroll_y);

    /* Setup horizontal scrollbar */
    GetClientRect(tl->win, &rect);
    si.nMax = tl->scroll_x_max;
    si.nPage = mc_width(&rect);
    scroll_x = SetScrollInfo(tl->win, SB_HORZ, &si, TRUE);
    treelist_do_hscroll(tl, &si, scroll_x);

    tl->dirty_scrollbars = 0;
}

static int
treelist_natural_item_height(treelist_t* tl)
{
    UINT height = ITEM_HEIGHT_MIN;
    HDC dc;
    HFONT old_font;
    TEXTMETRIC tm;

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    old_font = SelectObject(dc, tl->font ? tl->font : GetStockObject(SYSTEM_FONT));
    GetTextMetrics(dc, &tm);
    SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);
    if(height < tm.tmHeight + tm.tmExternalLeading + ITEM_HEIGHT_FONT_MARGIN_V)
        height = tm.tmHeight + tm.tmExternalLeading + ITEM_HEIGHT_FONT_MARGIN_V;

    if(tl->imglist_normal != NULL) {
        int w, h;
        ImageList_GetIconSize(tl->imglist_normal, &w, &h);
        if(height < h)
            height = h;
    }

    if(!(tl->style & MC_TLS_NONEVENHEIGHT))
        height &= ~0x1;

    return height;
}

static int
treelist_set_item_height(treelist_t* tl, int height, BOOL redraw)
{
    int old_height = tl->item_height;

    if(height == -1) {
        height = treelist_natural_item_height(tl);
        tl->item_height_set = 0;
    } else {
        if(height < 1)
            height = 1;
        else if(!(tl->style & MC_TLS_NONEVENHEIGHT))
            height &= ~0x1;
        tl->item_height_set = 1;
    }

    if(old_height != height) {
        tl->item_height = height;
        treelist_setup_scrollbars(tl);
        if(redraw  &&  !tl->no_redraw)
            InvalidateRect(tl->win, NULL, TRUE);
    }

    return old_height;
}

static void
treelist_setup_scrolled_item(treelist_t* tl)
{
    treelist_item_t* item = tl->root_head;
    int i;
    int level = 0;

    for(i = 0; i < tl->scroll_y; i++)
        item = item_next_displayed(item, &level);
    tl->scrolled_item = item;
    tl->scrolled_level = level;
}

static inline treelist_item_t*
treelist_scrolled_item(treelist_t* tl, int* level)
{
    if(tl->scrolled_item == NULL  &&  tl->displayed_items > 0)
        treelist_setup_scrolled_item(tl);
    *level = tl->scrolled_level;
    return tl->scrolled_item;
}

static void
treelist_label_rect(treelist_t* tl, HDC dc, const TCHAR* str, UINT dtjustify,
                    RECT* rect, int* padding_h, int* padding_v)
{
    UINT w;

    if(tl->theme != NULL  &&
       mcIsThemePartDefined(tl->theme, TVP_TREEITEM, 0)) {
#if 0
        /* What the hack???
         * For some reason mcGetThemeBackgroundContentRect() gets very
         * small borders with standard win7 style and so it seems std. controls
         * probably do not rely on it.
         */

        RECT tmp;
        mc_rect_copy(&tmp, rect);
        mcGetThemeBackgroundContentRect(tl->theme, dc, TVP_TREEITEM,
                        0, &tmp, rect);
        *padding_h = rect->left - tmp.left;
        *padding_v = rect->top - tmp.top;
#else
        *padding_h = ITEM_PADDING_H + ITEM_PADDING_H_THEMEEXTRA;
        *padding_v = ITEM_PADDING_V;
#endif
    } else {
        *padding_h = ITEM_PADDING_H;
        *padding_v = ITEM_PADDING_V;
    }

    mc_rect_inflate(rect, -(*padding_h), -(*padding_v));

    if(str != NULL) {
        RECT tmp;
        mc_rect_copy(&tmp, rect);
        DrawText(dc, str, -1, &tmp, DT_CALCRECT | ITEM_DTFLAGS);
        w = mc_width(&tmp);
    } else {
        w = EMPTY_SELECT_WIDTH;
    }

    /* DT_CALCRECT does not respect justification so we must do that manually */
    if(w < mc_width(rect)) {
        switch(dtjustify) {
            case DT_RIGHT:   rect->left = rect->right - w; break;
            case DT_CENTER:  rect->left = (rect->left + rect->right - w) / 2; /* Fall through */
            default:         rect->right = rect->left + w; break;
        }
    }
    mc_rect_inflate(rect, *padding_h, *padding_v);
}

static void
treelist_paint_lines(treelist_t* tl, treelist_item_t* item, int level, HDC dc,
                     RECT* rect)
{
    treelist_item_t* it = item;
    int lvl = level;
    int lvl_end = (tl->style & MC_TLS_LINESATROOT) ? 0 : 1;
    LOGBRUSH lb;
    HPEN pen, old_pen;
    int x, y;

    lb.lbStyle = BS_SOLID;
    lb.lbColor = mcGetThemeSysColor(tl->theme, COLOR_GRAYTEXT);
    pen = ExtCreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &lb, 0, NULL);
    old_pen = SelectObject(dc, pen);

    x = rect->left - (tl->item_indent+1)/2;
    y = ((rect->top + rect->bottom) / 2) & ~1;

    /* Paint lines for the item */
    MoveToEx(dc, x, y, NULL);
    LineTo(dc, rect->left + 1, y);
    if(item->sibling_prev != NULL  ||  item->parent != NULL) {
        MoveToEx(dc, x, y, NULL);
        LineTo(dc, x, rect->top);
    }
    if(item->sibling_next != NULL) {
        MoveToEx(dc, x, y, NULL);
        LineTo(dc, x, rect->bottom + 1);
    }

    /* Paint vertical line segments for ancestors */
    while(lvl > lvl_end) {
        it = it->parent;
        lvl--;
        x -= tl->item_indent;
        if(it->sibling_next != NULL) {
            MoveToEx(dc, x, rect->top, NULL);
            LineTo(dc, x, rect->bottom + 1);
        }
    }

    SelectObject(dc, old_pen);
}

static void
treelist_paint_button(treelist_t* tl, treelist_item_t* item, HDC dc, RECT* rect)
{
    COLORREF old_bkcolor;

    old_bkcolor = SetBkColor(dc, GetSysColor(COLOR_WINDOW));

    if(tl->theme) {
        int part = TVP_GLYPH;
        int state = ((item->state & MC_TLIS_EXPANDED) ? GLPS_OPENED : GLPS_CLOSED);
        SIZE glyph_size;
        RECT r;

        mcGetThemePartSize(tl->theme, dc, part, state,
                               NULL, TS_DRAW, &glyph_size);
        r.left = (rect->left + rect->right - glyph_size.cx) / 2;
        r.top = (rect->top + rect->bottom - glyph_size.cy + 1) / 2;
        r.right = r.left + glyph_size.cx;
        r.bottom = r.top + glyph_size.cy;
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
        mcDrawThemeBackground(tl->theme, dc, part, state, &r, NULL);
    } else {
        int w = mc_width(rect);
        int h = mc_height(rect);
        int sz_rect = MC_MIN(w, h) / 2 + 1;
        int sz_glyph = (sz_rect + 1) * 3 / 4;
        int x, y;
        HPEN pen, old_pen;
        RECT r;

        pen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_GRAYTEXT));
        old_pen = SelectObject(dc, pen);

        /* Paint rectangle */
        r.left = (rect->left + rect->right - sz_rect) / 2;
        r.top = (rect->top + rect->bottom - sz_rect + 1) / 2;
        r.right = r.left + sz_rect;
        r.bottom = r.top + sz_rect;
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, old_pen);
        DeleteObject(pen);

        /* Paint glyph ('+'/'-') */
        x = (r.left + r.right) / 2;
        y = (r.top + r.bottom) / 2;
        MoveToEx(dc, x - sz_glyph/2 + 1, y, NULL);
        LineTo(dc, x + sz_glyph/2, y);
        if(!(item->state & MC_TLIS_EXPANDED)) {
            MoveToEx(dc, x, y - sz_glyph/2 + 1, NULL);
            LineTo(dc, x, y + sz_glyph/2);
        }
    }

    SetBkColor(dc, old_bkcolor);
}

static void
treelist_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    treelist_t* tl = (treelist_t*) control;
    RECT rect;
    RECT header_rect;
    int header_height;
    HFONT old_font = NULL;
    RECT item_rect;
    treelist_item_t* item;
    HDITEM header_item;
    int col_ix;
    int y;
    int level;
    COLORREF old_bkcolor;
    COLORREF old_textcolor;
    BOOL reset_color_after_whole_row = FALSE;
    BOOL use_theme;
    int state = 0;
    int padding_h, padding_v;
    int img_w = 0, img_h = 0;

    /* We handle WM_ERASEBKGND, so we should never need erasing here. */
    MC_ASSERT(erase == FALSE);

    GetClientRect(tl->win, &rect);
    GetWindowRect(tl->header_win, &header_rect);
    header_height = mc_height(&header_rect);

    header_item.mask = HDI_FORMAT;

    if(tl->font)
        old_font = SelectObject(dc, tl->font);
    SetBkColor(dc, GetSysColor(COLOR_WINDOW));
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));

    use_theme = (tl->theme != NULL  &&
                 mcIsThemePartDefined(tl->theme, TVP_TREEITEM, 0));

    if(tl->imglist_normal)
        ImageList_GetIconSize(tl->imglist_normal, &img_w, &img_h);

    for(item = treelist_scrolled_item(tl, &level), y = header_height;
        item != NULL;
        item = item_next_displayed(item, &level), y += tl->item_height)
    {
        if(y + tl->item_height < dirty->top)
            continue;

        if(y >= dirty->bottom)
            break;

        if(use_theme) {
            if(item->state & MC_TLIS_SELECTED)
                state = (tl->focus ? TREIS_SELECTED : TREIS_SELECTEDNOTFOCUS);
            else
                state = TREIS_NORMAL;
        }

        for(col_ix = 0; col_ix < tl->col_count; col_ix++) {
            MC_MSG(tl->header_win, HDM_GETITEMRECT, col_ix, &item_rect);
            item_rect.left += ITEM_PAINT_MARGIN_H - tl->scroll_x;
            item_rect.top = y;
            item_rect.right += ITEM_PAINT_MARGIN_H - tl->scroll_x;
            item_rect.bottom = y + tl->item_height;

            if(col_ix == 0) {
                /* Paint lines, buttons etc. of the main item */
                item_rect.left += level * tl->item_indent;
                if(tl->style & MC_TLS_LINESATROOT)
                    item_rect.left += tl->item_indent;
                if((level > 0 || (tl->style & MC_TLS_LINESATROOT))  &&
                   (tl->style & (MC_TLS_HASBUTTONS | MC_TLS_HASLINES))) {
                    if(tl->style & MC_TLS_HASLINES) {
                        treelist_paint_lines(tl, item, level, dc, &item_rect);
                    }

                    if((tl->style & MC_TLS_HASBUTTONS)  &&  item_has_children(item)) {
                        RECT button_rect;
                        mc_rect_set(&button_rect, item_rect.left - tl->item_indent, item_rect.top,
                                    item_rect.left, item_rect.bottom);
                        treelist_paint_button(tl, item, dc, &button_rect);
                    }

                    item_rect.left += ITEM_PADDING_H;
                }

                /* Paint image of the main item */
                if(tl->imglist_normal != NULL) {
                    int img;

                    if(item->state & MC_TLIS_SELECTED)
                        img = item->img_selected;
                    else if(item->state & MC_TLIS_EXPANDED)
                        img = item->img_expanded;
                    else
                        img = item->img;

                    if(img >= 0) {
                        ImageList_DrawEx(tl->imglist_normal, img, dc,
                                item_rect.left, item_rect.top + (mc_height(&item_rect) - img_h) / 2,
                                0, 0, CLR_NONE, CLR_DEFAULT, ILD_NORMAL);
                    }

                    item_rect.left += img_w + ITEM_PADDING_H;
                }

                /* Paint label (and background) of the main item */
                if(use_theme) {
                    int old_right = 0;

                    if(tl->style & MC_TLS_FULLROWSELECT) {
                        old_right = item_rect.right;
                        item_rect.right = tl->scroll_x_max - tl->scroll_x;
                    }
                    if(state != TREIS_NORMAL) {
                        mcDrawThemeBackground(tl->theme, dc,
                                TVP_TREEITEM, state, &item_rect, NULL);
                    }
                    if(tl->style & MC_TLS_FULLROWSELECT)
                        item_rect.right = old_right;
                    treelist_label_rect(tl, dc, item->text, DT_LEFT, &item_rect,
                                        &padding_h, &padding_v);
                    mc_rect_inflate(&item_rect, -padding_h, -padding_v);
                    mcDrawThemeText(tl->theme, dc, TVP_TREEITEM, state,
                                        item->text, -1, ITEM_DTFLAGS, 0, &item_rect);
                    if(!(tl->style & MC_TLS_FULLROWSELECT))
                        state = TREIS_NORMAL;
                } else {
                    if(item->state & MC_TLIS_SELECTED) {
                        int old_right = 0;

                        if(tl->focus) {
                            old_bkcolor = SetBkColor(dc, GetSysColor(COLOR_HIGHLIGHT));
                            old_textcolor = SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                        } else {
                            if(!(tl->style & MC_TLS_SHOWSELALWAYS))
                                goto paint_no_sel;
                            old_bkcolor = SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
                            old_textcolor = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
                        }
                        treelist_label_rect(tl, dc, item->text, DT_LEFT,
                                            &item_rect, &padding_h, &padding_v);
                        if(tl->style & MC_TLS_FULLROWSELECT) {
                            old_right = item_rect.right;
                            item_rect.right = tl->scroll_x_max - tl->scroll_x;
                        }
                        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &item_rect, NULL, 0, NULL);
                        if(tl->focus)
                            DrawFocusRect(dc, &item_rect);
                        if(tl->style & MC_TLS_FULLROWSELECT)
                            item_rect.right = old_right;
                        mc_rect_inflate(&item_rect, -padding_h, -padding_v);
                        DrawText(dc, item->text, -1, &item_rect, ITEM_DTFLAGS);

                        if(tl->style & MC_TLS_FULLROWSELECT) {
                            reset_color_after_whole_row = TRUE;
                        } else {
                            SetBkColor(dc, old_bkcolor);
                            SetTextColor(dc, old_textcolor);
                        }
                    } else {
paint_no_sel:
                        mc_rect_inflate(&item_rect, -ITEM_PADDING_H, -ITEM_PADDING_V);
                        DrawText(dc, item->text, -1, &item_rect, ITEM_DTFLAGS);
                    }
                }
            } else {
                /* Paint subitem */
                DWORD justify;

                MC_MSG(tl->header_win, HDM_GETITEM, col_ix, &header_item);
                switch(header_item.fmt & HDF_JUSTIFYMASK) {
                    case HDF_RIGHT:   justify = DT_RIGHT; break;
                    case HDF_CENTER:  justify = DT_CENTER; break;
                    default:          justify = DT_LEFT; break;
                }

                treelist_label_rect(tl, dc, item->subitems[col_ix], justify,
                                    &item_rect, &padding_h, &padding_v);
                mc_rect_inflate(&item_rect, -padding_h, -padding_v);

                if(use_theme) {
                    mcDrawThemeText(tl->theme, dc, TVP_TREEITEM, state,
                            item->subitems[col_ix], -1, ITEM_DTFLAGS | justify,
                            0, &item_rect);
                } else {
                    DrawText(dc, item->subitems[col_ix], -1, &item_rect,
                             ITEM_DTFLAGS | justify);
                }
            }
        }

        if(reset_color_after_whole_row) {
            SetBkColor(dc, old_bkcolor);
            SetTextColor(dc, old_textcolor);
            reset_color_after_whole_row = FALSE;
        }
    }

    if(tl->font)
        SelectObject(dc, old_font);
}

static treelist_item_t*
treelist_hit_test(treelist_t* tl, MC_TLHITTESTINFO* info)
{
    RECT rect;
    RECT header_rect;
    RECT header_item_rect;
    RECT item_rect;
    HDC dc;
    HFONT old_font = NULL;
    int header_height;
    treelist_item_t* item;
    int level;
    int col_ix;
    int y;
    int ignored;

    /* Handle if outside client */
    GetClientRect(tl->win, &rect);
    if(!mc_rect_contains_pt(&rect, &info->pt)) {
        info->flags = 0;
        if(info->pt.x < rect.left)
            info->flags |= MC_TLHT_TOLEFT;
        else if(info->pt.x >= rect.right)
            info->flags |= MC_TLHT_TORIGHT;
        if(info->pt.y < rect.top)
            info->flags |= MC_TLHT_ABOVE;
        else if(info->pt.y >= rect.bottom)
            info->flags |= MC_TLHT_BELOW;
        info->hItem = NULL;
        info->iSubItem = -1;
        return NULL;
    }

    /* Handle if on the header window */
    GetWindowRect(tl->header_win, &header_rect);
    header_height = mc_height(&header_rect);
    if(info->pt.y < header_height) {
        /* List view gets "nowhere" if point hits the header child control.
         * Lets follow it. */
        goto nowhere;
    }

    /* Find the item */
    item = treelist_scrolled_item(tl, &level);
    y = header_height;
    while(item != NULL) {
        if(y >= rect.bottom)
            goto nowhere;

        if(info->pt.y < y + tl->item_height)
            break;

        item = item_next_displayed(item, &level);
        y += tl->item_height;
    }
    if(item == NULL)
        goto nowhere;

    /* Find column */
    info->iSubItem = -1;
    for(col_ix = 0; col_ix < tl->col_count; col_ix++) {
        MC_MSG(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);
        if(header_item_rect.left <= info->pt.x  &&  info->pt.x < header_item_rect.right) {
            info->iSubItem = col_ix;
            break;
        }
    }
    if(info->iSubItem < 0) {
        /* On right after the last column */
        goto nowhere;
    }

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    if(tl->font)
        old_font = SelectObject(dc, tl->font);

    mc_rect_set(&item_rect, header_item_rect.left + ITEM_PAINT_MARGIN_H, y,
                            header_item_rect.right + ITEM_PAINT_MARGIN_H, y + tl->item_height);

    if(info->iSubItem == 0) {
        /* Analyze tree item rect */
        item_rect.left += level * tl->item_indent;
        if(tl->style & MC_TLS_LINESATROOT)
            item_rect.left += tl->item_indent;
        if(level > 0  ||  (tl->style & MC_TLS_LINESATROOT)) {
            if(tl->style & (MC_TLS_HASBUTTONS | MC_TLS_HASLINES)) {
                if((tl->style & MC_TLS_HASBUTTONS)  &&  item_has_children(item)) {
                    RECT button_rect;
                    mc_rect_set(&button_rect, item_rect.left - tl->item_indent, item_rect.top,
                                item_rect.left, item_rect.bottom);
                    if(mc_rect_contains_pt(&button_rect, &info->pt)) {
                        info->flags = MC_TLHT_ONITEMBUTTON;
                        goto done;
                    }
                }
                item_rect.left += ITEM_PAINT_MARGIN_H;
            }
        }

        if(tl->imglist_normal) {
            int img_w, img_h;

            ImageList_GetIconSize(tl->imglist_normal, &img_w, &img_h);
            if(item_rect.left <= info->pt.x  &&  info->pt.x < item_rect.left + img_w) {
                info->flags = MC_TLHT_ONITEMICON;
                goto done;
            }

            item_rect.left += img_w + ITEM_PADDING_H;
        }

        treelist_label_rect(tl, dc, item->text, DT_LEFT, &item_rect,
                            &ignored, &ignored);
    } else {
        /* Analyze subitem rect */
        HDITEM header_item;
        DWORD dtjustify;

        header_item.mask = HDI_FORMAT;
        MC_MSG(tl->header_win, HDM_GETITEM, col_ix, &header_item);
        switch(header_item.fmt) {
            case HDF_RIGHT:   dtjustify = DT_RIGHT; break;
            case HDF_CENTER:  dtjustify = DT_CENTER; break;
            default:          dtjustify = DT_LEFT; break;
        }

        treelist_label_rect(tl, dc, item->subitems[col_ix], dtjustify,
                            &item_rect, &ignored, &ignored);
    }

    if(info->pt.x < item_rect.left)
        info->flags = MC_TLHT_ONITEMLEFT;
    else if(info->pt.x >= item_rect.right)
        info->flags = MC_TLHT_ONITEMRIGHT;
    else
        info->flags = MC_TLHT_ONITEMLABEL;

done:
    if(tl->font)
        SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);
    info->hItem = item;
    return item;

nowhere:
    info->flags = MC_TLHT_NOWHERE;
    info->hItem = NULL;
    info->iSubItem = -1;
    return NULL;
}

static int
treelist_get_item_y(treelist_t* tl, treelist_item_t* item, BOOL visible_only)
{
    RECT header_rect;
    treelist_item_t* it;
    int ignored;
    int y;

    GetWindowRect(tl->header_win, &header_rect);
    y = mc_height(&header_rect);
    if(visible_only) {
        it = tl->scrolled_item;
    } else {
        y -= tl->scroll_y * tl->item_height;
        it = tl->root_head;
    }

    while(TRUE) {
        if(it == item)
            return y;
        if(it == NULL)
            return -1;

        y += tl->item_height;
        it = item_next_displayed(it, &ignored);
    }
}

static int
treelist_ncpaint(treelist_t* tl, HRGN orig_clip)
{
    HDC dc;
    int edge_h, edge_v;
    RECT rect;
    HRGN clip;
    HRGN tmp;
    int ret;

    if(tl->theme == NULL)
        return DefWindowProc(tl->win, WM_NCPAINT, (WPARAM)orig_clip, 0);

    edge_h = GetSystemMetrics(SM_CXEDGE);
    edge_v = GetSystemMetrics(SM_CYEDGE);
    GetWindowRect(tl->win, &rect);

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
    dc = GetWindowDC(tl->win);
    ExcludeClipRect(dc, edge_h, edge_v, rect.right - 2*edge_h, rect.bottom - 2*edge_v);
    if(mcIsThemeBackgroundPartiallyTransparent(tl->theme, 0, 0))
        mcDrawThemeParentBackground(tl->win, dc, &rect);
    mcDrawThemeBackground(tl->theme, dc, 0, 0, &rect, NULL);
    ReleaseDC(tl->win, dc);

    /* Use DefWindowProc() to paint scrollbars */
    ret = DefWindowProc(tl->win, WM_NCPAINT, (WPARAM)clip, 0);
    if(clip != orig_clip)
        DeleteObject(clip);
    return ret;
}

static void
treelist_erasebkgnd(treelist_t* tl, HDC dc)
{
    RECT header_rect;
    RECT rect;
    HBRUSH brush;

    GetWindowRect(tl->header_win, &header_rect);
    GetClientRect(tl->win, &rect);
    rect.top = mc_height(&header_rect);

    brush = mcGetThemeSysColorBrush(tl->theme, COLOR_WINDOW);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

static void
treelist_invalidate_item(treelist_t* tl, treelist_item_t* item, int col_ix, int scroll)
{
    RECT client_rect;
    RECT header_rect;
    RECT rect;

    GetClientRect(tl->win, &client_rect);
    GetWindowRect(tl->header_win, &header_rect);

    /* Calculate item's rect */
    if(col_ix >= 0) {
        MC_MSG(tl->header_win, HDM_GETITEMRECT, col_ix, &rect);
    } else {
        rect.left = client_rect.left;
        rect.right = client_rect.right;
    }
    rect.top = treelist_get_item_y(tl, item, TRUE);
    if(rect.top < 0)
        return;
    rect.bottom = rect.top + tl->item_height;
    InvalidateRect(tl->win, &rect, TRUE);

    if(scroll != 0) {
        client_rect.top = rect.bottom;
        ScrollWindowEx(tl->win, 0, scroll * tl->item_height, &client_rect,
                       &client_rect, NULL, NULL, SW_ERASE | SW_INVALIDATE);
    }
}

static void
treelist_invalidate_column(treelist_t* tl, int col_ix)
{
    RECT tmp;
    RECT rect;

    MC_MSG(tl->header_win, HDM_GETITEMRECT, col_ix, &tmp);
    rect.left = tmp.left + ITEM_PAINT_MARGIN_H;
    rect.right = tmp.right + ITEM_PAINT_MARGIN_H;
    GetWindowRect(tl->header_win, &tmp);
    rect.top = mc_height(&tmp);
    GetClientRect(tl->win, &tmp);
    rect.bottom = tmp.bottom;

    InvalidateRect(tl->win, &rect, TRUE);
}

static void
treelist_do_expand_all(treelist_t* tl)
{
    treelist_item_t* item;

    /* FIXME: expand notifications? use item_has_children()? */

    tl->displayed_items = 0;
    for(item = tl->root_head; item != NULL; item = item_next(item)) {
        tl->displayed_items++;
        if(item->child_head != NULL)
            item->state |= MC_TLIS_EXPANDED;
    }

    treelist_setup_scrollbars(tl);
    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

/* Forward declarations */
static int treelist_do_expand(treelist_t* tl, treelist_item_t* item, BOOL surely_displayed);
static int treelist_do_collapse(treelist_t* tl, treelist_item_t* item, BOOL surely_displayed);
static void treelist_do_select(treelist_t* tl, treelist_item_t* item);

static inline BOOL
treelist_ensure_visible(treelist_t* tl, treelist_item_t* item0, treelist_item_t* item1)
{
    RECT header_rect;
    RECT rect;
    treelist_item_t* it;
    int y0, y1;
    BOOL expanded = FALSE;
    int row_delta;

    MC_ASSERT(item1 == NULL  ||  item1->parent == item0);

    for(it = item0->parent; it != NULL; it = it->parent) {
        if(!(it->state & MC_TLIS_EXPANDED)) {
            if(treelist_do_expand(tl, it, FALSE) == 0)
                expanded = TRUE;
        }
    }

    GetWindowRect(tl->header_win, &header_rect);
    GetClientRect(tl->win, &rect);
    rect.top = mc_height(&header_rect);

    y0 = treelist_get_item_y(tl, item0, FALSE);
    y1 = (item1 == NULL ? y0 : treelist_get_item_y(tl, item1, FALSE)) + tl->item_height;
    if(y1 - y0 > mc_height(&rect))
        y1 = y0 + mc_height(&rect);

    row_delta = 0;
    if(y0 < rect.top)
        row_delta = (y0 - rect.top) / tl->item_height;
    else if(y1 > rect.bottom)
        row_delta = (y1 - rect.bottom + tl->item_height - 1) / tl->item_height;

    if(row_delta != 0) {
        TREELIST_TRACE("treelist_ensure_visible: Scrolling for %d rows.", row_delta);
        treelist_vscroll_rel(tl, row_delta);
    }

    return (expanded ? FALSE : TRUE);
}

static void
treelist_do_select(treelist_t* tl, treelist_item_t* item)
{
    BOOL do_single_expand;
    treelist_item_t* old_sel = tl->selected_item;
    int col_ix;
    MC_NMTREELIST nm = { {0}, 0 };

    if(old_sel == item)
        return;

    /* Send MC_TLN_SELCHANGING */
    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = MC_TLN_SELCHANGING;
    if(old_sel != NULL) {
        nm.hItemOld = old_sel;
        nm.lParamOld = old_sel->lp;
    }
    if(item != NULL) {
        nm.hItemNew = item;
        nm.lParamNew = item->lp;
    }
    if(MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm) != FALSE) {
        TREELIST_TRACE("treelist_do_select: Denied by app.");
        return;
    }

    do_single_expand = (tl->style & MC_TLS_SINGLEEXPAND)  &&
                       !(GetAsyncKeyState(VK_CONTROL) & 0x8000);
    col_ix = (tl->style & MC_TLS_FULLROWSELECT) ? -1 : 0;

    TREELIST_TRACE("treelist_do_select(%S -> %S)",
        (old_sel ? old_sel->text : L"[NULL]"), (item ? item->text : L"[NULL]"));

    if(old_sel) {
        old_sel->state &= ~MC_TLIS_SELECTED;

        if(do_single_expand) {
            /* Collapse the old selection and all its ancestors. */
            treelist_item_t* it;

            /* avoid treelist_do_collapse() to call us reentrantly back. */
            tl->selected_item = NULL;

            for(it = old_sel; it != NULL; it = it->parent) {
                if(it->state & MC_TLIS_EXPANDED)
                    treelist_do_collapse(tl, it, FALSE);
            }
        }

        if(!tl->no_redraw)
            treelist_invalidate_item(tl, old_sel, col_ix, 0);
    }

    if(item) {
        item->state |= MC_TLIS_SELECTED;

        if(do_single_expand) {
            treelist_item_t* it;
            if(item_has_children(item)  &&  !(item->state & MC_TLIS_EXPANDED))
                treelist_do_expand(tl, item, FALSE);
            for(it = item->parent; it != NULL; it = it->parent) {
                if(!(it->state & MC_TLIS_EXPANDED))
                    treelist_do_expand(tl, it, FALSE);
            }
        }

        tl->selected_item = item;

        if(!tl->no_redraw)
            treelist_invalidate_item(tl, item, col_ix, 0);
    }

    /* Send MC_TLN_SELCHANGED */
    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = MC_TLN_SELCHANGED;
    if(old_sel != NULL) {
        nm.hItemOld = old_sel;
        nm.lParamOld = old_sel->lp;
    }
    if(item != NULL) {
        nm.hItemNew = item;
        nm.lParamNew = item->lp;
    }
    MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);
}

static int
treelist_do_expand(treelist_t* tl, treelist_item_t* item, BOOL surely_displayed)
{
    MC_NMTREELIST nm = { {0}, 0 };

    MC_ASSERT(!(item->state & MC_TLIS_EXPANDED));

    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = MC_TLN_EXPANDING;
    nm.action = MC_TLE_EXPAND;
    nm.hItemNew = item;
    nm.lParamNew = item->lp;
    if(MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm) != FALSE) {
        TREELIST_TRACE("treelist_do_expand: Denied by app.");
        return -1;
    }

    item->state |= MC_TLIS_EXPANDED;

    if(surely_displayed || item_is_displayed(item)) {
        treelist_item_t* it;
        int ignored = 0;
        int exposed_items = 0;

        for(it = item->child_head; it != NULL; it = item_next_displayed_ex(it, item, &ignored))
            exposed_items++;
        tl->displayed_items += exposed_items;

        treelist_setup_scrollbars(tl);
        if(!tl->no_redraw)
            treelist_invalidate_item(tl, item, 0, exposed_items);
    }

    treelist_ensure_visible(tl, item, item->child_tail);

    nm.hdr.code = MC_TLN_EXPANDED;
    MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);

    return 0;
}

static int
treelist_do_collapse(treelist_t* tl, treelist_item_t* item, BOOL surely_displayed)
{
    MC_NMTREELIST nm = { {0}, 0 };

    MC_ASSERT(item->state & MC_TLIS_EXPANDED);

    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = MC_TLN_EXPANDING;
    nm.action = MC_TLE_COLLAPSE;
    nm.hItemNew = item;
    nm.lParamNew = item->lp;
    if(MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm) != FALSE) {
        TREELIST_TRACE("treelist_do_collapse: Denied by app.");
        return -1;
    }

    item->state &= ~MC_TLIS_EXPANDED;
    if(surely_displayed || item_is_displayed(item)) {
        treelist_item_t* it;
        int ignored = 0;
        int hidden_items = 0;

        for(it = item->child_head; it != NULL; it = item_next_displayed_ex(it, item, &ignored)) {
            if(it == tl->selected_item)
                treelist_do_select(tl, item);
            hidden_items++;
        }
        tl->displayed_items -= hidden_items;

        treelist_setup_scrollbars(tl);
        if(!tl->no_redraw)
            treelist_invalidate_item(tl, item, 0, -hidden_items);
    }

    nm.hdr.code = MC_TLN_EXPANDED;
    MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);

    return 0;
}

static BOOL
treelist_expand_item(treelist_t* tl, int action, treelist_item_t* item)
{
    BOOL expanded;

    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_expand_item: item == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    expanded = (item->state & MC_TLIS_EXPANDED);

    switch(action) {
        case MC_TLE_EXPAND:
            if(!expanded) {
                return (treelist_do_expand(tl, item, FALSE) == 0);
            } else {
                MC_TRACE("treelist_expand_item: Item already expanded.");
                return FALSE;
            }

        case MC_TLE_COLLAPSE:
            if(expanded) {
                return (treelist_do_collapse(tl, item, FALSE) == 0);
            } else {
                MC_TRACE("treelist_expand_item: Item already collapsed.");
                return FALSE;
            }

        case MC_TLE_TOGGLE:
            if(expanded)
                return (treelist_do_collapse(tl, item, FALSE) == 0);
            else
                return (treelist_do_expand(tl, item, FALSE) == 0);

        default:
            MC_TRACE("treelist_expand_item: Unsupported action %x", action);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }
}

static treelist_item_t*
treelist_last_displayed_item(treelist_t* tl)
{
    treelist_item_t* item = tl->root_tail;
    if(item != NULL) {
        while(item->child_tail != NULL  &&  (item->state & MC_TLIS_EXPANDED))
            item = item->child_tail;
    }
    return item;
}

static treelist_item_t*
treelist_get_next_item(treelist_t* tl, int relation, treelist_item_t* item)
{
    int ignored;

    if(item == MC_TLI_ROOT)
        item = NULL;

    switch(relation) {
        case MC_TLGN_CARET:
            return tl->selected_item;
        case MC_TLGN_ROOT:
            return tl->root_head;
        case MC_TLGN_CHILD:
            return (item == NULL ? tl->root_head : item->child_head);
        case MC_TLGN_FIRSTVISIBLE:
            return treelist_scrolled_item(tl, &ignored);
        case MC_TLGN_LASTVISIBLE:
            /* Note this is not symmetric to MC_TLGN_FIRSTVISIBLE. We follow
             * the silly nomenclature of the treeview here, i.e. we get the
             * last deepest item which has all its parent are expanded). */
            return treelist_last_displayed_item(tl);
    }

    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_expand_item: hItem == TVI_ROOT not allowed "
                 "for specified relation %d", relation);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    switch(relation) {
        case MC_TLGN_NEXT:             return item->sibling_next;
        case MC_TLGN_PREVIOUS:         return item->sibling_prev;
        case MC_TLGN_PARENT:           return item->parent;
        case MC_TLGN_NEXTVISIBLE:      return item_next_displayed(item, &ignored);
        case MC_TLGN_PREVIOUSVISIBLE:  return item_prev_displayed(item);
    }

    MC_TRACE("treelist_expand_item: Unknown relation %d", relation);
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

static BOOL
treelist_is_common_hit(treelist_t* tl, MC_TLHITTESTINFO* info)
{
    const UINT tlht_mask = (MC_TLHT_ONITEMLABEL | MC_TLHT_ONITEMICON);
    const UINT tlht_mask_fullrow = tlht_mask | MC_TLHT_ONITEMRIGHT;

    if(tl->style & MC_TLS_FULLROWSELECT) {
        if(info->iSubItem > 0)
            return TRUE;
        if(info->flags & tlht_mask_fullrow)
            return TRUE;
    } else {
        if(info->iSubItem == 0  &&  (info->flags & tlht_mask))
            return TRUE;
    }

    return FALSE;
}

static void
treelist_left_button(treelist_t* tl, int x, int y, BOOL dblclick)
{
    MC_TLHITTESTINFO info;
    treelist_item_t* item;

    info.pt.x = x;
    info.pt.y = y;
    item = treelist_hit_test(tl, &info);

    if(treelist_is_common_hit(tl, &info)) {
        if(dblclick) {
            if(item->state & MC_TLIS_EXPANDED)
                treelist_do_collapse(tl, item, TRUE);
            else
                treelist_do_expand(tl, item, TRUE);
        } else {
            treelist_do_select(tl, item);
        }
    } else if(info.flags & MC_TLHT_ONITEMBUTTON) {
        if(item->state & MC_TLIS_EXPANDED)
            treelist_do_collapse(tl, item, TRUE);
        else
            treelist_do_expand(tl, item, TRUE);
    }

    if(!dblclick)
        SetFocus(tl->win);
}

static void
treelist_key_down(treelist_t* tl, int key)
{
    if(GetKeyState(VK_CONTROL) & 0x8000) {
        switch(key) {
            case VK_PRIOR:  treelist_vscroll(tl, SB_PAGEUP); break;
            case VK_NEXT:   treelist_vscroll(tl, SB_PAGEDOWN); break;
            case VK_HOME:   treelist_vscroll(tl, SB_TOP); break;
            case VK_END:    treelist_vscroll(tl, SB_BOTTOM); break;
            case VK_UP:     treelist_vscroll(tl, SB_LINEUP); break;
            case VK_DOWN:   treelist_vscroll(tl, SB_LINEDOWN); break;
            case VK_LEFT:   treelist_hscroll(tl, SB_LINELEFT); break;
            case VK_RIGHT:  treelist_hscroll(tl, SB_LINERIGHT); break;
        }
    } else {
        treelist_item_t* old_sel;
        treelist_item_t* sel;
        int ignored;

        old_sel = tl->selected_item;
        sel = old_sel;

        switch(key) {
            case VK_UP:
                sel = item_prev_displayed(sel);
                break;

            case VK_DOWN:
                sel = item_next_displayed(sel, &ignored);
                break;

            case VK_HOME:
                sel = tl->root_head;
                break;

            case VK_END:
                sel = treelist_last_displayed_item(tl);
                break;

            case VK_LEFT:
                if(sel->state & MC_TLIS_EXPANDED)
                    treelist_do_collapse(tl, sel, FALSE);
                else
                    sel = sel->parent;
                break;

            case VK_RIGHT:
                if(item_has_children(sel)) {
                    if(!(sel->state & MC_TLIS_EXPANDED))
                        treelist_do_expand(tl, sel, FALSE);
                    else
                        sel = sel->child_head;
                }
                break;

            case VK_MULTIPLY:
                treelist_do_expand_all(tl);
                break;

            case VK_ADD:
                if(!(sel->state & MC_TLIS_EXPANDED)  &&  item_has_children(sel))
                    treelist_do_expand(tl, sel, FALSE);
                break;

            case VK_SUBTRACT:
                if((sel->state & MC_TLIS_EXPANDED))
                    treelist_do_collapse(tl, sel, FALSE);
                break;

            case VK_PRIOR:
            case VK_NEXT:
            {
                int n;

                n = treelist_items_per_page(tl);
                if(n < 1)
                    n = 1;
                while(n > 0) {
                    treelist_item_t* tmp;
                    if(key == VK_NEXT)
                        tmp = item_next_displayed(sel, &ignored);
                    else
                        tmp = item_prev_displayed(sel);
                    if(tmp == NULL)
                        break;
                    sel = tmp;
                    n--;
                }
                break;
            }

            case VK_BACK:
                sel = sel->parent;
                break;

            case VK_SPACE:
                /* TODO: if has checkbox, toggle its state */
                break;
        }

        if(sel != NULL  &&  sel != old_sel) {
            treelist_do_select(tl, sel);
            treelist_ensure_visible(tl, sel, NULL);
        }
    }
}

static int
treelist_setup_header_item(HDITEM* header_item, const MC_TLCOLUMN* col)
{
    if(MC_ERR(col->fMask & ~MC_TLCF_ALL)) {
        MC_TRACE("treelist_setup_header_item: Unsupported column mask "
                 "0x%x", col->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    header_item->mask = 0;

    if(col->fMask & MC_TLCF_FORMAT) {
        MC_STATIC_ASSERT(HDF_LEFT == MC_TLFMT_LEFT);
        MC_STATIC_ASSERT(HDF_CENTER == MC_TLFMT_CENTER);
        MC_STATIC_ASSERT(HDF_RIGHT == MC_TLFMT_RIGHT);
        MC_STATIC_ASSERT(HDF_JUSTIFYMASK == MC_TLFMT_JUSTIFYMASK);

        header_item->fmt = (col->fmt & MC_TLFMT_JUSTIFYMASK);
        header_item->mask |= HDI_FORMAT;
    }

    if(col->fMask & MC_TLCF_WIDTH) {
        header_item->cxy = col->cx;
        header_item->mask |= HDI_WIDTH;
    }

    if(col->fMask & MC_TLCF_TEXT) {
        header_item->pszText = col->pszText;
        header_item->mask |= HDI_TEXT;
    }

    if(col->fMask & MC_TLCF_IMAGE) {
        header_item->iImage = col->iImage;
        header_item->mask |= HDI_IMAGE;
    }

    if(col->fMask & MC_TLCF_ORDER) {
        header_item->iOrder = col->iOrder;
        header_item->mask |= HDI_ORDER;
    }

    return 0;
}

static int
treelist_insert_column(treelist_t* tl, int col_ix, const MC_TLCOLUMN* col,
                       BOOL unicode)
{
    HDITEM header_item = { 0 };

    TREELIST_TRACE("treelist_insert_column(%p, %d, %p, %d)", tl, col_ix, col, unicode);

    if(col_ix == 0) {
        if(MC_ERR(tl->root_head != NULL)) {
            MC_TRACE("treelist_insert_column: Can not insert column[0] when items exist.");
            SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }
        if(MC_ERR((col->fMask & MC_TLCF_ORDER) && col->iOrder != 0)) {
            MC_TRACE("treelist_insert_column: col[0] must have iOrder == 0");
            SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }
    } else {
        if(MC_ERR((col->fMask & MC_TLCF_ORDER) && col->iOrder == 0)) {
            MC_TRACE("treelist_insert_column: col[%d] must have iOrder != 0", col_ix);
            SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }
    }

    /* Insert new header item */
    if(MC_ERR(treelist_setup_header_item(&header_item, col) != 0)) {
        MC_TRACE("treelist_insert_column: treelist_setup_header_item() failed.");
        return -1;
    }
    if(!(header_item.mask & HDI_WIDTH)) {
        /* Listview defaults to width 10 for new columns. Lets follow it.
         * But for 1st columm (tree) do more as it is generally more space
         * consuming. */
        header_item.mask |= HDI_WIDTH;
        header_item.cxy = (col_ix > 0 ? 10 : 100);
    }
    col_ix = MC_MSG(tl->header_win, (unicode ? HDM_INSERTITEMW : HDM_INSERTITEMA),
                    col_ix, &header_item);
    if(MC_ERR(col_ix == -1)) {
        MC_TRACE_ERR("treelist_insert_column: HDM_INSERTITEM failed");
        return -1;
    }

    if(tl->root_head != NULL) {
        treelist_item_t* item;

        /* Realloc all subitems and init the new one. We do it in two passes
         * to simplify error handling, even at the cost of some performance.
         * Most apps adds column before adding any data. */
        for(item = tl->root_head; item != NULL; item = item_next(item)) {
            TCHAR** subitems;

            subitems = (TCHAR**) realloc(item->subitems, (tl->col_count + 1) * sizeof(TCHAR*));
            if(MC_ERR(subitems == NULL)) {
                MC_TRACE("treelist_insert_column: realloc(subitems) failed.");
                mc_send_notify(tl->notify_win, tl->win, NM_OUTOFMEMORY);
                return -1;
            }
            item->subitems = subitems;
        }

        for(item = tl->root_head; item != NULL; item = item_next(item)) {
            memmove(item->subitems + (col_ix+1), item->subitems + col_ix,
                    (tl->col_count - col_ix) * sizeof(TCHAR*));
            item->subitems[col_ix] = NULL;
        }
    }

    MC_ASSERT(header_item.mask & HDI_WIDTH);
    tl->scroll_x_max += header_item.cxy;
    tl->col_count++;

    treelist_setup_scrollbars(tl);

    if(!tl->no_redraw) {
        RECT header_item_rect;
        RECT rect;

        MC_MSG(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);

        GetClientRect(tl->win, &rect);
        rect.left = header_item_rect.left - tl->scroll_x + ITEM_PAINT_MARGIN_H;
        rect.top = mc_height(&header_item_rect);
        rect.right = tl->scroll_x_max - tl->scroll_x;

        ScrollWindowEx(tl->win, mc_width(&header_item_rect), 0, &rect, &rect,
                    NULL, NULL, SW_ERASE | SW_INVALIDATE);
        if(tl->selected_item != NULL  &&  (tl->style & MC_TLS_FULLROWSELECT))
            treelist_invalidate_item(tl, tl->selected_item, -1, 0);
    }

    return col_ix;
}

static BOOL
treelist_set_column(treelist_t* tl, int col_ix, const MC_TLCOLUMN* col,
                    BOOL unicode)
{
    HDITEM header_item = { 0 };
    BOOL ok;

    TREELIST_TRACE("treelist_set_column(%p, %d, %p, %d)", tl, col_ix, col, unicode);

    if(MC_ERR(treelist_setup_header_item(&header_item, col) != 0)) {
        MC_TRACE("treelist_insert_column: treelist_setup_header_item() failed.");
        return FALSE;
    }

    ok = MC_MSG(tl->header_win, (unicode ? HDM_SETITEMW : HDM_SETITEMA),
                col_ix, &header_item);
    if(MC_ERR(!ok)) {
        MC_TRACE_ERR("treelist_set_column: HDM_SETITEM failed");
        return FALSE;
    }

    /* The header sends HDN_ITEMCHANGING so refresh of contents is handled
     * there. */

    return TRUE;
}

static BOOL
treelist_get_column(treelist_t* tl, int col_ix, MC_TLCOLUMN* col, BOOL unicode)
{
    HDITEM header_item;
    BOOL ok;

    TREELIST_TRACE("treelist_get_column(%p, %d, %p, %d)", tl, col_ix, col, unicode);

    if(MC_ERR(col->fMask & ~MC_TLCF_ALL)) {
        MC_TRACE("treelist_get_column: Unsupported column mask "
                 "0x%x", col->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    header_item.mask = 0;
    if(col->fMask & MC_TLCF_FORMAT)
        header_item.mask |= HDI_FORMAT;
    if(col->fMask & MC_TLCF_WIDTH)
        header_item.mask |= HDI_WIDTH;
    if(col->fMask & MC_TLCF_TEXT) {
        header_item.pszText = col->pszText;
        header_item.cchTextMax = col->cchTextMax;
        header_item.mask |= HDI_TEXT;
    }
    if(col->fMask & MC_TLCF_IMAGE)
        header_item.mask |= HDI_IMAGE;
    if(col->fMask & MC_TLCF_ORDER)
        header_item.mask |= HDI_ORDER;

    ok = MC_MSG(tl->header_win, (unicode ? HDM_GETITEMW : HDM_GETITEMA),
                col_ix, &header_item);
    if(MC_ERR(!ok)) {
        MC_TRACE_ERR("treelist_get_column: HDM_GETITEM failed");
        return FALSE;
    }

    if(col->fMask & MC_TLCF_FORMAT)
        col->fmt = (header_item.fmt & MC_TLFMT_JUSTIFYMASK);
    if(col->fMask & MC_TLCF_WIDTH)
        col->cx = header_item.cxy;
    if(col->fMask & MC_TLCF_IMAGE)
        col->iImage = header_item.iImage;
    if(col->fMask & MC_TLCF_ORDER)
        col->iOrder = header_item.iOrder;
    return TRUE;
}

static BOOL
treelist_delete_column(treelist_t* tl, int col_ix)
{
    RECT header_item_rect;
    BOOL ok;

    TREELIST_TRACE("treelist_get_column(%p, %d)", tl, col_ix);

    if(MC_ERR(tl->root_head != NULL  &&  col_ix == 0)) {
        MC_TRACE("treelist_delete_column: Can not delete col[0] when items exist.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MC_MSG(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);

    ok = MC_MSG(tl->header_win, HDM_DELETEITEM, col_ix, 0);
    if(MC_ERR(!ok)) {
        MC_TRACE("treelist_delete_column: HDM_DELETEITEM failed.");
        return FALSE;
    }

    if(tl->root_head != NULL) {
        treelist_item_t* item;

        /* TODO: we do not save memory by reallocating subitems[] */

        for(item = tl->root_head; item != NULL; item = item_next(item)) {
            if(item->subitems[col_ix])
                free(item->subitems[col_ix]);
            memmove(item->subitems + col_ix, item->subitems + (col_ix+1),
                    (tl->col_count - col_ix - 1) * sizeof(TCHAR*));
        }
    }

    tl->col_count--;
    tl->scroll_x_max -= mc_width(&header_item_rect);
    treelist_setup_scrollbars(tl);

    if(!tl->no_redraw) {
        RECT rect;

        GetClientRect(tl->win, &rect);
        rect.left = header_item_rect.left - tl->scroll_x + ITEM_PAINT_MARGIN_H;
        rect.top = mc_height(&header_item_rect);
        rect.right = tl->scroll_x_max - tl->scroll_x;

        ScrollWindowEx(tl->win, -mc_width(&header_item_rect), 0, &rect, &rect,
                    NULL, NULL, SW_ERASE | SW_INVALIDATE);
        if(tl->selected_item != NULL  &&  (tl->style & MC_TLS_FULLROWSELECT))
            treelist_invalidate_item(tl, tl->selected_item, -1, 0);
    }

    return TRUE;
}

static BOOL
treelist_set_column_order_array(treelist_t* tl, int n, int* array)
{
    if(MC_ERR(n > 0  &&  array[0] != 0)) {
        MC_TRACE("treelist_set_column_order_array: col[0] must stay on order 0");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return MC_MSG(tl->header_win, HDM_SETORDERARRAY, n, array);
}

static treelist_item_t*
treelist_insert_item(treelist_t* tl, MC_TLINSERTSTRUCT* insert, BOOL unicode)
{
    MC_TLITEM* item_data = &insert->item;
    treelist_item_t* parent;
    treelist_item_t* prev;
    treelist_item_t* next;
    TCHAR* text = NULL;
    TCHAR** subitems;
    treelist_item_t* item;
    BOOL parent_displayed;
    BOOL displayed;

    TREELIST_TRACE("treelist_insert_item(%p, %p, %d)", tl, insert, unicode);

    /* Analyze family relations */
    if(MC_ERR(item_data->fMask & ~MC_TLIF_ALL)) {
        MC_TRACE("treelist_insert_item: Unsupported item mask 0x%x",
                 item_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    parent = (insert->hParent == MC_TLI_ROOT ? NULL : insert->hParent);

    if(insert->hInsertAfter == MC_TLI_FIRST) {
        prev = NULL;
    } else if(insert->hInsertAfter == MC_TLI_LAST  ||  insert->hInsertAfter == NULL) {
        prev = (parent ? parent->child_tail : tl->root_tail);
    } else {
        prev = (treelist_item_t*) insert->hInsertAfter;
        if(MC_ERR(prev->parent != parent)) {
            MC_TRACE("treelist_insert_item: MC_TLINSERTSTRUCT::hParent is "
                     "not parent of ::hInsertAfter.");
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
    }

    next = (prev ? prev->sibling_next
                 : (parent ? parent->child_head : tl->root_head));

    /* Allocate */
    item = (treelist_item_t*) malloc(sizeof(treelist_item_t));
    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_insert_item: malloc(item) failed");
        goto err_alloc_item;
    }
    if(item_data->fMask & MC_TLIF_TEXT) {
        text = mc_str(item_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(text == NULL  &&  item_data->pszText != NULL)) {
            MC_TRACE("treelist_insert_item: mc_str() failed.");
            goto err_alloc_text;
        }
    }
    if(tl->col_count > 0) {
        subitems = (TCHAR**) malloc(tl->col_count * sizeof(TCHAR*));
        if(MC_ERR(subitems == NULL)) {
            MC_TRACE("treelist_insert_item: malloc(subitems) failed");
            goto err_alloc_subitems;
        }
        memset(subitems, 0, tl->col_count * sizeof(TCHAR*));
    } else {
        subitems = NULL;
    }

    /* Connect it to the family */
    item->parent = parent;
    item->sibling_prev = prev;
    item->sibling_next = next;
    item->child_head = NULL;
    item->child_tail = NULL;
    if(prev) {
        prev->sibling_next = item;
    } else {
        if(parent)
            parent->child_head = item;
        else
            tl->root_head = item;
    }
    if(next) {
        next->sibling_prev = item;
    } else {
        if(parent)
            parent->child_tail = item;
        else
            tl->root_tail = item;
    }

    /* Setup the item data */
    item->subitems = subitems;
    item->state = ((item_data->fMask & MC_TLIF_STATE)
                            ? (item_data->state & item_data->stateMask) : 0);
    item->text = text;
    item->lp = ((item_data->fMask & MC_TLIF_LPARAM) ? item_data->lParam : 0);
    item->img = (SHORT)((item_data->fMask & MC_TLIF_IMAGE)
                                ? item_data->iImage : MC_I_IMAGENONE);
    item->img_selected = (SHORT)((item_data->fMask & MC_TLIF_SELECTEDIMAGE)
                                ? item_data->iSelectedImage : MC_I_IMAGENONE);
    item->img_expanded = (SHORT)((item_data->fMask & MC_TLIF_EXPANDEDIMAGE)
                                ? item_data->iExpandedImage : MC_I_IMAGENONE);
    item->children = ((item_data->fMask & MC_TLIF_CHILDREN)
                                ? item_data->cChildren : 0);

    if(parent != NULL) {
        parent_displayed = item_is_displayed(parent);
        displayed = (parent_displayed && (parent->state & MC_TLIS_EXPANDED));
    } else {
        parent_displayed = FALSE;
        displayed = TRUE;
    }
    if(displayed) {
        tl->displayed_items++;
        treelist_setup_scrollbars(tl);
    }

    /* Refresh */
    if(!tl->no_redraw) {
        /* If we are 1st child of the parent item, then parent now may need
         * to paint a button. */
        if(parent_displayed  &&  parent->child_head == parent->child_tail  &&
           (tl->style & MC_TLS_HASBUTTONS))
            treelist_invalidate_item(tl, parent, 0, 0);

        /* Scroll down to make space for the new item */
        if(displayed) {
            RECT rect;
            GetClientRect(tl->win, &rect);
            rect.top = treelist_get_item_y(tl, item, TRUE);
            if(rect.top < 0)
                rect.top = treelist_get_item_y(tl, tl->scrolled_item, TRUE);
            ScrollWindowEx(tl->win, 0, tl->item_height, &rect, &rect,
                               NULL, NULL, SW_INVALIDATE | SW_ERASE);
        }
    }

    /* Success */
    return item;

    /* Error path */
err_alloc_subitems:
    if(text)
        free(text);
err_alloc_text:
    free(item);
err_alloc_item:
    mc_send_notify(tl->notify_win, tl->win, NM_OUTOFMEMORY);
    return NULL;
}

static BOOL
treelist_set_item(treelist_t* tl, treelist_item_t* item, MC_TLITEM* item_data,
                  BOOL unicode)
{
    TREELIST_TRACE("treelist_set_item(%p, %p, %p, %d)",
                   tl, item, item_data, unicode);

    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_set_item: hItem == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(item_data->fMask & ~MC_TLIF_ALL)) {
        MC_TRACE("treelist_set_item: Unsupported item mask 0x%x",
                 item_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(item_data->fMask & MC_TLIF_TEXT) {
        TCHAR* text;

        text = mc_str(item_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(text == NULL  &&  item_data->pszText != NULL)) {
            MC_TRACE("treelist_set_item: mc_str() failed.");
            return FALSE;
        }

        if(item->text)
            free(item->text);
        item->text = text;
    }

    if(item_data->fMask & MC_TLIF_LPARAM)
        item->lp = item_data->lParam;

    if(item_data->fMask & MC_TLIF_STATE) {
        WORD state = item->state;

        state &= ~item_data->stateMask;
        state |= (item_data->state & item_data->stateMask);

        if((state & MC_TLIS_EXPANDED) != (item->state & MC_TLIS_EXPANDED)) {
            if(state & MC_TLIS_EXPANDED)
                treelist_do_expand(tl, item, FALSE);
            else
                treelist_do_collapse(tl, item, FALSE);
        }

        if((state & MC_TLIS_SELECTED) != (item->state & MC_TLIS_SELECTED))
            treelist_do_select(tl, (state & MC_TLIS_SELECTED) ? item : NULL);
    }

    if(item_data->fMask & MC_TLIF_IMAGE)
        item->img = item_data->iImage;

    if(item_data->fMask & MC_TLIF_SELECTEDIMAGE)
        item->img_selected = item_data->iSelectedImage;

    if(item_data->fMask & MC_TLIF_EXPANDEDIMAGE)
        item->img_expanded = item_data->iExpandedImage;

    if(item_data->fMask & MC_TLIF_CHILDREN)
        item->children = item_data->cChildren;

    if(!tl->no_redraw)
        treelist_invalidate_item(tl, item, 0, 0);

    return TRUE;
}

static BOOL
treelist_get_item(treelist_t* tl, treelist_item_t* item, MC_TLITEM* item_data,
                  BOOL unicode)
{
    TREELIST_TRACE("treelist_get_item(%p, %p, %p, %d)",
                   tl, item, item_data, unicode);

    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_get_item: hItem == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(item_data->fMask & ~MC_TLIF_ALL)) {
        MC_TRACE("treelist_get_item: Unsupported item mask 0x%x",
                 item_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(item_data->fMask & MC_TLIF_TEXT) {
        mc_str_inbuf(item->text, MC_STRT, item_data->pszText,
                     (unicode ? MC_STRW : MC_STRA), item_data->cchTextMax);
    }

    if(item_data->fMask & MC_TLIF_LPARAM)
        item_data->lParam = item->lp;

    if(item_data->fMask & MC_TLIF_STATE)
        item_data->state = item->state;

    if(item_data->fMask & MC_TLIF_IMAGE)
        item_data->iImage = item->img;

    if(item_data->fMask & MC_TLIF_SELECTEDIMAGE)
        item_data->iSelectedImage = item->img_selected;

    if(item_data->fMask & MC_TLIF_EXPANDEDIMAGE)
        item_data->iExpandedImage = item->img_expanded;

    return TRUE;
}

static void
treelist_delete_notify(treelist_t* tl, treelist_item_t* item)
{
    treelist_item_t* stopper = (item->parent ? item->parent : NULL);
    MC_NMTREELIST nm = { {0}, 0 };

    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = MC_TLN_DELETEITEM;

    while(item != NULL) {
        nm.hItemOld = item;
        nm.lParamOld = item->lp;
        MC_MSG(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);

        item = item_next_ex(item, stopper);
    }
}

/* This is helper for treelist_delete_item(). It physically deletes the item
 * as well as all items linked through sibling_next, and all their children. */
static int
treelist_delete_item_helper(treelist_t* tl, treelist_item_t* item, BOOL displayed)
{
    treelist_item_t* next_to_delete;
    int deleted_visible = 0;

    while(item != NULL) {
        /* Handle deletion of children */
        if(item->child_head != NULL) {
            if(displayed  &&  !(item->state & MC_TLIS_EXPANDED)) {
                /* Unlike this item, all the children are hidden. So recurse
                 * without getting the return value. */
                treelist_delete_item_helper(tl, item->child_head, FALSE);
                next_to_delete = item->sibling_next;
            } else {
                /* Artificially "upgrade" the children to our level (small
                 * trick to avoid recursion). */
                item->child_tail->sibling_next = item->sibling_next;
                next_to_delete = item->child_head;
            }
        } else {
            next_to_delete = item->sibling_next;
        }

        /* The deletion of the item */
        if(tl->selected_item == item)
            tl->selected_item = NULL;
        if(item->subitems) {
            int i;
            for(i = 0; i < tl->col_count; i++) {
                if(item->subitems[0])
                    free(item->subitems[0]);
            }
            free(item->subitems);
        }
        if(item->text)
            free(item->text);
        free(item);

        deleted_visible++;

        item = next_to_delete;
    }

    return deleted_visible;
}

static BOOL
treelist_delete_item(treelist_t* tl, treelist_item_t* item)
{
    DWORD old_displayed_items = tl->displayed_items;
    treelist_item_t* old_selected_item = tl->selected_item;
    treelist_item_t* sel_replacement;
    treelist_item_t* parent = NULL;
    BOOL parent_displayed;
    BOOL displayed;
    int displayed_del_count;
    int y = -1;

    TREELIST_TRACE("treelist_delete_item(%p, %p)", tl, item);

    if(item == MC_TLI_ROOT  ||  item == NULL) {
        /* Delete all items */
        if(tl->root_head != NULL) {
            treelist_delete_notify(tl, tl->root_head);
            treelist_delete_item_helper(tl, tl->root_head, FALSE);
            tl->root_head = NULL;
            tl->root_tail = NULL;
            tl->displayed_items = 0;
            tl->selected_item = NULL;
            tl->scrolled_item = NULL;
            treelist_setup_scrollbars(tl);
            if(!tl->no_redraw)
                InvalidateRect(tl->win, NULL, TRUE);
        }
        return TRUE;
    }

    /* Inspect status of parent */
    parent = item->parent;
    if(parent != NULL) {
        parent_displayed = item_is_displayed(parent);
        displayed = (parent_displayed  &&  (parent->state & MC_TLIS_EXPANDED));
    } else {
        parent_displayed = FALSE;
        displayed = TRUE;
    }
    if(parent  &&  parent->child_head == parent->child_tail)
        parent->state &= ~MC_TLIS_EXPANDED;

    /* Remeber top of the dirty rect. */
    if(displayed)
        y = treelist_get_item_y(tl, item, TRUE);

    /* if the deleted subtree contains selection, we must know what to select
     * instead before we delete it. */
    if(item->sibling_next)
        sel_replacement = item->sibling_next;
    else if(item->sibling_prev)
        sel_replacement = item->sibling_prev;
    else
        sel_replacement = parent;

    /* Disconnect from the tree */
    if(item->sibling_prev) {
        item->sibling_prev->sibling_next = item->sibling_next;
    } else {
        if(item->parent)
            item->parent->child_head = item->sibling_next;
        else
            tl->root_head = item->sibling_next;
    }
    if(item->sibling_next) {
        item->sibling_next->sibling_prev = item->sibling_prev;
    } else {
        if(item->parent)
            item->parent->child_tail = item->sibling_prev;
        else
            tl->root_tail = item->sibling_prev;
    }
    item->sibling_next = NULL;  /* stopper for treelist_delete_item_helper() */

    /* Delete the item and whole its subtree, and count how many of deleted
     * items were displayed. */
    treelist_delete_notify(tl, item);
    displayed_del_count = treelist_delete_item_helper(tl, item, displayed);
    if(displayed)
        tl->displayed_items -= displayed_del_count;

    /* If the selected item was deleted too, select another one. */
    if(tl->selected_item != old_selected_item  &&  sel_replacement != NULL)
        treelist_do_select(tl, sel_replacement);

    /* Refresh */
    if(tl->displayed_items != old_displayed_items) {
        treelist_setup_scrollbars(tl);
        if(!tl->no_redraw) {
            /* If it was last child of parent, then the parent must be collapsed
             * and painted without a button */
            if(parent != NULL  &&  parent->child_head == NULL  &&  parent->child_tail == NULL)
                treelist_invalidate_item(tl, parent, 0, 0);

            /* Scroll the items below up to the place of the deleted ones. */
            if(y >= 0) {
                RECT rect;
                GetClientRect(tl->win, &rect);
                if(y < rect.bottom) {
                    rect.top = y;
                    ScrollWindowEx(tl->win, 0, -(displayed_del_count * tl->item_height),
                                   &rect, &rect, NULL, NULL, SW_INVALIDATE | SW_ERASE);
                }
            }
            tl->scrolled_item = NULL;
        }
    }

    return TRUE;
}

static BOOL
treelist_set_subitem(treelist_t* tl, treelist_item_t* item,
                     MC_TLSUBITEM* subitem_data, BOOL unicode)
{
    TREELIST_TRACE("treelist_set_subitem(%p, %p, %p, %d)",
                   tl, item, subitem_data, unicode);

    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_set_subitem: hItem == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(subitem_data->iSubItem < 1  ||  subitem_data->iSubItem >= tl->col_count)) {
        /* iSubItem == 0 is responsibility of item, not subitem!!! */
        MC_TRACE("treelist_set_subitem: Invalid iSubItem %d", subitem_data->iSubItem);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(subitem_data->fMask & ~MC_TLSIF_ALL)) {
        MC_TRACE("treelist_set_subitem: Unsupported subitem mask 0x%x",
                 subitem_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(subitem_data->fMask & MC_TLSIF_TEXT) {
        TCHAR* text;

        text = mc_str(subitem_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(text == NULL  &&  subitem_data->pszText != NULL)) {
            MC_TRACE("treelist_set_subitem: mc_str() failed.");
            return FALSE;
        }

        if(item->subitems[subitem_data->iSubItem])
            free(item->subitems[subitem_data->iSubItem]);
        item->subitems[subitem_data->iSubItem] = text;
    }

    if(!tl->no_redraw)
        treelist_invalidate_item(tl, item, subitem_data->iSubItem, 0);

    return TRUE;
}

static BOOL
treelist_get_subitem(treelist_t* tl, treelist_item_t* item,
                     MC_TLSUBITEM* subitem_data, BOOL unicode)
{
    TREELIST_TRACE("treelist_get_subitem(%p, %p, %p, %d)",
                   tl, item, subitem_data, unicode);

    if(MC_ERR(item == NULL)) {
        MC_TRACE("treelist_get_subitem: hItem == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(subitem_data->iSubItem < 1  ||  subitem_data->iSubItem >= tl->col_count)) {
        /* iSubItem == 0 is responsibility of item, not subitem! */
        MC_TRACE("treelist_get_subitem: Invalid iSubItem %d", subitem_data->iSubItem);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(subitem_data->fMask & ~MC_TLSIF_ALL)) {
        MC_TRACE("treelist_get_subitem: Unsupported subitem mask 0x%x",
                 subitem_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(subitem_data->fMask & MC_TLSIF_TEXT) {
        mc_str_inbuf(item->subitems[subitem_data->iSubItem],
                     MC_STRT,
                     subitem_data->pszText,
                     (unicode ? MC_STRW : MC_STRA), subitem_data->cchTextMax);
    }

    return TRUE;
}

static void
treelist_set_indent(treelist_t* tl, int indent)
{
    if(indent < ITEM_INDENT_MIN)
        indent = ITEM_INDENT_MIN;

    tl->item_indent = indent;
    if(!tl->no_redraw)
        treelist_invalidate_column(tl, 0);
}

static void
treelist_set_imagelist(treelist_t* tl, HIMAGELIST imglist)
{
    TREELIST_TRACE("treelist_set_imagelist(%p, %p)", tl, imglist);

    if(imglist == tl->imglist_normal)
        return;

    tl->imglist_normal = imglist;
    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

static int
treelist_header_notify(treelist_t* tl, NMHEADER* info)
{
    switch(info->hdr.code) {
        case HDN_BEGINDRAG:   /* We disable reoder of the column[0] */
        case HDN_ENDDRAG:     /* (both as source as well as destination) */
            if(info->iItem == 0)
                return TRUE;

            /* It seems header control is buggy (Win 7, COMCTL32.DLL version 6).
             * Sometimes it sends wrong info->iItem for these notifications.
             * We workaround with consulting also iOrder. */
            {
                int order;

                if(info->pitem != NULL && (info->pitem->mask & HDI_ORDER)) {
                    order = info->pitem->iOrder;
                } else {
                    HDITEM item;
                    item.mask = HDI_ORDER;
                    MC_MSG(tl->header_win, HDM_GETITEM, info->iItem, &item);
                    order = item.iOrder;
                }

                if(order == 0) {
                    TREELIST_TRACE("treelist_header_notify: iOrder workaround "
                                   "took effect.");
                    return TRUE;
                }
            }

            if(info->hdr.code == HDN_ENDDRAG  &&  !tl->no_redraw) {
                /* This could be probably optimized with ScrollWIndowEx()
                 * but there is a problem with the bug mention above, so lets
                 * go the safer way. */
                InvalidateRect(tl->win, NULL, TRUE);
            }
            return FALSE;

        case HDN_ITEMCHANGING:
            if(info->pitem  &&  (info->pitem->mask & HDI_WIDTH)) {
                RECT header_item_rect;
                int old_width;
                int new_width = info->pitem->cxy;

                MC_MSG(tl->header_win, HDM_GETITEMRECT, info->iItem, &header_item_rect);
                old_width = mc_width(&header_item_rect);

                tl->scroll_x_max += new_width - old_width;
                treelist_setup_scrollbars(tl);

                if(!tl->no_redraw) {
                    RECT rect;
                    GetClientRect(tl->win, &rect);
                    rect.left = header_item_rect.right + ITEM_PAINT_MARGIN_H;
                    rect.top = mc_height(&header_item_rect);
                    ScrollWindowEx(tl->win, new_width - old_width, 0, &rect, &rect,
                                   NULL, NULL, SW_ERASE | SW_INVALIDATE);
                    treelist_invalidate_column(tl, info->iItem);
                    if(tl->selected_item != NULL  &&  (tl->style & MC_TLS_FULLROWSELECT))
                        treelist_invalidate_item(tl, tl->selected_item, -1, 0);
                }
            }
            return FALSE;
    }

    return 0;
}

static void
treelist_notify_format(treelist_t* tl)
{
    LRESULT lres;
    lres = MC_MSG(tl->notify_win, WM_NOTIFYFORMAT, tl->win, NF_QUERY);
    tl->unicode_notifications = (lres == NFR_UNICODE ? 1 : 0);
    TREELIST_TRACE("treelist_notify_format: Will use %s notifications.",
                   tl->unicode_notifications ? "Unicode" : "ANSI");
}

static void
treelist_theme_changed(treelist_t* tl)
{
    if(tl->theme)
        mcCloseThemeData(tl->theme);
    tl->theme = mcOpenThemeData(tl->win, treelist_tc);

    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

static treelist_t*
treelist_nccreate(HWND win, CREATESTRUCT* cs)
{
    treelist_t* tl;

    tl = (treelist_t*) malloc(sizeof(treelist_t));
    if(MC_ERR(tl == NULL)) {
        MC_TRACE("treelist_nccreate: malloc() failed.");
        return NULL;
    }

    memset(tl, 0, sizeof(treelist_t));
    tl->win = win;
    tl->notify_win = cs->hwndParent;
    tl->style = cs->style;
    tl->item_height = treelist_natural_item_height(tl);
    tl->item_indent = ITEM_INDENT_MIN;

    treelist_notify_format(tl);

    mcBufferedPaintInit();
    return tl;
}

static int
treelist_create(treelist_t* tl)
{
    DWORD header_style;

    header_style = WS_CHILD | WS_VISIBLE | HDS_HORZ | HDS_FULLDRAG | HDS_HOTTRACK | HDS_BUTTONS;
    if(tl->style & MC_TLS_NOCOLUMNHEADER)
        header_style |= HDS_HIDDEN;
    if(tl->style & MC_TLS_HEADERDRAGDROP)
        header_style |= HDS_DRAGDROP | HDS_FULLDRAG;

    tl->header_win = CreateWindow(WC_HEADER, NULL, header_style, 0, 0, 0, 0,
                                  tl->win, NULL, mc_instance, NULL);
    if(MC_ERR(tl->header_win == NULL)) {
        MC_TRACE_ERR("treelist_create: CreateWindow(header) failed");
        return -1;
    }
#ifdef UNICODE
    MC_MSG(tl->header_win, HDM_SETUNICODEFORMAT, TRUE, 0);
#else
    MC_MSG(tl->header_win, HDM_SETUNICODEFORMAT, FALSE, 0);
#endif

    tl->theme = mcOpenThemeData(tl->win, treelist_tc);
    return 0;
}

static void
treelist_destroy(treelist_t* tl)
{
    treelist_delete_item(tl, NULL);

    if(tl->theme) {
        mcCloseThemeData(tl->theme);
        tl->theme = NULL;
    }
}

static void
treelist_ncdestroy(treelist_t* tl)
{
    mcBufferedPaintUnInit();
    free(tl);
}

static LRESULT CALLBACK
treelist_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    treelist_t* tl = (treelist_t*) GetWindowLongPtr(win, 0);
    int ignored;

    switch(msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(win, &ps);
            if(!tl->no_redraw) {
                if(tl->style & MC_TLS_DOUBLEBUFFER)
                    mc_doublebuffer(tl, &ps, treelist_paint);
                else
                    treelist_paint(tl, ps.hdc, &ps.rcPaint, ps.fErase);
            }
            EndPaint(win, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            RECT rect;
            GetClientRect(win, &rect);
            treelist_paint(tl, (HDC) wp, &rect, TRUE);
            return 0;
        }

        case WM_NCPAINT:
            return treelist_ncpaint(tl, (HRGN) wp);

        case WM_ERASEBKGND:
            treelist_erasebkgnd(tl, (HDC) wp);
            return TRUE;

        case MC_TLM_INSERTCOLUMNW:
        case MC_TLM_INSERTCOLUMNA:
            return treelist_insert_column(tl, (int) wp, (const MC_TLCOLUMN*) lp,
                                          (msg == MC_TLM_INSERTCOLUMNW));

        case MC_TLM_SETCOLUMNW:
        case MC_TLM_SETCOLUMNA:
            return treelist_set_column(tl, (int) wp, (const MC_TLCOLUMN*) lp,
                                       (msg == MC_TLM_SETCOLUMNW));

        case MC_TLM_GETCOLUMNW:
        case MC_TLM_GETCOLUMNA:
            return treelist_get_column(tl, (int) wp, (MC_TLCOLUMN*) lp,
                                       (msg == MC_TLM_GETCOLUMNW));

        case MC_TLM_DELETECOLUMN:
            return treelist_delete_column(tl, (int) wp);

        case MC_TLM_SETCOLUMNORDERARRAY:
            return treelist_set_column_order_array(tl, wp, (int*) lp);

        case MC_TLM_GETCOLUMNORDERARRAY:
            return MC_MSG(tl->header_win, HDM_GETORDERARRAY, wp, lp);

        case MC_TLM_SETCOLUMNWIDTH:
        {
            MC_TLCOLUMN col;
            col.fMask = MC_TLCF_WIDTH;
            col.cx = lp;
            return treelist_set_column(tl, wp, &col, TRUE);
        }

        case MC_TLM_GETCOLUMNWIDTH:
        {
            HDITEM header_item;
            BOOL ok;
            header_item.mask = HDI_WIDTH;
            ok = MC_MSG(tl->header_win, HDM_GETITEM, wp, &header_item);
            if(MC_ERR(!ok)) {
                MC_TRACE("treelist_get_column_width(%d): HDM_GETITEM failed.", wp);
                return 0;
            }
            return header_item.cxy;
        }

        case MC_TLM_INSERTITEMW:
        case MC_TLM_INSERTITEMA:
            return (LRESULT) treelist_insert_item(tl, (MC_TLINSERTSTRUCT*) lp,
                                     (msg == MC_TLM_INSERTITEMW));

        case MC_TLM_SETITEMW:
        case MC_TLM_SETITEMA:
            return treelist_set_item(tl, (treelist_item_t*) wp, (MC_TLITEM*) lp,
                                     (msg == MC_TLM_SETITEMW));

        case MC_TLM_GETITEMW:
        case MC_TLM_GETITEMA:
            return treelist_get_item(tl, (treelist_item_t*) wp, (MC_TLITEM*) lp,
                                     (msg == MC_TLM_GETITEMW));

        case MC_TLM_DELETEITEM:
            return treelist_delete_item(tl, (treelist_item_t*) lp);

        case MC_TLM_SETITEMHEIGHT:
            return treelist_set_item_height(tl, (int) wp, TRUE);

        case MC_TLM_GETITEMHEIGHT:
            return tl->item_height;

        case MC_TLM_SETSUBITEMW:
        case MC_TLM_SETSUBITEMA:
            return treelist_set_subitem(tl, (treelist_item_t*) wp,
                            (MC_TLSUBITEM*) lp, (msg == MC_TLM_SETSUBITEMW));

        case MC_TLM_GETSUBITEMW:
        case MC_TLM_GETSUBITEMA:
            return treelist_get_subitem(tl, (treelist_item_t*) wp,
                            (MC_TLSUBITEM*) lp, (msg == MC_TLM_GETSUBITEMW));

        case MC_TLM_SETINDENT:
            treelist_set_indent(tl, wp);
            return 0;

        case MC_TLM_GETINDENT:
            return tl->item_indent;

        case MC_TLM_HITTEST:
            return (LRESULT) treelist_hit_test(tl, (MC_TLHITTESTINFO*) lp);

        case MC_TLM_EXPAND:
            return treelist_expand_item(tl, wp, (treelist_item_t*) lp);

        case MC_TLM_GETNEXTITEM:
            return (LRESULT) treelist_get_next_item(tl, wp, (treelist_item_t*) lp);

        case MC_TLM_GETVISIBLECOUNT:
            return treelist_items_per_page(tl);

        case MC_TLM_ENSUREVISIBLE:
            return treelist_ensure_visible(tl, (treelist_item_t*) lp, NULL);

        case MC_TLM_SETIMAGELIST:
        {
            HIMAGELIST imglist = tl->imglist_normal;
            treelist_set_imagelist(tl, (HIMAGELIST) lp);
            return (LRESULT) imglist;
        }

        case MC_TLM_GETIMAGELIST:
            return (LRESULT) tl->imglist_normal;

        case WM_NOTIFY:
            if(((NMHDR*)lp)->hwndFrom == tl->header_win)
                return treelist_header_notify(tl, (NMHEADER*) lp);
            break;

        case WM_SIZE:
            treelist_layout_header(tl);
            treelist_setup_scrollbars(tl);
            if(!tl->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_VSCROLL:
            treelist_vscroll(tl, LOWORD(wp));
            return 0;

        case WM_HSCROLL:
            treelist_hscroll(tl, LOWORD(wp));
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            /* Std. listview does not scroll when SHIFT or CONTROL is pressed */
            if(!(wp & (MK_SHIFT | MK_CONTROL))) {
                treelist_mouse_wheel(tl, (msg == WM_MOUSEWHEEL), (int)(SHORT)HIWORD(wp));
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            treelist_left_button(tl, GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                 (msg == WM_LBUTTONDBLCLK));
            return 0;

        case WM_KEYDOWN:
            treelist_key_down(tl, wp);
            return 0;

        case WM_SETFOCUS:
            if(tl->selected_item == NULL) {
                treelist_item_t* item;
                item = treelist_scrolled_item(tl, &ignored);
                if(item)
                    treelist_do_select(tl, item);
            }
            /* Fall through */
        case WM_KILLFOCUS:
            tl->focus = (msg == WM_SETFOCUS);
            if(!tl->no_redraw  &&  tl->selected_item != NULL) {
                treelist_invalidate_item(tl, tl->selected_item,
                        ((tl->style & MC_TLS_FULLROWSELECT) ? -1 : 0), 0);
            }
            return 0;

        case WM_GETFONT:
            return (LRESULT) tl->font;

        case WM_SETFONT:
            tl->font = (HFONT) wp;
            MC_MSG(tl->header_win, WM_SETFONT, wp, lp);
            treelist_set_item_height(tl, -1, (BOOL) lp);
            return 0;

        case WM_SETREDRAW:
            tl->no_redraw = !wp;
            if(!tl->no_redraw  &&  tl->dirty_scrollbars)
                treelist_setup_scrollbars(tl);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                treelist_setup_scrollbars(tl);
                if(!tl->no_redraw)
                    InvalidateRect(tl->win, NULL, TRUE);
                return 0;
            }
            break;

        case WM_THEMECHANGED:
            treelist_theme_changed(tl);
            return 0;

        case WM_NOTIFYFORMAT:
            if(lp == NF_REQUERY)
                treelist_notify_format(tl);
            return (tl->unicode_notifications ? NFR_UNICODE : NFR_ANSI);

        case CCM_SETUNICODEFORMAT:
        {
            BOOL tmp = tl->unicode_notifications;
            tl->unicode_notifications = (wp != 0);
            return tmp;
        }

        case CCM_GETUNICODEFORMAT:
            return tl->unicode_notifications;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = tl->notify_win;
            tl->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case CCM_SETWINDOWTHEME:
            mcSetWindowTheme(win, (const WCHAR*) lp, NULL);
            return 0;

        case WM_NCCREATE:
            tl = (treelist_t*) treelist_nccreate(win, (CREATESTRUCT*) lp);
            if(MC_ERR(tl == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR) tl);
            break;

        case WM_CREATE:
            return treelist_create(tl);

        case WM_DESTROY:
            treelist_destroy(tl);
            break;

        case WM_NCDESTROY:
            if(tl)
                treelist_ncdestroy(tl);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}

int
treelist_init_module(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_GLOBALCLASS | CS_DBLCLKS;
    wc.lpfnWndProc = treelist_proc;
    wc.cbWndExtra = sizeof(treelist_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = treelist_wc;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("treelist_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
treelist_fini_module(void)
{
    UnregisterClass(treelist_wc, NULL);
}
