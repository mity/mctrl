/*
 * Copyright (c) 2008-2013 Martin Mitas
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

#include "mditab.h"
#include "dsa.h"
#include "theme.h"

/* TODO:
 *
 * Features:
 * - Support drag&drop:
 *    [a] reordering tabs with mouse drag&drop in the control
 *        (fully implemented internally in mCtrl.dll)
 *    [b] dragging the tab to other targets (only if some new style is used).
 *        Parent window will get notifications about the drag&drop attempt.
 *        So for example, application can implement moving the tab from one
 *        MTAB control to another MTAB control (which can be in another
 *        top-level window).
 *
 * Messages:
 * - MC_MTM_REMOVEIMAGE (see TCM_REMOVEIMAGE)
 * - MC_MTM_SETTOOLTIPS/MC_MTM_GETTOOLTIPS (see TCM_SETTOOLTIPS/TCM_GETTOOLTIPS)
 * - MC_MTM_HITTEST - add support for MC_MTHT_ONITEMCLOSEBUTTON
 *
 * Styles:
 * - MC_MTS_CBONEACHTAB - close button on each tab
 * - MC_MTS_CBONACTIVETAB - close button on the active tab
 */


/* Uncomment this to have more verbose traces from this module. */
/*#define MDITAB_DEBUG     1*/

#ifdef MDITAB_DEBUG
    #define MDITAB_TRACE       MC_TRACE
#else
    #define MDITAB_TRACE(...)  do {} while(0)
#endif



static const TCHAR mditab_wc[] = MC_WC_MDITAB;  /* window class name */
static const WCHAR mditab_tc[] = L"TAB";        /* theme class name */

static const TCHAR toolbar_wc[] = TOOLBARCLASSNAME;


/* Geometry constants */
#define DEFAULT_ITEM_MIN_WIDTH       60
#define DEFAULT_ITEM_DEF_WIDTH        0

#define ITEM_PADDING_H                4
#define ITEM_PADDING_V                2
#define ITEM_CORNER_SIZE              3

#define SELECTED_EXTRA_LEFT           2
#define SELECTED_EXTRA_TOP            2
#define SELECTED_EXTRA_RIGHT          1
#define SELECTED_EXTRA_BOTTOM         1

#define TOOLBAR_BTN_WIDTH            16

#define MARGIN_H                      2

/* Timer for hot tab tracking */
#define HOT_TRACK_TIMER_ID            1
#define HOT_TRACK_TIMER_INTERVAL    100

/* Timer for animation */
#define ANIM_TIMER_ID                 2
#define ANIM_TIMER_INTERVAL       (1000 / 25)   /* 25 frames per sec. */
#define ANIM_DELTA                   32         /* max. pixels to move between frames */

/* Child control IDs */
#define IDC_TBAR_1                  100  /* left toolbar */
#define IDC_SCROLL_LEFT             101
#define IDC_TBAR_2                  200  /* right toolbar */
#define IDC_SCROLL_RIGHT            201
#define IDC_LIST_ITEMS              202
#define IDC_CLOSE_ITEM              203

#define BTN_SCROLL                  0x1
#define BTN_LIST                    0x2
#define BTN_CLOSE                   0x4


/* Per-tab structure */
typedef struct mditab_item_tag mditab_item_t;
struct mditab_item_tag {
    TCHAR* text;
    LPARAM lp;
    SHORT img;
    USHORT left;
    USHORT right;
    USHORT desired_width;
};

/* Per-control structure */
typedef struct mditab_tag mditab_t;
struct mditab_tag {
    HWND win;
    HWND tb1_win;              /* left-side toolbar */
    HWND tb2_win;              /* right-side toolbar */
    HWND notify_win;           /* notifications target window */
    HIMAGELIST img_list;
    HTHEME theme;
    HFONT font;
    dsa_t items;
    RECT main_rect;
    DWORD ui_state;
    DWORD style        : 16;
    DWORD btn_mask     :  3;
    DWORD no_redraw    :  1;
    DWORD hot_tracking :  1;
    DWORD dirty_layout :  1;
    DWORD dirty_scroll :  1;
    DWORD is_animating :  1;
    int scroll_x;
    int scroll_x_desired;
    int scroll_x_max;
    SHORT item_selected;
    SHORT item_hot;            /* tracked only when themed */
    SHORT item_mclose;         /* candidate item to close by middle button */
    USHORT item_min_width;
    USHORT item_def_width;
};


static inline mditab_item_t*
mditab_item(mditab_t* mditab, WORD index)
{
    return DSA_ITEM(&mditab->items, index, mditab_item_t);
}

static inline WORD
mditab_count(mditab_t* mditab)
{
    return dsa_size(&mditab->items);
}

static void
mditab_item_dtor(dsa_t* dsa, void* it)
{
    mditab_item_t* item = (mditab_item_t*) it;
    if(item->text != NULL)
        free(item->text);
}

static int
mditab_calc_desired_width(mditab_t* mditab, WORD index)
{
    int width;

    width = mc_width(&mditab->main_rect) / mditab_count(mditab);
    if(width > mditab->item_def_width) {
        if(mditab->item_def_width > 0) {
            width = mditab->item_def_width;
        } else {
            mditab_item_t* item = mditab_item(mditab, index);

            width = 2 *  ITEM_PADDING_H;
            if(mditab->img_list != NULL) {
                int w, h;

                ImageList_GetIconSize(mditab->img_list, &w, &h);
                width += w + 3;
            }

            if(item->text != NULL)
                width += mc_string_width(item->text, mditab->font);
        }
    }

    if(mditab->item_min_width > 0  &&  width < mditab->item_min_width)
        width = mditab->item_min_width;

    return width;
}

static inline void
mditab_inflate_selected_item_rect(RECT* rect)
{
    rect->left -= SELECTED_EXTRA_LEFT;
    rect->top -= SELECTED_EXTRA_TOP;
    rect->right += SELECTED_EXTRA_RIGHT;
    rect->bottom += SELECTED_EXTRA_BOTTOM;
}

static inline void
mditab_calc_item_rect(mditab_t* mditab, WORD index, RECT* rect)
{
    mditab_item_t* item = mditab_item(mditab, index);

    rect->left = mditab->main_rect.left + item->left - mditab->scroll_x;
    rect->top = mditab->main_rect.top;
    rect->right = mditab->main_rect.left + item->right - mditab->scroll_x;
    rect->bottom = mditab->main_rect.bottom - 4;

    if(index == mditab->item_selected)
        mditab_inflate_selected_item_rect(rect);
}

static inline void
mditab_calc_contents_rect(RECT* contents, const RECT* tab_rect)
{
    contents->left = tab_rect->left + ITEM_PADDING_H;
    contents->top = tab_rect->top + ITEM_PADDING_V;
    contents->right = tab_rect->right - ITEM_PADDING_H;
    contents->bottom = tab_rect->bottom;
}

static inline void
mditab_calc_ico_rect(RECT* ico_rect, const RECT* contents, int ico_w, int ico_h)
{
    ico_rect->left = contents->left;
    ico_rect->top = (contents->top + contents->bottom - ico_h) / 2;
    ico_rect->right = ico_rect->left + ico_w;
    ico_rect->bottom = ico_rect->top + ico_h;
}

static BOOL
mditab_is_item_visible(mditab_t* mditab, int index)
{
    mditab_item_t* item = mditab_item(mditab, index);

    if(item->right - mditab->scroll_x <= mditab->main_rect.left)
        return FALSE;
    if(item->left - mditab->scroll_x >= mditab->main_rect.right)
        return FALSE;
    return TRUE;
}

static mditab_item_t*
mditab_item_by_x(mditab_t* mditab, int x, BOOL relaxed)
{
    mditab_item_t* item;
    WORD i, n;

    n = mditab_count(mditab);
    if(n == 0)
        return NULL;

    x += mditab->scroll_x - mditab->main_rect.left;
    if(x >= mditab->scroll_x_max)
        return NULL;

    i = (x * n) / mditab->scroll_x_max;
    MDITAB_TRACE("mditab_item_by_x(%p, %d) --> 1st guess %d", mditab, x, i);
    if(i >= n)
        i = n-1;
    item = mditab_item(mditab, i);
    if(x < item->left) {
        while(i > 0  &&  mditab_item(mditab, i-1)->right >= x)
            i--;
        item = mditab_item(mditab, i);
    } else if(x >= item->right) {
        while(TRUE) {
            if(i == n-1)
                return NULL;
            i++;
            item = mditab_item(mditab, i);
            if(x < item->right)
                break;
        }
    }

    MC_ASSERT(item != NULL);
    MC_ASSERT(i < n);
    if(!relaxed  &&  x < item->left)
        return NULL;
    return item;
}

static int
mditab_hit_test(mditab_t* mditab, MC_MTHITTESTINFO* hti)
{
    mditab_item_t* item;

    MDITAB_TRACE("mditab_hit_test(%p, %p): x = %d, y = %d",
                 mditab, hti, hti->pt.x, hti->pt.y);

    if(!mc_rect_contains_pt(&mditab->main_rect, &hti->pt))
        goto nowhere;

    item = mditab_item_by_x(mditab, hti->pt.x, FALSE);
    if(item == NULL)
        goto nowhere;

    if(mditab->img_list != NULL) {
        RECT r;
        RECT contents;
        RECT ico_rect;
        int ico_w, ico_h;

        ImageList_GetIconSize(mditab->img_list, &ico_w, &ico_h);
        mc_rect_set(&r, item->left, mditab->main_rect.top,
                        item->right, mditab->main_rect.bottom);
        mditab_calc_contents_rect(&contents, &r);
        mditab_calc_ico_rect(&ico_rect, &contents, ico_w, ico_h);

        if(mc_rect_contains_pt(&ico_rect, &hti->pt)) {
            hti->flags = MC_MTHT_ONITEMICON;
            goto found;
        }
    }

    /* TODO: check for MC_MTHT_ONITEMCLOSEBUTTON */

    hti->flags = MC_MTHT_ONITEMLABEL;

found:
    return dsa_index(&mditab->items, item);

nowhere:
    hti->flags = MC_MTHT_NOWHERE;
    return -1;
}

static void
mditab_invalidate_item(mditab_t* mditab, int index)
{
    RECT r;

    if(mditab->no_redraw  ||  !mditab_is_item_visible(mditab, index))
        return;

    mditab_calc_item_rect(mditab, index, &r);

    /* Invalidate a bit more to avoid artifacts for item loosing the selected
     * state. */
    if(mditab->item_selected != index)
        mditab_inflate_selected_item_rect(&r);

    InvalidateRect(mditab->win, &r, TRUE);
}

static void CALLBACK
mditab_track_hot_timer_proc(HWND win, UINT msg, UINT_PTR id, DWORD time)
{
    POINT pt;

    if(!GetCursorPos(&pt)  ||  WindowFromPoint(pt) != win) {
        /* The mouse cursor left the tab control, so no tab should be
         * marked as hot */
        mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);
        if(mditab->item_hot >= 0) {
            mditab_invalidate_item(mditab, mditab->item_hot);
            mditab->item_hot = -1;
        }

        /* Kill this timer */
        KillTimer(win, HOT_TRACK_TIMER_ID);
        mditab->hot_tracking = 0;
    }
}

static void
mditab_track_hot(mditab_t* mditab, int x, int y)
{
    int index;

    MDITAB_TRACE("mditab_track_hot(%p, %d, %d)", mditab, x, y);

    if(mditab->theme) {
        MC_MTHITTESTINFO hti;

        hti.pt.x = x;
        hti.pt.y = y;
        index = mditab_hit_test(mditab, &hti);
    } else {
        /* When not themed, hot tabs look as any inactive one, so there is
         * no need to track it. So lets save the timer and lot of useless
         * redrawing when user moves the mouse over the control. */
        index = -1;
    }

    if(index != mditab->item_hot) {
        /* Setup the new hot tab */
        int old_index = mditab->item_hot;
        mditab->item_hot = index;

        /* Redraw old hot tab */
        if(old_index >= 0)
            mditab_invalidate_item(mditab, old_index);

        if(index >= 0) {
            /* Cause to redraw the new hot tab */
            mditab_invalidate_item(mditab, index);

            /* Set a timer to detect when mouse leavs the window */
            if(SetTimer(mditab->win, HOT_TRACK_TIMER_ID,
                    HOT_TRACK_TIMER_INTERVAL, mditab_track_hot_timer_proc))
                mditab->hot_tracking = 1;
        } else {
            /* No tab is hot so the timer is not needed. */
            if(mditab->hot_tracking) {
                KillTimer(mditab->win, HOT_TRACK_TIMER_ID);
                mditab->hot_tracking = 0;
            }
        }
    }
}

static BOOL
mditab_setup_all_desired_widths(mditab_t* mditab)
{
    mditab_item_t* item;
    int desired_width;
    int i, n;
    BOOL changed = FALSE;

    MDITAB_TRACE("mditab_setup_all_desired_widths(%p)", mditab);

    n = mditab_count(mditab);
    if(n == 0)
        return FALSE;

    if(mditab->item_def_width > 0) {
        /* All items have the same width */
        item = mditab_item(mditab, 0);
        desired_width = mditab_calc_desired_width(mditab, 0);
        for(i = 0; i < n; i++) {
            if(desired_width != item->desired_width) {
                item->desired_width = desired_width;
                changed = TRUE;
            }
        }
    } else {
        for(i = 0; i < n; i++) {
            item = mditab_item(mditab, i);
            desired_width = mditab_calc_desired_width(mditab, i);
            if(desired_width != item->desired_width) {
                item->desired_width = desired_width;
                changed = TRUE;
            }
        }
    }

    return changed;
}

static void
mditab_setup_toolbar_states(mditab_t* mditab)
{
    if(mditab->btn_mask & BTN_SCROLL) {
        MC_MSG(mditab->tb1_win, TB_ENABLEBUTTON, IDC_SCROLL_LEFT,
               MAKELPARAM((mditab->scroll_x > 0), 0));
        MC_MSG(mditab->tb2_win, TB_ENABLEBUTTON, IDC_SCROLL_RIGHT,
               MAKELPARAM((mditab->scroll_x < mditab->scroll_x_max - mc_width(&mditab->main_rect)), 0));
    }
    if(mditab->btn_mask & BTN_LIST) {
        MC_MSG(mditab->tb2_win, TB_ENABLEBUTTON, IDC_LIST_ITEMS,
               MAKELPARAM((mditab_count(mditab) > 0), 0));
    }
    if(mditab->btn_mask & BTN_CLOSE) {
        MC_MSG(mditab->tb2_win, TB_ENABLEBUTTON, IDC_CLOSE_ITEM,
               MAKELPARAM((mditab->item_selected >= 0), 0));
    }
}

/* Forward declaration */
static void mditab_animate(mditab_t* mditab);

static int
mditab_anim_next_value(int value, int desired_value)
{
    if(value < desired_value)
        value = MC_MIN(value + ANIM_DELTA, desired_value);
    else if(value > desired_value)
        value = MC_MAX(value - ANIM_DELTA, desired_value);
    return value;
}

static void
mditab_do_scroll(mditab_t* mditab, int scroll, BOOL animate, BOOL refresh)
{
    int max_scroll = mditab->scroll_x_max - mc_width(&mditab->main_rect);
    int old_scroll;

    if(scroll > max_scroll)
        scroll = max_scroll;
    if(scroll < 0)
        scroll = 0;
    if(mditab->scroll_x == scroll) {
        mditab->dirty_scroll = FALSE;
        return;
    }

    mditab->scroll_x_desired = scroll;
    old_scroll = mditab->scroll_x;

    if(mditab->style & MC_MTS_ANIMATE) {
        mditab->scroll_x = mditab_anim_next_value(mditab->scroll_x, mditab->scroll_x_desired);
        mditab->dirty_scroll = (mditab->scroll_x != mditab->scroll_x_desired);
        if(mditab->dirty_scroll)
            mditab_animate(mditab);
    } else {
        mditab->scroll_x = mditab->scroll_x_desired;
        mditab->dirty_scroll = FALSE;
    }

    mditab_setup_toolbar_states(mditab);
    if(refresh  &&  !mditab->no_redraw) {
        RECT r;
        GetClientRect(mditab->win, &r);
        r.left = mditab->main_rect.left;
        r.right = mditab->main_rect.right;
        ScrollWindowEx(mditab->win, old_scroll - mditab->scroll_x, 0, &r, &r,
                       NULL, NULL, SW_ERASE | SW_INVALIDATE);

        /* There may be more painted outside the ->main_rect if the active tab
         * is at the edge. */
        r.left = mditab->main_rect.left - SELECTED_EXTRA_LEFT;
        r.right = mditab->main_rect.left;
        InvalidateRect(mditab->win, &r, TRUE);
        r.left = mditab->main_rect.right;
        r.right = mditab->main_rect.right + SELECTED_EXTRA_RIGHT;
        InvalidateRect(mditab->win, &r, TRUE);
    }
}

static inline void
mditab_do_scroll_rel(mditab_t* mditab, int delta, BOOL animate, BOOL refresh)
{
    mditab_do_scroll(mditab, mditab->scroll_x_desired + delta, animate, refresh);
}

static inline void
mditab_scroll(mditab_t* mditab, int scroll, BOOL refresh)
{
    mditab_do_scroll(mditab, scroll, (mditab->style & MC_MTS_ANIMATE), refresh);
}

static inline void
mditab_scroll_rel(mditab_t* mditab, int delta, BOOL refresh)
{
    mditab_scroll(mditab, mditab->scroll_x_desired + delta, refresh);
}

static void
mditab_scroll_to_item(mditab_t* mditab, int index)
{
    mditab_item_t* item;
    int scroll;

    item = mditab_item(mditab, index);

    if(item->left < mditab->scroll_x  ||
       (item->right - item->left) > mc_width(&mditab->main_rect))
        scroll = item->left;
    else if(item->right > mditab->scroll_x + mc_width(&mditab->main_rect))
        scroll = item->right - mc_width(&mditab->main_rect);
    else
        return;

    mditab_scroll(mditab, scroll, TRUE);
}

static inline void
mditab_add_button(HWND tb_win, int cmd_id, int bmp_id, BYTE style)
{
    TBBUTTON btn = {0};

    btn.iBitmap = bmp_id;
    btn.idCommand = cmd_id;
    btn.fsStyle = style;
    MC_MSG(tb_win, TB_ADDBUTTONS, 1, &btn);
}

static void
mditab_do_layout(mditab_t* mditab, BOOL animate, BOOL refresh)
{
    RECT rect;
    DWORD btn_mask;
    int tb1_width, tb2_width;
    int i, n;
    BOOL need_scroll = FALSE;
    BOOL scrolled_to_max = FALSE;
    mditab_item_t* item;
    int old_space;
    POINT pt;

    MDITAB_TRACE("mditab_do_layout(%p, %d)", mditab, refresh);

    GetClientRect(mditab->win, &rect);
    n = mditab_count(mditab);

    if(mditab->scroll_x_desired + mc_width(&mditab->main_rect) >= mditab->scroll_x_max)
        scrolled_to_max = TRUE;

    /* Detect what buttons we need */
    btn_mask = 0;
    tb1_width = 0;
    tb2_width = 0;
    if(need_scroll || (mditab->style &  MC_MTS_SCROLLALWAYS)) {
        btn_mask |= BTN_SCROLL;
        tb1_width += TOOLBAR_BTN_WIDTH;
        tb2_width += TOOLBAR_BTN_WIDTH;
    }
    if(((mditab->style & MC_MTS_TLBMASK) == MC_MTS_TLBALWAYS) ||
       ((mditab->style & MC_MTS_TLBMASK) == MC_MTS_TLBONSCROLL && need_scroll)) {
        btn_mask |= BTN_LIST;
        tb2_width += TOOLBAR_BTN_WIDTH;
    }
    if((mditab->style & MC_MTS_CBMASK) == MC_MTS_CBONTOOLBAR) {
        btn_mask |= BTN_CLOSE;
        tb2_width += TOOLBAR_BTN_WIDTH;
    }

    if(!(btn_mask & BTN_SCROLL)) {
        int space;
        int needed_space;

        space = mc_width(&rect) - 4 * MARGIN_H;
        if(tb1_width > 0)
            space -= tb1_width + MARGIN_H;
        if(tb2_width > 0)
            space -= tb2_width + MARGIN_H;

        if(mditab->style & MC_MTS_ANIMATE) {
            /* Strictly speaking, this is wrong. We would need oraculum to
             * get the new ->scroll_x_max after the calculations below
             * (or perform those calculations twice). But instead we make sure
             * that when the animation ends, mditab_animate_timer_proc() calls
             * whole this function once more, syncing the toolbars to the
             * expected state. The impact to user experience is negligible, and
             * it saves some CPU cycles and simplifies the code very much. */
            needed_space = mditab->scroll_x_max;
        } else {
            needed_space = n * mditab->item_min_width;
        }

        if(needed_space > space) {
            btn_mask |= BTN_SCROLL;
            tb1_width += TOOLBAR_BTN_WIDTH;
            tb2_width += TOOLBAR_BTN_WIDTH;
        }
    }

    /* Calculate ->main_rect */
    old_space = mc_width(&mditab->main_rect);

    mditab->main_rect.left = rect.left + 2 * MARGIN_H;
    mditab->main_rect.right = rect.right - 2 * MARGIN_H;
    if(tb1_width > 0)
        mditab->main_rect.left += tb1_width + MARGIN_H;
    if(tb2_width > 0)
        mditab->main_rect.right -= tb2_width + MARGIN_H;
    mditab->main_rect.top = rect.top + 5;
    mditab->main_rect.bottom = rect.bottom;

    if(old_space != mc_width(&mditab->main_rect))
        mditab_setup_all_desired_widths(mditab);

    /* Layout items */
    mditab->dirty_layout = FALSE;
    if(animate) {
        int desired_x = 0;

        for(i = 0; i < n; i++) {
            item = mditab_item(mditab, i);
            item->left = mditab_anim_next_value(item->left, desired_x);
            if(item->left != desired_x)
                mditab->dirty_layout = TRUE;
            desired_x += item->desired_width;
            item->right = mditab_anim_next_value(item->right, desired_x);
            if(item->right != desired_x)
                mditab->dirty_layout = TRUE;
        }

        if(mditab->dirty_layout)
            mditab_animate(mditab);
    } else {
        int x = 0;

        for(i = 0; i < n; i++) {
            item = mditab_item(mditab, i);
            item->left = x;
            x += item->desired_width;
            item->right = x;
        }
    }

    /* Ensure we are scrolled in the allowed range of the ->main_rect */
    if(n > 0)
        mditab->scroll_x_max = mditab_item(mditab, n-1)->right;
    else
        mditab->scroll_x_max = 0;
    if(!mditab->dirty_scroll) {
        if(mditab->scroll_x > mditab->scroll_x_max - mc_width(&mditab->main_rect))
            mditab_do_scroll_rel(mditab, 0, animate, FALSE);
        else if(scrolled_to_max)
            mditab_do_scroll(mditab, mditab->scroll_x_max, FALSE, FALSE);
    }

    /* Update ->hot_item */
    GetCursorPos(&pt);
    MapWindowPoints(NULL, mditab->win, &pt, 1);
    if(mc_rect_contains_pt(&mditab->main_rect, &pt))
        mditab_track_hot(mditab, pt.x, pt.y);

    /* Setup the toolbars */
    if(mditab->btn_mask != btn_mask) {
        while(MC_MSG(mditab->tb1_win, TB_DELETEBUTTON, 0, 0) == TRUE);
        while(MC_MSG(mditab->tb2_win, TB_DELETEBUTTON, 0, 0) == TRUE);

        if(btn_mask & BTN_SCROLL) {
            mditab_add_button(mditab->tb1_win, IDC_SCROLL_LEFT, MC_BMP_GLYPH_CHEVRON_L, TBSTYLE_BUTTON);
            mditab_add_button(mditab->tb2_win, IDC_SCROLL_RIGHT, MC_BMP_GLYPH_CHEVRON_R, TBSTYLE_BUTTON);
        }
        if(btn_mask & BTN_LIST)
            mditab_add_button(mditab->tb2_win, IDC_LIST_ITEMS, MC_BMP_GLYPH_MORE_OPTIONS, TBSTYLE_DROPDOWN);
        if(btn_mask & BTN_CLOSE)
            mditab_add_button(mditab->tb2_win, IDC_CLOSE_ITEM, MC_BMP_GLYPH_CLOSE, TBSTYLE_BUTTON);
        mditab->btn_mask = btn_mask;
    }

    mditab_setup_toolbar_states(mditab);

    /* Refresh */
    SetWindowPos(mditab->tb1_win, NULL, MARGIN_H, mditab->main_rect.top,
                 tb1_width, TOOLBAR_BTN_WIDTH,
                 SWP_NOZORDER | (tb1_width > 0 ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    SetWindowPos(mditab->tb2_win, NULL, rect.right - MARGIN_H - tb2_width,
                 mditab->main_rect.top, tb2_width, TOOLBAR_BTN_WIDTH,
                 SWP_NOZORDER | (tb2_width > 0 ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    if(refresh  &&  !mditab->no_redraw)
        InvalidateRect(mditab->win, NULL, TRUE);
}

static inline void
mditab_layout(mditab_t* mditab, BOOL refresh)
{
    mditab_do_layout(mditab, (mditab->style & MC_MTS_ANIMATE), refresh);
}

static void CALLBACK
mditab_animate_timer_proc(HWND win, UINT msg, UINT_PTR id, DWORD time)
{
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);
    BOOL do_layout = mditab->dirty_layout;
    BOOL do_scroll = mditab->dirty_scroll;

    if(!do_layout  &&  do_scroll) {
        /* Path for optimized refresh via ScrollWindowEx() */
        mditab_do_scroll_rel(mditab, 0, TRUE, TRUE);
    } else {
        if(do_layout) {
            mditab_do_layout(mditab, TRUE, FALSE);
            if(!mditab->dirty_layout) {
                /* This is a bit ugly hack. The animated layout change could
                 * lead to new ->scroll_x_max, which may require to show/hide
                 * scroll buttons. If we reached the desired state, we need
                 * to make sure the toolbar is synced. */
                mditab_do_layout(mditab, FALSE, FALSE);
            }
        }
        if(do_scroll)
            mditab_do_scroll_rel(mditab, 0, TRUE, FALSE);

        if(!mditab->no_redraw)
            InvalidateRect(mditab->win, NULL, TRUE);
    }

    /* Stop the timer if we reached the desired state */
    if(!mditab->dirty_scroll  &&  !mditab->dirty_layout) {
        KillTimer(mditab->win, ANIM_TIMER_ID);
        mditab->is_animating = FALSE;
    }
}

static void
mditab_animate_abort(mditab_t* mditab)
{
    if(mditab->is_animating) {
        KillTimer(mditab->win, ANIM_TIMER_ID);
        mditab->is_animating = FALSE;
    }

    if(mditab->dirty_layout)
        mditab_do_layout(mditab, FALSE, TRUE);
    if(mditab->dirty_scroll)
        mditab_do_scroll_rel(mditab, 0, FALSE, TRUE);
}

static void
mditab_animate(mditab_t* mditab)
{
    int timer_id;

    if(mditab->is_animating)
        return;

    timer_id = SetTimer(mditab->win, ANIM_TIMER_ID, ANIM_TIMER_INTERVAL,
                        mditab_animate_timer_proc);
    if(MC_ERR(timer_id == 0)) {
        MC_TRACE("mditab_animate: SetTimer() failed.");
        mditab_animate_abort(mditab);
        return;
    }

    mditab->is_animating = TRUE;
}

static void
mditab_paint_item(mditab_t* mditab, HDC dc, UINT index, RECT* rect)
{
    mditab_item_t* item;
    RECT contents;
    HTHEME theme = mditab->theme;
    int state = 0;
    UINT flags;

    item = mditab_item(mditab, index);

    MDITAB_TRACE("mditab_paint_item(%p, %p, %u, { %d, %d, %d, %d })",
                 mditab, dc, index, rect->left, rect->top, rect->right, rect->bottom);

    /* Draw tab background */
    if(theme) {
        if(!IsWindowEnabled(mditab->win)) {
            state = TTIS_DISABLED;
        } else {
            if(index == mditab->item_selected)
                state = TTIS_SELECTED;
            else if(index == mditab->item_hot)
                state = TTIS_HOT;
            else
                state = TTIS_NORMAL;
        }

        mcDrawThemeBackground(theme, dc, TABP_TOPTABITEM, state, rect, rect);
    } else {
        RECT r;

        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
        DrawEdge(dc, rect, EDGE_RAISED, BF_SOFT | BF_LEFT | BF_TOP | BF_RIGHT);

        /* Make left corner rounded */
        mc_rect_set(&r, rect->left, rect->top, rect->left + ITEM_CORNER_SIZE,
                    rect->top + ITEM_CORNER_SIZE);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, 0);
        DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_DIAGONAL_ENDTOPRIGHT);

        /* Make right corner rounded */
        mc_rect_set(&r, rect->right - ITEM_CORNER_SIZE + 1, rect->top, rect->right,
                    rect->top + ITEM_CORNER_SIZE);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, 0);
        r.top++;
        DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_DIAGONAL_ENDBOTTOMRIGHT);
    }

    /* If needed, draw focus rect */
    if(mditab->win == GetFocus()  &&  index == mditab->item_selected) {
        if((mditab->style & MC_MTS_FOCUSMASK) != MC_MTS_FOCUSNEVER) {
            if(!(mditab->ui_state & UISF_HIDEFOCUS)) {
                mc_rect_set(&contents, rect->left + 3, rect->top + 3,
                            rect->right - 3, rect->bottom - 2);
                DrawFocusRect(dc, &contents);
            }
        }
    }

    mditab_calc_contents_rect(&contents, rect);

    /* Draw tab icon */
    if(mditab->img_list != NULL  &&  item->img != (SHORT) I_IMAGENONE) {
        int ico_w, ico_h;
        RECT rect_ico;

        /* Compute where to draw the icon */
        ImageList_GetIconSize(mditab->img_list, &ico_w, &ico_h);
        mditab_calc_ico_rect(&rect_ico, &contents, ico_w, ico_h);

        /* Do the draw */
        if(theme) {
            mcDrawThemeIcon(theme, dc, TABP_TOPTABITEM, TTIS_NORMAL,
                           &rect_ico, mditab->img_list, item->img);
        } else {
            ImageList_Draw(mditab->img_list, item->img, dc,
                           rect_ico.left, rect_ico.top, ILD_TRANSPARENT);
        }

        /* Adjust the contents rect, where the text will be drawn */
        contents.left += ico_w + 3;
    }

    /* Draw text */
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, mditab->font);
    flags = 0;
    if(mditab->ui_state & UISF_HIDEACCEL)
        flags |= DT_HIDEPREFIX;
    if(theme) {
        contents.top += 2;
        mcDrawThemeText(theme, dc, TABP_TOPTABITEM, state, item->text, -1,
                            flags, 0, &contents);
    } else {
        if(index == mditab->item_selected)
            contents.bottom -= 2;
        DrawText(dc, item->text, -1, &contents, flags | DT_SINGLELINE | DT_VCENTER);
    }
}

static void
mditab_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    mditab_t* mditab = (mditab_t*) control;
    mditab_item_t* item;
    RECT rect;
    HFONT old_font;
    int old_bk_mode;
    COLORREF old_text_color;
    int dirty_left;
    int dirty_right;
    HRGN old_clip;

    MDITAB_TRACE("mditab_paint(%p, %p, %p, %d)", mditab, dc, dirty, erase);

    old_font = SelectObject(dc, mditab->font);
    old_bk_mode = GetBkMode(dc);
    old_text_color = GetTextColor(dc);

    GetClientRect(mditab->win, &rect);

    if(erase)
        mcDrawThemeParentBackground(mditab->win, dc, &rect);

    dirty_left = MC_MAX(dirty->left, mditab->main_rect.left);
    dirty_right = MC_MIN(dirty->right, mditab->main_rect.right);

    old_clip = mc_clip_get(dc);

    /* Draw unselected tabs */
    item = mditab_item_by_x(mditab, dirty_left, TRUE);
    if(item != NULL) {
        UINT i, i0, n;

        mc_clip_set(dc, mditab->main_rect.left, 0, mditab->main_rect.right, rect.bottom);

        i0 = dsa_index(&mditab->items, item);
        n = mditab_count(mditab);
        for(i = i0; i < n; i++) {
            RECT r;

            if(i == mditab->item_selected)
                continue;

            mditab_calc_item_rect(mditab, i, &r);
            mditab_paint_item(mditab, dc, i, &r);
            if(r.right >= dirty_right)
                break;
        }

        mc_clip_reset(dc, old_clip);
    }

    /* Draw pane */
    item = (mditab->item_selected >= 0) ? mditab_item(mditab, mditab->item_selected) : NULL;
    if(mditab->theme) {
        RECT r;

        mc_rect_set(&r, rect.left - 5, rect.bottom - 5,
                    rect.right + 5, rect.bottom + 1);
        mcDrawThemeBackground(mditab->theme, dc, TABP_PANE, 0, &r, &r);
    } else {
        RECT r;

        if(item != NULL) {
            RECT s;
            mditab_calc_item_rect(mditab, mditab->item_selected, &s);
            mc_rect_set(&r, rect.left - 5, rect.bottom - 4, s.left, rect.bottom + 1);
            DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_TOP);
            mc_rect_set(&r, s.right, rect.bottom - 4, rect.right + 5, rect.bottom + 1);
            DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_TOP);
        } else {
            mc_rect_set(&r, rect.left - 5, rect.bottom - 4, rect.right + 5, rect.bottom + 1);
            DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_TOP);
        }
    }

    mc_clip_set(dc, mditab->main_rect.left - SELECTED_EXTRA_LEFT, 0,
                    mditab->main_rect.right + SELECTED_EXTRA_RIGHT, rect.bottom);

    /* Draw the selected tab */
    if(item != NULL) {
        RECT r;
        mditab_calc_item_rect(mditab, mditab->item_selected, &r);
        if(r.right >= dirty->left  &&  r.left < dirty->right)
            mditab_paint_item(mditab, dc, mditab->item_selected, &r);
    }

    mc_clip_reset(dc, old_clip);

    SelectObject(dc, old_font);
    SetBkMode(dc, old_bk_mode);
    SetTextColor(dc, old_text_color);
}

static void
mditab_notify_sel_change(mditab_t* mditab, int old_index, int new_index)
{
    MC_NMMTSELCHANGE notify;

    notify.hdr.hwndFrom = mditab->win;
    notify.hdr.idFrom = GetDlgCtrlID(mditab->win);
    notify.hdr.code = MC_MTN_SELCHANGE;
    notify.iItemOld = old_index;
    notify.lParamOld = (old_index >= 0  ?  mditab_item(mditab, old_index)->lp  :  0);
    notify.iItemNew = new_index;
    notify.lParamNew = (new_index >= 0  ?  mditab_item(mditab, new_index)->lp  :  0);

    MC_MSG(mditab->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify);
}

static int
mditab_insert_item(mditab_t* mditab, int index, MC_MTITEM* id, BOOL unicode)
{
    TCHAR* item_text;
    mditab_item_t* item;

    MDITAB_TRACE("mditab_insert_item(%p, %d, %p, %d)", mditab, index, id, unicode);

    if(MC_ERR(id == NULL)) {
        MC_TRACE("mditab_insert_item: id == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    if(MC_ERR(index < 0)) {
        MC_TRACE("mditab_insert_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    if(index > mditab_count(mditab))
        index = mditab_count(mditab);

    /* Preallocate the string now, so we can stop early if there is not
     * enough memory. */
    if((id->dwMask & MC_MTIF_TEXT) && id->pszText != NULL) {
        item_text = (TCHAR*) mc_str(id->pszText,
                (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(item_text == NULL)) {
            MC_TRACE("mditab_insert_item: mc_str() failed.");
            mc_send_notify(mditab->notify_win, mditab->win, NM_OUTOFMEMORY);
            return -1;
        }
    } else {
        item_text = NULL;
    }

    /* Allocate a cell in item DSA */
    item = (mditab_item_t*) dsa_insert_raw(&mditab->items, index);
    if(MC_ERR(item == NULL)) {
        MC_TRACE("mditab_insert_item: dsa_insert_raw() failed.");
        if(item_text != NULL)
            free(item_text);
        mc_send_notify(mditab->notify_win, mditab->win, NM_OUTOFMEMORY);
        return -1;
    }

    /* Setup the new item */
    item->text = item_text;
    item->img = ((id->dwMask & MC_MTIF_IMAGE) ? id->iImage : (SHORT)MC_I_IMAGENONE);
    item->lp = ((id->dwMask & MC_MTIF_PARAM) ? id->lParam : 0);
    item->left = (index > 0  ?  mditab_item(mditab, index-1)->right  :  0);
    item->right = item->left;
    item->desired_width = mditab_calc_desired_width(mditab, index);

    /* Update stored item indexes */
    if(index <= mditab->item_selected)
        mditab->item_selected++;
    if(mditab->item_selected < 0) {
        mditab->item_selected = index;
        mditab_notify_sel_change(mditab, -1, index);
    }
    if(index <= mditab->item_mclose)
        mditab->item_mclose++;
    /* We don't update ->item_hot. This is determined by mouse and set
     * in mditab_layout() below anyway... */

    /* Refresh */
    mditab_setup_all_desired_widths(mditab);
    mditab_layout(mditab, TRUE);
    return index;
}

static BOOL
mditab_set_item(mditab_t* mditab, int index, MC_MTITEM* id, BOOL unicode)
{
    mditab_item_t* item;
    int desired_width;

    MDITAB_TRACE("mditab_set_item(%p, %d, %p, %d)", mditab, index, id, unicode);

    if(MC_ERR(id == NULL)) {
        MC_TRACE("mditab_set_item: id == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if(MC_ERR(index < 0  ||  index >= mditab_count(mditab))) {
        MC_TRACE("mditab_set_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    item = mditab_item(mditab, index);

    if(id->dwMask & MC_MTIF_TEXT) {
        TCHAR* item_text;

        item_text = (TCHAR*) mc_str(id->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(item_text == NULL && id->pszText != NULL)) {
            MC_TRACE("mditab_set_item: mc_str() failed.");
            mc_send_notify(mditab->notify_win, mditab->win, NM_OUTOFMEMORY);
            return FALSE;
        }

        if(item->text != NULL)
            free(item->text);
        item->text = item_text;
    }
    if(id->dwMask & MC_MTIF_IMAGE)
        item->img = (SHORT)id->iImage;
    if(id->dwMask & MC_MTIF_PARAM)
        item->lp = id->lParam;

    desired_width = mditab_calc_desired_width(mditab, index);
    if(desired_width != item->desired_width) {
        item->desired_width = desired_width;
        mditab_layout(mditab, TRUE);
    } else {
        mditab_invalidate_item(mditab, index);
    }
    return TRUE;
}

static BOOL
mditab_get_item(mditab_t* mditab, int index, MC_MTITEM* id, BOOL unicode)
{
    mditab_item_t* item;

    MDITAB_TRACE("mditab_get_item(%p, %d, %p, %d)", mditab, index, id, unicode);

    if(MC_ERR(id == NULL)) {
        MC_TRACE("mditab_get_item: id == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if(MC_ERR(index < 0  || index >= mditab_count(mditab))) {
        MC_TRACE("mditab_get_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    item = mditab_item(mditab, index);

    if(id->dwMask & MC_MTIF_TEXT) {
        mc_str_inbuf(item->text, MC_STRT, id->pszText,
                (unicode ? MC_STRW : MC_STRA), id->cchTextMax);
    }
    if(id->dwMask & MC_MTIF_IMAGE)
        id->iImage = item->img;
    if(id->dwMask & MC_MTIF_PARAM)
        id->lParam = item->lp;

    return TRUE;
}

static void
mditab_notify_delete_item(mditab_t* mditab, int index)
{
    MC_NMMTDELETEITEM notify;

    notify.hdr.hwndFrom = mditab->win;
    notify.hdr.idFrom = GetDlgCtrlID(mditab->win);
    notify.hdr.code = MC_MTN_DELETEITEM;
    notify.iItem = index;
    notify.lParam = mditab_item(mditab, index)->lp;

    MC_MSG(mditab->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify);
}

static BOOL
mditab_delete_item(mditab_t* mditab, int index)
{
    MDITAB_TRACE("mditab_delete_item(%p, %d)", mditab, index);

    if(index < 0  ||  index >= mditab_count(mditab)) {
        MC_TRACE("mditab_delete_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* Perhaps we have to set another tab as selected. We need to do this
     * before the deletion takes effect as the application still might want
     * to use it in the notification handler. */
    if(index == mditab->item_selected) {
        int old_item_selected = mditab->item_selected;
        int n = mditab_count(mditab);
        if(mditab->item_selected < n-1)
            mditab->item_selected++;
        else
            mditab->item_selected = n-2;

        mditab_notify_sel_change(mditab, old_item_selected, mditab->item_selected);
    }

    mditab_notify_delete_item(mditab, index);
    mditab_invalidate_item(mditab, index);
    dsa_remove(&mditab->items, index, mditab_item_dtor);

    /* Update stored item indexes */
    if(index < mditab->item_selected)
        mditab->item_selected--;
    mditab->item_hot = -1;

    /* Refresh */
    mditab_setup_all_desired_widths(mditab);
    mditab_layout(mditab, TRUE);
    return TRUE;
}

static void
mditab_notify_delete_all_items(mditab_t* mditab)
{
    UINT i;

    if(mc_send_notify(mditab->notify_win, mditab->win, MC_MTN_DELETEALLITEMS) != 0) {
        /* App. has canceled sending per-item notifications. */
        return;
    }

    for(i = 0; i < mditab_count(mditab); i++)
        mditab_notify_delete_item(mditab, i);
}

static BOOL
mditab_delete_all_items(mditab_t* mditab)
{
    MDITAB_TRACE("mditab_notify_delete_all_items(%p)", mditab);

    if(mditab_count(mditab) == 0)
        return TRUE;

    if(mditab->item_selected >= 0) {
        int old_index = mditab->item_selected;
        mditab->item_selected = -1;
        mditab_notify_sel_change(mditab, old_index, mditab->item_selected);
    }

    mditab_notify_delete_all_items(mditab);
    dsa_clear(&mditab->items, mditab_item_dtor);

    mditab->item_hot = -1;
    mditab->item_mclose = -1;

    mditab_layout(mditab, FALSE);
    if(!mditab->no_redraw)
        InvalidateRect(mditab->win, NULL, TRUE);
    return TRUE;
}

static HIMAGELIST
mditab_set_img_list(mditab_t* mditab, HIMAGELIST img_list)
{
    HIMAGELIST old_img_list;

    MDITAB_TRACE("mditab_set_img_list(%p, %p)", mditab, img_list);

    if(img_list == mditab->img_list)
        return img_list;

    old_img_list = mditab->img_list;
    mditab->img_list = img_list;

    if(!mditab->no_redraw)
        InvalidateRect(mditab->win, NULL, TRUE);
    return old_img_list;
}

static int
mditab_set_cur_sel(mditab_t* mditab, int index)
{
    int old_sel_index;

    MDITAB_TRACE("mditab_set_cur_sel(%p, %d)", mditab, index);

    if(index < 0  ||  index >= mditab_count(mditab)) {
        MC_TRACE("mditab_set_cur_sel: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        index = -1;
    }

    old_sel_index = mditab->item_selected;
    if(index == old_sel_index)
        return old_sel_index;

    mditab->item_selected = index;
    mditab_scroll_to_item(mditab, index);

    /* Redraw the tabs with changed status */
    if(old_sel_index >= 0)
        mditab_invalidate_item(mditab, old_sel_index);
    if(index >= 0)
        mditab_invalidate_item(mditab, index);

    /* Notify parent window */
    mditab_notify_sel_change(mditab, old_sel_index, index);

    return old_sel_index;
}

static void
mditab_list_items(mditab_t* mditab)
{
    HMENU popup;
    MENUINFO mi;
    MENUITEMINFO mii;
    TPMPARAMS tpm_param;
    int i;
    DWORD btn_state;

    MDITAB_TRACE("mditab_list_items(%p)", mditab);

    /* Construct the menu */
    popup = CreatePopupMenu();
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_STRING | MIIM_BITMAP;
    mii.hbmpItem = HBMMENU_CALLBACK;
    for(i = 0; i < mditab_count(mditab); i++) {
        mii.dwItemData = i;
        mii.wID = 1000 + i;
        mii.fState = (i == mditab->item_selected ? MFS_DEFAULT : 0);
        mii.dwTypeData = mditab_item(mditab, i)->text;
        mii.cch = _tcslen(mii.dwTypeData);
        InsertMenuItem(popup, i, TRUE, &mii);
    }

    /* Disable the place for checkmarks in the menu as it is never used
     * and looks ugly. */
    mi.cbSize = sizeof(MENUINFO);
    mi.fMask = MIM_STYLE;
    GetMenuInfo(popup, &mi);
    mi.dwStyle |= MNS_CHECKORBMP;
    SetMenuInfo(popup, &mi);

    /* Show the menu */
    i = MC_MSG(mditab->tb2_win, TB_COMMANDTOINDEX, IDC_LIST_ITEMS, 0);
    tpm_param.cbSize = sizeof(TPMPARAMS);
    MC_MSG(mditab->tb2_win, TB_GETITEMRECT, i, &tpm_param.rcExclude);
    btn_state = MC_MSG(mditab->tb2_win, TB_GETSTATE, IDC_LIST_ITEMS, 0);
    MC_MSG(mditab->tb2_win, TB_SETSTATE, IDC_LIST_ITEMS,
                MAKELONG(btn_state | TBSTATE_PRESSED, 0));
    MapWindowPoints(mditab->tb2_win, HWND_DESKTOP, (POINT*) &tpm_param.rcExclude, 2);
    TrackPopupMenuEx(popup, TPM_LEFTBUTTON | TPM_RIGHTALIGN,
                     tpm_param.rcExclude.right, tpm_param.rcExclude.bottom,
                     mditab->win, &tpm_param);
    DestroyMenu(popup);
    MC_MSG(mditab->tb2_win, TB_SETSTATE, IDC_LIST_ITEMS, MAKELONG(btn_state, 0));
}

static void
mditab_measure_menu_icon(mditab_t* mditab, MEASUREITEMSTRUCT* mis)
{
    mditab_item_t* item = mditab_item(mditab, mis->itemID - 1000);

    if(mditab->img_list != NULL  &&  item->img >= 0) {
        int w, h;

        ImageList_GetIconSize(mditab->img_list, &w, &h);
        mis->itemWidth = w;
        mis->itemHeight = h;
    } else {
        mis->itemWidth = 0;
        mis->itemHeight = 0;
    }
}

static void
mditab_draw_menu_icon(mditab_t* mditab, DRAWITEMSTRUCT* dis)
{
    mditab_item_t* item = mditab_item(mditab, dis->itemID - 1000);

    if(mditab->img_list != NULL  &&  item->img >= 0) {
        ImageList_Draw(mditab->img_list, item->img, dis->hDC,
                       dis->rcItem.left, dis->rcItem.top, ILD_TRANSPARENT);
    }
}

static BOOL
mditab_close_item(mditab_t* mditab, int index)
{
    MC_NMMTCLOSEITEM notify;

    MDITAB_TRACE("mditab_close_item(%p, %d)", mditab, index);

    if(index < 0  ||  index >= mditab_count(mditab)) {
        MC_TRACE("mditab_close_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    notify.hdr.hwndFrom = mditab->win;
    notify.hdr.idFrom = GetDlgCtrlID(mditab->win);
    notify.hdr.code = MC_MTN_CLOSEITEM;
    notify.iItem = index;
    notify.lParam = mditab_item(mditab, index)->lp;

    if(MC_MSG(mditab->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify) == FALSE)
        return mditab_delete_item(mditab, index);
    else
        return FALSE;
}

static BOOL
mditab_set_item_width(mditab_t* mditab, MC_MTITEMWIDTH* tw)
{
    USHORT def_w;
    USHORT min_w;

    MDITAB_TRACE("mditab_set_item_width(%p, %p)", mditab, tw);

    if(tw != NULL) {
        def_w = (USHORT)tw->dwDefWidth;
        min_w = (USHORT)tw->dwMinWidth;
    } else {
        def_w = DEFAULT_ITEM_DEF_WIDTH;
        min_w = DEFAULT_ITEM_MIN_WIDTH;
    }

    if(def_w < min_w)
        def_w = min_w;

    if(def_w != mditab->item_def_width  ||  min_w != mditab->item_min_width) {
        mditab->item_def_width = def_w;
        mditab->item_min_width = min_w;

        if(mditab_setup_all_desired_widths(mditab))
            mditab_layout(mditab, TRUE);
    }
    return TRUE;
}

static BOOL
mditab_get_item_width(mditab_t* mditab, MC_MTITEMWIDTH* tw)
{
    tw->dwDefWidth = mditab->item_def_width;
    tw->dwMinWidth = mditab->item_min_width;
    return TRUE;
}

static BOOL
mditab_get_item_rect(mditab_t* mditab, int index, RECT* rect)
{
    if(MC_ERR(index < 0  ||  index >= mditab_count(mditab))) {
        MC_TRACE("mditab_get_item_rect: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    mditab_calc_item_rect(mditab, index, rect);
    if(rect->left < mditab->main_rect.left)
        rect->left = mditab->main_rect.left;
    if(rect->right > mditab->main_rect.right)
        rect->right = mditab->main_rect.right;

    return TRUE;
}

static BOOL
mditab_ensure_visible(mditab_t* mditab, int index)
{
    if(MC_ERR(index < 0  ||  index >= mditab_count(mditab))) {
        MC_TRACE("mditab_ensure_visible: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    mditab_scroll_to_item(mditab, index);
    return TRUE;
}

static BOOL
mditab_command(mditab_t* mditab, WORD code, WORD ctrl_id, HWND ctrl)
{
    MDITAB_TRACE("mditab_command(%p, %d, %d, %p)", mditab, code, ctrl_id, ctrl);

    switch(ctrl_id) {
        case IDC_SCROLL_LEFT:
            mditab_scroll_rel(mditab, -80, TRUE);
            break;

        case IDC_SCROLL_RIGHT:
            mditab_scroll_rel(mditab, +80, TRUE);
            break;

        case IDC_CLOSE_ITEM:
            mditab_close_item(mditab, mditab->item_selected);
            break;

        default:
            if(code == 0  &&  ctrl_id >= 1000) {
                /* user clicked in tab list popup menu */
                mditab_set_cur_sel(mditab, ctrl_id - 1000);
                break;
            }
            return FALSE;
    }

    return TRUE;
}

static LRESULT
mditab_notify(mditab_t* mditab, NMHDR* hdr)
{
    if(hdr->idFrom == IDC_TBAR_1  ||  hdr->idFrom == IDC_TBAR_2) {
        if(hdr->code == TBN_DROPDOWN) {
            NMTOOLBAR* nm = (NMTOOLBAR*) hdr;
            if(nm->iItem == IDC_LIST_ITEMS)
                mditab_list_items(mditab);
            return TBDDRET_DEFAULT;
        }
    }

    return 0;
}

static BOOL
mditab_key_up(mditab_t* mditab, int key_code, DWORD key_data)
{
    int index;

    MDITAB_TRACE("mditab_key_up(%p, %d, 0x%x)", mditab, key_code, key_data);

    switch(key_code) {
        case VK_LEFT:
            index = mditab->item_selected - 1;
            break;
        case VK_RIGHT:
            index = mditab->item_selected + 1;
            break;
        default:
            return FALSE;
    }

    if(0 <= index  &&  index < mditab_count(mditab))
        mditab_set_cur_sel(mditab, index);

    return TRUE;
}

static void
mditab_left_button_down(mditab_t* mditab, UINT keys, short x, short y)
{
    int index;
    MC_MTHITTESTINFO hti;

    if((mditab->style & MC_MTS_FOCUSMASK) == MC_MTS_FOCUSONBUTTONDOWN)
        SetFocus(mditab->win);

    hti.pt.x = x;
    hti.pt.y = y;
    index = mditab_hit_test(mditab, &hti);
    if(index < 0)
        return;

    if(index == mditab->item_selected) {
        if((mditab->style & MC_MTS_FOCUSMASK) != MC_MTS_FOCUSNEVER)
            SetFocus(mditab->win);
    } else {
        mditab_set_cur_sel(mditab, index);
    }
}

static void
mditab_middle_button_down(mditab_t* mditab, UINT keys, short x, short y)
{
    MC_MTHITTESTINFO hti;

    if((mditab->style & MC_MTS_CLOSEONMCLICK) == 0)
        return;

    hti.pt.x = x;
    hti.pt.y = y;
    mditab->item_mclose = mditab_hit_test(mditab, &hti);
    SetCapture(mditab->win);
}

static void
mditab_middle_button_up(mditab_t* mditab, UINT keys, short x, short y)
{
    int index;
    MC_MTHITTESTINFO hti;

    if(GetCapture() == mditab->win) {
        ReleaseCapture();
        mc_send_notify(mditab->notify_win, mditab->win, NM_RELEASEDCAPTURE);
    }

    if((mditab->style & MC_MTS_CLOSEONMCLICK) == 0  ||  mditab->item_mclose < 0)
        return;

    hti.pt.x = x;
    hti.pt.y = y;
    index = mditab_hit_test(mditab, &hti);

    if(index == mditab->item_mclose)
        mditab_close_item(mditab, index);
    mditab->item_mclose = -1;
}

static void
mditab_change_focus(mditab_t* mditab)
{
    /* The selected tab needs refresh to draw/hide focus rect. */
    if(mditab->item_selected < 0)
        return;
    mditab_invalidate_item(mditab, mditab->item_selected);
}

static void
mditab_style_changed(mditab_t* mditab, STYLESTRUCT* ss)
{
    mditab->style = ss->styleNew;

    if(mditab->is_animating  &&  !(mditab->style & MC_MTS_ANIMATE))
        mditab_animate_abort(mditab);

    mditab_layout(mditab, FALSE);
    if(!mditab->no_redraw)
        InvalidateRect(mditab->win, NULL, TRUE);
}

static void
mditab_theme_changed(mditab_t* mditab)
{
    POINT pos;

    if(mditab->theme)
        mcCloseThemeData(mditab->theme);
    mditab->theme = mcOpenThemeData(mditab->win, mditab_tc);
    if(!mditab->no_redraw)
        InvalidateRect(mditab->win, NULL, TRUE);

    /* As an optimization, we do not track hot item when not themed, so
     * it might be in inconsistant state now. Refresh the state. */
    GetCursorPos(&pos);
    ScreenToClient(mditab->win, &pos);
    mditab_track_hot(mditab, pos.x, pos.y);
}

static mditab_t*
mditab_nccreate(HWND win, CREATESTRUCT* cs)
{
    mditab_t* mditab;

    mditab = (mditab_t*) malloc(sizeof(mditab_t));
    if(MC_ERR(mditab == NULL)) {
        MC_TRACE("mditab_nccreate malloc() failed.");
        return NULL;
    }

    memset(mditab, 0, sizeof(mditab_t));

    mditab->win = win;
    mditab->notify_win = cs->hwndParent;
    dsa_init(&mditab->items, sizeof(mditab_item_t));
    mditab->item_selected = -1;
    mditab->item_hot = -1;
    mditab->item_mclose = -1;
    mditab->item_min_width = DEFAULT_ITEM_MIN_WIDTH;
    mditab->item_def_width = DEFAULT_ITEM_DEF_WIDTH;
    mditab->style = cs->style;

    mcBufferedPaintInit();

    return mditab;
}

static int
mditab_create(mditab_t* mditab)
{
    mditab->tb1_win = CreateWindow(toolbar_wc, NULL,
                WS_CHILD | TBSTYLE_FLAT | CCS_NORESIZE | CCS_NODIVIDER,
                0, 0, 0, 0, mditab->win, (HMENU) IDC_TBAR_1, NULL, NULL);
    mditab->tb2_win = CreateWindow(toolbar_wc,
                NULL, WS_CHILD | TBSTYLE_FLAT | CCS_NORESIZE | CCS_NODIVIDER,
                0, 0, 0, 0, mditab->win, (HMENU) IDC_TBAR_2, NULL, NULL);
    if(MC_ERR(mditab->tb1_win == NULL  ||  mditab->tb2_win == NULL)) {
        MC_TRACE("mditab_create: CreateWindow() failed.");
        return -1;
    }

    MC_MSG(mditab->tb1_win, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    MC_MSG(mditab->tb1_win, TB_SETIMAGELIST, 0, mc_bmp_glyphs);
    MC_MSG(mditab->tb1_win, TB_SETBUTTONSIZE, 0,
           MAKELPARAM(TOOLBAR_BTN_WIDTH, TOOLBAR_BTN_WIDTH));
    MC_MSG(mditab->tb2_win, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    MC_MSG(mditab->tb2_win, TB_SETIMAGELIST, 0, mc_bmp_glyphs);
    MC_MSG(mditab->tb2_win, TB_SETBUTTONSIZE, 0,
           MAKELPARAM(TOOLBAR_BTN_WIDTH, TOOLBAR_BTN_WIDTH));

    mditab->theme = mcOpenThemeData(mditab->win, mditab_tc);
    mditab_layout(mditab, FALSE);
    return 0;
}

static void
mditab_destroy(mditab_t* mditab)
{
    mditab_notify_delete_all_items(mditab);

    if(mditab->hot_tracking) {
        KillTimer(mditab->win, HOT_TRACK_TIMER_ID);
        mditab->hot_tracking = 0;
    }

    if(mditab->theme) {
        mcCloseThemeData(mditab->theme);
        mditab->theme = NULL;
    }

    dsa_fini(&mditab->items, mditab_item_dtor);
}

static void
mditab_ncdestroy(mditab_t* mditab)
{
    mcBufferedPaintUnInit();
    free(mditab);
}

static LRESULT CALLBACK
mditab_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(win, &ps);
            if(!mditab->no_redraw) {
                if(mditab->style & MC_MTS_DOUBLEBUFFER)
                    mc_doublebuffer(mditab, &ps, mditab_paint);
                else
                    mditab_paint(mditab, ps.hdc, &ps.rcPaint, ps.fErase);
            }
            EndPaint(win, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            RECT rect;
            GetClientRect(win, &rect);
            mditab_paint(mditab, (HDC) wp, &rect, TRUE);
            return 0;
        }

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

        case MC_MTM_GETITEMCOUNT:
            return mditab_count(mditab);

        case MC_MTM_INSERTITEMW:
        case MC_MTM_INSERTITEMA:
            return mditab_insert_item(mditab, (int)wp, (MC_MTITEM*)lp, (msg == MC_MTM_INSERTITEMW));

        case MC_MTM_SETITEMW:
        case MC_MTM_SETITEMA:
            return mditab_set_item(mditab, (int)wp, (MC_MTITEM*)lp, (msg == MC_MTM_SETITEMW));

        case MC_MTM_GETITEMW:
        case MC_MTM_GETITEMA:
            return mditab_get_item(mditab, (int)wp, (MC_MTITEM*)lp, (msg == MC_MTM_GETITEMW));

        case MC_MTM_DELETEITEM:
            return mditab_delete_item(mditab, wp);

        case MC_MTM_CLOSEITEM:
            return mditab_close_item(mditab, wp);

        case MC_MTM_HITTEST:
            return mditab_hit_test(mditab, (MC_MTHITTESTINFO*)lp);

        case MC_MTM_SETCURSEL:
            return mditab_set_cur_sel(mditab, wp);

        case MC_MTM_GETCURSEL:
            return mditab->item_selected;

        case MC_MTM_DELETEALLITEMS:
            return mditab_delete_all_items(mditab);

        case MC_MTM_SETIMAGELIST:
            return (LRESULT) mditab_set_img_list(mditab, (HIMAGELIST) lp);

        case MC_MTM_GETIMAGELIST:
            return (LRESULT) mditab->img_list;

        case MC_MTM_SETITEMWIDTH:
            return (LRESULT) mditab_set_item_width(mditab, (MC_MTITEMWIDTH*)lp);

        case MC_MTM_GETITEMWIDTH:
            return (LRESULT) mditab_get_item_width(mditab, (MC_MTITEMWIDTH*)lp);

        case MC_MTM_INITSTORAGE:
            return (dsa_reserve(&mditab->items, (UINT)wp) == 0 ? TRUE : FALSE);

        case MC_MTM_GETITEMRECT:
            return mditab_get_item_rect(mditab, wp, (RECT*) lp);

        case MC_MTM_ENSUREVISIBLE:
            return mditab_ensure_visible(mditab, wp);

        case WM_LBUTTONDOWN:
            mditab_left_button_down(mditab, wp, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_MBUTTONDOWN:
            mditab_middle_button_down(mditab, wp, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_MBUTTONUP:
            mditab_middle_button_up(mditab, wp, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_LBUTTONUP:
            mc_send_notify(mditab->notify_win, win, NM_CLICK);
            return 0;

        case WM_RBUTTONUP:
            mc_send_notify(mditab->notify_win, win, NM_RCLICK);
            return 0;

        case WM_MOUSEMOVE:
            mditab_track_hot(mditab, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_COMMAND:
            if(mditab_command(mditab, HIWORD(wp), LOWORD(wp), (HWND)lp))
                return 0;
            break;

        case WM_NOTIFY:
            return mditab_notify(mditab, (NMHDR*) lp);

        case WM_SIZE:
            mditab_layout(mditab, FALSE);
            if(!mditab->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_KEYUP:
            if(mditab_key_up(mditab, (int)wp, (DWORD)lp))
                return 0;
            break;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            mditab_change_focus(mditab);
            break;

        case WM_GETFONT:
            return (LRESULT) mditab->font;

        case WM_SETFONT:
            mditab->font = (HFONT) wp;
            if(mditab->item_def_width == 0)
                mditab_setup_all_desired_widths(mditab);
            if((BOOL) lp  &&  !mditab->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_MEASUREITEM:
            mditab_measure_menu_icon(mditab, (MEASUREITEMSTRUCT*) lp);
            return TRUE;

        case WM_DRAWITEM:
            if(wp == 0)
                mditab_draw_menu_icon(mditab, (DRAWITEMSTRUCT*) lp);
            return TRUE;

        case WM_SETREDRAW:
            mditab->no_redraw = !wp;
            return 0;

        case WM_GETDLGCODE:
            /* FIXME: Wine-1.1.2/dlls/comctl32/tab.c returns (DLGC_WANTARROWS
             *        | DLGC_WANTCHARS). Why? Shouldn't we follow it? */
            return DLGC_WANTARROWS;

        case WM_STYLECHANGED:
            mditab_style_changed(mditab, (STYLESTRUCT*) lp);
            return 0;

        case WM_THEMECHANGED:
            mditab_theme_changed(mditab);
            return 0;

        case WM_UPDATEUISTATE:
            switch(LOWORD(wp)) {
                case UIS_CLEAR:       mditab->ui_state &= ~HIWORD(wp); break;
                case UIS_SET:         mditab->ui_state |= HIWORD(wp); break;
                case UIS_INITIALIZE:  mditab->ui_state = HIWORD(wp); break;
            }
            if(!mditab->no_redraw)
                InvalidateRect(win, NULL, FALSE);
            break;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = mditab->notify_win;
            mditab->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case CCM_SETWINDOWTHEME:
            mcSetWindowTheme(win, (const WCHAR*) lp, NULL);
            return 0;

        case WM_NCCREATE:
            mditab = mditab_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(mditab == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)mditab);
            return TRUE;

        case WM_CREATE:
            return (mditab_create(mditab) == 0 ? 0 : -1);

        case WM_DESTROY:
            mditab_destroy(mditab);
            return 0;

        case WM_NCDESTROY:
            if(mditab)
                mditab_ncdestroy(mditab);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}


int
mditab_init_module(void)
{
    WNDCLASS wc = { 0 };

    mc_init_common_controls(ICC_BAR_CLASSES);

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = mditab_proc;
    wc.cbWndExtra = sizeof(mditab_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = mditab_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("mditab_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
mditab_fini_module(void)
{
    UnregisterClass(mditab_wc, NULL);
}

