/*
 * Copyright (c) 2013-2015 Martin Mitas
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
#include "generic.h"
#include "mousewheel.h"
#include "theme.h"
#include "tooltip.h"


/* TODO:
 *  -- Incremental search.
 *  -- Handling state and style MC_TLS_CHECKBOXES.
 *  -- Support editing labels and style MC_TLS_EDITLABELS and related
 *     notifications.
 */


/* Uncomment this to have more verbose traces from this module. */
/* #define TREELIST_DEBUG     1 */

#ifdef TREELIST_DEBUG
    #define TREELIST_TRACE      MC_TRACE
#else
    #define TREELIST_TRACE      MC_NOOP
#endif


/* Note:
 *   In comments and function names in this module, the term "displayed item"
 *   means the item is not hidden due any collapsed item on its parent chain
 *   upward to the root. It does NOT mean the item must be scrolled into the
 *   current view-port.
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

    /* For new items, we try to use a callback_map: Each bit corresponds to
     * a column where zero means the subitem is NULL, and 1 means it is
     * MC_LPSTR_TEXTCALLBACK.
     *
     * subitems[] is only allocated lazily when needed, i.e. any subitem is
     * set to a string (i.e. not NULL and not MC_LPSTR_TEXTCALLBACK), or if
     * the callback_map cannot hold the 1 because there are too many subitems
     * (i.e. if col_count >= 8 * sizeof(callback_map)).
     *
     * However once allocated, it keeps allocated for lifetime of the item.
     */
    union {
        TCHAR** subitems;
        uintptr_t callback_map;
    };

    LPARAM lp;
    SHORT img;
    SHORT img_selected;
    SHORT img_expanded;
    WORD state                        : 8;
    WORD children                     : 1;
    WORD children_callback            : 1;
    /* Flag treelist_do_expand/collapse() is in progress. Used to detect nested
     * call (i.e. from the notification) to prevent endless recursion.
     * Typically happens for (MC_TLE_COLLAPSE|MC_TLE_COLLAPSERESET) in
     * dynamically populated tree list. */
    WORD expanding_notify_in_progress : 1;
    /* If set, then ->subitems[] is alloc'ed and valid, otherwise ->callback_map */
    WORD has_alloced_subitems         : 1;
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

/* Iterator over items displayed (i.e. not hidden by collapsed parent) below
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
item_is_ancestor(treelist_item_t* ancestor, treelist_item_t* item)
{
    while(item != NULL) {
        if(item == ancestor)
            return TRUE;
        item = item->parent;
    }
    return FALSE;
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


typedef struct treelist_tag treelist_t;
struct treelist_tag {
    HWND win;
    HWND header_win;
    HWND tooltip_win;
    HWND notify_win;
    HTHEME theme;
    HFONT font;
    HIMAGELIST imglist;
    treelist_item_t* root_head;
    treelist_item_t* root_tail;
    treelist_item_t* scrolled_item;   /* can be NULL if not known */
    treelist_item_t* selected_from;
    treelist_item_t* selected_last;
    treelist_item_t* hot_item;
    treelist_item_t* hotbutton_item;
    int scrolled_level;               /* level of the scrolled_item */
    DWORD style                  : 16;
    DWORD no_redraw              :  1;
    DWORD unicode_notifications  :  1;
    DWORD rtl                    :  1;
    DWORD dirty_scrollbars       :  1;
    DWORD item_height_set        :  1;
    DWORD focus                  :  1;
    DWORD tracking_leave         :  1;
    DWORD theme_treeitem_defined :  1;
    DWORD theme_hotglyph_defined :  1;
    DWORD active_tooltip         :  1;
    DWORD displayed_items;
    WORD col_count;
    WORD item_height;
    WORD item_indent;
    SHORT hot_col;
    WORD scroll_y;                    /* in rows */
    int scroll_x;                     /* in pixels */
    int scroll_x_max;
    unsigned int selected_count;
};


#define MC_TLCF_ALL     (MC_TLCF_FORMAT | MC_TLCF_WIDTH | MC_TLCF_TEXT |      \
                         MC_TLCF_IMAGE | MC_TLCF_ORDER)

#define MC_TLIF_ALL     (MC_TLIF_STATE | MC_TLIF_TEXT | MC_TLIF_PARAM |       \
                         MC_TLIF_IMAGE | MC_TLIF_SELECTEDIMAGE |              \
                         MC_TLIF_EXPANDEDIMAGE | MC_TLIF_CHILDREN)

#define MC_TLSIF_ALL    (MC_TLSIF_TEXT)


#define SCROLL_H_UNIT              5
#define EMPTY_SELECT_WIDTH        80
#define ITEM_INDENT_MIN           19  /* minimal item indent (per level) */
#define ITEM_HEIGHT_MIN           16  /* minimal item height */
#define ITEM_HEIGHT_FONT_MARGIN_V  3  /* margin of item height derived from font size */
#define ITEM_PADDING_H             2  /* inner padding of the item */
#define ITEM_PADDING_V             1  /* inner padding of the item */
#define ITEM_PADDING_H_THEMEEXTRA  2  /* when using theme, use extra more horiz. padding. */
#define ITEM_DTFLAGS              (DT_EDITCONTROL | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER | DT_END_ELLIPSIS)


typedef struct treelist_dispinfo_tag treelist_dispinfo_t;
struct treelist_dispinfo_tag {
    TCHAR* text;
    int img;
    int img_selected;
    int img_expanded;
    int children;
};


#define CALLBACK_MAP_SIZE       (sizeof(uintptr_t) * 8)
#define CALLBACK_MAP_BIT(i)     ((uintptr_t)1 << (i))


static inline TCHAR*
treelist_subitem_text(treelist_t* tl, treelist_item_t* item, int subitem_id)
{
    int i = subitem_id - 1;

    if(item->has_alloced_subitems)
        return item->subitems[i];
    else if(i < CALLBACK_MAP_SIZE  &&  (item->callback_map & CALLBACK_MAP_BIT(i)))
        return MC_LPSTR_TEXTCALLBACK;
    else
        return NULL;
}

static int
treelist_subitems_alloc(treelist_t* tl, treelist_item_t* item, WORD col_count)
{
    TCHAR** subitems;
    WORD count = col_count - 1;

    /* col_count is used to request more space when called from
     * treelist_insert_column() for the new column */
    MC_ASSERT(col_count == tl->col_count || col_count == tl->col_count + 1);
    MC_ASSERT(item->has_alloced_subitems == FALSE);

    subitems = (TCHAR**) malloc(sizeof(TCHAR*) * count);
    if(MC_ERR(subitems == NULL)) {
        MC_TRACE("treelist_subitem_alloc: malloc() failed.");
        mc_send_notify(tl->win, tl->notify_win, NM_OUTOFMEMORY);
        return -1;
    }

    memset(subitems, 0, sizeof(TCHAR*) * count);

    /* Convert the item->callback_map into allocated subitems. */
    if(item->callback_map != 0) {
        int i;
        int n = MC_MIN(count, CALLBACK_MAP_SIZE);

        for(i = 0; i < n; i++) {
            if(item->callback_map & CALLBACK_MAP_BIT(i))
                subitems[i] = MC_LPSTR_TEXTCALLBACK;
        }
    }

    item->subitems = subitems;
    item->has_alloced_subitems = TRUE;
    return 0;
}

static void
treelist_get_dispinfo(treelist_t* tl, treelist_item_t* item, treelist_dispinfo_t* di, DWORD mask)
{
    MC_NMTLDISPINFO info;

    MC_ASSERT((mask & ~(MC_TLIF_TEXT | MC_TLIF_IMAGE | MC_TLIF_SELECTEDIMAGE |
                        MC_TLIF_EXPANDEDIMAGE | MC_TLIF_CHILDREN)) == 0);

    if(item->text != MC_LPSTR_TEXTCALLBACK) {
        di->text = item->text;
        mask &= ~MC_TLIF_TEXT;
    }
    if(item->img != MC_I_IMAGECALLBACK) {
        di->img = item->img;
        mask &= ~MC_TLIF_IMAGE;
    }
    if(item->img_selected != MC_I_IMAGECALLBACK) {
        di->img_selected = item->img_selected;
        mask &= ~MC_TLIF_SELECTEDIMAGE;
    }
    if(item->img_expanded != MC_I_IMAGECALLBACK) {
        di->img_expanded = item->img_expanded;
        mask &= ~MC_TLIF_EXPANDEDIMAGE;
    }
    if(!item->children_callback) {
        di->children = item->children;
        mask &= ~MC_TLIF_CHILDREN;
    }

    if(mask == 0)
        return;

    info.hdr.hwndFrom = tl->win;
    info.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    info.hdr.code = (tl->unicode_notifications ? MC_TLN_GETDISPINFOW : MC_TLN_GETDISPINFOA);
    info.hItem = (MC_HTREELISTITEM) item;
    info.item.fMask = mask;
    info.item.lParam = item->lp;
    MC_SEND(tl->notify_win, WM_NOTIFY, 0, &info);

    if(mask & MC_TLIF_TEXT) {
        if(tl->unicode_notifications == MC_IS_UNICODE)
            di->text = info.item.pszText;
        else
            di->text = mc_str(info.item.pszText, (tl->unicode_notifications ? MC_STRW : MC_STRA), MC_STRT);
    }

    /* Small optimization: We do not ask about the corresponding bits in the
     * mask for these. If not set, the assignment does no hurt and we save few
     * instructions. */
    di->img = info.item.iImage;
    di->img_selected = info.item.iSelectedImage;
    di->img_expanded = info.item.iExpandedImage;
    di->children = (info.item.cChildren ? 1 : 0);
}

static inline void
treelist_free_dispinfo(treelist_t* tl, treelist_item_t* item, treelist_dispinfo_t* di)
{
    if(tl->unicode_notifications != MC_IS_UNICODE  &&
       di->text != item->text  &&  di->text != NULL)
        free(di->text);
}


typedef struct treelist_subdispinfo_tag treelist_subdispinfo_t;
struct treelist_subdispinfo_tag {
    TCHAR* text;
};

static void
treelist_get_subdispinfo(treelist_t* tl, treelist_item_t* item, int subitem_id,
                         treelist_subdispinfo_t* si, DWORD mask)
{
    MC_NMTLSUBDISPINFO info;
    TCHAR* text;

    MC_ASSERT((mask & ~MC_TLSIF_TEXT) == 0);

    text = treelist_subitem_text(tl, item, subitem_id);

    if(text != MC_LPSTR_TEXTCALLBACK) {
        si->text = text;
        mask &= ~MC_TLIF_TEXT;
    }

    if(mask == 0)
        return;

    info.hdr.hwndFrom = tl->win;
    info.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    info.hdr.code = (tl->unicode_notifications ? MC_TLN_GETSUBDISPINFOW : MC_TLN_GETSUBDISPINFOA);
    info.hItem = (MC_HTREELISTITEM) item;
    info.lItemParam = item->lp;
    info.subitem.fMask = mask;
    info.subitem.iSubItem = subitem_id;
    MC_SEND(tl->notify_win, WM_NOTIFY, 0, &info);

    if(mask & MC_TLSIF_TEXT) {
        if(tl->unicode_notifications == MC_IS_UNICODE)
            si->text = info.subitem.pszText;
        else
            si->text = mc_str(info.subitem.pszText, (tl->unicode_notifications ? MC_STRW : MC_STRA), MC_STRT);
    }
}

static inline void
treelist_free_subdispinfo(treelist_t* tl, treelist_item_t* item, int subitem_id,
                          treelist_subdispinfo_t* si)
{
    if(si->text != NULL  &&  (!item->has_alloced_subitems || si->text != item->subitems[subitem_id-1]))
        free(si->text);
}

static int
treelist_label_width(treelist_t* tl, treelist_item_t* item, int col_ix)
{
    int w = 0;

    if(col_ix == 0) {
        treelist_dispinfo_t di;

        treelist_get_dispinfo(tl, item, &di, MC_TLIF_TEXT);
        if(di.text != NULL)
            w = mc_string_width(di.text, tl->font);
        treelist_free_dispinfo(tl, item, &di);
    } else {
        treelist_subdispinfo_t sdi;

        treelist_get_subdispinfo(tl, item, col_ix, &sdi, MC_TLIF_TEXT);
        if(sdi.text != NULL)
            w = mc_string_width(sdi.text, tl->font);
        treelist_free_subdispinfo(tl, item, col_ix, &sdi);
    }

    return w;
}

static BOOL
treelist_item_has_children(treelist_t* tl, treelist_item_t* item)
{
    treelist_dispinfo_t dispinfo;
    BOOL res;

    if(item->child_head != NULL)
        return TRUE;

    treelist_get_dispinfo(tl, item, &dispinfo, MC_TLIF_CHILDREN);
    res = dispinfo.children;
    treelist_free_dispinfo(tl, item, &dispinfo);
    return res;
}

static treelist_item_t*
treelist_first_selected(treelist_t* tl)
{
    treelist_item_t *walk;
    treelist_item_t *ret;

    if(tl->selected_count == 0)
        return NULL;

    if(tl->selected_count == 1)
        return tl->selected_last;

    ret = NULL;
    walk = tl->selected_last;
    while(walk != NULL) {
        if(walk->state & MC_TLIS_SELECTED)
            ret = walk;
        walk = walk->sibling_prev;
    }

    return ret;
}

static treelist_item_t*
treelist_next_selected(treelist_t* tl, treelist_item_t* item)
{
    if(tl->selected_count <= 1)
        return NULL;    /* treelist_first_selected() already returned all selected items. */

    do {
        item = item->sibling_next;
    } while(item != NULL && !(item->state & MC_TLIS_SELECTED));

    return item;
}

/* Forward declarations */
static void treelist_refresh_hot(treelist_t* tl);
static void treelist_invalidate_item(treelist_t* tl, treelist_item_t* item, int col_ix, int scroll);
static int treelist_do_get_item_rect(treelist_t* tl, MC_HTREELISTITEM item, int col_ix, int what, RECT* rect);


static void
treelist_layout_header(treelist_t* tl)
{
    RECT rect;
    WINDOWPOS header_pos;
    HD_LAYOUT header_layout = { &rect, &header_pos };

    GetClientRect(tl->win, &rect);

    MC_SEND(tl->header_win, HDM_LAYOUT, 0, &header_layout);
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
    treelist_refresh_hot(tl);
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
    treelist_refresh_hot(tl);
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

    line_delta = mousewheel_scroll(tl->win, wheel_delta, si.nPage, vertical);
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

    if(tl->imglist != NULL) {
        int w, h;
        ImageList_GetIconSize(tl->imglist, &w, &h);
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
        if(redraw  &&  !tl->no_redraw) {
            InvalidateRect(tl->win, NULL, TRUE);
        }
        treelist_refresh_hot(tl);
    }

    return old_height;
}

static inline treelist_item_t*
treelist_scrolled_item(treelist_t* tl, int* level)
{
    if(tl->scrolled_item == NULL  &&  tl->displayed_items > 0) {
        /* ->scrolled_item is not know so recompute it. */
        treelist_item_t* item = tl->root_head;
        int i;
        int lvl = 0;

        for(i = 0; i < tl->scroll_y; i++) {
            item = item_next_displayed(item, &lvl);
            MC_ASSERT(item != NULL);
        }
        tl->scrolled_item = item;
        tl->scrolled_level = lvl;
    }

    *level = tl->scrolled_level;
    return tl->scrolled_item;
}

static void
treelist_label_rect(treelist_t* tl, HDC dc, const TCHAR* str, UINT dtjustify,
                    RECT* rect, int* padding_h, int* padding_v)
{
    UINT w;

    if(tl->theme != NULL  &&  tl->theme_treeitem_defined) {
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
    DeleteObject(pen);
}

static void
treelist_paint_button(treelist_t* tl, treelist_item_t* item, HDC dc, RECT* rect)
{
    if(tl->theme) {
        int part = TVP_GLYPH;
        int state = ((item->state & MC_TLIS_EXPANDED) ? GLPS_OPENED : GLPS_CLOSED);
        SIZE glyph_size;
        RECT r;
        DWORD pos;
        POINT pt;

        mcGetThemePartSize(tl->theme, dc, part, state,
                               NULL, TS_DRAW, &glyph_size);
        r.left = (rect->left + rect->right - glyph_size.cx) / 2;
        r.top = (rect->top + rect->bottom - glyph_size.cy + 1) / 2;
        r.right = r.left + glyph_size.cx;
        r.bottom = r.top + glyph_size.cy;
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);

        pos = GetMessagePos();
        pt.x = GET_X_LPARAM(pos);
        pt.y = GET_Y_LPARAM(pos);
        ScreenToClient(tl->win, &pt);
        if(item == tl->hotbutton_item  &&  tl->theme_hotglyph_defined)
            part = TVP_HOTGLYPH;

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
}

static inline UINT
treelist_custom_draw_item_state(treelist_t* tl, treelist_item_t* item)
{
    UINT state = 0;

    if(item->state & MC_TLIS_SELECTED) {
        if(tl->focus)
            state |= CDIS_FOCUS | CDIS_SELECTED;
        else if(tl->style & MC_TLS_SHOWSELALWAYS)
            state |= CDIS_SELECTED;
    }
    if(item == tl->hot_item)
        state |= CDIS_HOT;
    return state;
}

static void
treelist_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    treelist_t* tl = (treelist_t*) control;
    RECT rect;
    RECT header_rect;
    int header_height;
    HDITEM header_item;
    treelist_item_t* item;
    int level;
    int y;
    int col_ix;
    RECT subitem_rect;
    RECT label_rect;
    BOOL theme_treeitem_defined = tl->theme_treeitem_defined;
    int state = 0;
    int padding_h = ITEM_PADDING_H;
    int padding_v = ITEM_PADDING_V;
    int img_w = 0, img_h = 0;
    MC_NMTLCUSTOMDRAW cd = { { { 0 }, 0 }, 0 };
    int cd_mode[3];          /* control/item/subitem */
    HFONT old_font = NULL;
    BOOL paint_selected;
    COLORREF item_text_color;
    COLORREF item_bk_color;
    COLORREF subitem_text_color;
    COLORREF subitem_bk_color;

    /* We handle WM_ERASEBKGND, so we should never need erasing here. */
    MC_ASSERT(erase == FALSE);

    old_font = GetCurrentObject(dc, OBJ_FONT);
    if(tl->font)
        SelectObject(dc, tl->font);

    /* Custom draw: Control pre-paint notification */
    cd.nmcd.hdr.hwndFrom = tl->win;
    cd.nmcd.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    cd.nmcd.hdr.code = NM_CUSTOMDRAW;
    cd.nmcd.dwDrawStage = CDDS_PREPAINT;
    cd.nmcd.hdc = dc;
    cd.iLevel = -1;
    cd.iSubItem = -1;
    cd.clrText = GetSysColor(COLOR_WINDOWTEXT);
    cd.clrTextBk = MC_CLR_NONE;
    cd_mode[0] = MC_SEND(tl->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
    if(cd_mode[0] & (CDRF_SKIPDEFAULT | CDRF_DOERASE))
        goto skip_control_paint;

    /* Control geometry */
    GetClientRect(tl->win, &rect);
    GetWindowRect(tl->header_win, &header_rect);
    header_height = mc_height(&header_rect);
    if(tl->imglist)
        ImageList_GetIconSize(tl->imglist, &img_w, &img_h);

    /* Paint grid */
    if(tl->style & MC_TLS_GRIDLINES) {
        HPEN pen;
        HPEN old_pen;

        pen = CreatePen(PS_SOLID, 1, mcGetThemeSysColor(tl->theme, COLOR_3DFACE));
        old_pen = SelectObject(dc, pen);

        for(y = header_height + tl->item_height - 1; y < rect.bottom; y += tl->item_height) {
            MoveToEx(dc, 0, y, NULL);
            LineTo(dc, rect.right, y);
        }

        for(col_ix = 0; col_ix < tl->col_count; col_ix++) {
            header_item.mask = HDI_FORMAT;
            MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &subitem_rect);
            subitem_rect.right -= tl->scroll_x;
            MoveToEx(dc, subitem_rect.right, header_height, NULL);
            LineTo(dc, subitem_rect.right, rect.bottom);
        }

        SelectObject(dc, old_pen);
    }

    /* Paint items */
    for(item = treelist_scrolled_item(tl, &level), y = header_height;
        item != NULL;
        item = item_next_displayed(item, &level), y += tl->item_height)
    {
        /* Item geometry */
        if(y + tl->item_height < dirty->top)
            continue;
        if(y >= dirty->bottom)
            break;

        item_text_color = GetSysColor(COLOR_WINDOWTEXT);
        item_bk_color = MC_CLR_NONE;

        /* Custom draw: Item pre-paint notification */
        if(cd_mode[0] & CDRF_NOTIFYITEMDRAW) {
            cd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
            cd.nmcd.rc.left = -(tl->scroll_x);
            cd.nmcd.rc.top = y;
            cd.nmcd.rc.right = tl->scroll_x_max;
            cd.nmcd.rc.bottom = y + tl->item_height;
            cd.nmcd.dwItemSpec = (DWORD_PTR) item;
            cd.nmcd.uItemState = treelist_custom_draw_item_state(tl, item);
            cd.nmcd.lItemlParam = item->lp;
            cd.iLevel = level;
            cd.iSubItem = -1;
            cd.clrText = item_text_color;
            cd.clrTextBk = item_bk_color;
            cd_mode[1] = MC_SEND(tl->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
            if(cd_mode[1] & (CDRF_SKIPDEFAULT | CDRF_DOERASE))
                goto skip_item_paint;
            item_text_color = cd.clrText;
            item_bk_color = cd.clrTextBk;
        } else {
            cd_mode[1] = 0;
        }

        /* Determine item state for themed paint */
        if(theme_treeitem_defined) {
            if(!IsWindowEnabled(tl->win)) {
                state = TREIS_DISABLED;
            } else if(item->state & MC_TLIS_SELECTED) {
                if(item == tl->hot_item)
                    state = TREIS_HOTSELECTED;
                else if(tl->focus)
                    state = TREIS_SELECTED;
                else
                    state = TREIS_SELECTEDNOTFOCUS;
            } else if(item == tl->hot_item) {
                state = TREIS_HOT;
            } else {
                state = TREIS_NORMAL;
            }
        }

        /* Paint all subitems */
        for(col_ix = 0; col_ix < tl->col_count; col_ix++) {
            /* Subitem geometry */
            header_item.mask = HDI_FORMAT;
            MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &subitem_rect);
            subitem_rect.left -= tl->scroll_x;
            if(col_ix == 0) {
                subitem_rect.left += level * tl->item_indent;
                if(tl->style & MC_TLS_LINESATROOT)
                    subitem_rect.left += tl->item_indent;
                subitem_rect.left += ITEM_PADDING_H;
            }
            subitem_rect.top = y;
            subitem_rect.right -= + tl->scroll_x + 1;
            subitem_rect.bottom = y + tl->item_height;

            /* Determine subitem colors */
            paint_selected = (item->state & MC_TLIS_SELECTED)  &&
                    ((tl->style & MC_TLS_SHOWSELALWAYS) || tl->focus)  &&
                    ((tl->style & MC_TLS_FULLROWSELECT) || col_ix == 0);
            if(paint_selected  &&  !theme_treeitem_defined) {
                if(tl->focus) {
                    subitem_text_color = GetSysColor(COLOR_HIGHLIGHTTEXT);
                    subitem_bk_color = GetSysColor(COLOR_HIGHLIGHT);
                } else {
                    subitem_text_color = item_text_color;
                    subitem_bk_color = GetSysColor(COLOR_BTNFACE);
                }
            } else {
                subitem_text_color = item_text_color;
                subitem_bk_color = item_bk_color;
            }

            /* Custom draw: subitem pre-paint notification */
            if(cd_mode[1] & CDRF_NOTIFYSUBITEMDRAW) {
                cd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT | CDDS_SUBITEM;
                cd.nmcd.rc.left = subitem_rect.left;
                cd.nmcd.rc.right = subitem_rect.right;
                cd.iSubItem = col_ix;
                cd.clrText = subitem_text_color;
                cd.clrTextBk = subitem_bk_color;
                cd_mode[2] = MC_SEND(tl->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
                if(cd_mode[2] & (CDRF_SKIPDEFAULT | CDRF_DOERASE))
                    goto skip_subitem_paint;
                subitem_text_color = cd.clrText;
                subitem_bk_color = cd.clrTextBk;
            } else {
                cd_mode[2] = 0;
            }

            /* Set the colors into DC */
            SetTextColor(dc, subitem_text_color);
            if(subitem_bk_color != MC_CLR_NONE) {
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, subitem_bk_color);
            } else {
                SetBkMode(dc, TRANSPARENT);
            }

            if(col_ix == 0) {
                treelist_dispinfo_t dispinfo;
                treelist_get_dispinfo(tl, item, &dispinfo,
                        MC_TLIF_TEXT | MC_TLIF_IMAGE | MC_TLIF_SELECTEDIMAGE |
                        MC_TLIF_EXPANDEDIMAGE | MC_TLIF_CHILDREN);

                /* Paint decoration of the main item (line, button etc.) */
                if((level > 0 || (tl->style & MC_TLS_LINESATROOT))  &&
                   (tl->style & (MC_TLS_HASBUTTONS | MC_TLS_HASLINES))) {
                    subitem_rect.left -= ITEM_PADDING_H;
                    if(tl->style & MC_TLS_HASLINES)
                        treelist_paint_lines(tl, item, level, dc, &subitem_rect);

                    if((tl->style & MC_TLS_HASBUTTONS)  &&
                       (item->child_head != NULL  ||  dispinfo.children))
                    {
                        RECT button_rect;
                        mc_rect_set(&button_rect, subitem_rect.left - tl->item_indent, subitem_rect.top,
                                    subitem_rect.left, subitem_rect.bottom);
                        treelist_paint_button(tl, item, dc, &button_rect);
                    }
                    subitem_rect.left += ITEM_PADDING_H;
                }

                /* Paint image of the main item */
                if(tl->imglist != NULL) {
                    int img;
                    if(item->state & MC_TLIS_SELECTED)
                        img = dispinfo.img_selected;
                    else if(item->state & MC_TLIS_EXPANDED)
                        img = dispinfo.img_expanded;
                    else
                        img = dispinfo.img;
                    if(img >= 0) {
                        ImageList_DrawEx(tl->imglist, img, dc,
                                subitem_rect.left, subitem_rect.top + (mc_height(&subitem_rect) - img_h) / 2,
                                0, 0, CLR_NONE, CLR_DEFAULT, ILD_NORMAL);
                    }
                    subitem_rect.left += img_w + ITEM_PADDING_H;
                }

                /* Calculate label rectangle */
                mc_rect_copy(&label_rect, &subitem_rect);
                treelist_label_rect(tl, dc, item->text, DT_LEFT, &label_rect,
                                    &padding_h, &padding_v);

                /* Paint background of the main item. We expand it to all
                 * subitems in case of MC_TLS_FULLROWSELECT and selected item. */
                if((tl->style & MC_TLS_FULLROWSELECT)  &&
                   (paint_selected  ||  (theme_treeitem_defined && state != TREIS_NORMAL)))
                    subitem_rect.right = tl->scroll_x_max - tl->scroll_x;
                if(theme_treeitem_defined  &&  state != TREIS_NORMAL) {
                    mcDrawThemeBackground(tl->theme, dc,
                            TVP_TREEITEM, state, &subitem_rect, NULL);
                } else {
                    RECT* r;
                    if(paint_selected  &&  (tl->style & MC_TLS_FULLROWSELECT))
                        r = &subitem_rect;
                    else
                        r = &label_rect;
                    if(subitem_bk_color != MC_CLR_NONE)
                        ExtTextOut(dc, 0, 0, ETO_OPAQUE, r, NULL, 0, NULL);
                    if(paint_selected  &&  tl->focus)
                        DrawFocusRect(dc, r);
                }

                /* Paint label of the main item */
                mc_rect_inflate(&label_rect, -padding_h, -padding_v);
                if(theme_treeitem_defined) {
                    mcDrawThemeText(tl->theme, dc, TVP_TREEITEM, state,
                                    dispinfo.text, -1, ITEM_DTFLAGS, 0, &label_rect);
                    if(!(tl->style & MC_TLS_FULLROWSELECT))
                        state = TREIS_NORMAL;
                } else {
                    DrawText(dc, dispinfo.text, -1, &label_rect, ITEM_DTFLAGS);
                }

                treelist_free_dispinfo(tl, item, &dispinfo);
            } else {
                /* Paint subitem */
                treelist_subdispinfo_t subdispinfo;
                DWORD justify;

                treelist_get_subdispinfo(tl, item, col_ix, &subdispinfo, MC_TLSIF_TEXT);

                MC_SEND(tl->header_win, HDM_GETITEM, col_ix, &header_item);
                switch(header_item.fmt & HDF_JUSTIFYMASK) {
                    case HDF_RIGHT:   justify = DT_RIGHT; break;
                    case HDF_CENTER:  justify = DT_CENTER; break;
                    default:          justify = DT_LEFT; break;
                }
                treelist_label_rect(tl, dc, subdispinfo.text, justify,
                                    &subitem_rect, &padding_h, &padding_v);
                mc_rect_inflate(&subitem_rect, -padding_h, -padding_v);

                if(theme_treeitem_defined) {
                    mcDrawThemeText(tl->theme, dc, TVP_TREEITEM, state,
                            subdispinfo.text, -1, ITEM_DTFLAGS | justify,
                            0, &subitem_rect);
                } else {
                    DrawText(dc, subdispinfo.text, -1, &subitem_rect,
                             ITEM_DTFLAGS | justify);
                }

                treelist_free_subdispinfo(tl, item, col_ix, &subdispinfo);
            }

            /* Custom draw: Item post-paint notification */
            if(cd_mode[2] & CDRF_NOTIFYPOSTPAINT) {
                cd.nmcd.dwDrawStage = CDDS_ITEMPOSTPAINT | CDDS_SUBITEM;
                MC_SEND(tl->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
            }

skip_subitem_paint:
            ;
        }

        /* Custom draw: Item post-paint notification */
        if(cd_mode[1] & CDRF_NOTIFYPOSTPAINT) {
            cd.nmcd.dwDrawStage = CDDS_POSTPAINT;
            MC_SEND(tl->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
        }

skip_item_paint:
        ;
    }

    /* Custom draw: Control post-paint notification */
    if(cd_mode[0] & CDRF_NOTIFYPOSTPAINT) {
        cd.nmcd.dwDrawStage = CDDS_POSTPAINT;
        MC_SEND(tl->notify_win, WM_NOTIFY, cd.nmcd.hdr.idFrom, &cd);
    }

skip_control_paint:
    if(old_font)
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
        MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);
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

    mc_rect_set(&item_rect, header_item_rect.left, y, header_item_rect.right, y + tl->item_height);

    if(info->iSubItem == 0) {
        treelist_dispinfo_t dispinfo;

        treelist_get_dispinfo(tl, item, &dispinfo, MC_TLIF_CHILDREN | MC_TLIF_TEXT);

        /* Analyze tree item rect */
        item_rect.left += level * tl->item_indent;
        if(tl->style & MC_TLS_LINESATROOT)
            item_rect.left += tl->item_indent;
        if(level > 0  ||  (tl->style & MC_TLS_LINESATROOT)) {
            if(tl->style & (MC_TLS_HASBUTTONS | MC_TLS_HASLINES)) {
                if((tl->style & MC_TLS_HASBUTTONS)  &&
                   (item->child_head != NULL  ||  dispinfo.children))
                {
                    RECT button_rect;
                    mc_rect_set(&button_rect, item_rect.left - tl->item_indent, item_rect.top,
                                item_rect.left, item_rect.bottom);
                    if(mc_rect_contains_pt(&button_rect, &info->pt)) {
                        info->flags = MC_TLHT_ONITEMBUTTON;
                        goto done;
                    }
                }
            }
        }

        if(tl->imglist) {
            int img_w, img_h;

            ImageList_GetIconSize(tl->imglist, &img_w, &img_h);
            if(item_rect.left <= info->pt.x  &&  info->pt.x < item_rect.left + img_w) {
                info->flags = MC_TLHT_ONITEMICON;
                goto done;
            }

            item_rect.left += img_w + ITEM_PADDING_H;
        }

        treelist_label_rect(tl, dc, item->text, DT_LEFT, &item_rect,
                            &ignored, &ignored);

        treelist_free_dispinfo(tl, item, &dispinfo);
    } else {
        /* Analyze subitem rect */
        HDITEM header_item;
        DWORD dtjustify;
        treelist_subdispinfo_t subdispinfo;

        header_item.mask = HDI_FORMAT;
        MC_SEND(tl->header_win, HDM_GETITEM, col_ix, &header_item);
        switch(header_item.fmt) {
            case HDF_RIGHT:   dtjustify = DT_RIGHT; break;
            case HDF_CENTER:  dtjustify = DT_CENTER; break;
            default:          dtjustify = DT_LEFT; break;
        }

        treelist_get_subdispinfo(tl, item, col_ix, &subdispinfo, MC_TLSIF_TEXT);
        treelist_label_rect(tl, dc, subdispinfo.text, dtjustify,
                            &item_rect, &ignored, &ignored);
        treelist_free_subdispinfo(tl, item, col_ix, &subdispinfo);
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
        it = treelist_scrolled_item(tl, &ignored);
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
        MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &rect);
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
        treelist_refresh_hot(tl);
    }
}

static void
treelist_invalidate_column(treelist_t* tl, int col_ix)
{
    RECT tmp;
    RECT rect;

    MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &tmp);
    rect.left = tmp.left;
    rect.right = tmp.right;
    GetWindowRect(tl->header_win, &tmp);
    rect.top = mc_height(&tmp);
    GetClientRect(tl->win, &tmp);
    rect.bottom = tmp.bottom;

    InvalidateRect(tl->win, &rect, TRUE);
}

static void
treelist_invalidate_selected(treelist_t* tl, int col_ix, int scroll)
{
    treelist_item_t* item;
    item = treelist_first_selected(tl);

    while(item != NULL) {
        treelist_invalidate_item(tl, item, col_ix, scroll);
        item = treelist_next_selected(tl, item);
    }
}

static void
treelist_do_expand_all(treelist_t* tl)
{
    treelist_item_t* item;

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
static void treelist_delete_children(treelist_t* tl, treelist_item_t* item);

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

static LRESULT
treelist_sel_notify(treelist_t* tl, UINT code,
                    treelist_item_t* old_sel, treelist_item_t* new_sel)
{
    MC_NMTREELIST nm;

    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = code;

    if(old_sel != NULL) {
        nm.hItemOld = old_sel;
        nm.lParamOld = old_sel->lp;
    } else {
        nm.hItemOld = NULL;
        nm.lParamOld = 0;
    }

    if(new_sel != NULL) {
        nm.hItemNew = new_sel;
        nm.lParamNew = new_sel->lp;
    } else {
        nm.hItemNew = NULL;
        nm.lParamNew = 0;
    }

    return MC_SEND(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);
}

static void
treelist_set_sel(treelist_t* tl, treelist_item_t* item)
{
    BOOL do_single_expand;
    int col_ix;
    treelist_item_t* old_sel;

    if(tl->selected_count <= 1  &&  item == tl->selected_last)
        return;

    do_single_expand = (tl->style & MC_TLS_SINGLEEXPAND)  &&
                      !(tl->style & MC_TLS_MULTISELECT)  &&
                      !(GetAsyncKeyState(VK_CONTROL) & 0x8000);
    col_ix = (tl->style & MC_TLS_FULLROWSELECT) ? -1 : 0;

    /* Send MC_TLN_SELCHANGING */
    old_sel = (!(tl->style & MC_TLS_MULTISELECT) ? tl->selected_last : NULL);
    if(!(tl->style & MC_TLS_MULTISELECT)  ||  item != NULL) {
        if(treelist_sel_notify(tl, MC_TLN_SELCHANGING, old_sel, item) != 0) {
            TREELIST_TRACE("treelist_set_sel: Denied by app.");
            return;
        }
    }

    /* Remove old selection */
    if(tl->selected_count > 0) {
        if(tl->selected_count == 1) {
            tl->selected_last->state &= ~MC_TLIS_SELECTED;

            if(do_single_expand) {
                /* Collapse the old selection and all its ancestors. */
                treelist_item_t* it;
                for(it = tl->selected_last; it != NULL; it = it->parent) {
                    if(it->state & MC_TLIS_EXPANDED)
                        treelist_do_collapse(tl, it, FALSE);
                }
            }

            if(!tl->no_redraw)
                treelist_invalidate_item(tl, tl->selected_last, col_ix, 0);
        } else {
            treelist_item_t* it;

            for(it = treelist_first_selected(tl); it != NULL; it = treelist_next_selected(tl, it)) {
                if(it == item)
                    continue;

                it->state &= ~MC_TLIS_SELECTED;
                if(!tl->no_redraw)
                    treelist_invalidate_item(tl, it, col_ix, 0);
            }
        }
    }

    /* Do new selection */
    tl->selected_last = item;
    tl->selected_from = item;
    if(item != NULL) {
        item->state |= MC_TLIS_SELECTED;
        tl->selected_count = 1;

        if(do_single_expand) {
            treelist_item_t* it;
            if(treelist_item_has_children(tl, item) && !(item->state & MC_TLIS_EXPANDED))
                treelist_do_expand(tl, item, FALSE);
            for(it = item->parent; it != NULL; it = it->parent) {
                if(!(it->state & MC_TLIS_EXPANDED))
                    treelist_do_expand(tl, it, FALSE);
            }
        }

        if(!tl->no_redraw)
            treelist_invalidate_item(tl, item, col_ix, 0);
    } else {
        tl->selected_count = 0;
    }

    /* Send MC_TLN_SELCHANGED */
    if(!(tl->style & MC_TLS_MULTISELECT))
        treelist_sel_notify(tl, MC_TLN_SELCHANGED, old_sel, item);
    else
        treelist_sel_notify(tl, MC_TLN_SELCHANGED, NULL, NULL);
}

static void
treelist_toggle_sel(treelist_t* tl, treelist_item_t* item)
{
    BOOL do_select = !(item->state & MC_TLIS_SELECTED);

    MC_ASSERT(item != NULL);

    /* Without the multi-selection mode, we degenerate to treelist_set_sel(). */
    if(!(tl->style & MC_TLS_MULTISELECT)) {
        treelist_set_sel(tl, (do_select ? item : NULL));
        return;
    }

    /* If we toggle in the tree elsewhere then the current selection is, we may
     * need to unselect old selection. Function treelist_set_sel() already
     * knows how to do that. */
    if(do_select  &&  tl->selected_count > 0  &&  tl->selected_last->parent != item->parent) {
        treelist_set_sel(tl, item);
        return;
    }

    if(do_select) {
        /* Send MC_TLN_SELCHANGING */
        if(treelist_sel_notify(tl, MC_TLN_SELCHANGING, NULL, item) != 0) {
            TREELIST_TRACE("treelist_toggle_sel: Denied by app.");
            return;
        }

        item->state |= MC_TLIS_SELECTED;
        tl->selected_count++;
        tl->selected_last = item;
    } else {
        item->state &= ~MC_TLIS_SELECTED;
        if(tl->selected_last == item) {
            /* We must ensure ->selected_last is set properly. */
            if(tl->selected_count == 1)
                tl->selected_last = NULL;
            else
                tl->selected_last = treelist_first_selected(tl);
        }
        tl->selected_count--;
    }

    if(!tl->no_redraw)
        treelist_invalidate_item(tl, item, (tl->style & MC_TLS_FULLROWSELECT) ? -1 : 0, 0);

    /* Send MC_TLN_SELCHANGED */
    treelist_sel_notify(tl, MC_TLN_SELCHANGED, NULL, NULL);
}

static void
treelist_set_sel_range(treelist_t* tl, treelist_item_t* item)
{
    treelist_item_t* it;
    treelist_item_t* item0 = tl->selected_from;
    treelist_item_t* item1;
    int col_ix;

    MC_ASSERT(tl->style & MC_TLS_MULTISELECT);
    MC_ASSERT(item != NULL);

    if(item0 == NULL  ||  item0->parent != item->parent) {
        treelist_set_sel(tl, item);
        return;
    }

    col_ix = (tl->style & MC_TLS_FULLROWSELECT) ? -1 : 0;

    /* Make sure there is no item selected before the item0 or item
     * (whatever comes first). */
    it = (item->parent ? item->parent->child_head : tl->root_head);
    while(it != item0  &&  it != item) {
        if(it->state & MC_TLIS_SELECTED) {
            it->state &= ~MC_TLIS_SELECTED;
            tl->selected_count--;
            if(!tl->no_redraw)
                treelist_invalidate_item(tl, it, col_ix, 0);
        }
        it = it->sibling_next;
    }

    /* Make sure all items between item0 and item1 are selected. */
    item1 = (it == item ? item0 : item);
    while(TRUE) {
        if(!(it->state & MC_TLIS_SELECTED)) {
            /* Send MC_TLN_SELCHANGING */
            if(treelist_sel_notify(tl, MC_TLN_SELCHANGING, NULL, it) != 0) {
                TREELIST_TRACE("treelist_set_sel_range: Denied by app.");
            } else {
                it->state |= MC_TLIS_SELECTED;
                tl->selected_count++;
                if(!tl->no_redraw)
                    treelist_invalidate_item(tl, it, col_ix, 0);
            }
        }

        if(it == item1) {
            it = it->sibling_next;
            break;
        }

        it = it->sibling_next;
    }

    /* Make sure no more items are selected */
    while(it != NULL) {
        if(it->state & MC_TLIS_SELECTED) {
            it->state &= ~MC_TLIS_SELECTED;
            tl->selected_count--;
            if(!tl->no_redraw)
                treelist_invalidate_item(tl, it, col_ix, 0);
        }
        it = it->sibling_next;
    }

    tl->selected_last = item;

    treelist_sel_notify(tl, MC_TLN_SELCHANGED, NULL, NULL);
}

static int
treelist_do_expand(treelist_t* tl, treelist_item_t* item, BOOL surely_displayed)
{
    MC_NMTREELIST nm = { {0}, 0 };

    MC_ASSERT(!(item->state & MC_TLIS_EXPANDED));

    if(!item->expanding_notify_in_progress) {
        LRESULT res;

        nm.hdr.hwndFrom = tl->win;
        nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
        nm.hdr.code = MC_TLN_EXPANDING;
        nm.action = MC_TLE_EXPAND;
        nm.hItemNew = item;
        nm.lParamNew = item->lp;
        item->expanding_notify_in_progress = 1;
        res = MC_SEND(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);
        item->expanding_notify_in_progress = 0;
        if(res != 0) {
            TREELIST_TRACE("treelist_do_expand: Denied by app.");
            return -1;
        }
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
    MC_SEND(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);

    return 0;
}

static int
treelist_do_collapse(treelist_t* tl, treelist_item_t* item, BOOL surely_displayed)
{
    MC_NMTREELIST nm = { {0}, 0 };

    MC_ASSERT(item->state & MC_TLIS_EXPANDED);

    if(!item->expanding_notify_in_progress) {
        LRESULT res;

        nm.hdr.hwndFrom = tl->win;
        nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
        nm.hdr.code = MC_TLN_EXPANDING;
        nm.action = MC_TLE_COLLAPSE;
        nm.hItemNew = item;
        nm.lParamNew = item->lp;
        item->expanding_notify_in_progress = 1;
        res = MC_SEND(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);
        item->expanding_notify_in_progress = 0;
        if(res != 0) {
            TREELIST_TRACE("treelist_do_collapse: Denied by app.");
            return -1;
        }
    }

    item->state &= ~MC_TLIS_EXPANDED;
    if(surely_displayed || item_is_displayed(item)) {
        treelist_item_t* it;
        int ignored = 0;
        int hidden_items = 0;

        for(it = item->child_head; it != NULL; it = item_next_displayed_ex(it, item, &ignored)) {
            if(it->state & MC_TLIS_SELECTED)
                treelist_set_sel(tl, item);

            if(it == tl->scrolled_item)
                tl->scrolled_item = NULL;

            hidden_items++;
        }
        tl->displayed_items -= hidden_items;

        treelist_setup_scrollbars(tl);
        if(!tl->no_redraw)
            treelist_invalidate_item(tl, item, 0, -hidden_items);
    }

    nm.hdr.code = MC_TLN_EXPANDED;
    MC_SEND(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);

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

        case MC_TLE_COLLAPSE | MC_TLE_COLLAPSERESET:
            if(expanded)
                treelist_do_collapse(tl, item, FALSE);
            treelist_delete_children(tl, item);
            return TRUE;

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
            if(item == NULL)
                return treelist_first_selected(tl);
            else
                return treelist_next_selected(tl, item);
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
        MC_TRACE("treelist_get_next_item: hItem == TVI_ROOT not allowed "
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

    MC_TRACE("treelist_get_next_item: Unknown relation %d", relation);
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
treelist_update_tooltip_pos(treelist_t* tl)
{
    RECT rect;

    if(tl->hot_item == NULL)
        return;

    treelist_do_get_item_rect(tl, tl->hot_item, tl->hot_col, MC_TLIR_LABEL, &rect);
    ClientToScreen(tl->win, (POINT*) &rect);
    MC_SEND(tl->tooltip_win, TTM_ADJUSTRECT, TRUE, &rect);
    SetWindowPos(tl->tooltip_win, NULL, rect.left, rect.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void
treelist_mouse_move(treelist_t* tl, int x, int y)
{
    MC_TLHITTESTINFO info;
    treelist_item_t* item;
    treelist_item_t* hot_item;
    treelist_item_t* hotbutton_item;
    treelist_item_t* old_hot_item = tl->hot_item;
    SHORT old_hot_col = tl->hot_col;

    info.pt.x = x;
    info.pt.y = y;
    item = treelist_hit_test(tl, &info);

    hot_item = treelist_is_common_hit(tl, &info) ? item : NULL;
    hotbutton_item = (info.flags & MC_TLHT_ONITEMBUTTON) ? item : NULL;

    tl->hot_col = info.iSubItem;

    /* Make sure the right item is hot */
    if(hot_item != old_hot_item) {
        if(old_hot_item != NULL  &&  !tl->no_redraw)
            treelist_invalidate_item(tl, old_hot_item, -1, 0);
        if(hot_item != NULL  &&  !tl->no_redraw)
            treelist_invalidate_item(tl, hot_item, -1, 0);
        tl->hot_item = hot_item;
    }

    /* Make sure the right item's button is hot */
    if(hotbutton_item != tl->hotbutton_item) {
        if(tl->hotbutton_item != NULL  &&  !tl->no_redraw)
            treelist_invalidate_item(tl, tl->hotbutton_item, 0, 0);
        if(hotbutton_item != NULL  &&  !tl->no_redraw)
            treelist_invalidate_item(tl, hotbutton_item, 0, 0);
        tl->hotbutton_item = hotbutton_item;
    }

    /* Checker whether we need to update tooltip. */
    if(tl->tooltip_win != NULL) {
        if(hot_item != old_hot_item  ||  info.iSubItem != old_hot_col) {
            BOOL need_label_ellipses;

            if(hot_item != NULL) {
                RECT rect;
                int str_width;

                treelist_do_get_item_rect(tl, hot_item, info.iSubItem,
                            MC_TLIR_LABEL, &rect);
                str_width = treelist_label_width(tl, hot_item, info.iSubItem);

                need_label_ellipses = (rect.left + str_width >= rect.right);
            } else {
                need_label_ellipses = FALSE;
            }

            if(need_label_ellipses) {
                tooltip_update_text(tl->tooltip_win, tl->win, LPSTR_TEXTCALLBACK);
                treelist_update_tooltip_pos(tl);
            } else {
                tooltip_update_text(tl->tooltip_win, tl->win, NULL);
            }
        }
    }

    if(!tl->tracking_leave) {
        mc_track_mouse(tl->win, TME_LEAVE);
        tl->tracking_leave = TRUE;
    }
}

static void
treelist_mouse_leave(treelist_t* tl)
{
    /* If the tooltip is visible, we defer handling of WM_MOUSELEAVE until
     * the tooltip window disapperas (TTN_POP). */
    if(tl->active_tooltip)
        return;

    if(tl->hot_item != NULL  &&  !tl->no_redraw)
        treelist_invalidate_item(tl, tl->hot_item, -1, 0);
    tl->hot_item = NULL;

    if(tl->hotbutton_item != NULL  &&  !tl->no_redraw)
        treelist_invalidate_item(tl, tl->hotbutton_item, 0, 0);
    tl->hotbutton_item = NULL;

    tl->tracking_leave = FALSE;
}

static inline void
treelist_refresh_hot(treelist_t* tl)
{
    DWORD pos;
    POINT pt;

    pos = GetMessagePos();
    pt.x = GET_X_LPARAM(pos);
    pt.y = GET_Y_LPARAM(pos);
    ScreenToClient(tl->win, &pt);
    treelist_mouse_move(tl, pt.x, pt.y);
}

static void
treelist_left_button(treelist_t* tl, int x, int y, BOOL dblclick, WPARAM wp)
{
    MC_TLHITTESTINFO info;
    treelist_item_t* item;
    UINT notify_code = (dblclick ? NM_DBLCLK : NM_CLICK);

    info.pt.x = x;
    info.pt.y = y;
    item = treelist_hit_test(tl, &info);

    if(mc_send_notify(tl->notify_win, tl->win, notify_code) != 0) {
        /* Application suppresses the default processing of the message */
        goto out;
    }

    if(treelist_is_common_hit(tl, &info)) {
        if(dblclick) {
            if(item->state & MC_TLIS_EXPANDED)
                treelist_do_collapse(tl, item, TRUE);
            else
                treelist_do_expand(tl, item, TRUE);
        } else {
            if(tl->style & MC_TLS_MULTISELECT) {
                if(wp & MK_SHIFT) {
                    treelist_set_sel_range(tl, item);
                } else if(wp & MK_CONTROL) {
                    treelist_toggle_sel(tl, item);
                    tl->selected_from = item;
                } else {
                    treelist_set_sel(tl, item);
                }
            } else {
                treelist_set_sel(tl, item);
            }
        }
    } else if(info.flags & MC_TLHT_ONITEMBUTTON) {
        if(item->state & MC_TLIS_EXPANDED)
            treelist_do_collapse(tl, item, TRUE);
        else
            treelist_do_expand(tl, item, TRUE);
    }

out:
    if(!dblclick)
        SetFocus(tl->win);
}

static void
treelist_right_button(treelist_t* tl, int x, int y, BOOL dblclick, WPARAM wp)
{
    UINT notify_code = (dblclick ? NM_RDBLCLK : NM_RCLICK);
    POINT pt;

    if(mc_send_notify(tl->notify_win, tl->win, notify_code) != 0) {
        /* Application suppresses the default processing of the message */
        return;
    }

    /* Send WM_CONTEXTMENU */
    pt.x = x;
    pt.y = y;
    ClientToScreen(tl->win, &pt);
    MC_SEND(tl->notify_win, WM_CONTEXTMENU, tl->win, MAKELPARAM(pt.x, pt.y));
}

static void
treelist_key_down(treelist_t* tl, int key)
{
    treelist_item_t* old_sel;
    treelist_item_t* sel;
    int ignored;

    if((tl->style & MC_TLS_MULTISELECT)  &&  (GetKeyState(VK_SHIFT) & 0x8000)  &&
       tl->selected_from != NULL  &&  (key == VK_UP || key == VK_DOWN))
    {
        sel = (tl->selected_last ? tl->selected_last : tl->selected_from);

        switch(key) {
            case VK_UP:
                if(sel->sibling_prev)
                    sel = sel->sibling_prev;
                break;
            case VK_DOWN:
                if(sel->sibling_next)
                    sel = sel->sibling_next;
                break;
        }

        treelist_set_sel_range(tl, sel);
        treelist_ensure_visible(tl, sel, NULL);
        return;
    }

    if(GetKeyState(VK_CONTROL) & 0x8000) {
        switch(key) {
            case VK_PRIOR:  treelist_vscroll(tl, SB_PAGEUP); break;
            case VK_NEXT:   treelist_vscroll(tl, SB_PAGEDOWN); break;
            case VK_HOME:   treelist_vscroll(tl, SB_TOP); break;
            case VK_END:    treelist_vscroll(tl, SB_BOTTOM); break;
            case VK_UP:     treelist_vscroll(tl, SB_LINEUP); break;
            case VK_DOWN:   treelist_vscroll(tl, SB_LINEDOWN); break;
            case VK_LEFT:   treelist_hscroll(tl, !tl->rtl ? SB_LINELEFT : SB_LINERIGHT); break;
            case VK_RIGHT:  treelist_hscroll(tl, !tl->rtl ? SB_LINERIGHT : SB_LINELEFT); break;
        }
        return;
    }

    old_sel = tl->selected_last;
    sel = old_sel;

    switch(key) {
        case VK_UP:
            if(sel != NULL)
                sel = item_prev_displayed(sel);
            break;

        case VK_DOWN:
            if(sel != NULL)
                sel = item_next_displayed(sel, &ignored);
            break;

        case VK_HOME:
            sel = tl->root_head;
            break;

        case VK_END:
            sel = treelist_last_displayed_item(tl);
            break;

        case VK_LEFT:
            if(sel != NULL) {
                if(sel->state & MC_TLIS_EXPANDED)
                    treelist_do_collapse(tl, sel, FALSE);
                else
                    sel = sel->parent;
            }
            break;

        case VK_RIGHT:
            if(sel != NULL) {
                if(treelist_item_has_children(tl, sel)) {
                    if(!(sel->state & MC_TLIS_EXPANDED))
                        treelist_do_expand(tl, sel, FALSE);
                    else
                        sel = sel->child_head;
                }
            }
            break;

        case VK_MULTIPLY:
            treelist_do_expand_all(tl);
            break;

        case VK_ADD:
            if(sel != NULL && !(sel->state & MC_TLIS_EXPANDED)  &&  treelist_item_has_children(tl, sel))
                treelist_do_expand(tl, sel, FALSE);
            break;

        case VK_SUBTRACT:
            if(sel != NULL && (sel->state & MC_TLIS_EXPANDED))
                treelist_do_collapse(tl, sel, FALSE);
            break;

        case VK_PRIOR:
        case VK_NEXT:
        {
            int n;

            n = treelist_items_per_page(tl);
            if(n < 1)
                n = 1;
            while(sel != NULL && n > 0) {
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
            if(sel != NULL)
                sel = sel->parent;
            break;

        case VK_SPACE:
            /* TODO: if has checkbox, toggle its state */
            break;
    }

    if(sel != NULL  &&  sel != old_sel) {
        treelist_set_sel(tl, sel);
        treelist_ensure_visible(tl, sel, NULL);
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
         * But for 1st columm (the tree) use more as it is generally more space
         * consuming. */
        header_item.mask |= HDI_WIDTH;
        header_item.cxy = (col_ix == 0 ? 100 : 10);
    }
    col_ix = MC_SEND(tl->header_win, (unicode ? HDM_INSERTITEMW : HDM_INSERTITEMA),
                    col_ix, &header_item);
    if(MC_ERR(col_ix == -1)) {
        MC_TRACE_ERR("treelist_insert_column: HDM_INSERTITEM failed");
        return -1;
    }

    /* Update subitems. Hopefully, sane applications should not insert columns
     * after inserting the data, hence MC_UNLIKELY. */
    if(MC_UNLIKELY(tl->root_head != NULL)) {
        treelist_item_t* item;
        uintptr_t i = col_ix - 1;

        /* Realloc subitems[] to make a space for rearranging the data. Note
         * the rearranging is delayed until we succeed to reallocate everything
         * to simplify the error handling (to not keep some items in incorrect
         * state).
         *
         * Note if an error really happens, some items may be already realloc'ed
         * and we waste the new space in them. But gosh, if app adds columns
         * while having some data in the tree, it deserves the punishment....
         */
        for(item = tl->root_head; item != NULL; item = item_next(item)) {
            if(item->has_alloced_subitems) {
                TCHAR** subitems;

                subitems = (TCHAR**) realloc(item->subitems, (tl->col_count + 1) * sizeof(TCHAR*));
                if(MC_ERR(subitems == NULL)) {
                    MC_TRACE("treelist_insert_column: realloc(subitems) failed.");
                    mc_send_notify(tl->notify_win, tl->win, NM_OUTOFMEMORY);
                    MC_SEND(tl->header_win, HDM_DELETEITEM, col_ix, 0);
                    return -1;
                }
                item->subitems = subitems;
            } else {
                /* If there are too many columns and the new column would cause
                 * to shift a set bit in the callback_map out of it, we need
                 * to switch to alloc'ed subitems. */
                if(i < CALLBACK_MAP_SIZE  &&
                   (item->callback_map & CALLBACK_MAP_BIT(CALLBACK_MAP_SIZE-1)))
                {
                    if(MC_ERR(treelist_subitems_alloc(tl, item, tl->col_count + 1) != 0)) {
                        MC_TRACE("treelist_insert_column: treelist_subitems_alloc() failed.");
                        MC_SEND(tl->header_win, HDM_DELETEITEM, col_ix, 0);
                        return -1;
                    }
                }
            }
        }

        /* All items are now successfully realloc'ed, so we may rearrange
         * the data in them. */
        for(item = tl->root_head; item != NULL; item = item_next(item)) {
            if(item->has_alloced_subitems) {
                memmove(item->subitems + (i+1), item->subitems + i,
                        (tl->col_count - col_ix) * sizeof(TCHAR*));
                item->subitems[i] = NULL;
            } else {
                if(item->callback_map != 0  &&  i < CALLBACK_MAP_SIZE-1) {
                    /* mask0 specifies columns to keep on the same place,
                     * mask1 specifies columns to shift to make a space for
                     * the i-th one. */
                    uintptr_t mask0 = CALLBACK_MAP_BIT(i)-1;
                    uintptr_t mask1 = ~mask0;
                    item->callback_map = (item->callback_map & mask0) |
                                         ((item->callback_map & mask1) << 1);
                }
            }
        }
    }

    MC_ASSERT(header_item.mask & HDI_WIDTH);
    tl->scroll_x_max += header_item.cxy;
    tl->col_count++;

    treelist_setup_scrollbars(tl);

    if(!tl->no_redraw) {
        RECT header_item_rect;
        RECT rect;

        MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);

        GetClientRect(tl->win, &rect);
        rect.left = header_item_rect.left - tl->scroll_x;
        rect.top = mc_height(&header_item_rect);
        rect.right = tl->scroll_x_max - tl->scroll_x;

        ScrollWindowEx(tl->win, mc_width(&header_item_rect), 0, &rect, &rect,
                    NULL, NULL, SW_ERASE | SW_INVALIDATE);
        if(tl->style & MC_TLS_FULLROWSELECT)
            treelist_invalidate_selected(tl, -1, 0);
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

    ok = MC_SEND(tl->header_win, (unicode ? HDM_SETITEMW : HDM_SETITEMA),
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

    ok = MC_SEND(tl->header_win, (unicode ? HDM_GETITEMW : HDM_GETITEMA),
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

    MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);

    ok = MC_SEND(tl->header_win, HDM_DELETEITEM, col_ix, 0);
    if(MC_ERR(!ok)) {
        MC_TRACE("treelist_delete_column: HDM_DELETEITEM failed.");
        return FALSE;
    }

    /* Update subitems. Hopefully, sane applications should not remove columns
     * after inserting the data, hence MC_UNLIKELY. */
    if(MC_UNLIKELY(tl->root_head != NULL)) {
        treelist_item_t* item;
        int i = col_ix - 1;

        for(item = tl->root_head; item != NULL; item = item_next(item)) {
            if(item->has_alloced_subitems) {
                if(item->subitems[i] != NULL  &&
                   item->subitems[i] != MC_LPSTR_TEXTCALLBACK)
                    free(item->subitems[i]);

                if(tl->col_count > 1) {
                    TCHAR** subitems = item->subitems;

                    memmove(subitems + i, subitems + (i + 1),
                            (tl->col_count - i - 1) * sizeof(TCHAR*));

                    subitems = (TCHAR**) realloc(subitems, (tl->col_count - 1) * sizeof(TCHAR*));
                    if(MC_ERR(subitems == NULL))
                        MC_TRACE("treelist_delete_column: realloc() failed.");
                    else
                        item->subitems = subitems;
                } else {
                    free(item->subitems);
                    item->subitems = NULL;
                    item->has_alloced_subitems = FALSE;
                }
            } else {
                if(item->callback_map != 0) {
                    /* mask0 specifies columns to keep on the same place,
                     * mask1 specifies columns to shift to erase the i-th one. */
                    uintptr_t mask0 = CALLBACK_MAP_BIT(i)-1;
                    uintptr_t mask1 = (i < CALLBACK_MAP_SIZE-1 ? ~(CALLBACK_MAP_BIT(i+1)-1) : 0);
                    item->callback_map = (item->callback_map & mask0) |
                                         ((item->callback_map & mask1) >> 1);
                }
            }
        }
    }

    tl->col_count--;
    tl->scroll_x_max -= mc_width(&header_item_rect);
    treelist_setup_scrollbars(tl);

    if(!tl->no_redraw) {
        RECT rect;

        GetClientRect(tl->win, &rect);
        rect.left = header_item_rect.left - tl->scroll_x;
        rect.top = mc_height(&header_item_rect);
        rect.right = tl->scroll_x_max - tl->scroll_x;

        ScrollWindowEx(tl->win, -mc_width(&header_item_rect), 0, &rect, &rect,
                    NULL, NULL, SW_ERASE | SW_INVALIDATE);
        if(tl->style & MC_TLS_FULLROWSELECT)
            treelist_invalidate_selected(tl, -1, 0);
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

    return MC_SEND(tl->header_win, HDM_SETORDERARRAY, n, array);
}

static treelist_item_t*
treelist_insert_item(treelist_t* tl, MC_TLINSERTSTRUCT* insert, BOOL unicode)
{
    MC_TLITEM* item_data = &insert->item;
    treelist_item_t* parent;
    treelist_item_t* prev;
    treelist_item_t* next;
    TCHAR* text = NULL;
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
        if(item_data->pszText == MC_LPSTR_TEXTCALLBACK) {
            item->text = item_data->pszText;
        } else {
            text = mc_str(item_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
            if(MC_ERR(text == NULL  &&  item_data->pszText != NULL)) {
                MC_TRACE("treelist_insert_item: mc_str() failed.");
                goto err_alloc_text;
            }
        }
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
    item->callback_map = 0;
    item->state = ((item_data->fMask & MC_TLIF_STATE)
                            ? (item_data->state & item_data->stateMask) : 0);
    item->text = text;
    item->lp = ((item_data->fMask & MC_TLIF_PARAM) ? item_data->lParam : 0);
    item->img = (SHORT)((item_data->fMask & MC_TLIF_IMAGE)
                                ? item_data->iImage : MC_I_IMAGENONE);
    item->img_selected = (SHORT)((item_data->fMask & MC_TLIF_SELECTEDIMAGE)
                                ? item_data->iSelectedImage : MC_I_IMAGENONE);
    item->img_expanded = (SHORT)((item_data->fMask & MC_TLIF_EXPANDEDIMAGE)
                                ? item_data->iExpandedImage : MC_I_IMAGENONE);
    if(item_data->fMask & MC_TLIF_CHILDREN) {
        item->children = (item_data->cChildren != 0);
        item->children_callback = (item_data->cChildren == MC_I_CHILDRENCALLBACK);
    } else {
        item->children = 0;
        item->children_callback = 0;
    }
    item->expanding_notify_in_progress = 0;
    item->has_alloced_subitems = 0;

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
            if(rect.top < 0) {
                treelist_item_t* scrolled_item;
                int ignored;

                scrolled_item = treelist_scrolled_item(tl, &ignored);
                rect.top = treelist_get_item_y(tl, scrolled_item, TRUE);
            }
            ScrollWindowEx(tl->win, 0, tl->item_height, &rect, &rect,
                               NULL, NULL, SW_INVALIDATE | SW_ERASE);
        }
    }

    treelist_refresh_hot(tl);

    /* Success */
    return item;

    /* Error path */
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

        if(item_data->pszText == MC_LPSTR_TEXTCALLBACK) {
            text = MC_LPSTR_TEXTCALLBACK;
        } else {
            text = mc_str(item_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
            if(MC_ERR(text == NULL  &&  item_data->pszText != NULL)) {
                MC_TRACE("treelist_set_item: mc_str() failed.");
                return FALSE;
            }
        }

        if(item->text != NULL  &&  item->text != MC_LPSTR_TEXTCALLBACK)
            free(item->text);
        item->text = text;
    }

    if(item_data->fMask & MC_TLIF_PARAM)
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
            treelist_toggle_sel(tl, item);
    }

    if(item_data->fMask & MC_TLIF_IMAGE)
        item->img = item_data->iImage;

    if(item_data->fMask & MC_TLIF_SELECTEDIMAGE)
        item->img_selected = item_data->iSelectedImage;

    if(item_data->fMask & MC_TLIF_EXPANDEDIMAGE)
        item->img_expanded = item_data->iExpandedImage;

    if(item_data->fMask & MC_TLIF_CHILDREN) {
        item->children = (item_data->cChildren != 0);
        item->children_callback = (item_data->cChildren == MC_I_CHILDRENCALLBACK);
    }

    if(!tl->no_redraw)
        treelist_invalidate_item(tl, item, 0, 0);

    return TRUE;
}

static BOOL
treelist_get_item(treelist_t* tl, treelist_item_t* item, MC_TLITEM* item_data,
                  BOOL unicode)
{
    DWORD dispinfo_mask;
    treelist_dispinfo_t dispinfo;

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

    dispinfo_mask = item_data->fMask & (MC_TLIF_TEXT | MC_TLIF_IMAGE |
                MC_TLIF_SELECTEDIMAGE | MC_TLIF_EXPANDEDIMAGE | MC_TLIF_CHILDREN);
    treelist_get_dispinfo(tl, item, &dispinfo, dispinfo_mask);

    if(item_data->fMask & MC_TLIF_TEXT) {
        mc_str_inbuf(dispinfo.text, MC_STRT, item_data->pszText,
                     (unicode ? MC_STRW : MC_STRA), item_data->cchTextMax);
    }

    if(item_data->fMask & MC_TLIF_PARAM)
        item_data->lParam = item->lp;

    if(item_data->fMask & MC_TLIF_STATE)
        item_data->state = item->state;

    if(item_data->fMask & MC_TLIF_IMAGE)
        item_data->iImage = dispinfo.img;

    if(item_data->fMask & MC_TLIF_SELECTEDIMAGE)
        item_data->iSelectedImage = dispinfo.img_selected;

    if(item_data->fMask & MC_TLIF_EXPANDEDIMAGE)
        item_data->iExpandedImage = dispinfo.img_expanded;

    treelist_free_dispinfo(tl, item, &dispinfo);
    return TRUE;
}

static void
treelist_delete_notify(treelist_t* tl, treelist_item_t* item, treelist_item_t* stopper)
{
    MC_NMTREELIST nm = { {0}, 0 };

    nm.hdr.hwndFrom = tl->win;
    nm.hdr.idFrom = GetWindowLong(tl->win, GWL_ID);
    nm.hdr.code = MC_TLN_DELETEITEM;

    while(item != NULL) {
        nm.hItemOld = item;
        nm.lParamOld = item->lp;
        MC_SEND(tl->notify_win, WM_NOTIFY, nm.hdr.idFrom, &nm);
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
        if(item->has_alloced_subitems) {
            int i;
            for(i = 0; i < tl->col_count - 1; i++) {
                if(item->subitems[i] != NULL  &&
                   item->subitems[i] != MC_LPSTR_TEXTCALLBACK)
                    free(item->subitems[i]);
            }
            free(item->subitems);
        }
        if(item->text != NULL  &&  item->text != MC_LPSTR_TEXTCALLBACK)
            free(item->text);

        /* Update any selection information now */
        if(item->state & MC_TLIS_SELECTED)
            treelist_toggle_sel(tl, item);
        if(item == tl->selected_from)
            tl->selected_from = NULL;

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
    treelist_item_t* parent;
    treelist_item_t* sibling_prev;
    treelist_item_t* sibling_next;
    BOOL is_displayed;
    int y;
    int displayed_del_count;

    TREELIST_TRACE("treelist_delete_item(%p, %p)", tl, item);

    if(item == MC_TLI_ROOT  ||  item == NULL) {
        /* Delete all items */
        if(tl->root_head != NULL) {
            treelist_set_sel(tl, NULL);
            tl->scrolled_item = NULL;

            treelist_delete_notify(tl, tl->root_head, NULL);
            treelist_delete_item_helper(tl, tl->root_head, FALSE);
            tl->root_head = NULL;
            tl->root_tail = NULL;
            tl->displayed_items = 0;
            tl->selected_last = NULL;
            tl->selected_from = NULL;
            tl->selected_count = 0;
            treelist_setup_scrollbars(tl);
            if(!tl->no_redraw)
                InvalidateRect(tl->win, NULL, TRUE);
        }
        return TRUE;
    }

    /* Remember some info about the deleted item. */
    parent = item->parent;
    sibling_prev = item->sibling_prev;
    sibling_next = item->sibling_next;
    is_displayed = item_is_displayed(item);
    y = (is_displayed ? treelist_get_item_y(tl, item, TRUE) : -1);

    /* If the deleted subtree contains selection, we must choose another
     * selection. */
    if(!(tl->style & MC_TLS_MULTISELECT)) {
        if(item_is_ancestor(item, tl->selected_last)) {
            if(item->sibling_next)
                treelist_set_sel(tl, item->sibling_next);
            else if(item->sibling_prev)
                treelist_set_sel(tl, item->sibling_prev);
            else
                treelist_set_sel(tl, parent);
        }
    } else {
        if(item->state & MC_TLIS_SELECTED) {
            if(tl->selected_count == 1)
                treelist_set_sel(tl, parent);
        } else if(tl->selected_last != NULL) {
            if(item_is_ancestor(item, tl->selected_last->parent))
                treelist_set_sel(tl, parent);
        }
    }

    /* This should be very last notification about the item and its subtree. */
    treelist_delete_notify(tl, item, item);

    /* Disconnect the item from the tree. */
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

    /* Reset item bookmarks which will need recomputing anyway. */
    tl->scrolled_item = NULL;
    if(tl->hot_item != NULL  &&  !tl->no_redraw)
        treelist_invalidate_item(tl, tl->hot_item, -1, 0);
    tl->hot_item = NULL;
    if(tl->hotbutton_item != NULL  &&  !tl->no_redraw)
        treelist_invalidate_item(tl, tl->hotbutton_item, 0, 0);
    tl->hotbutton_item = NULL;

    /* Delete the item and whole its subtree, and count how many of deleted
     * items were displayed. */
    displayed_del_count = treelist_delete_item_helper(tl, item, is_displayed);
    if(is_displayed)
        tl->displayed_items -= displayed_del_count;

    /* Refresh */
    if(tl->displayed_items != old_displayed_items) {
        treelist_setup_scrollbars(tl);

        if(!tl->no_redraw) {
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

            /* If we were tail child, the previous sibling column 0 may need to
             * repaint without a line continuing to us. */
            if(sibling_prev != NULL  &&  sibling_next == NULL)
                treelist_invalidate_item(tl, sibling_prev, 0, 0);
        }
    }

    treelist_refresh_hot(tl);

    /* Parent may need to repaint column 0 without expand button. */
    if(!tl->no_redraw  &&  parent != NULL  &&
       parent->child_head == NULL  &&  parent->child_tail == NULL)
        treelist_invalidate_item(tl, parent, 0, 0);

    return TRUE;
}

static void
treelist_delete_children(treelist_t* tl, treelist_item_t* item)
{
    DWORD old_displayed_items = tl->displayed_items;
    treelist_item_t* child_head;
    BOOL is_displayed;
    int y;
    int displayed_del_count;

    TREELIST_TRACE("treelist_delete_children(%p, %p)", tl, item);

    MC_ASSERT(item != NULL);

    if(item->child_head == NULL)
        return;

    /* Remember some info about the item. */
    is_displayed = item_is_displayed(item->child_head);
    y = (is_displayed ? treelist_get_item_y(tl, item->child_head, TRUE) : -1);

    /* This should be very last notification about the item and its subtree. */
    treelist_delete_notify(tl, item->child_head, item);

    /* Disconnect the children from the tree. */
    child_head = item->child_head;
    item->child_head = NULL;
    item->child_tail = NULL;

    /* Reset item bookmarks which will need recomputing anyway. */
    if(tl->hot_item != NULL  &&  !tl->no_redraw)
        treelist_invalidate_item(tl, tl->hot_item, -1, 0);
    tl->hot_item = NULL;
    if(tl->hotbutton_item != NULL  &&  !tl->no_redraw)
        treelist_invalidate_item(tl, tl->hotbutton_item, 0, 0);
    tl->hotbutton_item = NULL;

    /* Delete the child items and their subtrees, and count how many of deleted
     * items were displayed. */
    displayed_del_count = treelist_delete_item_helper(tl, child_head, is_displayed);
    if(is_displayed)
        tl->displayed_items -= displayed_del_count;

    /* Refresh */
    if(tl->displayed_items != old_displayed_items) {
        treelist_setup_scrollbars(tl);

        if(!tl->no_redraw) {
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
        }
    }

   treelist_refresh_hot(tl);

    /* Parent may need to repaint column 0 without expand button. */
    if(!tl->no_redraw)
        treelist_invalidate_item(tl, item, 0, 0);
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
        int i = subitem_data->iSubItem - 1;

        if(!item->has_alloced_subitems  &&
           ((subitem_data->pszText != NULL && subitem_data->pszText != MC_LPSTR_TEXTCALLBACK) ||
            (subitem_data->pszText == MC_LPSTR_TEXTCALLBACK  &&  i >= CALLBACK_MAP_SIZE)))
        {
            /* ->callback_map cannot be used anymore to hold the subitems. */
            if(MC_ERR(treelist_subitems_alloc(tl, item, tl->col_count) != 0)) {
                MC_TRACE("treelist_set_subitem: treelist_subitems_alloc() failed.");
                return FALSE;
            }
        }

        if(item->has_alloced_subitems) {
            TCHAR* text;

            if(subitem_data->pszText == MC_LPSTR_TEXTCALLBACK)
                text = MC_LPSTR_TEXTCALLBACK;
            else {
                text = mc_str(subitem_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
                if(MC_ERR(text == NULL  &&  subitem_data->pszText != NULL)) {
                    MC_TRACE("treelist_set_subitem: mc_str() failed.");
                    return FALSE;
                }
            }

            if(item->subitems[i] != NULL  &&
               item->subitems[i] != MC_LPSTR_TEXTCALLBACK)
                free(item->subitems[i]);
            item->subitems[i] = text;
        } else {
            MC_ASSERT(subitem_data->pszText == NULL  ||
                      subitem_data->pszText == MC_LPSTR_TEXTCALLBACK);

            if(subitem_data->pszText == MC_LPSTR_TEXTCALLBACK)
                item->callback_map |= CALLBACK_MAP_BIT(i);
            else
                item->callback_map &= ~CALLBACK_MAP_BIT(i);
        }
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
        treelist_subdispinfo_t subdispinfo;

        treelist_get_subdispinfo(tl, item, subitem_data->iSubItem,
                                 &subdispinfo, MC_TLSIF_TEXT);
        mc_str_inbuf(subdispinfo.text, MC_STRT, subitem_data->pszText,
                     (unicode ? MC_STRW : MC_STRA), subitem_data->cchTextMax);
        treelist_free_subdispinfo(tl, item, subitem_data->iSubItem, &subdispinfo);
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

    if(imglist == tl->imglist)
        return;

    tl->imglist = imglist;
    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

static int
treelist_do_get_item_rect(treelist_t* tl, MC_HTREELISTITEM item, int col_ix,
                          int what, RECT* rect)
{
    MC_HTREELISTITEM iter;
    MC_HTREELISTITEM stopper;
    int level;
    int y;
    RECT header_rect;
    int header_height;
    RECT header_item_rect;

    /* No rect cases */
    if(!item_is_displayed(item)  ||  (what == MC_TLIR_ICON  &&  tl->imglist == NULL)) {
        mc_rect_set(rect, 0, 0, 0, 0);
        return -1;
    }

    /* Get header height */
    GetWindowRect(tl->header_win, &header_rect);
    header_height = mc_height(&header_rect);

    /* Optimization: It can be assumed that most calls to this function are
     * calls about items which are in the current view-port. Therefore we
     * try to start the search at the treelist_t::scrolled_item and if we are
     * unsuccessful, we then wrap and start at the root. */
    if(tl->scrolled_item != NULL) {
        iter = tl->scrolled_item;
        level = tl->scrolled_level;
        y = header_height;

        while(iter != NULL) {
            if(iter == item)
                break;
            iter = item_next_displayed(iter, &level);
            y += tl->item_height;
        }

        stopper = tl->scrolled_item;
    } else {
        iter = NULL;
        stopper = NULL;
    }

    /* If not yet found, we do regular search from the top of the tree. */
    if(iter == NULL) {
        iter = tl->root_head;
        level = 0;
        y = header_height - (tl->scroll_y * tl->item_height);

        while(iter != stopper) {
            if(iter == item)
                break;
            iter = item_next_displayed_ex(iter, stopper, &level);
            y += tl->item_height;
        }
    }

    if(MC_ERR(iter == NULL)) {
        MC_TRACE("treelist_do_get_item_rect: The item not found. "
                 "Likely invalid item handle. (App's bug).");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    /* Get header item rect */
    if(col_ix == 0  &&  what == MC_TLIR_BOUNDS) {
        int index;

        index = MC_SEND(tl->header_win, HDM_ORDERTOINDEX, tl->col_count - 1, 0);
        MC_SEND(tl->header_win, HDM_GETITEMRECT, index, &header_item_rect);
    } else {
        MC_SEND(tl->header_win, HDM_GETITEMRECT, col_ix, &header_item_rect);
    }

    mc_rect_set(rect, header_item_rect.left, y, header_item_rect.right, y + tl->item_height);

    /* For MC_TLIR_BOUNDS, we are done. */
    if(what == MC_TLIR_BOUNDS) {
        if(col_ix == 0)
            rect->left = -tl->scroll_x;
        return 0;
    }

    /* For the main item, get past the lines and indentation. */
    if(col_ix == 0) {
        rect->left += level * tl->item_indent;
        if(tl->style & MC_TLS_LINESATROOT)
            rect->left += tl->item_indent;
        rect->left += ITEM_PADDING_H;
    }

    if(col_ix == 0  &&  tl->imglist != NULL) {
        int img_w, img_h;

        ImageList_GetIconSize(tl->imglist, &img_w, &img_h);

        /* For MC_TLIR_ICON, we are done. */
        if(what == MC_TLIR_ICON) {
            MC_ASSERT(col_ix == 0);
            MC_ASSERT(tl->imglist != NULL);

            /* We do the same as listview (LVM_GETITEMRECT), which covers all
             * the row height. */
            rect->right = rect->left + img_w;
            return 0;
        }

        /* For MC_TLIR_LABEL, get past the image */
        if(what == MC_TLIR_LABEL)
            rect->left += img_w;
    }

    rect->left += ITEM_PADDING_H;

    if(what == MC_TLIR_LABEL) {
        /* We do the same as listview (LVM_GETITEMRECT), which returns rect
         * after the icon, which covers whole cell to the right column border
         * and all the row height. (i.e. it ignores text width). */
        /*noop*/
    } else {
        int str_width;

        MC_ASSERT(what == MC_TLIR_SELECTBOUNDS  &&  col_ix == 0);

        str_width = treelist_label_width(tl, item, col_ix);
        if(rect->left + str_width < rect->right)
            rect->right = rect->left + str_width;
    }

    return 0;
}

static int
treelist_get_item_rect(treelist_t* tl, MC_HTREELISTITEM item, int what,
                       RECT* rect)
{
    if(MC_ERR(what != MC_TLIR_BOUNDS  &&  what != MC_TLIR_ICON  &&
              what != MC_TLIR_LABEL  &&  what != MC_TLIR_SELECTBOUNDS)) {
        MC_TRACE("treelist_get_item_rect: what %d not supported.", what);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    return treelist_do_get_item_rect(tl, item, 0, what, rect);
}

static int
treelist_get_subitem_rect(treelist_t* tl, MC_HTREELISTITEM item, int subitem_id,
                          int what, RECT* rect)
{
    if(MC_ERR(what != MC_TLIR_BOUNDS  &&  what != MC_TLIR_LABEL)) {
        MC_TRACE("treelist_get_subitem_rect: what %d not supported.", what);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(MC_ERR(subitem_id >= tl->col_count)) {
        MC_TRACE("treelist_get_subitem_rect: Column %d out of range.", subitem_id);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    return treelist_do_get_item_rect(tl, item, subitem_id, what, rect);
}

static int
treelist_header_notify(treelist_t* tl, NMHEADER* info)
{
    switch(info->hdr.code) {
        case HDN_BEGINDRAG:   /* We disable reorder of the column[0] */
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
                    MC_SEND(tl->header_win, HDM_GETITEM, info->iItem, &item);
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

                MC_SEND(tl->header_win, HDM_GETITEMRECT, info->iItem, &header_item_rect);
                old_width = mc_width(&header_item_rect);

                tl->scroll_x_max += new_width - old_width;
                treelist_setup_scrollbars(tl);

                if(!tl->no_redraw) {
                    RECT rect;
                    GetClientRect(tl->win, &rect);
                    rect.left = header_item_rect.right;
                    rect.top = mc_height(&header_item_rect);
                    ScrollWindowEx(tl->win, new_width - old_width, 0, &rect, &rect,
                                   NULL, NULL, SW_ERASE | SW_INVALIDATE);
                    treelist_invalidate_column(tl, info->iItem);
                    if(tl->style & MC_TLS_FULLROWSELECT)
                        treelist_invalidate_selected(tl, -1, 0);
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
    lres = MC_SEND(tl->notify_win, WM_NOTIFYFORMAT, tl->win, NF_QUERY);
    tl->unicode_notifications = (lres == NFR_UNICODE ? 1 : 0);
    TREELIST_TRACE("treelist_notify_format: Will use %s notifications.",
                   tl->unicode_notifications ? "Unicode" : "ANSI");
}

static void
treelist_open_theme(treelist_t* tl)
{
    tl->theme = mcOpenThemeData(tl->win, treelist_tc);

    tl->theme_treeitem_defined = (tl->theme != NULL  &&
                mcIsThemePartDefined(tl->theme, TVP_TREEITEM, 0));
    tl->theme_hotglyph_defined = (tl->theme != NULL  &&
                mcIsThemePartDefined(tl->theme, TVP_HOTGLYPH, 0));
}

static void
treelist_theme_changed(treelist_t* tl)
{
    if(tl->theme)
        mcCloseThemeData(tl->theme);

    treelist_open_theme(tl);

    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

static void
treelist_style_changed(treelist_t* tl, STYLESTRUCT* ss)
{
    tl->style = ss->styleNew;

    if((ss->styleOld & MC_TLS_MULTISELECT) != (ss->styleNew & MC_TLS_MULTISELECT)) {
        if(!(ss->styleNew & MC_TLS_MULTISELECT)  &&  tl->selected_count > 1)
            treelist_set_sel(tl, tl->selected_last);
    }

    if((ss->styleOld & MC_TLS_NOTOOLTIPS) != (ss->styleNew & MC_TLS_NOTOOLTIPS)) {
        if(!(ss->styleNew & MC_TLS_NOTOOLTIPS)) {
            tl->tooltip_win = tooltip_create(tl->win, tl->notify_win, FALSE);
        } else {
            tooltip_destroy(tl->tooltip_win);
            tl->tooltip_win = NULL;
        }
    }

    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

static void
treelist_exstyle_changed(treelist_t* tl, STYLESTRUCT* ss)
{
    tl->rtl = mc_is_rtl_exstyle(ss->styleNew);

    if(!tl->no_redraw)
        InvalidateRect(tl->win, NULL, TRUE);
}

static LRESULT
treelist_tooltip_notify(treelist_t* tl, NMHDR* hdr)
{
    switch(hdr->code) {
        case TTN_SHOW:
            if(tl->hot_item != NULL  &&  tl->hot_col >= 0)
                treelist_update_tooltip_pos(tl);
            tl->active_tooltip = TRUE;
            return TRUE;

        case TTN_POP:
            tl->active_tooltip = FALSE;
            treelist_mouse_leave(tl);
            break;

        case TTN_GETDISPINFO:
        {
            NMTTDISPINFO* dispinfo = (NMTTDISPINFO*) hdr;

            if(tl->hot_item != 0  &&  tl->hot_col >= 0) {
                if(tl->hot_col == 0) {
                    treelist_dispinfo_t di;
                    treelist_get_dispinfo(tl, tl->hot_item, &di, MC_TLIF_TEXT);
                    dispinfo->lpszText = di.text;
                    treelist_free_dispinfo(tl, tl->hot_item, &di);
                } else {
                    treelist_subdispinfo_t sdi;
                    treelist_get_subdispinfo(tl, tl->hot_item, tl->hot_col, &sdi, MC_TLIF_TEXT);
                    dispinfo->lpszText = sdi.text;
                    treelist_free_subdispinfo(tl, tl->hot_item, tl->hot_col, &sdi);
                }
            } else {
                dispinfo->lpszText = NULL;
            }
            break;
        }
    }

    return 0;
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
    tl->hot_col = -1;
    tl->rtl = mc_is_rtl_exstyle(cs->dwExStyle);

    treelist_notify_format(tl);

    doublebuffer_init();
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
    MC_SEND(tl->header_win, HDM_SETUNICODEFORMAT, MC_IS_UNICODE, 0);

    if(!(tl->style & MC_TLS_NOTOOLTIPS))
        tl->tooltip_win = tooltip_create(tl->win, tl->notify_win, FALSE);

    treelist_open_theme(tl);
    return 0;
}

static void
treelist_destroy(treelist_t* tl)
{
    treelist_delete_item(tl, NULL);

    if(tl->tooltip_win != NULL) {
        if(!(tl->style & MC_TLS_NOTOOLTIPS))
            tooltip_destroy(tl->tooltip_win);
        else
            tooltip_uninstall(tl->tooltip_win, tl->win);
    }

    if(tl->theme) {
        mcCloseThemeData(tl->theme);
        tl->theme = NULL;
    }
}

static void
treelist_ncdestroy(treelist_t* tl)
{
    doublebuffer_fini();
    free(tl);
}

static LRESULT CALLBACK
treelist_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    treelist_t* tl = (treelist_t*) GetWindowLongPtr(win, 0);
    MC_ASSERT(tl != NULL  ||  msg == WM_NCCREATE  ||  msg == WM_NCDESTROY);

    if(tl != NULL  &&  tl->tooltip_win != NULL)
        tooltip_forward_msg(tl->tooltip_win, win, msg, wp, lp);

    switch(msg) {
        case WM_PAINT:
            return generic_paint(win, tl->no_redraw,
                                 (tl->style & MC_TLS_DOUBLEBUFFER),
                                 treelist_paint, tl);

        case WM_PRINTCLIENT:
            return generic_printclient(win, (HDC) wp, treelist_paint, tl);

        case WM_NCPAINT:
            return generic_ncpaint(win, tl->theme, (HRGN) wp);

        case WM_ERASEBKGND:
            return generic_erasebkgnd(win, tl->theme, (HDC) wp);

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
            return MC_SEND(tl->header_win, HDM_GETORDERARRAY, wp, lp);

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
            ok = MC_SEND(tl->header_win, HDM_GETITEM, wp, &header_item);
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
            HIMAGELIST imglist = tl->imglist;
            treelist_set_imagelist(tl, (HIMAGELIST) lp);
            return (LRESULT) imglist;
        }

        case MC_TLM_GETIMAGELIST:
            return (LRESULT) tl->imglist;

        case MC_TLM_GETSELECTEDCOUNT:
            return (LRESULT)tl->selected_count;

        case MC_TLM_GETITEMRECT:
        {
            MC_HTREELISTITEM item = (MC_HTREELISTITEM) wp;
            RECT* r = (RECT*) lp;
            return (treelist_get_item_rect(tl, item, r->left, r) == 0);
        }

        case MC_TLM_GETSUBITEMRECT:
        {
            MC_HTREELISTITEM item = (MC_HTREELISTITEM) wp;
            RECT* r = (RECT*) lp;
            return (treelist_get_subitem_rect(tl, item, r->top, r->left, r) == 0);
        }

        case MC_TLM_SETTOOLTIPS:
            return generic_settooltips(win, &tl->tooltip_win, (HWND) wp, FALSE);

        case MC_TLM_GETTOOLTIPS:
            return (LRESULT) tl->tooltip_win;

        case WM_NOTIFY:
            if(((NMHDR*)lp)->hwndFrom == tl->header_win)
                return treelist_header_notify(tl, (NMHEADER*) lp);
            if(((NMHDR*) lp)->hwndFrom == tl->tooltip_win)
                return treelist_tooltip_notify(tl, (NMHDR*) lp);
            break;

        case WM_SIZE:
            treelist_layout_header(tl);
            treelist_setup_scrollbars(tl);
            if(!tl->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_MOUSEMOVE:
            treelist_mouse_move(tl, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_MOUSELEAVE:
            treelist_mouse_leave(tl);
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
                                 (msg == WM_LBUTTONDBLCLK), wp);
            return 0;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            treelist_right_button(tl, GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                  (msg == WM_RBUTTONDBLCLK), wp);
            return 0;

        case WM_KEYDOWN:
            treelist_key_down(tl, wp);
            return 0;

        case WM_SETFOCUS:
            if(tl->selected_count == 0) {
                treelist_item_t* item;
                int ignored;

                item = treelist_scrolled_item(tl, &ignored);
                if(item)
                    treelist_set_sel(tl, item);
            }
            /* Fall through */
        case WM_KILLFOCUS:
            tl->focus = (msg == WM_SETFOCUS);
            mc_send_notify(tl->notify_win, win,
                           (msg == WM_SETFOCUS ? NM_SETFOCUS : NM_KILLFOCUS));
            if(!tl->no_redraw) {
                treelist_invalidate_selected(tl,
                    ((tl->style & MC_TLS_FULLROWSELECT) ? -1 : 0), 0);
            }
            return 0;

        case WM_GETFONT:
            return (LRESULT) tl->font;

        case WM_SETFONT:
            tl->font = (HFONT) wp;
            MC_SEND(tl->header_win, WM_SETFONT, wp, lp);
            treelist_set_item_height(tl, -1, (BOOL) lp);
            return 0;

        case WM_SETREDRAW:
            tl->no_redraw = !wp;
            if(!tl->no_redraw) {
                if(tl->dirty_scrollbars)
                    treelist_setup_scrollbars(tl);
                InvalidateRect(win, NULL, TRUE);
            }
            return 0;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE)
                treelist_style_changed(tl, (STYLESTRUCT*) lp);
            if(wp == GWL_EXSTYLE)
                treelist_exstyle_changed(tl, (STYLESTRUCT*) lp);
            break;

        case WM_THEMECHANGED:
            treelist_theme_changed(tl);
            return 0;

        case WM_SYSCOLORCHANGE:
            if(!tl->no_redraw)
                InvalidateRect(tl->win, NULL, TRUE);
            return 0;

        case WM_NOTIFYFORMAT:
            switch(lp) {
                case NF_REQUERY:
                    treelist_notify_format(tl);
                    return (tl->unicode_notifications ? NFR_UNICODE : NFR_ANSI);
                case NF_QUERY:
                    return (MC_IS_UNICODE ? NFR_UNICODE : NFR_ANSI);
            }
            break;

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
