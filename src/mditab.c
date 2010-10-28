/*
 * Copyright (c) 2008-2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mditab.h"
#include "theme.h"


/* TODO:
 *
 * Features:
 * - Clean-up geometry calculations (split it from the painting code), so
 *   MC_MTM_GETITEMRECT and MC_MTM_HITTEST can be implemented easily.
 * - Animation when tab removed, when scrolling tabs to left/right,
 *   when resizing.
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
 * - MC_MTM_GETITEMRECT (see TCM_GETITEMRECT)
 * - MC_MTM_SETTOOLTIPS/MC_MTM_GETTOOLTIPS (see TCM_SETTOOLTIPS/TCM_GETTOOLTIPS)
 * - MC_MTM_HITTEST - add support for MC_MTHT_ONITEMCLOSEBUTTON
 *
 * Styles:
 * - MC_MTS_CBONEACHTAB - close button on each tab
 * - MC_MTS_CBONACTIVETAB - close button on the active tab
 */


static const TCHAR mditab_wc[] = MC_WC_MDITAB;  /* window class name */
static const WCHAR mditab_tc[] = L"TAB";        /* theme class name */

static const TCHAR toolbar_wc[] = TOOLBARCLASSNAME;


/* Geometry constants */
#define DEFAULT_ITEM_MIN_WIDTH       60
#define DEFAULT_ITEM_DEF_WIDTH      120

#define ITEM_PADDING_H                4
#define ITEM_PADDING_V                2
#define ITEM_CORNER_SIZE              3

#define BTN_WIDTH                    21
#define BTN_MARGIN_H                  2
#define BTN_MARGIN_V                  3

/* Timer for hot tab tracking */
#define HOT_TRACK_TIMER_ID            1
#define HOT_TRACK_TIMER_INTERVAL    100

/* Child control IDs */
#define IDC_TBAR_1                  100  /* left toolbar */
#define IDC_SCROLL_LEFT             101
#define IDC_TBAR_2                  200  /* right toolbar */
#define IDC_SCROLL_RIGHT            201
#define IDC_LIST_ITEMS              202
#define IDC_CLOSE_ITEM              203


/* Per-tab structure */
typedef struct mditab_item_tag mditab_item_t;
struct mditab_item_tag {
    TCHAR* text;
    int img;
    RECT rect;
    LPARAM lp;
};

/* Per-control structure */
typedef struct mditab_tag mditab_t;
struct mditab_tag {
    HWND win;
    HWND toolbar1;          /* leftside toolbar */
    HWND toolbar2;          /* tightside toolbar */
    RECT rect_main;         /* main area where we draw the tabs and toolbars */
    HTHEME theme;           /* theming handle */
    DWORD ui_state;
    HIMAGELIST img_list;    /* image list, optional */
    HFONT font;             /* font */
    mditab_item_t* items;   /* list of items (== tabs) */
    SHORT item_count;       /* count of items */
    SHORT item_selected;    /* selected (== active) item */
    SHORT item_hot;         /* tracked only when themed */
    SHORT item_first_visible;  /* first visible item (helper for scrolling) */
    SHORT item_mclose;      /* candidate item to close by middle button */
    UINT hot_track_timer;   /* helper timer for tracking of hot item */
    USHORT item_min_width;  /* minimal width of each tab */
    USHORT item_def_width;  /* default width of each tab */
    UINT style       : 30;  /* window styles */
    UINT do_redraw   :  1;  /* redraw flag */
    UINT need_scroll :  1;  /* when need scrolling, scrolling buttons appear */
};


static void
mditab_calc_need_scroll(mditab_t* mditab)
{
    RECT rect;
    UINT space;

    if(mditab->item_count <= 1) {
        mditab->need_scroll = FALSE;
        return;
    }

    GetClientRect(mditab->win, &rect);
    space = rect.right - rect.left - 2 * BTN_MARGIN_H;

    if((mditab->style & MC_MTS_CBMASK) == MC_MTS_CBONTOOLBAR)
        space -= BTN_WIDTH + BTN_MARGIN_H;
    if((mditab->style & MC_MTS_TLBMASK) == MC_MTS_TLBALWAYS)
        space -= BTN_WIDTH + BTN_MARGIN_H;
    if(mditab->style & MC_MTS_SCROLLALWAYS)
        space -= 2 * (BTN_WIDTH + BTN_MARGIN_H);

    mditab->need_scroll = (mditab->item_count * mditab->item_min_width > space);
}

static void
mditab_calc_layout(mditab_t* mditab)
{
    TBBUTTON btn = {0};
    BOOL need_btn_scroll;
    BOOL need_btn_list_items;
    BOOL need_btn_close_item;
    int btn_count1 = 0, btn_count2 = 0;  /* button counts on toolbars */
    RECT rect;

    GetClientRect(mditab->win, &rect);

    /* Clear toolbars */
    while(SendMessage(mditab->toolbar1, TB_DELETEBUTTON, 0, 0) == TRUE);
    while(SendMessage(mditab->toolbar2, TB_DELETEBUTTON, 0, 0) == TRUE);

    /* Add buttons */
    need_btn_scroll = (mditab->need_scroll  ||  (mditab->style &  MC_MTS_SCROLLALWAYS));
    need_btn_list_items = ((mditab->style & MC_MTS_TLBMASK) == MC_MTS_TLBALWAYS  ||
                          ((mditab->style & MC_MTS_TLBMASK) == MC_MTS_TLBONSCROLL && mditab->need_scroll));
    need_btn_close_item = ((mditab->style & MC_MTS_CBMASK) == MC_MTS_CBONTOOLBAR);
    btn.fsStyle = TBSTYLE_BUTTON;
    if(need_btn_scroll) {
        /* Scroll left button */
        btn.iBitmap = MC_BMP_GLYPH_CHEVRON_L;
        btn.idCommand = IDC_SCROLL_LEFT;
        SendMessage(mditab->toolbar1, TB_ADDBUTTONS, 1, (LPARAM) &btn);
        btn_count1++;

        /* Scroll right button */
        btn.iBitmap = MC_BMP_GLYPH_CHEVRON_R;
        btn.idCommand = IDC_SCROLL_RIGHT;
        SendMessage(mditab->toolbar2, TB_ADDBUTTONS, 1, (LPARAM) &btn);
        btn_count2++;
    }
    if(need_btn_list_items) {
        /* Button for popup tab list */
        btn.iBitmap = MC_BMP_GLYPH_MORE_OPTIONS;
        btn.idCommand = IDC_LIST_ITEMS;
        SendMessage(mditab->toolbar2, TB_ADDBUTTONS, 1, (LPARAM) &btn);
        btn_count2++;
    }
    if(need_btn_close_item) {
        /* Close tab button */
        btn.iBitmap = MC_BMP_GLYPH_CLOSE;
        btn.idCommand = IDC_CLOSE_ITEM;
        SendMessage(mditab->toolbar2, TB_ADDBUTTONS, 1, (LPARAM) &btn);
        btn_count2++;
    }

    /* Determine layout */
    mditab->rect_main.top = BTN_MARGIN_V;
    mditab->rect_main.bottom = rect.bottom;
    if(btn_count1 > 0) {
        RECT r;
        UINT w, h;

        h = rect.bottom - rect.top - 6 - BTN_MARGIN_V - 2;
        SendMessage(mditab->toolbar1, TB_SETBUTTONSIZE, 0, MAKELONG(h, h));
        SendMessage(mditab->toolbar1, TB_GETITEMRECT, 0, (LPARAM) &r);
        w = r.right - r.left;
        SetWindowPos(mditab->toolbar1, NULL, BTN_MARGIN_H, BTN_MARGIN_H + 2,
                w, h, SWP_SHOWWINDOW | SWP_NOZORDER);
        mditab->rect_main.left = BTN_MARGIN_H + w + BTN_MARGIN_H + 2;
    } else {
        ShowWindow(mditab->toolbar1, SW_HIDE);
        mditab->rect_main.left = rect.left + BTN_MARGIN_H + 2;
    }
    if(btn_count2 > 0) {
        RECT r, s;
        UINT w, h;

        h = rect.bottom - rect.top - 6 - BTN_MARGIN_V - 2;
        SendMessage(mditab->toolbar2, TB_SETBUTTONSIZE, 0, MAKELONG(h, h));
        SendMessage(mditab->toolbar2, TB_GETITEMRECT, 0, (LPARAM) &r);
        SendMessage(mditab->toolbar2, TB_GETITEMRECT, btn_count2-1, (LPARAM) &s);
        w = s.right - r.left;
        SetWindowPos(mditab->toolbar2, NULL, rect.right - BTN_MARGIN_H - w,
                BTN_MARGIN_H + 2, w, h, SWP_SHOWWINDOW | SWP_NOZORDER);
        mditab->rect_main.right = rect.right - BTN_MARGIN_H - w - BTN_MARGIN_H - 2;
    } else {
        ShowWindow(mditab->toolbar2, SW_HIDE);
        mditab->rect_main.right = rect.right - BTN_MARGIN_H - 2;
    }
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
    RECT* rect_main;
    RECT* rect_item;

    if(index < mditab->item_first_visible)
        return FALSE;

    rect_main = &mditab->rect_main;
    rect_item = &mditab->items[index].rect;

    if(rect_item->right < rect_main->left)
        return FALSE;
    if(rect_item->left > rect_main->right)
        return FALSE;
    return TRUE;
}

static int
mditab_hit_test(mditab_t* mditab, mc_MT_HITTESTINFO* hti)
{
    int x, y;
    int i;
    RECT* r;

    x = hti->pt.x;
    y = hti->pt.y;

    for(i = mditab->item_first_visible; i < mditab->item_count; i++) {
        if(!mditab_is_item_visible(mditab, i))
            break;

        r = &mditab->items[i].rect;

        if(r->left <= x && x <= r->right && r->top <= y && y <= r->bottom) {
            if(mditab->img_list != NULL) {
                RECT contents;
                RECT ico_rect;
                int ico_w, ico_h;

                ImageList_GetIconSize(mditab->img_list, &ico_w, &ico_h);
                mditab_calc_contents_rect(&contents, r);
                mditab_calc_ico_rect(&ico_rect, &contents, ico_w, ico_h);

                if(ico_rect.left <= x && x <= ico_rect.right &&
                   ico_rect.top <= y && y <= ico_rect.bottom)
                {
                    hti->flags = MC_MTHT_ONITEMICON;
                    return i;
                }
            }

            /* TODO: check for MC_MTHT_ONITEMCLOSEBUTTON */

            hti->flags = MC_MTHT_ONITEMLABEL;
            return i;
        }
    }
    hti->flags = MC_MTHT_NOWHERE;
    return -1;
}

static void
mditab_invalidate_item(mditab_t* mditab, int index)
{
    RECT r;

    if(!mditab_is_item_visible(mditab, index))
        return;

    /* We invalidate slightly more around the tab rect, to prevent artifacts
     * when changing status of the tab to/from active one. */
    CopyRect(&r, &mditab->items[index].rect);
    r.left -= 2;
    r.top -= 2;
    r.right += 1;
    r.bottom += 1;
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
        KillTimer(win, mditab->hot_track_timer);
        mditab->hot_track_timer = 0;
    }
}

static void
mditab_track_hot(mditab_t* mditab, int x, int y)
{
    int index;

    if(mditab->theme) {
        mc_MT_HITTESTINFO hti;

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
            mditab->hot_track_timer = SetTimer(mditab->win, HOT_TRACK_TIMER_ID,
                    HOT_TRACK_TIMER_INTERVAL, mditab_track_hot_timer_proc);
        } else {
            /* No tab is hot so the timer is not needed. */
            if(mditab->hot_track_timer) {
                KillTimer(mditab->win, mditab->hot_track_timer);
                mditab->hot_track_timer = 0;
            }
        }
    }
}

static void
mditab_layout(mditab_t* mditab)
{
    UINT space, offset;
    UINT i;
    POINT pos;

    /* We need geometry of the main area */
    mditab_calc_need_scroll(mditab);
    mditab_calc_layout(mditab);

    SendMessage(mditab->toolbar2, TB_ENABLEBUTTON,
            IDC_LIST_ITEMS, MAKELONG((mditab->item_count > 0), 0));
    SendMessage(mditab->toolbar2, TB_ENABLEBUTTON,
            IDC_CLOSE_ITEM, MAKELONG((mditab->item_selected >= 0), 0));

    if(MC_ERR(mditab->items == NULL))
        return;

    offset = mditab->rect_main.left;
    space = mditab->rect_main.right - mditab->rect_main.left;

    /* Now we can finally compute the rects */
    if(!mditab->need_scroll) {
        mditab->item_first_visible = 0;
        if(space <= mditab->item_count * mditab->item_def_width) {
            for(i = 0; i < mditab->item_count; i++) {
                mditab->items[i].rect.left = offset + (i * space) / mditab->item_count;
                mditab->items[i].rect.right = offset + ((i+1) * space) / mditab->item_count;
            }
        } else {
            for(i = 0; i < mditab->item_count; i++) {
                mditab->items[i].rect.left = offset + i * mditab->item_def_width;
                mditab->items[i].rect.right = offset + (i+1) * mditab->item_def_width;
            }
        }
    } else {
        UINT visible_items = space / mditab->item_min_width;

        /* We might want to change mditab->items_first_visible if that would
         * allow to show more tabs. (E.g. after windows resize or if some
         * tabs are removed): */
        if(visible_items == 0)
            visible_items = 1;
        if(mditab->item_count - mditab->item_first_visible < visible_items)
            mditab->item_first_visible = mditab->item_count - visible_items;

        /* Divide the available space among the visible tabs */
        for(i = 0; i < visible_items; i++) {
            mditab->items[mditab->item_first_visible + i].rect.left = offset + (i * space) / visible_items;
            mditab->items[mditab->item_first_visible + i].rect.right = offset + ((i+1) * space) / visible_items;
        }

        /* Set rects of invisible tabs */
        for(i = 0; i < mditab->item_first_visible; i++) {
            mditab->items[i].rect.left = -1;
            mditab->items[i].rect.right = -1;
        }
        for(i = mditab->item_first_visible + visible_items;
                                    i < mditab->item_count; i++) {
            mditab->items[i].rect.left = -1;
            mditab->items[i].rect.right = -1;
        }
    }

    /* Set top and bottom coordinates of each tab rect */
    for(i = 0; i < mditab->item_count; i++) {
        mditab->items[i].rect.top = mditab->rect_main.top + 2;
        mditab->items[i].rect.bottom = mditab->rect_main.bottom - 5;
    }
    mditab->items[mditab->item_selected].rect.left -= 2;
    mditab->items[mditab->item_selected].rect.top -= 2;
    mditab->items[mditab->item_selected].rect.right += 1;
    mditab->items[mditab->item_selected].rect.bottom += 1;

    /* Setup hot item, because change of the layout (e.g. window size or new
     * tab) might change that. */
    GetCursorPos(&pos);
    ScreenToClient(mditab->win, &pos);
    mditab_track_hot(mditab, pos.x, pos.y);

    /* Update toolbar buttons state */
    SendMessage(mditab->toolbar1, TB_ENABLEBUTTON, IDC_SCROLL_LEFT,
            MAKELONG((!mditab_is_item_visible(mditab, 0)), 0));
    SendMessage(mditab->toolbar2, TB_ENABLEBUTTON, IDC_SCROLL_RIGHT,
            MAKELONG((!mditab_is_item_visible(mditab, mditab->item_count - 1)), 0));
}

static void
mditab_paint_item(mditab_t* mditab, HDC dc, UINT index)
{
    int state = 0;
    RECT* rect;
    RECT contents;
    UINT flags;

    if(!mditab->do_redraw)
        return;

    rect = &mditab->items[index].rect;

    /* Draw tab background */
    if(mditab->theme) {
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

        theme_DrawThemeBackground(mditab->theme, dc, TABP_TOPTABITEM, state, rect, rect);
    } else {
        RECT r;

        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
        DrawEdge(dc, rect, EDGE_RAISED, BF_SOFT | BF_LEFT | BF_TOP | BF_RIGHT);

        /* Make left corner rounded */
        SetRect(&r, rect->left, rect->top, rect->left + ITEM_CORNER_SIZE, rect->top + ITEM_CORNER_SIZE);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, 0);
        DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_DIAGONAL_ENDTOPRIGHT);

        /* Make right corner rounded */
        SetRect(&r, rect->right - ITEM_CORNER_SIZE + 1, rect->top, rect->right, rect->top + ITEM_CORNER_SIZE);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, 0);
        r.top++;
        DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_DIAGONAL_ENDBOTTOMRIGHT);
    }

    /* If needed, draw focus rect */
    if(mditab->win == GetFocus() && index == mditab->item_selected) {
        if((mditab->style & MC_MTS_FOCUSMASK) != MC_MTS_FOCUSNEVER) {
            if(!(mditab->ui_state & UISF_HIDEFOCUS)) {
                SetRect(&contents, rect->left + 3, rect->top + 3, 
                                   rect->right - 3, rect->bottom - 2);
                DrawFocusRect(dc, &contents);
            }
        }
    }

    mditab_calc_contents_rect(&contents, rect);

    /* Draw tab icon */
    if(mditab->img_list != NULL) {
        int ico_w, ico_h;
        RECT rect_ico;

        /* Compute where to draw the icon */
        ImageList_GetIconSize(mditab->img_list, &ico_w, &ico_h);
        mditab_calc_ico_rect(&rect_ico, &contents, ico_w, ico_h);

        /* Do the draw */
        if(mditab->theme  &&  mditab->items[index].img >= 0) {
            theme_DrawThemeIcon(mditab->theme, dc, TABP_TOPTABITEM, TTIS_NORMAL,
                           &rect_ico, mditab->img_list, mditab->items[index].img);
        } else {
            ImageList_Draw(mditab->img_list, mditab->items[index].img, dc,
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
    if(mditab->theme) {
        contents.top += 2;
        theme_DrawThemeText(mditab->theme, dc, TABP_TOPTABITEM, state,
                    mditab->items[index].text, -1, flags, 0, &contents);
    } else {
        if(index == mditab->item_selected)
            contents.bottom -= 2;
        DrawText(dc, mditab->items[index].text, -1, &contents, flags | DT_SINGLELINE | DT_VCENTER);
    }
}

static void
mditab_paint(mditab_t* mditab, HDC dc)
{
    RECT rect;
    RECT r;
    UINT i;
    HFONT old_font;
    int old_bk_mode;
    COLORREF old_text_color;

    old_font = SelectObject(dc, mditab->font);
    old_bk_mode = GetBkMode(dc);
    old_text_color = GetTextColor(dc);

    GetClientRect(mditab->win, &rect);

    if(mditab->theme)
        theme_DrawThemeParentBackground(mditab->win, dc, &rect);

    /* Draw unselected tabs */
    for(i = mditab->item_first_visible; i < mditab->item_count; i++) {
        if(!mditab_is_item_visible(mditab, i))
            break;
        if(i == mditab->item_selected)
            continue;
        mditab_paint_item(mditab, dc, i);
    }

    /* Draw pane */
    if(mditab->theme) {
        SetRect(&r, rect.left - 5, rect.bottom - 5,
                    rect.right + 5, rect.bottom + 1);
        theme_DrawThemeBackground(mditab->theme, dc, TABP_PANE, 0, &r, &r);
    } else {
        if(mditab->item_selected >= 0) {
            SetRect(&r, rect.left - 5, rect.bottom - 5,
                    mditab->items[mditab->item_selected].rect.left,
                    rect.bottom + 1);
            DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_TOP);

            SetRect(&r, mditab->items[mditab->item_selected].rect.right,
                    rect.bottom - 5, rect.right + 5, rect.bottom + 1);
            DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_TOP);
        } else {
            SetRect(&r, rect.left - 5, rect.bottom - 5,
                        rect.right + 5, rect.bottom + 1);
            DrawEdge(dc, &r, EDGE_RAISED, BF_SOFT | BF_TOP);
        }
    }

    /* Draw the selected tab */
    if(mditab_is_item_visible(mditab, mditab->item_selected))
        mditab_paint_item(mditab, dc, mditab->item_selected);

    SelectObject(dc, old_font);
    SetBkMode(dc, old_bk_mode);
    SetTextColor(dc, old_text_color);
}

static void
mditab_notify_sel_change(mditab_t* mditab, int old_index, int new_index)
{
    mc_NM_MT_SELCHANGE notify;

    notify.hdr.hwndFrom = mditab->win;
    notify.hdr.idFrom = GetDlgCtrlID(mditab->win);
    notify.hdr.code = MC_MTN_SELCHANGE;
    notify.iItemOld = old_index;
    notify.lParamOld = (old_index >= 0  ?  mditab->items[old_index].lp  :  0);
    notify.iItemNew = new_index;
    notify.lParamNew = (new_index >= 0  ?  mditab->items[new_index].lp  :  0);

    SendMessage(GetParent(mditab->win), WM_NOTIFY, 
                (WPARAM)notify.hdr.idFrom, (LPARAM)&notify);
}

static int
mditab_insert_item(mditab_t* mditab, int index, MC_MTITEM* id, BOOL unicode)
{
    mditab_item_t* items;
    TCHAR* item_text = NULL;
    BOOL need_scroll;
    BOOL changed_sel;

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
    if(index > mditab->item_count)
        index = mditab->item_count;

    /* Preallocate the string now, so we can stop early if there is not
     * enough memory. */
    if(id->dwMask & MC_MTIF_TEXT) {
        item_text = (TCHAR*) mc_str(id->pszText,
                (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(item_text == NULL && id->pszText != NULL)) {
            MC_TRACE("mditab_insert_item: mc_str() failed.");
            return -1;
        }
    }

    /* Allocate new items vector */
    items = (mditab_item_t*) malloc((mditab->item_count + 1) *
                                        sizeof(mditab_item_t));
    if(MC_ERR(items == NULL)) {
        MC_TRACE("mditab_insert_item: malloc() failed.");
        if(item_text != NULL)
            free(item_text);
        return -1;
    }

    /* Copy all items before and after the index */
    if(index > 0)
        memcpy(items, mditab->items, index * sizeof(mditab_item_t));
    if(index < mditab->item_count)
        memcpy(items + (index+1), mditab->items + index,
                        (mditab->item_count - index) * sizeof(mditab_item_t));

    /* Setup the new item */
    items[index].text = item_text;
    items[index].img = ((id->dwMask & MC_MTIF_IMAGE) ? id->iImage : -1);
    /*items[index].rect is setup later in mditab_layout() */
    items[index].lp = ((id->dwMask & MC_MTIF_PARAM) ? id->lParam : 0);

    /* If we inserted before remebered tab indexes, update them */
    if(index <= mditab->item_selected)
        mditab->item_selected++;
    changed_sel = FALSE;
    if(mditab->item_selected < 0) {
        mditab->item_selected = index;
        changed_sel = TRUE;
    }
    if(index <= mditab->item_first_visible)
        mditab->item_first_visible++;
    /* We don't do the same for item_hot. This is determined by mouse and set
     * in mditab_layout() below anyway... */

    /* Swap the vectors */
    if(mditab->items)
        free(mditab->items);
    mditab->items = items;
    mditab->item_count++;

    if(changed_sel)
        mditab_notify_sel_change(mditab, -1, index);

    /* Refresh */
    need_scroll = mditab->need_scroll;
    mditab_layout(mditab);
    if(mditab_is_item_visible(mditab, index) || mditab->need_scroll != need_scroll)
        InvalidateRect(mditab->win, NULL, TRUE);

    return index;
}

static BOOL
mditab_set_item(mditab_t* mditab, int index, MC_MTITEM* id, BOOL unicode)
{
    BOOL need_scroll;

    if(MC_ERR(id == NULL)) {
        MC_TRACE("mditab_set_item: id == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if(MC_ERR(index < 0  ||  index >= mditab->item_count)) {
        MC_TRACE("mditab_set_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(id->dwMask & MC_MTIF_TEXT) {
        TCHAR* item_text;

        item_text = (TCHAR*) mc_str(id->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(item_text == NULL && id->pszText != NULL)) {
            MC_TRACE("mditab_set_item: mc_str() failed.");
            return FALSE;
        }

        if(mditab->items[index].text)
            free(mditab->items[index].text);
        mditab->items[index].text = item_text;
    }
    if(id->dwMask & MC_MTIF_IMAGE)
        mditab->items[index].img = id->iImage;
    if(id->dwMask & MC_MTIF_PARAM)
        mditab->items[index].lp = id->lParam;

    /* Refresh */
    need_scroll = mditab->need_scroll;
    mditab_layout(mditab);
    if(mditab_is_item_visible(mditab, index) || mditab->need_scroll != need_scroll)
        InvalidateRect(mditab->win, NULL, TRUE);

    return TRUE;
}

static BOOL
mditab_get_item(mditab_t* mditab, int index, MC_MTITEM* id, BOOL unicode)
{
    if(MC_ERR(id == NULL)) {
        MC_TRACE("mditab_get_item: id == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if(MC_ERR(index < 0  ||  index >= mditab->item_count)) {
        MC_TRACE("mditab_get_item: invalid tab index (%d)", index);
        memset(id, 0, sizeof(MC_MTITEM));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(id->dwMask & MC_MTIF_TEXT) {
        mc_str_inbuf(mditab->items[index].text, MC_STRT, id->pszText,
                (unicode ? MC_STRW : MC_STRA), id->cchTextMax);
    }
    if(id->dwMask & MC_MTIF_IMAGE)
        id->iImage = mditab->items[index].img;
    if(id->dwMask & MC_MTIF_PARAM)
        id->lParam = mditab->items[index].lp;

    return TRUE;
}

static void
mditab_notify_delete_item(mditab_t* mditab, int index)
{
    mc_NM_MT_DELETEITEM notify;

    notify.hdr.hwndFrom = mditab->win;
    notify.hdr.idFrom = GetDlgCtrlID(mditab->win);
    notify.hdr.code = MC_MTN_DELETEITEM;
    notify.iItem = index;
    notify.lParam = mditab->items[index].lp;
    
    SendMessage(GetParent(mditab->win), WM_NOTIFY,
                (WPARAM)notify.hdr.idFrom, (LPARAM)&notify);
}

static BOOL
mditab_delete_item(mditab_t* mditab, int index)
{
    mditab_item_t* items;
    BOOL need_scroll;

    if(index < 0  ||  index >= mditab->item_count) {
        MC_TRACE("mditab_delete_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* Allocate and setup new tab vector */
    if(mditab->item_count > 1) {
        items = (mditab_item_t*) malloc((mditab->item_count - 1) * sizeof(mditab_item_t));
        if(MC_ERR(items == NULL)) {
            MC_TRACE("mditab_delete_item: malloc() failed.");
            return FALSE;
        }

        if(index > 0) {
            memcpy(items, mditab->items, index * sizeof(mditab_item_t));
        }
        if(index < mditab->item_count - 1) {
            memcpy(items + index, mditab->items + (index+1),
                    (mditab->item_count - index - 1) * sizeof(mditab_item_t));
        }
    } else {
        items = NULL;
    }

    /* Perhaps we have to set another tab as selected. We need to do this
     * before the deletion takes effect as the application still might want
     * to use it in the notification handler. */
    if(index == mditab->item_selected) {
        int old_item_selected = mditab->item_selected;
        if(mditab->item_selected < mditab->item_count - 1)
            mditab->item_selected++;
        else
            mditab->item_selected = mditab->item_count - 2;

        mditab_notify_sel_change(mditab, old_item_selected, mditab->item_selected);
    }

    /* Notify parent about the deletion */
    mditab_notify_delete_item(mditab, index);

    /* Swap items vector */
    free(mditab->items[index].text);
    free(mditab->items);
    mditab->items = items;
    mditab->item_count--;

    /* Refresh all stored tab indexes */
    if(index < mditab->item_selected)
        mditab->item_selected--;
    mditab->item_hot = -1;
    if(index >= mditab->item_first_visible)
        mditab->item_first_visible--;
    if(mditab->item_first_visible >= mditab->item_count)
        mditab->item_first_visible = mditab->item_count - 1;
    if(mditab->item_first_visible < 0)
        mditab->item_first_visible = 0;

    /* Refresh */
    need_scroll = mditab->need_scroll;
    mditab_layout(mditab);
    InvalidateRect(mditab->win, NULL, TRUE);

    return TRUE;
}

static void
mditab_notify_delete_all_items(mditab_t* mditab)
{
    NMHDR notify;
    UINT i;

    notify.hwndFrom = mditab->win;
    notify.idFrom = GetDlgCtrlID(mditab->win);
    notify.code = MC_MTN_DELETEALLITEMS;
    if(SendMessage(GetParent(mditab->win), WM_NOTIFY, (WPARAM)notify.idFrom, (LPARAM)&notify) == FALSE) {
        for(i = 0; i < mditab->item_count; i++)
            mditab_notify_delete_item(mditab, i);
    }
}

static BOOL
mditab_delete_all_items(mditab_t* mditab)
{
    if(mditab->item_selected >= 0) {
        int old_index = mditab->item_selected;
        mditab->item_selected = -1;
        mditab_notify_sel_change(mditab, old_index, mditab->item_selected);
    }

    mditab_notify_delete_all_items(mditab);

    if(mditab->items) {
        int i;

        for(i = 0; i < mditab->item_count; i++)
            free(mditab->items[i].text);

        free(mditab->items);
        mditab->items = NULL;
        mditab->item_count = 0;
        mditab->item_hot = -1;
        mditab->item_first_visible = 0;
        mditab->need_scroll = 0;
        InvalidateRect(mditab->win, NULL, TRUE);
    }
    return TRUE;
}

static HIMAGELIST
mditab_set_img_list(mditab_t* mditab, HIMAGELIST img_list)
{
    HIMAGELIST old_img_list;

    if(img_list == mditab->img_list)
        return img_list;

    old_img_list = mditab->img_list;
    mditab->img_list = img_list;
    InvalidateRect(mditab->win, NULL, TRUE);
    return old_img_list;
}

static int
mditab_set_cur_sel(mditab_t* mditab, int index)
{
    int old_sel_index;

    if(index < 0  ||  index >= mditab->item_count) {
        MC_TRACE("mditab_delete_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        index = -1;
    }

    old_sel_index = mditab->item_selected;
    if(index == old_sel_index)
        return old_sel_index;

    mditab->item_selected = index;

    if(!mditab_is_item_visible(mditab, index)) {
        /* If the newly selected item is not visible, make it visible.
         * In this case, we have to redraw everything. */
        mditab->item_first_visible = index;
        mditab_layout(mditab);
        InvalidateRect(mditab->win, NULL, TRUE);
    } else if(index != old_sel_index) {
        /* Otherwise we only redraw the tabs with changed status */
        mditab_layout(mditab);

        if(old_sel_index >= 0)
            mditab_invalidate_item(mditab, old_sel_index);
        if(index >= 0)
            mditab_invalidate_item(mditab, index);
    }

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
    RECT rect;
    TPMPARAMS tpm_param;
    int i;

    /* Construct the menu */
    popup = CreatePopupMenu();
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_STRING | MIIM_BITMAP;
    mii.hbmpItem = HBMMENU_CALLBACK;
    for(i = 0; i < mditab->item_count; i++) {
        mii.dwItemData = i;
        mii.wID = 1000 + i;
        mii.fState = (i == mditab->item_selected ? MFS_DEFAULT : 0);
        mii.dwTypeData = mditab->items[i].text;
        mii.cch = _tcslen(mii.dwTypeData);
        InsertMenuItem(popup, i, TRUE, &mii);
    }

    /* Disable the place for checkmarks in the menu is it is never used
     * and looks ugly. */
    mi.cbSize = sizeof(MENUINFO);
    mi.fMask = MIM_STYLE;
    GetMenuInfo(popup, &mi);
    mi.dwStyle |= MNS_CHECKORBMP;
    SetMenuInfo(popup, &mi);

    /* Show the menu */
    i = SendMessage(mditab->toolbar2, TB_COMMANDTOINDEX, IDC_LIST_ITEMS, 0);
    SendMessage(mditab->toolbar2, TB_GETITEMRECT, i, (LPARAM) &rect);
    MapWindowPoints(mditab->toolbar2, HWND_DESKTOP, (POINT*) &rect, 2);
    tpm_param.cbSize = sizeof(TPMPARAMS);
    CopyRect(&tpm_param.rcExclude, &rect);
    TrackPopupMenuEx(popup, TPM_LEFTBUTTON | TPM_RIGHTALIGN,
            rect.right, rect.bottom, mditab->win, &tpm_param);
    DestroyMenu(popup);
}

static void
mditab_measure_menu_icon(mditab_t* mditab, MEASUREITEMSTRUCT* mis)
{
    if(mditab->img_list != NULL  &&  mditab->items[mis->itemID - 1000].img >= 0) {
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
    if(mditab->img_list != NULL  &&  mditab->items[dis->itemID - 1000].img >= 0) {
        ImageList_Draw(mditab->img_list, mditab->items[dis->itemID - 1000].img,
                       dis->hDC, dis->rcItem.left, dis->rcItem.top, ILD_TRANSPARENT);
    }
}

static BOOL
mditab_close_item(mditab_t* mditab, int index)
{
    mc_NM_MT_CLOSEITEM notify;

    if(index < 0  ||  index >= mditab->item_count) {
        MC_TRACE("mditab_close_item: invalid tab index (%d)", index);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    notify.hdr.hwndFrom = mditab->win;
    notify.hdr.idFrom = GetDlgCtrlID(mditab->win);
    notify.hdr.code = MC_MTN_CLOSEITEM;
    notify.iItem = index;
    notify.lParam = mditab->items[index].lp;

    if(SendMessage(GetParent(mditab->win), WM_NOTIFY, (WPARAM)notify.hdr.idFrom, (LPARAM)&notify) == FALSE)
        return mditab_delete_item(mditab, index);
    else
        return FALSE;
}

static BOOL
mditab_set_item_width(HWND win, mc_MT_ITEMWIDTH* tw)
{
    USHORT def_w;
    USHORT min_w;
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    if(tw != NULL) {
        def_w = tw->dwDefWidth;
        min_w = tw->dwMinWidth;
    } else {
        def_w = DEFAULT_ITEM_DEF_WIDTH;
        min_w = DEFAULT_ITEM_MIN_WIDTH;
    }

    if(def_w < min_w)
        def_w = min_w;

    if(def_w != mditab->item_def_width  ||  min_w != mditab->item_min_width) {
        mditab->item_def_width = def_w;
        mditab->item_min_width = min_w;
        InvalidateRect(win, NULL, TRUE);
    }
    return TRUE;
}

static BOOL
mditab_get_item_width(HWND win, mc_MT_ITEMWIDTH* tw)
{
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    tw->dwDefWidth = mditab->item_def_width;
    tw->dwMinWidth = mditab->item_min_width;
    return TRUE;
}

static BOOL
mditab_command(HWND win, WORD code, WORD ctrl_id, HWND ctrl)
{
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    switch(ctrl_id) {
        case IDC_SCROLL_LEFT:
            if(mditab->item_first_visible > 0)
                mditab->item_first_visible--;
            mditab_layout(mditab);
            InvalidateRect(win, NULL, TRUE);
            break;

        case IDC_SCROLL_RIGHT:
            if(mditab->item_first_visible < mditab->item_count)
                mditab->item_first_visible++;
            mditab_layout(mditab);
            InvalidateRect(win, NULL, TRUE);
            break;

        case IDC_LIST_ITEMS:
            mditab_list_items(mditab);
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

static BOOL
mditab_key_up(HWND win, int key_code, DWORD key_data)
{
    int index;
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

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

    if(0 <= index  &&  index < mditab->item_count)
        mditab_set_cur_sel(mditab, index);

    return TRUE;
}

static void
mditab_left_button_down(HWND win, UINT keys, short x, short y)
{
    int index;
    mc_MT_HITTESTINFO hti;
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    if((mditab->style & MC_MTS_FOCUSMASK) == MC_MTS_FOCUSONBUTTONDOWN)
        SetFocus(win);

    hti.pt.x = x;
    hti.pt.y = y;
    index = mditab_hit_test(mditab, &hti);
    if(index < 0)
        return;

    if(index == mditab->item_selected) {
        if((mditab->style & MC_MTS_FOCUSMASK) != MC_MTS_FOCUSNEVER)
            SetFocus(win);
    } else {
        mditab_set_cur_sel(mditab, index);
    }
}

static void
mditab_middle_button_down(HWND win, UINT keys, short x, short y)
{
    int index;
    mc_MT_HITTESTINFO hti;
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    if((mditab->style & MC_MTS_CLOSEONMCLICK) == 0)
        return;

    hti.pt.x = x;
    hti.pt.y = y;
    index = mditab_hit_test(mditab, &hti);

    mditab->item_mclose = index;
    SetCapture(win);
}

static void
mditab_middle_button_up(HWND win, UINT keys, short x, short y)
{
    int index;
    mc_MT_HITTESTINFO hti;
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);

    if(GetCapture() == win)
        ReleaseCapture();

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
mditab_notify_click(HWND win, int code)
{
    NMHDR hdr;

    hdr.hwndFrom = win;
    hdr.idFrom = GetDlgCtrlID(win);
    hdr.code = code;

    SendMessage(GetParent(win), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
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
    mditab_layout(mditab);
    InvalidateRect(mditab->win, NULL, TRUE);
}

static void
mditab_theme_changed(mditab_t* mditab)
{
    POINT pos;

    if(mditab->theme)
        theme_CloseThemeData(mditab->theme);
    mditab->theme = theme_OpenThemeData(mditab->win, mditab_tc);
    InvalidateRect(mditab->win, NULL, TRUE);

    /* As an optimization, we do not track hot item when not themed, so
     * it might be in inconsistant state now. Refresh the state. */
    GetCursorPos(&pos);
    ScreenToClient(mditab->win, &pos);
    mditab_track_hot(mditab, pos.x, pos.y);
}

static int
mditab_create(HWND win, CREATESTRUCT* cs)
{
    mditab_t* mditab;
    
    mditab = (mditab_t*) malloc(sizeof(mditab_t));
    if(MC_ERR(mditab == NULL)) {
        MC_TRACE("mditab_create: malloc() failed.");
        return -1;
    }

    mditab->win = win;
    mditab->toolbar1 = CreateWindow(toolbar_wc, NULL,
                WS_CHILD | TBSTYLE_FLAT | CCS_NORESIZE | CCS_NODIVIDER,
                0, 0, 0, 0, win, (HMENU) IDC_TBAR_1, mc_instance_exe, NULL);
    SendMessage(mditab->toolbar1, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(mditab->toolbar1, TB_SETIMAGELIST, 0, (LPARAM)mc_bmp_glyphs);
    mditab->toolbar2 = CreateWindow(toolbar_wc,
                NULL, WS_CHILD | TBSTYLE_FLAT | CCS_NORESIZE | CCS_NODIVIDER,
                0, 0, 0, 0, win, (HMENU) IDC_TBAR_2, mc_instance_exe, NULL);
    SendMessage(mditab->toolbar2, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(mditab->toolbar2, TB_SETIMAGELIST, 0, (LPARAM)mc_bmp_glyphs);
    mditab->theme = theme_OpenThemeData(win, mditab_tc);
    mditab->img_list = NULL;
    mditab->font = NULL;
    mditab->items = NULL;
    mditab->item_count = 0;
    mditab->item_selected = -1;
    mditab->item_hot = -1;
    mditab->item_first_visible = 0;
    mditab->item_mclose = -1;
    mditab->hot_track_timer = 0;
    mditab->item_min_width = DEFAULT_ITEM_MIN_WIDTH;
    mditab->item_def_width = DEFAULT_ITEM_DEF_WIDTH;
    mditab->style = cs->style;
    mditab->do_redraw = 1;
    mditab->need_scroll = 0;
    
    SetWindowLongPtr(win, 0, (LONG_PTR)mditab);
    return 0;
}

static void
mditab_destroy(mditab_t* mditab)
{
    int i;

    mditab_notify_delete_all_items(mditab);

    if(mditab->hot_track_timer)
        KillTimer(mditab->win, mditab->hot_track_timer);
    if(mditab->theme)
        theme_CloseThemeData(mditab->theme);
    for(i = 0; i < mditab->item_count; i++)
        free(mditab->items[i].text);
    free(mditab->items);
    free(mditab);
}

static LRESULT CALLBACK
mditab_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    mditab_t* mditab = (mditab_t*) GetWindowLongPtr(win, 0);
    
    switch(msg) {
        case WM_PAINT:
        case WM_PRINTCLIENT:
        {
            HDC dc = (HDC)wp;
            PAINTSTRUCT ps;

            if(wp == 0)
                dc = BeginPaint(win, &ps);
            mditab_paint(mditab, dc);
            if(wp == 0)
                EndPaint(win, &ps);
            return 0;
        }

        case MC_MTM_GETITEMCOUNT:
            return mditab->item_count;

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
            return mditab_hit_test(mditab, (mc_MT_HITTESTINFO*)lp);

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
            return (LRESULT) mditab_set_item_width(win, (mc_MT_ITEMWIDTH*)lp);

        case MC_MTM_GETITEMWIDTH:
            return (LRESULT) mditab_get_item_width(win, (mc_MT_ITEMWIDTH*)lp);

        case WM_LBUTTONDOWN:
            mditab_left_button_down(win, wp, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_MBUTTONDOWN:
            mditab_middle_button_down(win, wp, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_MBUTTONUP:
            mditab_middle_button_up(win, wp, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_LBUTTONUP:
            mditab_notify_click(win, NM_CLICK);
            return 0;

        case WM_RBUTTONUP:
            mditab_notify_click(win, NM_RCLICK);
            return 0;

        case WM_MOUSEMOVE:
            mditab_track_hot(mditab, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_COMMAND:
            if(mditab_command(win, HIWORD(wp), LOWORD(wp), (HWND)lp))
                return 0;
            break;

        case WM_SIZE:
            mditab_layout(mditab);
            InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_KEYUP:
            if(mditab_key_up(win, (int)wp, (DWORD)lp))
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
            if((BOOL) lp)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_MEASUREITEM:
            mditab_measure_menu_icon(mditab, (MEASUREITEMSTRUCT*) lp);
            return TRUE;

        case WM_DRAWITEM:
            mditab_draw_menu_icon(mditab, (DRAWITEMSTRUCT*) lp);
            return TRUE;

        case WM_SETREDRAW:
            mditab->do_redraw = (wp ? 1 : 0);
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
            InvalidateRect(win, NULL, FALSE);
            break;

        case WM_CREATE:
            return mditab_create(win, (CREATESTRUCT*)lp);

        case WM_DESTROY:
            mditab_destroy(mditab);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}


int
mditab_init(void)
{
    WNDCLASS wc = { 0 };

    mc_init_common_controls(ICC_BAR_CLASSES);

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = mditab_proc;
    wc.cbWndExtra = sizeof(mditab_t*);
    wc.hInstance = mc_instance_exe;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = mditab_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE("mditab_init: RegisterClass() failed [%lu]", GetLastError());
        return -1;
    }

    return 0;
}

void
mditab_fini(void)
{
    UnregisterClass(mditab_wc, mc_instance_exe);
}

