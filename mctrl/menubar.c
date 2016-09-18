/*
 * Copyright (c) 2012-2016 Martin Mitas
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

#include "menubar.h"
#include "theme.h"


/* The MenuBar implementation is partially based on the MSDN article "How to
 * Create an Internet Explorer-Style Menu Bar":
 * http://msdn.microsoft.com/en-us/library/windows/desktop/bb775450%28v=vs.85%29.aspx
 */


/* Note that message processing in the module is a bit tricky and complex,
 * especially for keyboard messages. There are three different contexts
 * where we handle messages in this module. All the three have to play in
 * concert:
 *
 * (1) mcIsMenubarMessage() handles messages, supposedly in the context of
 *     the main application message loop. It is responsible for activating
 *     the menubar on <F10> or <ALT>, and for distributing accelerator keys.
 *     Note it processes <F10> or <ALT> only when no menubar is currently
 *     active (if it is active, it lets the message to propagate into (2)).
 *
 * (2) menubar_proc() handles messages sent directly to the menubar control
 *     (which is superclassed toolbar) while no child popup menu is dropped
 *     down. It handles for example <LEFT> and <RIGHT> arrows to change hot
 *     item in the menubar, <DOWN> or <RETURN> to open a popup menu. It also
 *     looks for <F10>, <ALT> or <ESC> to cancel menu navigation and
 *     deactivating the menu.
 *
 * (3) When a pop-up menu is opened, we install a message filter hook
 *     menubar_ht_proc() as the pop-up would otherwise grab all messages itself
 *     (so no message ever comes into (2)). It is responsible to switch to
 *     another popup on <LEFT> or <RIGHT> arrow keys (unless it is interpreted
 *     to open or close its own sub-menu), or to cancel the menu navigation
 *     completely with <F10> or <ALT>. <ESC> is kept on handling by the popup
 *     menu itself. The hook is kept in use until a menu item is fired, or until
 *     menu navigation is canceled. In any case, the menubar then deactivates
 *     itself.
 */


/* Uncomment this to have more verbose traces from this module. */
/*#define MENUBAR_DEBUG     1*/

#ifdef MENUBAR_DEBUG
    #define MENUBAR_TRACE   MC_TRACE
#else
    #define MENUBAR_TRACE   MC_NOOP
#endif


static const TCHAR menubar_wc[] = MC_WC_MENUBAR;    /* window class name */
static int extra_offset;
static WNDPROC orig_toolbar_proc = NULL;


typedef struct menubar_tag menubar_t;
struct menubar_tag {
    HWND win;
    HWND notify_win;
    HWND old_focus;  /* We return focus to it on <ESC> */
    HMENU menu;
    short hot_item;
    short pressed_item;
    WORD rtl                    : 1;
    WORD continue_hot_track     : 1;
    WORD select_from_keyboard   : 1;
    WORD is_dropdown_active     : 1;
};


static menubar_t* active_menubar = NULL;
static menubar_t* activate_with_f10 = FALSE;


#define MENUBAR_ITEM_LABEL_MAXSIZE     32
#define MENUBAR_SEPARATOR_WIDTH        10

#define MENUBAR_DTFLAGS               (DT_CENTER | DT_VCENTER | DT_SINGLELINE)

#define MENUBAR_SENDMSG(win,msg,wp,lp)    \
        CallWindowProc(orig_toolbar_proc, (win), (msg), (WPARAM)(wp), (LPARAM)(lp))


/* Forward declarations */
static void menubar_ht_enable(menubar_t* mb);
static void menubar_ht_disable(menubar_t* mb);


static void
menubar_update_ui_state(menubar_t* mb, BOOL keyboard_activity)
{
    WORD action;

    if(mc_win_version < MC_WIN_XP)  /* Win 2000 does not support ui state */
        return;

    if(keyboard_activity) {
        action = UIS_CLEAR;
    } else {
        BOOL show_accel_always;
        if(!SystemParametersInfo(SPI_GETMENUUNDERLINES, 0, &show_accel_always, 0))
            show_accel_always = TRUE;
        action = (show_accel_always ? UIS_CLEAR : UIS_SET);
    }

    MC_POST(mb->win, WM_CHANGEUISTATE, MAKELONG(action, UISF_HIDEACCEL), 0);
}

static int
menubar_set_menu(menubar_t* mb, HMENU menu, BOOL is_refresh)
{
    BYTE* buffer = NULL;
    TBBUTTON* buttons;
    TCHAR* labels;
    int i, n;

    MENUBAR_TRACE("menubar_set_menu(%p, %p)", mb, menu);

    if(menu == mb->menu  &&  !is_refresh)
        return 0;

    /* If dropped down, cancel it */
    if(mb->pressed_item >= 0) {
        menubar_ht_disable(mb);
        MENUBAR_SENDMSG(mb->win, WM_CANCELMODE, 0, 0);
    }

    /* Uninstall the old menu */
    if(mb->menu != NULL) {
        n = MENUBAR_SENDMSG(mb->win, TB_BUTTONCOUNT, 0, 0);
        for(i = 0; i < n; i++)
            MENUBAR_SENDMSG(mb->win, TB_DELETEBUTTON, 0, 0);
        mb->menu = NULL;
    }

    /* Install the new menu */
    n = (menu != NULL ? GetMenuItemCount(menu) : 0);
    if(MC_ERR(n < 0)) {
        MC_TRACE("menubar_set_menu: GetMenuItemCount() failed.");
        return -1;
    }

    if(n == 0) {
        mb->menu = menu;
        return 0;
    }

    buffer = (BYTE*) _malloca(n * sizeof(TBBUTTON) +
                              n * sizeof(TCHAR) * MENUBAR_ITEM_LABEL_MAXSIZE);
    if(MC_ERR(buffer == NULL)) {
        MC_TRACE("menubar_set_menu: _malloca() failed.");
        mc_send_notify(mb->notify_win, mb->win, NM_OUTOFMEMORY);
        return -1;
    }
    buttons = (TBBUTTON*) buffer;
    labels = (TCHAR*) (buffer + n * sizeof(TBBUTTON));

    memset(buttons, 0, n * sizeof(TBBUTTON));

    for(i = 0; i < n; i++) {
        UINT state;
        state = GetMenuState(menu, i, MF_BYPOSITION);

        buttons[i].iBitmap = I_IMAGENONE;
        buttons[i].fsState = 0;
        if(!(state & (MF_DISABLED | MF_GRAYED)))
            buttons[i].fsState |= TBSTATE_ENABLED;
        if((state & (MF_MENUBREAK | MF_MENUBARBREAK)) && i > 0)
            buttons[i-1].fsState |= TBSTATE_WRAP;

        if(state & MF_POPUP) {
            TCHAR* label = labels + i * MENUBAR_ITEM_LABEL_MAXSIZE;
            GetMenuString(menu, i, label, MENUBAR_ITEM_LABEL_MAXSIZE, MF_BYPOSITION);

            buttons[i].fsStyle = BTNS_AUTOSIZE | BTNS_DROPDOWN | BTNS_SHOWTEXT;
            buttons[i].dwData = i;
            buttons[i].iString = (INT_PTR) label;
            buttons[i].idCommand = i;
        } else {
            buttons[i].dwData = 0xffff;
            buttons[i].idCommand = 0xffff;
            if(state & MF_SEPARATOR) {
                buttons[i].fsStyle = BTNS_SEP;
                buttons[i].iBitmap = MENUBAR_SEPARATOR_WIDTH;
            }
        }
    }

    MENUBAR_SENDMSG(mb->win, TB_ADDBUTTONS, n, buttons);
    mb->menu = menu;
    _freea(buffer);
    return 0;
}

static void
menubar_reset_hot_item(menubar_t* mb)
{
    int item;
    POINT pt;

    GetCursorPos(&pt);
    MapWindowPoints(NULL, mb->win, &pt, 1);
    item = MENUBAR_SENDMSG(mb->win, TB_HITTEST, 0, &pt);
    MENUBAR_SENDMSG(mb->win, TB_SETHOTITEM, item, 0);
}

static void
menubar_perform_dropdown(menubar_t* mb)
{
    int item;
    DWORD btn_state;
    TPMPARAMS pmparams = {0};
    UINT pmflags;

    MENUBAR_TRACE("menubar_perform_dropdown(%p)", mb);

    pmparams.cbSize = sizeof(TPMPARAMS);

    mb->is_dropdown_active = TRUE;
    menubar_ht_enable(mb);
    SetFocus(mb->win);

    while(mb->pressed_item >= 0) {
        item = mb->pressed_item;

        if(mb->select_from_keyboard) {
            keybd_event(VK_DOWN, 0, 0, 0);
            keybd_event(VK_DOWN, 0, KEYEVENTF_KEYUP, 0);
        }

        mb->select_from_keyboard = FALSE;
        mb->continue_hot_track = FALSE;

        MENUBAR_SENDMSG(mb->win, TB_SETHOTITEM, item, 0);
        btn_state = MENUBAR_SENDMSG(mb->win, TB_GETSTATE, item, 0);
        MENUBAR_SENDMSG(mb->win, TB_SETSTATE, item,
                        MAKELONG(btn_state | TBSTATE_PRESSED, 0));

        MENUBAR_SENDMSG(mb->win, TB_GETITEMRECT, item, &pmparams.rcExclude);

        if(mc_win_version >= MC_WIN_VISTA  &&  mcIsAppThemed()) {
            /* Fix for consistency with a native menu on newer Windows
             * when styles are enabled. */
            pmparams.rcExclude.bottom--;
        }

        MapWindowPoints(mb->win, HWND_DESKTOP, (POINT*)&pmparams.rcExclude, 2);

        /* Win 2000 do not know TPM_LAYOUTRTL. */
        pmflags = TPM_LEFTBUTTON | TPM_VERTICAL;
        if(mb->rtl  &&  mc_win_version > MC_WIN_2000)
            mb->rtl |= TPM_LAYOUTRTL;

        MENUBAR_TRACE("menubar_perform_dropdown: ENTER TrackPopupMenuEx()");
        TrackPopupMenuEx(GetSubMenu(mb->menu, item), pmflags,
                (mb->rtl ? pmparams.rcExclude.right : pmparams.rcExclude.left),
                pmparams.rcExclude.bottom, mb->win, &pmparams);
        MENUBAR_TRACE("menubar_perform_dropdown: LEAVE TrackPopupMenuEx()");

        MENUBAR_SENDMSG(mb->win, TB_SETSTATE, item, MAKELONG(btn_state, 0));

        if(!mb->continue_hot_track)
            mb->pressed_item = -1;
    }

    menubar_reset_hot_item(mb);
    menubar_ht_disable(mb);
    mb->is_dropdown_active = FALSE;
    SetFocus(mb->old_focus);
}

static inline void
menubar_dropdown(menubar_t* mb, int item, BOOL from_keyboard)
{
    MENUBAR_TRACE("menubar_dropdown(%p, %d)", mb, item);

    if(item == mb->pressed_item)
        return;

    mb->pressed_item = item;
    mb->select_from_keyboard = from_keyboard;

    /* We cannot just immediately open the popup menu: This function may be
     * called in the context of a TBN_DROPDOWN notification, and the toolbar
     * won't update hot item until we return from the notification. So
     * postpone it to the end of message queue.
     *
     * Note we misuse TB_CUSTOMIZE, which does not send it down to original
     * win procedure, so there is no risk of message id clash.
     */
    MC_POST(mb->win, TB_CUSTOMIZE, 0, 0);
}

static LRESULT
menubar_notify(menubar_t* mb, NMHDR* hdr)
{
    switch(hdr->code) {
        case TBN_DROPDOWN:
        {
            NMTOOLBAR* info = (NMTOOLBAR*) hdr;
            MENUBAR_TRACE("menubar_notify(%p, TBN_DROPDOWN, %d)", mb, info->iItem);
            menubar_dropdown(mb, info->iItem, FALSE);
            break;
        }

        case TBN_HOTITEMCHANGE:
        {
            NMTBHOTITEM* info = (NMTBHOTITEM*) hdr;
            MENUBAR_TRACE("menubar_notify(%p, TBN_HOTITEMCHANGE, %d -> %d)", mb,
                          (info->dwFlags & HICF_ENTERING) ? -1 : info->idOld,
                          (info->dwFlags & HICF_LEAVING) ? -1 : info->idNew);
            mb->hot_item = (info->dwFlags & HICF_LEAVING) ? -1 : info->idNew;
            break;
        }

        case NM_CUSTOMDRAW:
        {
            NMTBCUSTOMDRAW* info = (NMTBCUSTOMDRAW*) hdr;
            switch(info->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    if(mc_win_version >= MC_WIN_VISTA  &&  mcIsAppThemed())
                        return CDRF_DODEFAULT;
                    else if(mc_win_version < MC_WIN_XP)
                        return CDRF_DODEFAULT;
                    else
                        return CDRF_NOTIFYITEMDRAW;

                case CDDS_ITEMPREPAINT:
                {
                    int text_color_id;
                    int bk_color_id;
                    TCHAR buffer[MENUBAR_ITEM_LABEL_MAXSIZE];
                    TBBUTTONINFO btn;
                    UINT flags = MENUBAR_DTFLAGS;
                    HDC dc = info->nmcd.hdc;
                    HFONT old_font;

                    if(info->nmcd.uItemState & (CDIS_HOT | CDIS_SELECTED)) {
                        text_color_id = COLOR_HIGHLIGHTTEXT;
                        bk_color_id = COLOR_HIGHLIGHT;
                    } else {
                        text_color_id = COLOR_MENUTEXT;
                        bk_color_id = -1;
                    }

                    btn.cbSize = sizeof(TBBUTTONINFO);
                    btn.dwMask = TBIF_TEXT;
                    btn.pszText = buffer;
                    btn.cchText = MC_SIZEOF_ARRAY(buffer);
                    MENUBAR_SENDMSG(mb->win, TB_GETBUTTONINFO, info->nmcd.dwItemSpec, &btn);

                    if(MENUBAR_SENDMSG(mb->win, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL)
                        flags |= DT_HIDEPREFIX;

                    if(bk_color_id >= 0)
                        FillRect(dc, &info->nmcd.rc, (HBRUSH) (INT_PTR) (bk_color_id+1));

                    SetTextColor(dc, GetSysColor(text_color_id));
                    old_font = SelectObject(dc, (HFONT) MENUBAR_SENDMSG(mb->win, WM_GETFONT, 0, 0));
                    SetBkMode(dc, TRANSPARENT);
                    DrawText(dc, buffer, -1, &info->nmcd.rc, flags);
                    SelectObject(dc, old_font);
                    return CDRF_SKIPDEFAULT;
                }

                default:
                    return CDRF_DODEFAULT;
            }
        }
    }

    return MC_SEND(mb->notify_win, WM_NOTIFY, hdr->idFrom, hdr);
}

static BOOL
menubar_key_down(menubar_t* mb, int vk, DWORD key_data)
{
    /* Swap meaning of VK_LEFT and VK_RIGHT if having right-to-left layout. */
    if(mb->rtl) {
        if(vk == VK_LEFT)
            vk = VK_RIGHT;
        else if(vk == VK_RIGHT)
            vk = VK_LEFT;
    }

    switch(vk) {
        case VK_ESCAPE:
        case VK_F10:
        case VK_MENU:
            MENUBAR_TRACE("menubar_key_down(VK_ESCAPE/VK_F10/VK_MENU)");
            SetFocus(mb->old_focus);
            menubar_reset_hot_item(mb);
            active_menubar = NULL;
            menubar_update_ui_state(mb, FALSE);
            return TRUE;

        case VK_LEFT:
            MENUBAR_TRACE("menubar_key_down(VK_LEFT)");
            if(mb->hot_item >= 0) {
                int item = mb->hot_item - 1;
                if(item < 0)
                    item = MENUBAR_SENDMSG(mb->win, TB_BUTTONCOUNT, 0, 0) - 1;
                MENUBAR_SENDMSG(mb->win, TB_SETHOTITEM, item, 0);
                menubar_update_ui_state(mb, TRUE);
                return TRUE;
            }
            break;

        case VK_RIGHT:
            MENUBAR_TRACE("menubar_key_down(VK_RIGHT)");
            if(mb->hot_item >= 0) {
                int item = mb->hot_item + 1;
                if(item >= MENUBAR_SENDMSG(mb->win, TB_BUTTONCOUNT, 0, 0))
                    item = 0;
                MENUBAR_SENDMSG(mb->win, TB_SETHOTITEM, item, 0);
                menubar_update_ui_state(mb, TRUE);
                return TRUE;
            }
            break;

        case VK_DOWN:
        case VK_UP:
        case VK_RETURN:
            MENUBAR_TRACE("menubar_key_down(VK_DOWN/VK_UP/VK_RETURN)");
            if(mb->hot_item >= 0) {
                menubar_dropdown(mb, mb->hot_item, TRUE);
                menubar_update_ui_state(mb, TRUE);
                return TRUE;
            }
            break;
    }

    /* If we have not consume the key, report it to the caller. */
    return FALSE;
}

static menubar_t*
menubar_nccreate(HWND win, CREATESTRUCT *cs)
{
    menubar_t* mb;
    TCHAR parent_class[16];

    MENUBAR_TRACE("menubar_nccreate(%p, %p)", win, cs);

    mb = (menubar_t*) malloc(sizeof(menubar_t));
    if(MC_ERR(mb == NULL)) {
        MC_TRACE("menubar_nccreate: malloc() failed.");
        return NULL;
    }

    memset(mb, 0, sizeof(menubar_t));
    mb->win = win;

    /* Lets be a little friendly to the app. developers: If the parent is
     * ReBar control, lets send WM_NOTIFY/WM_COMMAND to the ReBar's parent
     * as ReBar really is not interested in it, and embedding the menubar
     * in the ReBar is actually main advantage of this control in comparison
     * with the standard window menu. */
    GetClassName(cs->hwndParent, parent_class, MC_SIZEOF_ARRAY(parent_class));
    if(_tcscmp(parent_class, _T("ReBarWindow32")) == 0)
        mb->notify_win = GetAncestor(cs->hwndParent, GA_PARENT);
    else
        mb->notify_win = cs->hwndParent;

    mb->hot_item = -1;
    mb->pressed_item = -1;
    mb->rtl = mc_is_rtl_exstyle(cs->dwExStyle);

    return mb;
}

static int
menubar_create(menubar_t* mb, CREATESTRUCT *cs)
{
    MENUBAR_TRACE("menubar_create(%p, %p)", mb, cs);

    if(MC_ERR(MENUBAR_SENDMSG(mb->win, WM_CREATE, 0, cs) != 0)) {
        MC_TRACE_ERR("menubar_create: CallWindowProc() failed");
        return -1;
    }

    MENUBAR_SENDMSG(mb->win, TB_SETPARENT, mb->win, 0);
    MENUBAR_SENDMSG(mb->win, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    MENUBAR_SENDMSG(mb->win, TB_SETBITMAPSIZE, 0, MAKELONG(0, -2));
    MENUBAR_SENDMSG(mb->win, TB_SETPADDING, 0, MAKELONG(10, 6));
    MENUBAR_SENDMSG(mb->win, TB_SETDRAWTEXTFLAGS, MENUBAR_DTFLAGS, MENUBAR_DTFLAGS);

    /* Add some styles we consider default */
    SetWindowLongPtr(mb->win, GWL_STYLE, cs->style | TBSTYLE_FLAT |
                     TBSTYLE_TRANSPARENT | CCS_NODIVIDER);

    if(cs->lpCreateParams != NULL) {
        if(MC_ERR(menubar_set_menu(mb, (HMENU) cs->lpCreateParams, FALSE) != 0)) {
            MC_TRACE("menubar_create: menubar_set_menu() failed.");
            return -1;
        }
    }

    menubar_update_ui_state(mb, FALSE);
    return 0;
}

static inline void
menubar_destroy(menubar_t* mb)
{
    MENUBAR_TRACE("menubar_destroy(%p)", mb);
}

static inline void
menubar_ncdestroy(menubar_t* mb)
{
    MENUBAR_TRACE("menubar_ncdestroy(%p)", mb);
    free(mb);
}

static LRESULT CALLBACK
menubar_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    menubar_t* mb = (menubar_t*) GetWindowLongPtr(win, extra_offset);

    switch(msg) {
        case MC_MBM_REFRESH:
            lp = (LPARAM)mb->menu;
            /* no break */
        case MC_MBM_SETMENU:
            return (menubar_set_menu(mb, (HMENU)lp, (msg == MC_MBM_REFRESH)) == 0 ? TRUE : FALSE);

        case TB_SETPARENT:
        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = mb->notify_win;
            mb->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case WM_COMMAND:
            MENUBAR_TRACE("menubar_proc(WM_COMMAND): code=%d; wid=%d; control=%p",
                          (int)HIWORD(wp), (int)LOWORD(wp), (HWND)lp);
            if(lp == 0  &&  HIWORD(wp) == 0)  /* msg came from the menu */
                return MC_SEND(mb->notify_win, msg, wp, lp);
            break;

        case WM_NOTIFY:
        {
            NMHDR* hdr = (NMHDR*) lp;
            if(hdr->hwndFrom == win)
                return menubar_notify(mb, hdr);
            break;
        }

        case WM_ENTERMENULOOP:
        case WM_EXITMENULOOP:
        case WM_CONTEXTMENU:
        case WM_INITMENU:
        case WM_INITMENUPOPUP:
        case WM_UNINITMENUPOPUP:
        case WM_MENUSELECT:
        case WM_MENUCHAR:
        case WM_MENURBUTTONUP:
        case WM_MENUCOMMAND:
        case WM_MENUDRAG:
        case WM_MENUGETOBJECT:
        case WM_MEASUREITEM:
        case WM_DRAWITEM:
            return MC_SEND(mb->notify_win, msg, wp, lp);

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if(menubar_key_down(mb, wp, lp))
                return 0;
            break;

        case WM_GETDLGCODE:
            return MENUBAR_SENDMSG(win, msg, wp, lp) | DLGC_WANTALLKEYS | DLGC_WANTARROWS;

        case WM_SETFOCUS:
            MENUBAR_TRACE("menubar_proc(WM_SETFOCUS): old focus %p", (HWND) wp);
            if(win != (HWND) wp)
                mb->old_focus = (HWND) wp;
            active_menubar = mb;
            break;

        case WM_KILLFOCUS:
            MENUBAR_TRACE("menubar_proc(WM_KILLFOCUS)");
            mb->old_focus = NULL;
            MENUBAR_SENDMSG(mb->win, TB_SETHOTITEM, -1, 0);
            menubar_update_ui_state(mb, FALSE);
            active_menubar = NULL;
            break;

        case WM_STYLECHANGED:
            if(wp == GWL_EXSTYLE) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                mb->rtl = mc_is_rtl_exstyle(ss->styleNew);
                InvalidateRect(mb->win, NULL, TRUE);
            }
            break;

        case WM_NCCREATE:
            if(MC_ERR(MENUBAR_SENDMSG(win, msg, wp, lp) == FALSE)) {
                MC_TRACE_ERR("menubar_proc: MENUBAR_SENDMSG(WM_NCCREATE) failed");
                return FALSE;
            }
            mb = menubar_nccreate(win, (CREATESTRUCT*) lp);
            if(MC_ERR(mb == NULL))
                return FALSE;
            SetWindowLongPtr(win, extra_offset, (LONG_PTR)mb);
            return TRUE;

        case WM_CREATE:
            return menubar_create(mb, (CREATESTRUCT*) lp);

        case WM_DESTROY:
            menubar_destroy(mb);
            break;

        case WM_NCDESTROY:
            if(mb) {
                menubar_ncdestroy(mb);
                mb = NULL;
            }
            break;

        /* Disable those standard toolbar messages, which modify contents of
         * the toolbar, as it is our internal responsibility to set it
         * according to the menu. */
        case TB_ADDBITMAP:
        case TB_ADDSTRING:
            MC_TRACE("menubar_proc: Suppressing message TB_xxxx (%d)", msg);
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            return -1;
        case TB_ADDBUTTONS:
        case TB_BUTTONSTRUCTSIZE:
        case TB_CHANGEBITMAP:
        case TB_DELETEBUTTON:
        case TB_ENABLEBUTTON:
        case TB_HIDEBUTTON:
        case TB_INDETERMINATE:
        case TB_INSERTBUTTON:
        case TB_LOADIMAGES:
        case TB_MARKBUTTON:
        case TB_MOVEBUTTON:
        case TB_PRESSBUTTON:
        case TB_REPLACEBITMAP:
        case TB_SAVERESTORE:
        case TB_SETANCHORHIGHLIGHT:
        case TB_SETBITMAPSIZE:
        case TB_SETBOUNDINGSIZE:
        case TB_SETCMDID:
        case TB_SETDISABLEDIMAGELIST:
        case TB_SETHOTIMAGELIST:
        case TB_SETIMAGELIST:
        case TB_SETINSERTMARK:
        case TB_SETPRESSEDIMAGELIST:
        case TB_SETSTATE:
            MC_TRACE("menubar_proc: Suppressing message TB_xxxx (%d)", msg);
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            return 0;  /* FALSE == NULL == 0 */

        case TB_CUSTOMIZE:
            /* Actually we suppress TB_CUSTOMIZE as the message above (i.e.
             * not passing it into the original proc), but we also misuse
             * is for our internal purpose of showing the popup menu. */
            menubar_perform_dropdown(mb);
            return 0;
    }

    return MENUBAR_SENDMSG(win, msg, wp, lp);
}


/********************
 *** Hot Tracking ***
 ********************/

static mc_mutex_t menubar_ht_mutex;
static HHOOK menubar_ht_hook = NULL;
static menubar_t* menubar_ht_mb = NULL;
static HMENU menubar_ht_sel_menu = NULL;
static int menubar_ht_sel_item = -1;
static UINT menubar_ht_sel_flags = 0;
static POINT menubar_ht_last_pos = {0};

static void
menubar_ht_change_dropdown(menubar_t* mb, int item, BOOL from_keyboard)
{
    MENUBAR_TRACE("menubar_ht_change_dropdown(%p,%d)", mb, item);

    mb->pressed_item = item;
    mb->select_from_keyboard = from_keyboard;
    mb->continue_hot_track = TRUE;
    MENUBAR_SENDMSG(mb->win, WM_CANCELMODE, 0, 0);
}

static LRESULT CALLBACK
menubar_ht_proc(int code, WPARAM wp, LPARAM lp)
{
    if(code >= 0) {
        MSG* msg = (MSG*)lp;
        menubar_t* mb = menubar_ht_mb;

        MC_ASSERT(mb != NULL);

        switch(msg->message) {
            case WM_MENUSELECT:
                menubar_ht_sel_menu = (HMENU)msg->lParam;
                menubar_ht_sel_item = LOWORD(msg->wParam);
                menubar_ht_sel_flags = HIWORD(msg->wParam);
                MENUBAR_TRACE("menubar_ht_proc: WM_MENUSELECT %p %d", menubar_ht_sel_menu, menubar_ht_sel_item);
                break;

            case WM_MOUSEMOVE:
            {
                POINT pt = msg->pt;
                int item;

                MapWindowPoints(NULL, mb->win, &pt, 1);
                item = MENUBAR_SENDMSG(mb->win, TB_HITTEST, 0, (LPARAM)&pt);
                if(menubar_ht_last_pos.x != pt.x  ||  menubar_ht_last_pos.y != pt.y) {
                    menubar_ht_last_pos = pt;
                    if(item != mb->pressed_item  &&
                       0 <= item  &&  item < MENUBAR_SENDMSG(mb->win, TB_BUTTONCOUNT, 0, 0)) {
                        MENUBAR_TRACE("menubar_ht_proc: Change dropdown by mouse move "
                                      "[%d -> %d]", mb->pressed_item, item);
                        menubar_ht_change_dropdown(mb, item, FALSE);
                    }
                }
                break;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                int vk = msg->wParam;

                /* Swap meaning of VK_LEFT and VK_RIGHT if having right-to-left layout. */
                if(mb->rtl) {
                    if(vk == VK_LEFT)
                        vk = VK_RIGHT;
                    else if(vk == VK_RIGHT)
                        vk = VK_LEFT;
                }

                switch(vk) {
                    case VK_MENU:
                    case VK_F10:
                        menubar_ht_change_dropdown(mb, -1, TRUE);
                        return 0;

                    case VK_LEFT:
                        if(menubar_ht_sel_menu == NULL  ||
                           menubar_ht_sel_menu == GetSubMenu(mb->menu, mb->pressed_item))
                        {
                            int item = mb->pressed_item - 1;
                            if(item < 0)
                                item = MENUBAR_SENDMSG(mb->win, TB_BUTTONCOUNT, 0, 0) - 1;
                            MENUBAR_TRACE("menubar_ht_proc: Change dropdown by VK_LEFT");
                            if(item != mb->pressed_item)
                                menubar_ht_change_dropdown(mb, item, TRUE);
                        }
                        break;

                    case VK_RIGHT:
                        if(menubar_ht_sel_menu == NULL  ||
                           !(menubar_ht_sel_flags & MF_POPUP) ||
                           (menubar_ht_sel_flags & (MF_GRAYED | MF_DISABLED)))
                        {
                            int item = mb->pressed_item + 1;
                            if(item >= MENUBAR_SENDMSG(mb->win, TB_BUTTONCOUNT, 0, 0))
                                item = 0;
                            MENUBAR_TRACE("menubar_ht_proc: Change dropdown by VK_RIGHT");
                            if(item != mb->pressed_item)
                                menubar_ht_change_dropdown(mb, item, TRUE);
                        }
                        break;
                }
                break;
            }
        }
    }

    return CallNextHookEx(menubar_ht_hook, code, wp, lp);
}

static void inline
menubar_ht_perform_disable(void)
{
    if(menubar_ht_hook) {
        UnhookWindowsHookEx(menubar_ht_hook);
        menubar_ht_hook = NULL;
        menubar_ht_mb = NULL;
        menubar_ht_sel_menu = NULL;
        menubar_ht_sel_item = -1;
        menubar_ht_sel_flags = 0;
    }
}

static void
menubar_ht_enable(menubar_t* mb)
{
    MENUBAR_TRACE("menubar_ht_enable(%p)", mb);

    mc_mutex_lock(&menubar_ht_mutex);

    if(MC_ERR(menubar_ht_hook != NULL)) {
        MC_TRACE("menubar_ht_enable: Another menubar hot tracks???");
        menubar_ht_perform_disable();
    }

    menubar_ht_hook = SetWindowsHookEx(WH_MSGFILTER, menubar_ht_proc, mc_instance, GetCurrentThreadId());
    if(MC_ERR(menubar_ht_hook == NULL)) {
        MC_TRACE_ERR("menubar_ht_enable: SetWindowsHookEx() failed");
        goto err_hook;
    }

    menubar_ht_mb = mb;
    GetCursorPos(&menubar_ht_last_pos);
    MapWindowPoints(NULL, mb->win, &menubar_ht_last_pos, 1);

err_hook:
    mc_mutex_unlock(&menubar_ht_mutex);
}

static void
menubar_ht_disable(menubar_t* mb)
{
    MENUBAR_TRACE("menubar_ht_disable(%p)", mb);

    mc_mutex_lock(&menubar_ht_mutex);

    if(MC_ERR(menubar_ht_mb != mb))
        MC_TRACE("menubar_ht_disable: Another menubar hot tracks???");
    else
        menubar_ht_perform_disable();

    mc_mutex_unlock(&menubar_ht_mutex);
}


/**********************
 *** Initialization ***
 **********************/

int
menubar_init_module(void)
{
    WNDCLASS wc = { 0 };

    if(MC_ERR(mc_init_comctl32(ICC_BAR_CLASSES | ICC_COOL_CLASSES) != 0)) {
        MC_TRACE("menubar_init_module: mc_init_comctl32() failed.");
        return -1;
    }

    if(MC_ERR(!GetClassInfo(NULL, _T("ToolbarWindow32"), &wc))) {
        MC_TRACE_ERR("menubar_init_module: GetClassInfo() failed");
        return -1;
    }

    /* Remember needed values of standard toolbar window class */
    orig_toolbar_proc = wc.lpfnWndProc;
    extra_offset = wc.cbWndExtra;

    /* Create our subclass. */
    wc.lpfnWndProc = menubar_proc;
    wc.cbWndExtra += sizeof(menubar_t*);
    wc.style |= CS_GLOBALCLASS;
    wc.hInstance = NULL;
    wc.lpszClassName = menubar_wc;
    if(MC_ERR(!RegisterClass(&wc))) {
        MC_TRACE_ERR("menubar_init_module: RegisterClass() failed");
        return -1;
    }

    mc_mutex_init(&menubar_ht_mutex);

    return 0;
}

void
menubar_fini_module(void)
{
    mc_mutex_fini(&menubar_ht_mutex);
    UnregisterClass(menubar_wc, NULL);
}


/**************************
 *** Exported functions ***
 **************************/

BOOL MCTRL_API
mcIsMenubarMessage(HWND hwndMenubar, LPMSG lpMsg)
{
    menubar_t* mb = (menubar_t*) GetWindowLongPtr(hwndMenubar, extra_offset);

    switch(lpMsg->message) {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            /* Handle <F10> or <ALT> */
            if(active_menubar != NULL)
                break;
            if((lpMsg->wParam == VK_MENU || (lpMsg->wParam == VK_F10 && !(lpMsg->lParam & 0x20000000)))  &&
               !(GetKeyState(VK_SHIFT) & 0x8000))
            {
                if(lpMsg->wParam == VK_MENU)
                    menubar_update_ui_state(mb, TRUE);
                if(activate_with_f10 == NULL)
                    activate_with_f10 = mb;
                return TRUE;
            }
            break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
            /* Handle <F10> or <ALT> */
            if(active_menubar != NULL)
                break;
            if((lpMsg->wParam == VK_MENU || (lpMsg->wParam == VK_F10 && !(lpMsg->lParam & 0x20000000)))  &&
               !(GetKeyState(VK_SHIFT) & 0x8000))
            {
                if(mb == activate_with_f10) {
                    SetFocus(hwndMenubar);
                    MENUBAR_SENDMSG(hwndMenubar, TB_SETHOTITEM, 0, 0);
                    menubar_update_ui_state(mb, TRUE);
                }
                activate_with_f10 = NULL;
                return TRUE;
            }
            break;

        case WM_SYSCHAR:
        case WM_CHAR:
            /* Handle hot keys (<ALT> + something) */
            if(lpMsg->wParam != VK_MENU  &&  (lpMsg->lParam & 0x20000000)) {
                UINT item;
                if(MENUBAR_SENDMSG(hwndMenubar, TB_MAPACCELERATOR, lpMsg->wParam, &item) != 0) {
                    menubar_update_ui_state(mb, TRUE);
                    menubar_dropdown(mb, item, TRUE);
                    return TRUE;
                }
            }
            break;
    }

    return FALSE;
}

BOOL MCTRL_API
mcMenubar_HandleRebarChevronPushed(HWND hwndMenubar,
                                   NMREBARCHEVRON* lpRebarChevron)
{
    REBARBANDINFO band_info;
    menubar_t* mb;
    RECT rect;
    HMENU menu;
    MENUITEMINFO mii;
    TCHAR buffer[MENUBAR_ITEM_LABEL_MAXSIZE];
    int i, n;
    TPMPARAMS params;

    /* Verify lpRebarChevron is from notification we assume. */
    if(MC_ERR(lpRebarChevron->hdr.code != RBN_CHEVRONPUSHED)) {
        MC_TRACE("mcMenubar_HandleRebarChevronPushed: Not RBN_CHEVRONPUSHED");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* Verify/get the menubar handle */
    band_info.cbSize = REBARBANDINFO_V3_SIZE;
    band_info.fMask = RBBIM_CHILD;
    MC_SEND(lpRebarChevron->hdr.hwndFrom, RB_GETBANDINFO, lpRebarChevron->uBand, &band_info);
    if(hwndMenubar != NULL) {
        if(MC_ERR(hwndMenubar != band_info.hwndChild)) {
            MC_TRACE("mcMenubar_HandleRebarChevronPushed: "
                     "Notification not about band with the specified menubar.");
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    } else {
        hwndMenubar = band_info.hwndChild;
        if(MC_ERR(hwndMenubar == NULL)) {
            MC_TRACE("mcMenubar_HandleRebarChevronPushed: "
                     "The band does not host any child window");
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    /* Create popup menu for not completely visible menu items */
    mb = (menubar_t*) GetWindowLongPtr(hwndMenubar, extra_offset);
    GetClientRect(hwndMenubar, &rect);
    menu = CreatePopupMenu();
    if(MC_ERR(menu == NULL)) {
        MC_TRACE_ERR("mcMenubar_HandleRebarChevronPushed: CreatePopupMenu() failed.");
        return FALSE;
    }
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STRING | MIIM_STATE | MIIM_ID | MIIM_SUBMENU;
    mii.dwTypeData = buffer;
    mii.cch = MC_SIZEOF_ARRAY(buffer);
    n = MENUBAR_SENDMSG(hwndMenubar, TB_BUTTONCOUNT, 0, 0);
    for(i = n-1; i >= 0; i--) {
        RECT item_rect;

        MENUBAR_SENDMSG(hwndMenubar, TB_GETITEMRECT, i, &item_rect);
        if(item_rect.right < rect.right)
            break;

        mii.cch = MC_SIZEOF_ARRAY(buffer);
        GetMenuItemInfo(mb->menu, i, TRUE, &mii);
        InsertMenuItem(menu, 0, TRUE, &mii);
    }
    params.cbSize = sizeof(TPMPARAMS);
    mc_rect_copy(&params.rcExclude, &lpRebarChevron->rc);

    /* Run the menu */
    MapWindowPoints(lpRebarChevron->hdr.hwndFrom, NULL, (POINT*) &params.rcExclude, 2);
    TrackPopupMenuEx(menu,
            (mb->rtl ? TPM_LAYOUTRTL : 0) | TPM_LEFTBUTTON | TPM_VERTICAL,
            (mb->rtl ? params.rcExclude.right : params.rcExclude.left),
            params.rcExclude.bottom, mb->win, &params);

    /* Destroy the popup menu. Note submenus have to survive as they are shared
     * with the menubar itself. */
    n = GetMenuItemCount(menu);
    for(i = 0; i < n; i++)
        RemoveMenu(menu, 0, MF_BYPOSITION);
    DestroyMenu(menu);

    return TRUE;
}
