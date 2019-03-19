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

#include "button.h"
#include "theme.h"


/* Uncomment this to have more verbose traces about MC_BUTTON control. */
/*#define BUTTON_DEBUG     1*/

#ifdef BUTTON_DEBUG
    #define BUTTON_TRACE        MC_TRACE
#else
    #define BUTTON_TRACE        MC_NOOP
#endif



static const TCHAR button_wc[] = MC_WC_BUTTON;  /* Window class name */
static const WCHAR button_tc[] = L"BUTTON";     /* Theming identifier */
static int extra_offset;
static WNDPROC orig_button_proc = NULL;


#define DROPDOWN_W     16


typedef struct button_tag button_t;
struct button_tag {
    HTHEME theme;
    DWORD style              : 28;
    DWORD is_dropdown_pushed :  1;
    DWORD hide_accel         :  1;
    DWORD hide_focus         :  1;
    DWORD no_redraw          :  1;
};


static HRGN
get_clip(HDC dc)
{
    /* Create dummy region as we need a valid region handle beforehand. */
    HRGN rgn = CreateRectRgn(0, 0, 1, 1);

    if(GetClipRgn(dc, rgn) != 1) {
        /* GetClipRgn() failed. This should always mean there is no
         * clipping region applied at the moment. Delete the dummy region. */
        DeleteObject(rgn);
        return NULL;
    }

    return rgn;
}

static HBRUSH
button_send_ctlcolorbtn(HWND win, HDC dc)
{
    HBRUSH brush;
    HWND parent = GetAncestor(win, GA_PARENT);

    if(parent == NULL)
        parent = win;
    brush = (HBRUSH) MC_SEND(parent, WM_CTLCOLORBTN, dc, win);
    if(MC_ERR(brush == NULL)) {
        /* The parent window procedure does not handle WM_CTLCOLORBTN
         * correctly. Maybe it forgot to call DefWindowProc?
         *
         * Wine-1.1.27/dlls/user32/button.c does this workaround, so
         * probably they have good reason to do so. Perhaps they noticed
         * Microsoft standard controls do that too so lets try to be
         * consistent. */
        MC_TRACE("button_send_ctlcolorbtn: Parent window does not handle "
                 "WM_CTLCOLORBTN correctly.");
        brush = (HBRUSH) DefWindowProc(parent, WM_CTLCOLORBTN, (WPARAM)dc, (LPARAM)win);
    }
    return brush;
}

static void
button_paint_icon(HWND win, button_t* button, HDC dc)
{
    HICON icon;
    RECT rect;
    RECT content;
    int state;
    SIZE size;
    UINT flags;
    HFONT font, old_font;
    int old_bk_mode;
    COLORREF old_text_color;
    HRGN old_clip;

    /* When theming is not used, we keep all the work on COMCTL32 button
     * implementation. */
    MC_ASSERT(button->theme != NULL);

    GetClientRect(win, &rect);

    icon = (HICON) MC_SEND(win, BM_GETIMAGE, IMAGE_ICON, 0);
    font = (HFONT) MC_SEND(win, WM_GETFONT, 0, 0);
    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);

    old_font = SelectObject(dc, font);
    old_bk_mode = GetBkMode(dc);
    old_text_color = GetTextColor(dc);
    old_clip = get_clip(dc);

    /* Draw background */
    if(button->style & WS_DISABLED) {
        state = PBS_DISABLED;
    } else {
        LRESULT s = MC_SEND(win, BM_GETSTATE, 0, 0);
        if(s & BST_PUSHED)
            state = PBS_PRESSED;
        else if(s & BST_HOT)
            state = PBS_HOT;
        else if(button->style & BS_DEFPUSHBUTTON)
            state = PBS_DEFAULTED;
        else
            state = PBS_NORMAL;
    }
    if(mcIsThemeBackgroundPartiallyTransparent(button->theme, BP_PUSHBUTTON, state))
        mcDrawThemeParentBackground(win, dc, &rect);
    mcDrawThemeBackground(button->theme, dc, BP_PUSHBUTTON, state, &rect, &rect);

    /* Get content rectangle of the button and clip DC to it */
    mcGetThemeBackgroundContentRect(button->theme, dc, BP_PUSHBUTTON, state, &rect, &content);
    IntersectClipRect(dc, content.left, content.top, content.right, content.bottom);

    /* Draw focus rectangle */
    if(MC_SEND(win, BM_GETSTATE, 0, 0) & BST_FOCUS) {
        if(!button->hide_focus)
            DrawFocusRect(dc, &content);
    }

    /* Draw the contents (i.e. the icon) */
    if(icon != NULL) {
        mc_icon_size(icon, &size);
        flags = DST_ICON;
        if(button->style & WS_DISABLED)
            flags |= DSS_DISABLED;
        DrawState(dc, NULL, NULL, (LPARAM) icon, 0, (rect.right + rect.left - size.cx) / 2,
                  (rect.bottom + rect.top - size.cy) / 2, size.cx, size.cy, flags);
    }

    /* Revert DC into original state */
    SelectObject(dc, old_font);
    SetBkMode(dc, old_bk_mode);
    SetTextColor(dc, old_text_color);
    SelectObject(dc, old_clip);
}

static void
button_paint_split(HWND win, button_t* button, HDC dc)
{
    RECT rect;
    RECT rect_left, rect_right;
    int state_left, state_right;
    int text_offset = 0;
    HFONT font, old_font;
    int old_bk_mode;
    COLORREF old_text_color;
    HRGN old_clip;
    HICON glyph;
    int width_right = DROPDOWN_W;

    glyph = ImageList_GetIcon(mc_bmp_glyphs, MC_BMP_GLYPH_MORE_OPTIONS, ILD_TRANSPARENT);
    GetClientRect(win, &rect);

    font = (HFONT) MC_SEND(win, WM_GETFONT, 0, 0);
    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);

    old_font = SelectObject(dc, font);
    old_bk_mode = GetBkMode(dc);
    old_text_color = GetTextColor(dc);
    old_clip = get_clip(dc);

    /* Draw what's common for left and right parts background. */
    if(!button->theme  &&  (button->style & BS_DEFPUSHBUTTON)) {
        SelectObject(dc, GetSysColorBrush(COLOR_WINDOWFRAME));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        mc_rect_inflate(&rect, -1, -1);
        width_right--;
    }

    /* Setup subrectangles (mainpart 1 and push-down part 2) */
    mc_rect_copy(&rect_left, &rect);
    rect_left.right -= width_right;
    mc_rect_copy(&rect_right, &rect);
    rect_right.left = rect_left.right;

    /* Draw background. */
    if(button->theme) {
        UINT transparent;
        RECT tmp;

        /* Determine styles for left and right parts */
        if(button->style & WS_DISABLED) {
            state_left = state_right = PBS_DISABLED;
        } else {
            LRESULT state;

            state = MC_SEND(win, BM_GETSTATE, 0, 0);
            if(state & MC_BST_DROPDOWNPUSHED) {
                state_left = PBS_NORMAL;
                state_right = PBS_PRESSED;
            } else {
                if(state & BST_PUSHED)
                    state_left = state_right = PBS_PRESSED;
                else if(state & BST_HOT)
                    state_left = state_right = PBS_HOT;
                else if(button->style & BS_DEFPUSHBUTTON)
                    state_left = state_right = PBS_DEFAULTED;
                else
                    state_left = state_right = PBS_NORMAL;
            }
        }

        /* Handle (semi-)transparent themes. */
        transparent = 0;
        if(mcIsThemeBackgroundPartiallyTransparent(button->theme,
                    BP_PUSHBUTTON, state_left))
            transparent |= 0x1;
        if(mcIsThemeBackgroundPartiallyTransparent(button->theme,
                    BP_PUSHBUTTON, state_right))
            transparent |= 0x2;
        switch(transparent) {
            case 0x1:
                mcDrawThemeParentBackground(win, dc, &rect_left);
                break;
            case 0x2:
                mcDrawThemeParentBackground(win, dc, &rect_right);
                break;
            case 0x3:
                mcDrawThemeParentBackground(win, dc, &rect);
                break;
        }

        /* Draw backgrond. */
        mcDrawThemeBackground(button->theme, dc, BP_PUSHBUTTON, state_left, &rect, &rect_left);
        mcDrawThemeBackground(button->theme, dc, BP_PUSHBUTTON, state_right, &rect, &rect_right);

        /* Deflate both rects to content rects only */
        mcGetThemeBackgroundContentRect(button->theme, dc, BP_PUSHBUTTON, state_left, &rect_left, &tmp);
        rect_left.left = tmp.left;
        rect_left.top = tmp.top;
        rect_left.bottom = tmp.bottom;
        mcGetThemeBackgroundContentRect(button->theme, dc, BP_PUSHBUTTON, state_right, &rect_right, &tmp);
        rect_right.top = tmp.top;
        rect_right.right = tmp.right;
        rect_right.bottom = tmp.bottom;

        /* Draw delimiter of left and right parts. */
        rect_right.top += 1;
        rect_right.bottom -= 1;
        mcDrawThemeEdge(button->theme, dc, BP_PUSHBUTTON, state_right, &rect_right, BDR_SUNKEN, BF_LEFT, NULL);
        rect_right.left = tmp.left;
    } else {
        /* Determine styles for left and right parts */
        if(button->style & WS_DISABLED) {
            state_left = state_right = DFCS_INACTIVE;
        } else {
            LRESULT s = MC_SEND(win, BM_GETSTATE, 0, 0);
            if(s & MC_BST_DROPDOWNPUSHED) {
                state_left = 0;
                state_right = DFCS_PUSHED;
            } else {
                if(s & BST_PUSHED) {
                    state_left = state_right = DFCS_PUSHED;
                } else {
                    state_left = state_right = 0;
                }
            }
        }

        button_send_ctlcolorbtn(win, dc);

        /* Draw control edges */
        IntersectClipRect(dc, rect_left.left, rect_left.top, rect_left.right, rect_left.bottom);
        DrawFrameControl(dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH | state_left);
        SelectClipRgn(dc, NULL);
        IntersectClipRect(dc, rect_right.left, rect_right.top, rect_right.right, rect_right.bottom);
        DrawFrameControl(dc, &rect, DFC_BUTTON, DFCS_BUTTONPUSH | state_right);

        /* Parts which are pushed, should have the contents moved a bit */
        if(state_left == DFCS_PUSHED)
            mc_rect_offset(&rect_left, 1, 1);
        if(state_right == DFCS_PUSHED)
            mc_rect_offset(&rect_right, 1, 1);

        /* Draw delimiter */
        if(state_left == state_right) {
            DrawEdge(dc, &rect_right, BDR_SUNKENOUTER | BDR_RAISEDINNER, BF_LEFT | BF_SOFT);
        } else {
            rect_right.left--;
            DrawEdge(dc, &rect_right, BDR_SUNKENOUTER, BF_LEFT | BF_SOFT);
            rect_right.left++;
        }

        /* Adjust for the outer control edges */
        mc_rect_inflate(&rect_left, 0, -2);
        rect_left.left += 2;
        mc_rect_inflate(&rect_right, -2, -2);
    }

    /* Draw focus rectangle. */
    if((MC_SEND(win, BM_GETSTATE, 0, 0) & BST_FOCUS) && !button->hide_focus) {
        SelectClipRgn(dc, NULL);
        if(button->theme) {
            mc_rect_set(&rect, rect_left.left, rect_left.top,
                        rect_right.right - DROPDOWN_W, rect_right.bottom);
            DrawFocusRect(dc, &rect);
        } else {
            mc_rect_inflate(&rect_left, -1, -2);
            DrawFocusRect(dc, &rect_left);
            mc_rect_inflate(&rect_left, -1, -1);
        }
    }

    /* Draw glyph into the right part */
    SelectClipRgn(dc, NULL);
    IntersectClipRect(dc, rect_right.left, rect_right.top,
                          rect_right.right, rect_right.bottom);
    DrawIconEx(dc, (rect_right.right + rect_right.left - MC_BMP_GLYPH_W) / 2,
                   (rect_right.bottom + rect_right.top - MC_BMP_GLYPH_H) / 2,
                   glyph, MC_BMP_GLYPH_W, MC_BMP_GLYPH_H, 0, NULL, DI_NORMAL);

    /* Draw left part contents */
    SelectClipRgn(dc, NULL);
    IntersectClipRect(dc, rect_left.left, rect_left.top,
                          rect_left.right, rect_left.bottom);
    if(button->style & BS_ICON) {
        /* Paint (BS_SPLITBUTTON | BS_ICON). Note that this is used even on
         * Vista, as according to some my testing this style combination
         * is not supported there... */
        HICON icon;

        icon = (HICON) MC_SEND(win, BM_GETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) 0);
        if(icon != NULL) {
            SIZE size;
            UINT flags;

            mc_icon_size(icon, &size);

            flags = DST_ICON;
            if(button->style & WS_DISABLED)
                flags |= DSS_DISABLED;

            DrawState(dc, NULL, NULL, (LPARAM) icon, 0,
                      (rect_left.right + rect_left.left - size.cx) / 2,
                      (rect_left.bottom + rect_left.top - size.cy) / 2,
                      size.cx, size.cy, flags);
        }
    } else {
        /* Paint text label */
        TCHAR buffer[256];
        int n;
        UINT flags = 0;

        /* Setup flags for TextOut/mcDrawThemeText */
        switch(button->style & (BS_LEFT | BS_CENTER | BS_RIGHT)) {
            case BS_LEFT:
                flags |= DT_LEFT;
                break;
            case BS_RIGHT:
                flags |= DT_RIGHT;
                break;
            default:
                if(GetWindowLong(win, GWL_EXSTYLE) & WS_EX_RIGHT)
                    flags |= DT_RIGHT;
                else
                    flags |= DT_CENTER;
                break;
        }
        switch(button->style & (BS_TOP | BS_VCENTER | BS_BOTTOM)) {
            case BS_TOP:
                flags |= DT_TOP;
                break;
            case BS_BOTTOM:
                flags |= DT_BOTTOM;
                break;
            default:
                flags |= DT_VCENTER;
                break;
        }
        if(button->style & BS_MULTILINE)
            flags |= DT_WORDBREAK;
        else
            flags |= DT_SINGLELINE;

        if(button->hide_accel)
            flags |= DT_HIDEPREFIX;

        n = MC_SEND(win, WM_GETTEXT, MC_SIZEOF_ARRAY(buffer), buffer);

        if(button->theme) {
            mcDrawThemeText(button->theme, dc, BP_PUSHBUTTON,
                        state_left, buffer, n, flags, 0, &rect_left);
        } else {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            mc_rect_offset(&rect_left, text_offset, text_offset);
            DrawText(dc, buffer, n, &rect_left, flags);
        }
    }

    SelectObject(dc, old_font);
    SetBkMode(dc, old_bk_mode);
    SetTextColor(dc, old_text_color);
    SelectObject(dc, old_clip);
}

static LRESULT
button_update_ui_state(HWND win, button_t* button, WPARAM wp, LPARAM lp)
{
    LRESULT ret;
    DWORD flags;

    ret = CallWindowProc(orig_button_proc, win, WM_UPDATEUISTATE, wp, lp);
    flags = MC_SEND(win, WM_QUERYUISTATE, 0, 0);
    button->hide_focus = (flags & UISF_HIDEFOCUS) ? 1 : 0;
    button->hide_accel = (flags & UISF_HIDEACCEL) ? 1 : 0;
    if(!button->no_redraw)
        InvalidateRect(win, NULL, FALSE);
    return ret;
}

static BOOL
button_is_fake_split(button_t* button)
{
    DWORD type;

    type = (button->style & BS_TYPEMASK);
    if(type != MC_BS_SPLITBUTTON  &&  type != MC_BS_DEFSPLITBUTTON)
        return FALSE;

    if(mc_comctl32_version < MC_DLL_VER(6, 0))
        return TRUE;
    if(mc_win_version < MC_WIN_VISTA)
        return TRUE;
    if(mc_win_version < MC_WIN_7  &&  (button->style & BS_ICON))
        return TRUE;

    return FALSE;
}

static BOOL
button_is_fake_icon(button_t* button)
{
    if(!(button->style & BS_ICON))
        return FALSE;

    /* Win XP do not paint BS_ICON with themes even when themes enabled. */
    return (mc_win_version < MC_WIN_VISTA  &&  button->theme != NULL);
}

static LRESULT CALLBACK
button_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    button_t* button = (button_t*) GetWindowLongPtr(win, extra_offset);

    /* Window procedure for our subclassed BUTTON does some logic if
     * either [1] the control is split button and system does not support it
     *            (i.e. Windows is older then Vista).
     *     or [2] the control is BS_ICON and theming is in use, as std.
     *            control draws in the old unthemed style this kind of button.
     * In all other cases all messages are just forwarded to the standard
     * Microsoft button procedure.
     */

    switch(msg) {
        case WM_PAINT:
        case WM_PRINTCLIENT:
        {
            BOOL fake_split;
            BOOL fake_icon;
            PAINTSTRUCT ps;
            HDC dc;

            fake_split = button_is_fake_split(button);
            fake_icon = button_is_fake_icon(button);
            if(!fake_split  &&  !fake_icon) {
                /* Keep it on original proc */
                break;
            }

            if(msg == WM_PAINT)
                dc = BeginPaint(win, &ps);
            else
                dc = (HDC) wp;

            if(!button->no_redraw) {
                if(fake_split)
                    button_paint_split(win, button, dc);
                else
                    button_paint_icon(win, button, dc);
            }

            if(msg == WM_PAINT)
                EndPaint(win, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
            if(button_is_fake_split(button)) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                RECT rect;

                SetFocus(win);
                GetClientRect(win, &rect);
                rect.left = rect.right - DROPDOWN_W;

                if(mc_rect_contains_pt(&rect, &pt)) {
                    /* Handle the click in the drop-down part */
                    MC_NMBCDROPDOWN notify;

                    button->is_dropdown_pushed = 1;
                    InvalidateRect(win, &rect, TRUE);

                    notify.hdr.hwndFrom = win;
                    notify.hdr.idFrom = GetWindowLong(win, GWL_ID);
                    notify.hdr.code = MC_BCN_DROPDOWN;
                    mc_rect_copy(&notify.rcButton, &rect);
                    MC_SEND(GetAncestor(win, GA_PARENT), WM_NOTIFY, notify.hdr.idFrom, &notify);
                    /* We unpush immediately after the parent handles the
                     * notification. Usually it takes some time - parent
                     * typically shows some popup-menu or other stuff
                     * which includes a modal eventloop and/or mouse capture.
                     */
                    button->is_dropdown_pushed = 0;
                    InvalidateRect(win, NULL, TRUE);
                    return 0;
                }
            }
            break;

        case WM_LBUTTONDBLCLK:
            if(button_is_fake_split(button)) {
                RECT rect;

                GetClientRect(win, &rect);
                rect.left = rect.right - DROPDOWN_W;
                if(mc_rect_contains_pos(&rect, lp)) {
                    /* We ignore duble-click in the drop-down part. */
                    return 0;
                }
            }
            break;

        case WM_GETDLGCODE:
            /* Handling this message allows the dialogs to set the button
             * as default, as it is done for normal push buttons. Unfortunately
             * it causes other problems. See the comment in WM_STYLECHANGING.
             */
            if(button_is_fake_split(button)) {
                if((button->style & BS_TYPEMASK) == MC_BS_DEFSPLITBUTTON) {
                    BUTTON_TRACE("button_proc(WM_GETDLGCODE): -> DLGC_DEFPUSHBUTTON");
                    return DLGC_BUTTON | DLGC_DEFPUSHBUTTON;
                }
                if((button->style & BS_TYPEMASK) == MC_BS_SPLITBUTTON) {
                    BUTTON_TRACE("button_proc(WM_GETDLGCODE): -> DLGC_UNDEFPUSHBUTTON");
                    return DLGC_BUTTON | DLGC_UNDEFPUSHBUTTON;
                }
            }
            break;

        case BM_SETSTATE:
            if(button_is_fake_split(button)) {
                CallWindowProc(orig_button_proc, win, msg, wp, lp);
                /* USER32.DLL does some painting in BM_SETSTATE. Repaint
                 * the split button. */
                InvalidateRect(win, NULL, TRUE);
                return 0;
            }
            break;

        case BM_GETSTATE:
            if(button_is_fake_split(button)) {
                DWORD s = CallWindowProc(orig_button_proc, win, msg, wp, lp);
                if(button->is_dropdown_pushed)
                    s |= MC_BST_DROPDOWNPUSHED;
                return s;
            }
            break;

        case BM_SETSTYLE:
            if(button_is_fake_split(button)) {
                BUTTON_TRACE("button_proc(BM_SETSTYLE): split style fixup");
                wp &= ~(BS_TYPEMASK & ~BS_DEFPUSHBUTTON);
                wp |= MC_BS_SPLITBUTTON;
                CallWindowProc(orig_button_proc, win, msg, wp, lp);
                button->style = GetWindowLong(win, GWL_STYLE);
                return 0;
            }
            break;

        case WM_SETREDRAW:
            button->no_redraw = !wp;
            break;

        case WM_STYLECHANGING:
            if(button_is_fake_split(button)) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                if((ss->styleOld & BS_TYPEMASK) == MC_BS_SPLITBUTTON  ||
                   (ss->styleOld & BS_TYPEMASK) == MC_BS_DEFSPLITBUTTON) {
                    /* On older system which do not support split buttons
                     * (i.e. 2000, XP), the dialog procedure does not handle
                     * moving the default state correctly: It accidentaly
                     * removes our split button button type. Hence we perform
                     * this fixup.
                     *
                     * Unfortunately this means that app. cannot freely change
                     * BS_SPLITBUTTON to BS_BUTTON with SetWindowLong(GWL_STYLE)
                     * as this will prevent that too... */
                    BUTTON_TRACE("button_proc(WM_STYLECHANGING): split style fixup");
                    ss->styleNew &= ~(BS_TYPEMASK & ~BS_DEFPUSHBUTTON);
                    ss->styleNew |= MC_BS_SPLITBUTTON;
                }
            }
            break;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                button->style = ss->styleNew;
            }
            break;

        case WM_THEMECHANGED:
            if(button->theme)
                mcCloseThemeData(button->theme);
            button->theme = mcOpenThemeData(win, button_tc);
            InvalidateRect(win, NULL, FALSE);
            break;

        case WM_SYSCOLORCHANGE:
            InvalidateRect(win, NULL, FALSE);
            break;

        case WM_UPDATEUISTATE:
            return button_update_ui_state(win, button, wp, lp);

        case WM_NCCREATE:
            if(MC_ERR(!CallWindowProc(orig_button_proc, win, WM_NCCREATE, wp, lp))) {
                MC_TRACE_ERR("button_proc(WM_NCCREATE): orig_button_proc() failed");
                return FALSE;
            }
            button = (button_t*) malloc(sizeof(button_t));
            if(MC_ERR(button == NULL)) {
                MC_TRACE("button_proc(WM_CREATE): malloc() failed.");
                return FALSE;
            }
            memset(button, 0, sizeof(button_t));
            button->style = ((CREATESTRUCT*)lp)->style;
            SetWindowLongPtr(win, extra_offset, (LONG_PTR) button);
            return TRUE;

        case WM_CREATE:
            if(MC_ERR(CallWindowProc(orig_button_proc, win, WM_CREATE, wp, lp) != 0)) {
                MC_TRACE_ERR("button_proc(WM_CREATE): orig_button_proc() failed");
                return -1;
            }
            button->theme = mcOpenThemeData(win, button_tc);

            {
                WORD ui_state = MC_SEND(win, WM_QUERYUISTATE, 0, 0);
                button->hide_focus = (ui_state & UISF_HIDEFOCUS) ? 1 : 0;
                button->hide_accel = (ui_state & UISF_HIDEACCEL) ? 1 : 0;
            }
            return 0;

        case WM_DESTROY:
            if(button->theme) {
                mcCloseThemeData(button->theme);
                button->theme = NULL;
            }
            break;

        case WM_NCDESTROY:
            if(button)
                free(button);
            break;
    }

    return CallWindowProc(orig_button_proc, win, msg, wp, lp);
}

int
button_init_module(void)
{
    WNDCLASS wc;

    if(MC_ERR(!GetClassInfo(NULL, _T("BUTTON"), &wc))) {
        MC_TRACE_ERR("button_init_module: GetClassInfo() failed");
        return -1;
    }

    /* Remember needed values of standard "button" window class */
    orig_button_proc = wc.lpfnWndProc;
    extra_offset = wc.cbWndExtra;

    /* On Win7 we do not need to emulate anzthing, so we are just alias class
     * of the standard button. */
    if(mc_win_version < MC_WIN_7  ||  mc_comctl32_version < MC_DLL_VER(6, 0)) {
        wc.lpfnWndProc = button_proc;
        wc.cbWndExtra += sizeof(button_t*);
    }
    wc.style |= CS_GLOBALCLASS;
    wc.hInstance = NULL;
    wc.lpszClassName = button_wc;
    if(MC_ERR(!RegisterClass(&wc))) {
        MC_TRACE_ERR("button_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
button_fini_module(void)
{
    UnregisterClass(button_wc, NULL);
}
