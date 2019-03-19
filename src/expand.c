/*
 * Copyright (c) 2012-2015 Martin Mitas
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
#include "anim.h"
#include "doublebuffer.h"
#include "theme.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define EXPAND_DEBUG     1*/

#ifdef EXPAND_DEBUG
    #define EXPAND_TRACE    MC_TRACE
#else
    #define EXPAND_TRACE    MC_NOOP
#endif


#define GLYPH_TEXT_MARGIN     5
#define FOCUS_INFLATE_H       3
#define FOCUS_INFLATE_V       1


static const TCHAR expand_wc[] = MC_WC_EXPAND;    /* Window class name */
static const WCHAR expand_tc[] = L"BUTTON";       /* Theming identifier */


typedef struct expand_glyph_tag expand_glyph_t;
struct expand_glyph_tag {
    HBITMAP bmp;
    const WORD size;
    const WORD res_id;
};

static expand_glyph_t expand_glyphs[3] = {
    { NULL, 19, IDR_EXPAND_GLYPHS_19 },
    { NULL, 24, IDR_EXPAND_GLYPHS_24 },
    { NULL, 29, IDR_EXPAND_GLYPHS_29 }
};

static expand_glyph_t*
expand_get_glyph(LONG size)
{
    int i;

    for(i = 1; i < MC_SIZEOF_ARRAY(expand_glyphs); i++) {
        if(size < expand_glyphs[i].size)
            return &expand_glyphs[i - 1];
    }

    return &expand_glyphs[MC_SIZEOF_ARRAY(expand_glyphs) - 1];
}

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
    anim_t* anim;               /* animation of parent resizing */
    DWORD style          : 16;
    DWORD no_redraw      : 1;
    DWORD hide_accel     : 1;
    DWORD hide_focus     : 1;
    DWORD hot_track      : 1;
    DWORD mouse_captured : 1;
    DWORD space_pressed  : 1;
    DWORD state          : 3;
    DWORD old_state      : 3;  /* For painting state transitions. */
    WORD collapsed_w;
    WORD collapsed_h;
    WORD expanded_w;
    WORD expanded_h;
};


static inline const TCHAR*
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
    HFONT old_font = NULL;
    const TCHAR* str;
    SIZE extents;
    int glyph_size;

    GetClientRect(expand->win, &rect);
    right_align = (GetWindowLong(expand->win, GWL_EXSTYLE) & WS_EX_RIGHT) ? TRUE : FALSE;

    if(expand->font)
        old_font = SelectObject(dc, expand->font);
    str = expand_text(expand);
    GetTextExtentPoint32(dc, str, _tcslen(str), &extents);
    if(expand->font)
        SelectObject(dc, old_font);

    if(expand->theme != NULL) {
        expand_glyph_t* glyph = expand_get_glyph(rect.bottom);
        layout->glyph_bmp = glyph->bmp;
        glyph_size = glyph->size;
    } else {
        layout->glyph_bmp = NULL;
        glyph_size = (extents.cy - 2) & ~0x1;
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
        mcDrawThemeParentBackground(expand->win, dc, dirty);

    /* According to MSDN guidelines, the controls of such nature as this one
     * should never be disabled, but removed instead. I.e. application
     * should rather hide the control then to disable it. If it dows not,
     * we refuse to paint.
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
    if(layout.glyph_bmp != NULL) {
        int glyph_size;
        int glyph_index;
        HDC glyph_dc;
        const BLENDFUNCTION blend_func = { AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };
        HBITMAP old_bmp;

        glyph_size = mc_height(&layout.glyph_rect);
        glyph_index = (state & STATE_EXPANDED) ? 3 : 0;
        if(state & STATE_PRESSED)
            glyph_index += 2;
        else if(state & STATE_HOT)
            glyph_index += 1;

        glyph_dc = CreateCompatibleDC(dc);
        old_bmp = SelectObject(glyph_dc, layout.glyph_bmp);
        GdiAlphaBlend(dc, layout.glyph_rect.left, layout.glyph_rect.top, glyph_size, glyph_size,
                      glyph_dc, 0, glyph_size * glyph_index, glyph_size, glyph_size,
                      blend_func);
        SelectObject(glyph_dc, old_bmp);
        DeleteDC(glyph_dc);
    } else {
        POINT pt[3];
        int h;
        HBRUSH old_brush;

        if(state & STATE_EXPANDED) {
            h = mc_width(&layout.glyph_rect) / 2;

            pt[0].x = layout.glyph_rect.left + h / 2;
            pt[0].y = layout.glyph_rect.top;
            pt[1].x = layout.glyph_rect.left + h / 2;
            pt[1].y = layout.glyph_rect.bottom;
            pt[2].x = pt[0].x + h;
            pt[2].y = (layout.glyph_rect.top + layout.glyph_rect.bottom + 1) / 2;
        } else {
            h = mc_height(&layout.glyph_rect) / 2;

            pt[0].x = layout.glyph_rect.left;
            pt[0].y = layout.glyph_rect.top + h / 2;
            pt[1].x = layout.glyph_rect.right;
            pt[1].y = layout.glyph_rect.top + h / 2;
            pt[2].x = (layout.glyph_rect.left + layout.glyph_rect.right + 1) / 2;
            pt[2].y = pt[0].y + h;
        }

        old_brush = SelectObject(dc, GetSysColorBrush(COLOR_BTNTEXT));
        Polygon(dc, pt, MC_SIZEOF_ARRAY(pt));
        SelectObject(dc, old_brush);
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
expand_mcstate(DWORD state)
{
    switch(state & (STATE_PRESSED | STATE_HOT)) {
        case STATE_PRESSED:  return PBS_PRESSED;
        case STATE_HOT:      return PBS_HOT;
        default:             return PBS_NORMAL;
    }
}

static void
expand_paint(expand_t* expand)
{
    PAINTSTRUCT ps;
    int old_mcstate;
    int new_mcstate;

    BeginPaint(expand->win, &ps);

    /* Handle transition animation if already in progress */
    if(mcBufferedPaintRenderAnimation(expand->win, ps.hdc)) {
        EXPAND_TRACE("expand_paint: Transition in progress");
        goto done;
    }

    if(expand->no_redraw)
        goto done;

    /* If painting because of state change, start new transition animation */
    old_mcstate = expand_mcstate(expand->old_state);
    new_mcstate = expand_mcstate(expand->state);
    expand->old_state = expand->state;
    if(old_mcstate != new_mcstate) {
        HRESULT hr;
        DWORD duration;

        hr = mcGetThemeTransitionDuration(expand->theme, BP_PUSHBUTTON,
                old_mcstate, new_mcstate, TMT_TRANSITIONDURATIONS,
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

            buf = mcBeginBufferedAnimation(expand->win, ps.hdc, &rect,
                    BPBF_COMPATIBLEBITMAP, NULL, &params, &old_dc, &new_dc);
            if(buf != NULL) {
                expand_paint_state(expand, expand->old_state, old_dc, &rect, TRUE);
                expand_paint_state(expand, expand->state, new_dc, &rect, TRUE);
                EXPAND_TRACE("expand_paint: Transition start (%lu ms)", duration);
                mcEndBufferedAnimation(buf, TRUE);
                goto done;
            }
        }
    }

    /* Normal paint. We do not need double buffering without background erase. */
    if((expand->style & MC_EXS_DOUBLEBUFFER)  &&  ps.fErase)
        doublebuffer_simple(expand, &ps, expand_do_paint);
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

static LRESULT
expand_update_ui_state(expand_t* expand, WPARAM wp, LPARAM lp)
{
    LRESULT ret;
    DWORD flags;

    ret = DefWindowProc(expand->win, WM_UPDATEUISTATE, wp, lp);
    flags = MC_SEND(expand->win, WM_QUERYUISTATE, 0, 0);
    expand->hide_focus = (flags & UISF_HIDEFOCUS) ? 1 : 0;
    expand->hide_accel = (flags & UISF_HIDEACCEL) ? 1 : 0;
    if(!expand->no_redraw)
        InvalidateRect(expand->win, NULL, FALSE);
    return ret;
}

static void
expand_set_state(expand_t* expand, DWORD state)
{
    if(expand->state == state)
        return;

    EXPAND_TRACE("expand_set_state: 0x%lx -> 0x%lx", expand->state, state);

    expand->old_state = expand->state;
    expand->state = state;
    mc_send_notify(expand->notify_win, expand->win, MC_EXN_EXPANDING);

    mcBufferedPaintStopAllAnimations(expand->win);
    if(!expand->no_redraw)
        InvalidateRect(expand->win, NULL, TRUE);
}

static void
expand_guess_dlg_client_size(expand_t* expand, SIZE* size, BOOL expanded)
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
    dlg_font = (HFONT) MC_SEND(expand->notify_win, WM_GETFONT, 0, 0);
    dlg_padding = mc_pixels_from_dlus(dlg_font, 7, TRUE);
    if(top < dlg_padding)
        dlg_padding = top;

    if(expanded) {
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
expand_convert_size(expand_t* expand, SIZE* size, int sign)
{
    RECT entire_rect;
    RECT client_rect;

    GetWindowRect(expand->notify_win, &entire_rect);
    GetClientRect(expand->notify_win, &client_rect);
    MapWindowPoints(expand->notify_win, NULL, (POINT*) &client_rect, 2);

    size->cx += sign * (mc_width(&entire_rect) - mc_width(&client_rect));
    size->cy += sign * (mc_height(&entire_rect) - mc_height(&client_rect));
}

static inline void
expand_client_to_entire(expand_t* expand, SIZE* size)
{
    expand_convert_size(expand, size, +1);
}

static inline void
expand_entire_to_client(expand_t* expand, SIZE* size)
{
    expand_convert_size(expand, size, -1);
}

static void
expand_get_desired_dlg_size(expand_t* expand, SIZE* dlg_frame_size, SIZE* dlg_client_size)
{
    BOOL is_entire;
    BOOL guessed = FALSE;

    /* Get the size the parent should have */
    if(expand->state & STATE_EXPANDED) {
        dlg_client_size->cx = expand->expanded_w;
        dlg_client_size->cy = expand->expanded_h;
    } else {
        dlg_client_size->cx = expand->collapsed_w;
        dlg_client_size->cy = expand->collapsed_h;
    }

    /* If not set explicitly, try to guess */
    if(dlg_client_size->cx == 0  &&  dlg_client_size->cy == 0) {
        expand_guess_dlg_client_size(expand, dlg_client_size, (expand->state & STATE_EXPANDED));
        is_entire = FALSE;
        guessed = TRUE;
    } else {
        is_entire = (expand->style & MC_EXS_RESIZEENTIREWINDOW);
    }

    /* We need both, size of entire window and its client */
    dlg_frame_size->cx = dlg_client_size->cx;
    dlg_frame_size->cy = dlg_client_size->cy;
    if(is_entire)
        expand_entire_to_client(expand, dlg_client_size);
    else
        expand_client_to_entire(expand, dlg_frame_size);

    /* We may want to remember it for next time */
    if(guessed  &&  (expand->style & MC_EXS_CACHESIZES)) {
        SIZE* p_size = ((expand->style & MC_EXS_RESIZEENTIREWINDOW)
                                        ? dlg_frame_size : dlg_client_size);
        if(expand->state & STATE_EXPANDED) {
            expand->expanded_w = p_size->cx;
            expand->expanded_h = p_size->cy;
        } else {
            expand->collapsed_w = p_size->cx;
            expand->collapsed_h = p_size->cy;
        }
    }
}

static inline void
expand_do_resize(expand_t* expand, SIZE* dlg_frame_size)
{
    /* Resize the parent */
    SetWindowPos(expand->notify_win, NULL, 0, 0, dlg_frame_size->cx, dlg_frame_size->cy,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

static void
expand_handle_children(expand_t* expand, RECT* old_rect, RECT* new_rect)
{
    HWND child_win;

    if(expand->style & MC_EXS_IGNORECHILDREN)
        return;

    /* Enable/disable children to be (un)covered by the resize */
    for(child_win = GetWindow(expand->notify_win, GW_CHILD);
        child_win != NULL;
        child_win = GetWindow(child_win, GW_HWNDNEXT))
    {
        RECT child_rect;
        BOOL in_old_rect;
        BOOL in_new_rect;

        GetWindowRect(child_win, &child_rect);
        in_old_rect = mc_rect_overlaps_rect(old_rect, &child_rect);
        in_new_rect = mc_rect_contains_rect(new_rect, &child_rect);
        if(in_old_rect != in_new_rect) {
            EnableWindow(child_win, in_new_rect);
            ShowWindow(child_win, (in_new_rect ? SW_SHOW : SW_HIDE));
        }
    }
}

/* For parent resize animation, we need to remember the original window size. */
typedef struct expand_anim_ctx_tag expand_anim_ctx_t;
struct expand_anim_ctx_tag {
    SIZE orig_size;
};

static void
expand_animate_resize_callback(expand_t* expand)
{
    anim_t* anim = expand->anim;
    expand_anim_ctx_t* anim_ctx = ANIM_EXTRA_DATA(anim, expand_anim_ctx_t);
    LONG orig_w = anim_ctx->orig_size.cx;
    LONG orig_h = anim_ctx->orig_size.cy;
    RECT r;
    SIZE dlg_frame_size;
    SIZE dlg_client_size;

    GetWindowRect(expand->win, &r);
    expand_get_desired_dlg_size(expand, &dlg_frame_size, &dlg_client_size);

    anim_step(anim);

    if(!anim_is_done(anim)) {
        float progress = anim_progress(anim);

        dlg_frame_size.cx = orig_w + progress * (float)(dlg_frame_size.cx - orig_w);
        dlg_frame_size.cy = orig_h + progress * (float)(dlg_frame_size.cy - orig_h);
        expand_do_resize(expand, &dlg_frame_size);
    } else {
        RECT old_rect, new_rect;

        anim_stop(anim);
        expand->anim = NULL;

        mc_rect_set(&old_rect, 0, 0, orig_w, orig_h);
        mc_rect_set(&new_rect, 0, 0, dlg_client_size.cx, dlg_client_size.cy);

        expand_do_resize(expand, &dlg_frame_size);
        expand_handle_children(expand, &old_rect, &new_rect);
        mc_send_notify(expand->notify_win, expand->win, MC_EXN_EXPANDED);
    }
}

static void
expand_resize(expand_t* expand, DWORD flags)
{
    SIZE dlg_frame_size;
    SIZE dlg_client_size;
    RECT old_rect, new_rect;

    expand_get_desired_dlg_size(expand, &dlg_frame_size, &dlg_client_size);

    /* If an animation is in progress, stop it. */
    if(expand->anim != 0) {
        anim_stop(expand->anim);
        expand->anim = NULL;
    }

    /* Animate the resize */
    if((expand->style & MC_EXS_ANIMATE)  &&  !(flags & MC_EXE_NOANIMATE)) {
        RECT start_rect;
        expand_anim_ctx_t anim_ctx;
        DWORD duration;

        GetWindowRect(expand->notify_win, &start_rect);
        anim_ctx.orig_size.cx = mc_width(&start_rect);
        anim_ctx.orig_size.cy = mc_height(&start_rect);

        /* See http://blogs.msdn.com/b/oldnewthing/archive/2008/04/23/8417521.aspx */
        duration = GetDoubleClickTime() / 3;

        /* We store original (current) parent window size to deal correctly
         * with situations, when it changes while the animation is in
         * progress. */
        expand->anim = anim_start_ex(expand->win, duration,
                ANIM_DEFAULT_FREQUENCY, &anim_ctx, sizeof(expand_anim_ctx_t));
        if(expand->anim != NULL) {
            return;
        } else {
            MC_TRACE("expand_resize: anim_start() failed. "
                     "Falling back to instant resize.");
        }
    }

    /* Instant resize */
    expand_do_resize(expand, &dlg_frame_size);
    GetClientRect(expand->win, &old_rect);
    mc_rect_set(&new_rect, 0, 0, dlg_client_size.cx, dlg_client_size.cy);
    expand_handle_children(expand, &old_rect, &new_rect);
    mc_send_notify(expand->notify_win, expand->win, MC_EXN_EXPANDED);
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

    doublebuffer_init();

    return expand;
}

static int
expand_create(expand_t* expand)
{
    WORD ui_state;

    expand->theme = mcOpenThemeData(expand->win, expand_tc);

    ui_state = MC_SEND(expand->win, WM_QUERYUISTATE, 0, 0);
    expand->hide_focus = (ui_state & UISF_HIDEFOCUS) ? 1 : 0;
    expand->hide_accel = (ui_state & UISF_HIDEACCEL) ? 1 : 0;

    return 0;
}

static void
expand_destroy(expand_t* expand)
{
    if(expand->theme) {
        mcCloseThemeData(expand->theme);
        expand->theme = NULL;
    }
}

static void
expand_ncdestroy(expand_t* expand)
{
    if(expand->anim != NULL)
        anim_stop(expand->anim);

    doublebuffer_fini();
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
            expand_resize(expand, lp);
            return TRUE;

        case MC_EXM_TOGGLE:
            expand_set_state(expand, expand->state ^ STATE_EXPANDED);
            expand_resize(expand, lp);
            return TRUE;

        case MC_EXM_ISEXPANDED:
            return (expand->state & STATE_EXPANDED) ? TRUE : FALSE;

        case WM_TIMER:
            if(expand->anim != NULL  &&  wp == anim_timer_id(expand->anim)) {
                expand_animate_resize_callback(expand);
                return 0;
            }
            break;

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
                DWORD state = (expand->state & ~STATE_PRESSED);
                BOOL toggle;

                toggle = expand_is_mouse_in_active_rect(expand, x, y);
                if(toggle)
                    state ^= STATE_EXPANDED;
                expand_set_state(expand, state);
                if(expand->mouse_captured)
                    ReleaseCapture();
                mc_send_notify(expand->notify_win, expand->win, NM_RELEASEDCAPTURE);

                if(toggle)
                    expand_resize(expand, 0);
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
                if(expand->mouse_captured)
                    ReleaseCapture();
                mc_send_notify(expand->notify_win, expand->win, NM_RELEASEDCAPTURE);

                expand->space_pressed = 0;
                expand_set_state(expand, (expand->state & ~STATE_PRESSED) ^ STATE_EXPANDED);
                expand_resize(expand, 0);
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
            if(!expand->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_BUTTON;

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                expand->style = ss->styleNew;
                /* No repaint here: All our styles currently only have an
                 * impact on behavior, not look of the control. */
            }
            break;

        case WM_THEMECHANGED:
            if(expand->theme)
                mcCloseThemeData(expand->theme);
            if(!expand->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            break;

        case WM_SYSCOLORCHANGE:
            if(!expand->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;

        case WM_UPDATEUISTATE:
            return expand_update_ui_state(expand, wp, lp);

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = expand->notify_win;
            expand->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LRESULT) old;
        }

        case CCM_SETWINDOWTHEME:
            mcSetWindowTheme(win, (const WCHAR*) lp, NULL);
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
expand_init_module(void)
{
    int i;
    WNDCLASS wc = { 0 };

    for(i = 0; i < MC_SIZEOF_ARRAY(expand_glyphs); i++) {
        expand_glyphs[i].bmp = LoadImage(mc_instance,
                    MAKEINTRESOURCE(expand_glyphs[i].res_id), IMAGE_BITMAP,
                    0, 0, LR_SHARED | LR_CREATEDIBSECTION);
    }

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = expand_proc;
    wc.cbWndExtra = sizeof(expand_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = expand_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("expand_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
expand_fini_module(void)
{
    int i;

    UnregisterClass(expand_wc, NULL);

    for(i = 0; i < MC_SIZEOF_ARRAY(expand_glyphs); i++)
        DeleteObject(expand_glyphs[i].bmp);
}
