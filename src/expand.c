/*
 * Copyright (c) 2012 Martin Mitas
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

#include "expand.h"
#include "theme.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define EXPAND_DEBUG     1*/

#ifdef EXPAND_DEBUG
    #define EXPAND_TRACE       MC_TRACE
#else
    #define EXPAND_TRACE(...)  do {} while(0)
#endif


#define GLYPH_TEXT_MARGIN     5
#define FOCUS_INFLATE_H       3
#define FOCUS_INFLATE_V       1



static const TCHAR expand_wc[] = MC_WC_EXPAND;    /* Window class name */
static const WCHAR expand_tc[] = L"BUTTON";       /* Theming identifier */

static HBITMAP expand_glyphs[3];


typedef enum expand_state_tag expand_state_t;
enum expand_state_tag {
    COLLAPSED_NORMAL    = 0,
    COLLAPSED_HOT       = 1,
    COLLAPSED_PRESSED   = 2,
    EXPANDED_NORMAL     = 3,
    EXPANDED_HOT        = 4,
    EXPANDED_PRESSED    = 5
};


/* expand_t::state bits */
#define STATE_HOT              0x1
#define STATE_PRESSED          0x2
#define STATE_EXPANDED         0x4

typedef struct expand_tag expand_t;
struct expand_tag {
    HWND win;
    HWND notify_win;
    HTHEME theme;
    HFONT font;
    WORD collapsed_w;
    WORD collapsed_h;
    WORD expanded_w;
    WORD expanded_h;
    DWORD style          : 16;
    DWORD no_redraw      : 1;
    DWORD hide_accel     : 1;
    DWORD hide_focus     : 1;
    DWORD hot_track      : 1;
    DWORD mouse_captured : 1;
    DWORD space_pressed  : 1;
    DWORD state          : 3;
    DWORD old_state      : 3;  /* for painting state transitions */
};



static TCHAR*
expand_text(expand_t* expand)
{
    UINT ids;

    if(expand->state & STATE_EXPANDED)
        ids = IDS_EXPAND_FEWERDETAILS;
    else
        ids = IDS_EXPAND_MOREDETAILS;

    return mc_str_load(ids);
}


typedef struct expand_layout_tag expand_layout_t;
struct expand_layout_tag {
    HBITMAP glyph_bmp;
    RECT glyph_rect;
    RECT text_rect;
    RECT active_rect;
};

static void
expand_calc_layout(expand_t* expand, HDC dc, expand_layout_t* layout)
{
    RECT rect;
    BOOL right_align;
    HFONT old_font;
    TCHAR* str;
    SIZE extents;
    int glyph_size;

    GetClientRect(expand->win, &rect);
    right_align = (GetWindowLong(expand->win, GWL_EXSTYLE) & WS_EX_RIGHT) ? TRUE : FALSE;

    old_font = SelectObject(dc, expand->font);
    str = expand_text(expand);
    GetTextExtentPoint32(dc, str, _tcslen(str), &extents);
    SelectObject(dc, old_font);

    if(rect.bottom < 24) {
        layout->glyph_bmp = expand_glyphs[0];
        glyph_size = 19;
    } else if(rect.bottom < 29) {
        layout->glyph_bmp = expand_glyphs[1];
        glyph_size = 24;
    } else {
        layout->glyph_bmp = expand_glyphs[2];
        glyph_size = 29;
    }

    layout->glyph_rect.left = (right_align ? rect.right - glyph_size : 0);
    layout->glyph_rect.top =  (rect.bottom - glyph_size + 1) / 2;
    layout->glyph_rect.right = layout->glyph_rect.left + glyph_size;
    layout->glyph_rect.bottom = layout->glyph_rect.top + glyph_size;

    layout->text_rect.left = (right_align ? layout->glyph_rect.left - GLYPH_TEXT_MARGIN - extents.cx
                                          : layout->glyph_rect.right + GLYPH_TEXT_MARGIN);
    layout->text_rect.top = (rect.bottom - extents.cy + 1) / 2;
    layout->text_rect.right = layout->text_rect.left + extents.cx;
    layout->text_rect.bottom = layout->text_rect.top + extents.cy;

    layout->active_rect.left = MC_MIN(layout->glyph_rect.left, layout->text_rect.left);
    layout->active_rect.top = MC_MIN(layout->glyph_rect.top, layout->text_rect.top);
    layout->active_rect.right = MC_MAX(layout->glyph_rect.right, layout->text_rect.right);
    layout->active_rect.bottom = MC_MAX(layout->glyph_rect.bottom, layout->text_rect.bottom);
}

static void
expand_paint_state(expand_t* expand, DWORD state, HDC dc, RECT* dirty, BOOL erase)
{
    expand_layout_t layout;

    /* Paint background */
    if(erase)
        theme_DrawThemeParentBackground(expand->win, dc, dirty);

    /* According to MSDN guidelines, the controls of such nature as this one,
     * it should never be disabled, but removed instead. I.e. application
     * should rather hide the control then to disable it. Anyway we refuse
     * to paint it.
     *
     * Quote: "Remove (don't disable) progressive disclosure controls that
     * don't apply in the current context."
     *
     * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa511487.aspx
     * for more info.
     */
    if(!IsWindowEnabled(expand->win)) {
        MC_TRACE("expand_paint_state: Control disabled, do not paint at all.");
        return;
    }

    expand_calc_layout(expand, dc, &layout);

    /* Paint glyph */
    {
        int glyph_size;
        int glyph_index;
        HDC glyph_dc;
        const BLENDFUNCTION blend_func = { AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };

        glyph_size = mc_height(&layout.glyph_rect);
        glyph_index = (state & STATE_EXPANDED) ? 3 : 0;
        if(state & STATE_PRESSED)
            glyph_index += 2;
        else if(state & STATE_HOT)
            glyph_index += 1;

        glyph_dc = CreateCompatibleDC(dc);
        SelectObject(glyph_dc, layout.glyph_bmp);
        GdiAlphaBlend(dc, layout.glyph_rect.left, layout.glyph_rect.top, glyph_size, glyph_size,
                      glyph_dc, 0, glyph_size * glyph_index, glyph_size, glyph_size,
                      blend_func);
        DeleteDC(glyph_dc);
    }

    /* Paint text */
    {
        UINT format = DT_SINGLELINE;
        HFONT old_font;

        if(expand->hide_accel)
            format |= DT_HIDEPREFIX;
        old_font = SelectObject(dc, expand->font);
        DrawText(dc, expand_text(expand), -1, &layout.text_rect, format);
        SelectObject(dc, old_font);
    }

    /* Paint focus rect */
    if(!expand->hide_focus  &&  expand->win == GetFocus()) {
        mc_rect_inflate(&layout.text_rect, FOCUS_INFLATE_H, FOCUS_INFLATE_V);
        DrawFocusRect(dc, &layout.text_rect);
    }
}

static void
expand_do_paint(void* control, HDC dc, RECT* dirty, BOOL erase)
{
    expand_t* expand = (expand_t*) control;
    expand_paint_state(expand, expand->state, dc, dirty, erase);
}

static inline int
expand_theme_state(DWORD state)
{
    if(state & STATE_PRESSED)   return PBS_PRESSED;
    else if(state & STATE_HOT)  return PBS_HOT;
    else                        return PBS_NORMAL;
}

static void
expand_paint(expand_t* expand)
{
    PAINTSTRUCT ps;
    int old_theme_state;
    int new_theme_state;

    BeginPaint(expand->win, &ps);

    /* Handle transition animation if already in progress */
    if(theme_BufferedPaintRenderAnimation(expand->win, ps.hdc)) {
        EXPAND_TRACE("expand_paint: Transition in progress");
        goto done;
    }

    if(expand->no_redraw)
        goto done;

    /* If painting because of state change, start new transition animation */
    old_theme_state = expand_theme_state(expand->old_state);
    new_theme_state = expand_theme_state(expand->state);
    expand->old_state = expand->state;
    if(old_theme_state != new_theme_state) {
        HRESULT hr;
        DWORD duration;

        hr = theme_GetThemeTransitionDuration(expand->theme, BP_PUSHBUTTON,
                old_theme_state, new_theme_state, TMT_TRANSITIONDURATIONS,
                &duration);
        if(hr == S_OK  &&  duration > 0) {
            RECT rect;
            BP_ANIMATIONPARAMS params = {0};
            HANIMATIONBUFFER buf;
            HDC old_dc, new_dc;

            GetClientRect(expand->win, &rect);

            params.cbSize = sizeof(BP_ANIMATIONPARAMS);
            params.style = BPAS_LINEAR;
            params.dwDuration = duration;

            buf = theme_BeginBufferedAnimation(expand->win, ps.hdc, &rect,
                    BPBF_COMPATIBLEBITMAP, NULL, &params, &old_dc, &new_dc);
            if(buf != NULL) {
                expand_paint_state(expand, expand->old_state, old_dc, &rect, TRUE);
                expand_paint_state(expand, expand->state, new_dc, &rect, TRUE);
                EXPAND_TRACE("expand_paint: Transition start (%lu ms)", duration);
                theme_EndBufferedAnimation(buf, TRUE);
                goto done;
            }
        }
    }

    /* Normal paint. We do not need double buffering without background erase. */
    if((expand->style & MC_EXS_DOUBLEBUFFER)  &&  ps.fErase)
        mc_doublebuffer(expand, &ps, expand_do_paint);
    else
        expand_do_paint(expand, ps.hdc, &ps.rcPaint, ps.fErase);

done:
    EndPaint(expand->win, &ps);
}

static BOOL
expand_is_mouse_in_active_rect(expand_t* expand, int x, int y)
{
    HDC dc;
    expand_layout_t layout;

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    expand_calc_layout(expand, dc, &layout);
    ReleaseDC(NULL, dc);

    return mc_rect_contains_xy(&layout.active_rect, x, y);
}

static void
expand_update_ui_state(expand_t* expand, WORD action, WORD mask)
{
    switch(action) {
        case UIS_CLEAR:
            if(mask & UISF_HIDEACCEL)
                expand->hide_accel = 0;
            if(mask & UISF_HIDEFOCUS)
                expand->hide_focus = 0;
            break;

        case UIS_SET:
            if(mask & UISF_HIDEACCEL)
                expand->hide_accel = 1;
            if(mask & UISF_HIDEFOCUS)
                expand->hide_focus = 1;
            break;

        case UIS_INITIALIZE:
            expand->hide_accel = (mask & UISF_HIDEACCEL) ? 1 : 0;
            expand->hide_focus = (mask & UISF_HIDEFOCUS) ? 1 : 0;
            break;
    }

    if(!expand->no_redraw)
        InvalidateRect(expand->win, NULL, TRUE);
}

static void
expand_set_state(expand_t* expand, DWORD state)
{
    if(expand->state == state)
        return;

    EXPAND_TRACE("expand_set_state: 0x%lx -> 0x%lx", expand->state, state);

    expand->old_state = expand->state;
    expand->state = state;

    theme_BufferedPaintStopAllAnimations(expand->win);

    if(!expand->no_redraw)
        InvalidateRect(expand->win, NULL, TRUE);
}

static void
expand_guess_size(expand_t* expand, SIZE* size)
{
    HFONT dlg_font;
    int dlg_padding;
    RECT dlg_rect;
    HWND child;
    LONG top = INT32_MAX;
    LONG bottom = 0;

    GetClientRect(expand->notify_win, &dlg_rect);
    MapWindowPoints(expand->notify_win, NULL, (POINT*)&dlg_rect, 2);

    /* We never attempt to change dialog width. */
    size->cx = mc_width(&dlg_rect);

    /* Find minimal top and maximal bottom coordinates from all children. */
    child = GetWindow(expand->notify_win, GW_CHILD);
    if(MC_UNLIKELY(child == NULL)) {
        /* The dialog has no children? Probably the best thing is to not
         * change its size at all. */
        MC_TRACE("expand_guess_size: How to guess size? No children.");
        size->cy = mc_height(&dlg_rect);
        return;
    }
    while(child != NULL) {
        RECT rect;
        GetWindowRect(child, &rect);
        if(rect.top - dlg_rect.top < top  &&  rect.top - dlg_rect.top > 0)
            top = rect.top - dlg_rect.top;
        if(rect.bottom - dlg_rect.top  &&  rect.bottom - dlg_rect.top > 0)
            bottom = rect.bottom - dlg_rect.top;
        child = GetWindow(child, GW_HWNDNEXT);
    }

    /* MSDN dialog layout guildlines say a dialog padding should be 7 DLUs.
     * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa511279.aspx
     *
     * However if application disrespect it and places some child into it, we
     * respect it.
     */
    dlg_font = (HFONT) SendMessage(expand->notify_win, WM_GETFONT, 0, 0);
    dlg_padding = mc_pixels_from_dlus(dlg_font, 7, TRUE);
    if(top < dlg_padding)
        dlg_padding = top;

    if(expand->state & STATE_EXPANDED) {
        size->cy = bottom + dlg_padding;
    } else {
        RECT self_rect;
        GetWindowRect(expand->win, &self_rect);
        size->cy = self_rect.bottom - dlg_rect.top + dlg_padding;
    }

    EXPAND_TRACE("expand_guess_size: guessing %s size %ld x %ld",
                 (expand->state & STATE_EXPANDED) ? "expanded" : "collapsed",
                 size->cx, size->cy);
}

static void
expand_resize_parent(expand_t* expand)
{
    SIZE size;
    RECT entire;
    RECT old_rect;
    RECT new_rect;
    HWND child;
    RECT r;

    /* Get the size we need to resize to */
    if(expand->state & STATE_EXPANDED) {
        size.cx = expand->expanded_w;
        size.cy = expand->expanded_h;
    } else {
        size.cx = expand->collapsed_w;
        size.cy = expand->collapsed_h;
    }

    /* If not set explicitly, try to guess */
    if(size.cx == 0  &&  size.cy == 0) {
        expand_guess_size(expand, &size);

        if(expand->style & MC_EXS_CACHESIZES) {
            if(expand->state & STATE_EXPANDED) {
                expand->expanded_w = size.cx;
                expand->expanded_h = size.cy;
            } else {
                expand->collapsed_w = size.cx;
                expand->collapsed_h = size.cy;
            }
        }
    }

    /* We need old (i.e. current) and new (i.e. desired) client rects to
     * analyze what children are (in)covered. We want these in screen
     * coords. */
    GetWindowRect(expand->notify_win, &entire);
    GetClientRect(expand->notify_win, &old_rect);
    MapWindowPoints(expand->notify_win, NULL, (POINT*)&old_rect, 2);
    mc_rect_set(&new_rect, old_rect.left, old_rect.top,
                           old_rect.left + size.cx, old_rect.top + size.cy);

    if(!(expand->style & MC_EXS_RESIZEENTIREWINDOW)) {
        /* Add "padding" of the non-lient area for the parent resize */
        size.cx += mc_width(&entire) - mc_width(&old_rect);
        size.cy += mc_height(&entire) - mc_height(&old_rect);
    } else {
        /* Compensate new client rect for the fact the size already includes
         * non-client area. */
        new_rect.right -= mc_width(&entire) - mc_width(&old_rect);
        new_rect.bottom -= mc_height(&entire) - mc_height(&old_rect);
    }


    /* Show/hide children (un)covered by the resize of parent */
#define OVERLAP(a,b)   (!(a.bottom <= b.top  ||  a.top >= b.bottom  ||   \
                          a.right <= b.left  ||  a.left >= b.right))
    for(child = GetWindow(expand->notify_win, GW_CHILD);
        child != NULL;
        child = GetWindow(child, GW_HWNDNEXT))
    {
        GetWindowRect(child, &r);

        if(expand->state & STATE_EXPANDED) {
            if(!OVERLAP(r, old_rect)  &&  OVERLAP(r, new_rect)) {
                EnableWindow(child, TRUE);
                ShowWindow(child, SW_SHOW);
            }
        } else {
            if(OVERLAP(r, old_rect)  &&  !OVERLAP(r, new_rect)) {
                ShowWindow(child, SW_HIDE);
                EnableWindow(child, FALSE);
            }
        }
    }
#undef OVERLAP

    /* Finally resize the parent */
    SetWindowPos(expand->notify_win, NULL, 0, 0, size.cx, size.cy,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

static expand_t*
expand_nccreate(HWND win, CREATESTRUCT* cs)
{
    expand_t* expand;

    expand = (expand_t*) malloc(sizeof(expand_t));
    if(MC_ERR(expand == NULL)) {
        MC_TRACE("expand_nccreate: malloc() failed.");
        return NULL;
    }

    memset(expand, 0, sizeof(expand_t));
    expand->win = win;
    expand->notify_win = cs->hwndParent;
    expand->style = cs->style;

    theme_BufferedPaintInit();

    return expand;
}

static int
expand_create(expand_t* expand)
{
    WORD ui_state;

    expand->theme = theme_OpenThemeData(expand->win, expand_tc);

    ui_state = SendMessage(expand->win, WM_QUERYUISTATE, 0, 0);
    expand->hide_focus = (ui_state & UISF_HIDEFOCUS) ? 1 : 0;
    expand->hide_accel = (ui_state & UISF_HIDEACCEL) ? 1 : 0;

    return 0;
}

static void
expand_destroy(expand_t* expand)
{
    if(expand->theme) {
        theme_CloseThemeData(expand->theme);
        expand->theme = NULL;
    }
}

static void
expand_ncdestroy(expand_t* expand)
{
    theme_BufferedPaintUnInit();
    free(expand);
}

static LRESULT CALLBACK
expand_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    expand_t* expand = (expand_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            expand_paint(expand);
            return 0;

        case WM_PRINTCLIENT:
        {
            RECT rect;
            GetClientRect(win, &rect);
            expand_paint_state(expand, expand->state, (HDC) wp, &rect, TRUE);
            return 0;
        }

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

        case MC_EXM_SETCOLLAPSEDSIZE:
        {
            DWORD old = MAKELONG(expand->collapsed_w, expand->collapsed_h);
            expand->collapsed_w = LOWORD(lp);
            expand->collapsed_h = HIWORD(lp);
            return old;
        }

        case MC_EXM_GETCOLLAPSEDSIZE:
            return MAKELONG(expand->collapsed_w, expand->collapsed_h);

        case MC_EXM_SETEXPANDEDSIZE:
        {
            DWORD old = MAKELONG(expand->expanded_w, expand->expanded_h);
            expand->expanded_w = LOWORD(lp);
            expand->expanded_h = HIWORD(lp);
            return old;
        }

        case MC_EXM_GETEXPANDEDSIZE:
            return MAKELONG(expand->expanded_w, expand->expanded_h);

        case MC_EXM_EXPAND:
            if(wp)
                expand_set_state(expand, expand->state | STATE_EXPANDED);
            else
                expand_set_state(expand, expand->state & ~STATE_EXPANDED);
            expand_resize_parent(expand);
            return TRUE;

        case MC_EXM_TOGGLE:
            expand_set_state(expand, expand->state ^ STATE_EXPANDED);
            expand_resize_parent(expand);
            return TRUE;

        case MC_EXM_ISEXPANDED:
            return (expand->state & STATE_EXPANDED) ? TRUE : FALSE;

        case WM_MOUSEMOVE:
        {
            DWORD old_state = expand->state;
            DWORD state = expand->state & ~(STATE_PRESSED | STATE_HOT);
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);

            if(expand_is_mouse_in_active_rect(expand, x, y)) {
                state |= STATE_HOT;

                if((wp & MK_LBUTTON) && expand->mouse_captured)
                    state |= STATE_PRESSED;
            } else if(GetFocus() == win) {
                state |= STATE_HOT;
            }

            if(expand->space_pressed)
                state |= STATE_PRESSED;

            if(state != old_state)
                expand_set_state(expand, state);
            return 0;
        }

        case WM_LBUTTONDOWN:
            SetCapture(win);
            expand->mouse_captured = 1;
            SetFocus(win);
            expand_set_state(expand, expand->state | STATE_PRESSED);
            return 0;

        case WM_LBUTTONUP:
            if(expand->state & STATE_PRESSED) {
                int x = GET_X_LPARAM(lp);
                int y = GET_Y_LPARAM(lp);
                BOOL toggle;
                DWORD state = (expand->state & ~STATE_PRESSED);

                toggle = expand_is_mouse_in_active_rect(expand, x, y);
                if(toggle)
                    state ^= STATE_EXPANDED;
                expand_set_state(expand, state);
                if(expand->mouse_captured) {
                    ReleaseCapture();
                    mc_send_notify(expand->notify_win, expand->win, NM_RELEASEDCAPTURE);
                }

                if(toggle)
                    expand_resize_parent(expand);
            }
            return 0;

        case WM_KEYDOWN:
            if(wp == VK_SPACE) {
                SetCapture(win);
                expand->mouse_captured = 1;
                expand->space_pressed = 1;
                expand_set_state(expand, expand->state | STATE_PRESSED);
            }
            return 0;

        case WM_KEYUP:
            if(wp == VK_SPACE  &&  expand->space_pressed) {
                if(expand->mouse_captured) {
                    ReleaseCapture();
                    mc_send_notify(expand->notify_win, expand->win, NM_RELEASEDCAPTURE);
                }
                expand->space_pressed = 0;
                expand_set_state(expand, (expand->state & ~STATE_PRESSED) ^ STATE_EXPANDED);
                expand_resize_parent(expand);
            }
            return 0;

        case WM_CAPTURECHANGED:
            expand->mouse_captured = 0;
            expand_set_state(expand, (expand->state & ~STATE_PRESSED));
            return 0;

        case WM_SETFOCUS:
            expand_set_state(expand, expand->state | STATE_HOT);
            return 0;

        case WM_KILLFOCUS:
        {
            DWORD pos = GetMessagePos();
            int x = GET_X_LPARAM(pos);
            int y = GET_Y_LPARAM(pos);

            if(!expand_is_mouse_in_active_rect(expand, x, y))
                expand_set_state(expand, expand->state & ~STATE_HOT);
            return 0;
        }

        case WM_GETFONT:
            return (LRESULT) expand->font;

        case WM_SETFONT:
            expand->font = (HFONT) wp;
            if((BOOL) lp  &&  !expand->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_GETTEXT:
            if(wp > 0) {
                mc_str_inbuf(expand_text(expand), MC_STRT, (TCHAR*) lp, MC_STRT, wp);
                return _tcslen((TCHAR*) lp);
            }
            return 0;

        case WM_SETTEXT:
            return FALSE;

        case WM_SETREDRAW:
            expand->no_redraw = !wp;
            return 0;

        case WM_GETDLGCODE:
            return DLGC_BUTTON;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                expand->style = ss->styleNew;
            }
            break;

        case WM_THEMECHANGED:
            if(expand->theme)
                theme_CloseThemeData(expand->theme);
            if(!expand->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            break;

        case WM_UPDATEUISTATE:
            expand_update_ui_state(expand, LOWORD(wp), HIWORD(wp));
            break;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = expand->notify_win;
            expand->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case CCM_SETWINDOWTHEME:
            theme_SetWindowTheme(win, (const WCHAR*) lp, NULL);
            return 0;

        case WM_NCCREATE:
            expand = (expand_t*) expand_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(expand == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)expand);
            return TRUE;

        case WM_CREATE:
            return (expand_create(expand) == 0 ? 0 : -1);

        case WM_DESTROY:
            expand_destroy(expand);
            return 0;

        case WM_NCDESTROY:
            if(expand)
                expand_ncdestroy(expand);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}

int
expand_init(void)
{
    WNDCLASS wc = { 0 };

    expand_glyphs[0] = LoadImage(mc_instance, MAKEINTRESOURCE(IDR_EXPAND_GLYPHS_19),
                                 IMAGE_BITMAP, 0, 0, LR_SHARED | LR_CREATEDIBSECTION);
    expand_glyphs[1] = LoadImage(mc_instance, MAKEINTRESOURCE(IDR_EXPAND_GLYPHS_24),
                                 IMAGE_BITMAP, 0, 0, LR_SHARED | LR_CREATEDIBSECTION);
    expand_glyphs[2] = LoadImage(mc_instance, MAKEINTRESOURCE(IDR_EXPAND_GLYPHS_29),
                                 IMAGE_BITMAP, 0, 0, LR_SHARED | LR_CREATEDIBSECTION);

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = expand_proc;
    wc.cbWndExtra = sizeof(expand_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = expand_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("expand_init: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
expand_fini(void)
{
    UnregisterClass(expand_wc, NULL);
}
