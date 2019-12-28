/*
 * Copyright (c) 2019-2020 Martin Mitas
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

#include "mdview.h"
#include "mdtext.h"
#include "generic.h"
#include "mousewheel.h"
#include "url.h"

#include <math.h>


/* Uncomment this to have more verbose traces from this module. */
/*#define MDVIEW_DEBUG     1*/

#ifdef MDVIEW_DEBUG
    #define MDVIEW_TRACE    MC_TRACE
#else
    #define MDVIEW_TRACE    MC_NOOP
#endif


static const TCHAR mdview_wc[] = MC_WC_MDVIEW;  /* window class name */

static const WCHAR mdview_tc[] = L"EDIT";       /* window theme class */
                              /* Note we use it only for non-client area */

#define MDVIEW_xd2d_cache_tIMER_ID     1

#define MDVIEW_HAS_BORDER(style, exstyle)   ((((style) & WS_BORDER)  ||     \
                ((exstyle) & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE))) ? 1 : 0)
#define MDVIEW_IS_TRANSPARENT(exstyle)      (((exstyle) & WS_EX_TRANSPARENT) ? 1 : 0)

/* Per-control structure */
typedef struct mdview_tag mdview_t;
struct mdview_tag {
    HWND win;
    HTHEME theme;
    xd2d_cache_t xd2d_cache;
    mdtext_t* mdtext;
    TCHAR* text;
    DWORD text_len;
    UINT input_cp;
    DWORD style          : 16;
    DWORD no_redraw      :  1;
    DWORD has_border     :  1;
    DWORD is_transparent :  1;
    COLORREF back_color;
    HFONT gdi_font;
    c_IDWriteTextFormat* text_fmt;
    int scroll_x;
    int scroll_y;
};


static void
mdview_setup_scrollbars(mdview_t* mdview)
{
    SCROLLINFO si;
    RECT client;
    SIZE size;

    GetClientRect(mdview->win, &client);

    if(mdview->mdtext != NULL)
        mdtext_size(mdview->mdtext, &size);
    else
        size.cx = size.cy = 0;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;

    /* Setup horizontal scrollbar. */
    si.nMax = size.cx - 1;
    si.nPage = mc_width(&client);
    mdview->scroll_x = SetScrollInfo(mdview->win, SB_HORZ, &si, TRUE);

    /* SetScrollInfo() above could change client dimensions. */
    GetClientRect(mdview->win, &client);

    /* Setup vertical scrollbar */
    si.nMax = size.cy;
    si.nPage = mc_height(&client);
    mdview->scroll_y = SetScrollInfo(mdview->win, SB_VERT, &si, TRUE);
}

static void
mdview_setup_text_width_and_scrollbars(mdview_t* mdview)
{
    /* This function solves the catch-22 that ideal width of the document
     * depends on the client area width (which depends also on the fact
     * whether the vertical scrollbar is present), and that presence of the
     * scrollbar depends on the height of the document (and that depends on
     * its width). */
    RECT client;

    GetClientRect(mdview->win, &client);

    if(mdview->mdtext != NULL) {
        int w = mc_width(&client);

        mdtext_set_width(mdview->mdtext,
                MC_MAX(mdtext_min_width(mdview->mdtext), w));
        mdview_setup_scrollbars(mdview);

        /* If width of the document changed due to adding/removal of the vertical
         * scrollbar, we want to rearrange the document width. */
        GetClientRect(mdview->win, &client);
        if(w != mc_width(&client)) {
            w = mc_width(&client);
            mdtext_set_width(mdview->mdtext,
                    MC_MAX(mdtext_min_width(mdview->mdtext), w));
            mdview_setup_scrollbars(mdview);
        }
    } else {
        mdview_setup_scrollbars(mdview);
    }

    if(!mdview->no_redraw)
        xd2d_invalidate(mdview->win, NULL, TRUE, &mdview->xd2d_cache);
}

static void
mdview_scroll_xy(mdview_t* mdview, int scroll_x, int scroll_y)
{
    SCROLLINFO sih, siv;
    RECT client;
    int old_scroll_x = mdview->scroll_x;
    int old_scroll_y = mdview->scroll_y;

    sih.cbSize = sizeof(SCROLLINFO);
    sih.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(mdview->win, SB_HORZ, &sih);

    siv.cbSize = sizeof(SCROLLINFO);
    siv.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(mdview->win, SB_VERT, &siv);

    GetClientRect(mdview->win, &client);

    scroll_x = MC_MID3(scroll_x, 0, MC_MAX(0, sih.nMax - (int)sih.nPage));
    scroll_y = MC_MID3(scroll_y, 0, MC_MAX(0, siv.nMax - (int)siv.nPage));

    if(scroll_x == old_scroll_x  &&  scroll_y == old_scroll_y)
        return;

    /* FIXME: Some WinDrawLib moral equivalent for ScrollWindowEx() would be
     * better here. */
    if(!mdview->no_redraw)
        xd2d_invalidate(mdview->win, NULL, TRUE, &mdview->xd2d_cache);

    SetScrollPos(mdview->win, SB_HORZ, scroll_x, TRUE);
    SetScrollPos(mdview->win, SB_VERT, scroll_y, TRUE);
    mdview->scroll_x = scroll_x;
    mdview->scroll_y = scroll_y;
}

static void
mdview_scroll(mdview_t* mdview, BOOL is_vertical, WORD opcode, int factor)
{
    int line_height = 12;
    SCROLLINFO si;
    int scroll_x = mdview->scroll_x;
    int scroll_y = mdview->scroll_y;

    if(mdview->text_fmt != NULL)
        line_height = (int) ceilf(1.25f * c_IDWriteTextFormat_GetFontSize(mdview->text_fmt));

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

    if(is_vertical) {
        GetScrollInfo(mdview->win, SB_VERT, &si);

        /* Make some overlap when scrolling for whole pages. */
        if(si.nPage > 10 * line_height)
            si.nPage -= 2 * line_height;
        else if(si.nPage > 3 * line_height)
            si.nPage -= line_height;

        switch(opcode) {
            case SB_BOTTOM:         scroll_y = si.nMax; break;
            case SB_LINEUP:         scroll_y -= MC_MIN(factor * line_height, (int)si.nPage); break;
            case SB_LINEDOWN:       scroll_y += MC_MIN(factor * line_height, (int)si.nPage); break;
            case SB_PAGEUP:         scroll_y -= si.nPage; break;
            case SB_PAGEDOWN:       scroll_y += si.nPage; break;
            case SB_THUMBPOSITION:  scroll_y = si.nPos; break;
            case SB_THUMBTRACK:     scroll_y = si.nTrackPos; break;
            case SB_TOP:            scroll_y = 0; break;
        }
    } else {
        GetScrollInfo(mdview->win, SB_HORZ, &si);
        switch(opcode) {
            case SB_BOTTOM:         scroll_x = si.nMax; break;
            case SB_LINELEFT:       scroll_x -= MC_MIN(factor * line_height, si.nPage); break;
            case SB_LINERIGHT:      scroll_x += MC_MIN(factor * line_height, si.nPage); break;
            case SB_PAGELEFT:       scroll_x -= si.nPage; break;
            case SB_PAGERIGHT:      scroll_x += si.nPage; break;
            case SB_THUMBPOSITION:  scroll_x = si.nPos; break;
            case SB_THUMBTRACK:     scroll_x = si.nTrackPos; break;
            case SB_TOP:            scroll_x = 0; break;
        }
    }

    mdview_scroll_xy(mdview, scroll_x, scroll_y);
}

static void
mdview_mouse_wheel(mdview_t* mdview, BOOL is_vertical, int wheel_delta)
{
    SCROLLINFO si;
    int line_delta;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    GetScrollInfo(mdview->win, (is_vertical ? SB_VERT : SB_HORZ), &si);

    line_delta = mousewheel_scroll(mdview->win, wheel_delta, si.nPage, is_vertical);
    if(line_delta != 0)
        mdview_scroll(mdview, is_vertical, SB_LINEDOWN, line_delta);
}

static void
mdview_key_down(mdview_t* mdview, int key)
{
    BOOL is_shift_down;

    is_shift_down = (GetKeyState(VK_SHIFT) & 0x8000);

    switch(key) {
        case VK_HOME:   mdview_scroll(mdview, !is_shift_down, SB_TOP, 1); break;
        case VK_END:    mdview_scroll(mdview, !is_shift_down, SB_BOTTOM, 1); break;
        case VK_UP:     mdview_scroll(mdview, TRUE, SB_LINEUP, 1); break;
        case VK_DOWN:   mdview_scroll(mdview, TRUE, SB_LINEDOWN, 1); break;
        case VK_LEFT:   mdview_scroll(mdview, FALSE, SB_LINEUP, 1); break;
        case VK_RIGHT:  mdview_scroll(mdview, FALSE, SB_LINEDOWN, 1); break;
        case VK_PRIOR:  mdview_scroll(mdview, !is_shift_down, SB_PAGEUP, 1); break;
        case VK_NEXT:   mdview_scroll(mdview, !is_shift_down, SB_PAGEDOWN, 1); break;
    }
}

static mdtext_t*
mdview_mdtext(mdview_t* mdview)
{
    RECT client;
    DWORD flags;

    if(mdview->mdtext != NULL)
        return mdview->mdtext;

    GetClientRect(mdview->win, &client);

    flags = (mdview->style & MC_MDS_NOJUSTIFY) ? MDTEXT_FLAG_NOJUSTIFY : 0;
    mdview->mdtext = mdtext_create(mdview->text_fmt, mdview->text, mdview->text_len,
                                mc_width(&client), flags);
    if(MC_ERR(mdview->mdtext == NULL))
        MC_TRACE("mdview_set_text: mdtext_create() failed.");
    mdview_setup_text_width_and_scrollbars(mdview);

    return mdview->mdtext;
}

static void
mdview_paint(void* ctrl, xd2d_ctx_t* ctx)
{
    c_ID2D1RenderTarget* rt = ctx->rt;
    mdview_t* mdview = (mdview_t*) ctrl;
    mdtext_t* mdtext;

    /* Paint background. */
    if(ctx->erase) {
        COLORREF cref = mdview->back_color;
        c_D2D1_COLOR_F c;

        if(cref == CLR_DEFAULT) {
            /* With WS_EX_TRANSPARENT or without any borders, we likely serve
             * similar purpose as a standard STATIC control. So, lets paint
             * the background transparently in that case. */
            if(mdview->is_transparent  ||  !mdview->has_border)
                cref = CLR_NONE;
            else
                cref = GetThemeSysColor(mdview->theme, COLOR_WINDOW);
        }

        if(cref == CLR_NONE) {
            c_ID2D1GdiInteropRenderTarget* gdi_interop;
            HRESULT hr;
            HDC dc;

            hr = c_ID2D1RenderTarget_QueryInterface(rt, &c_IID_ID2D1GdiInteropRenderTarget, (void**) &gdi_interop);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("mdview_paint: ID2D1RenderTarget::QueryInterface(IID_ID2D1GdiInteropRenderTarget) failed.");
                goto err_QueryInterface;
            }

            hr = c_ID2D1GdiInteropRenderTarget_GetDC(gdi_interop, c_D2D1_DC_INITIALIZE_MODE_CLEAR, &dc);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("mdview_paint: ID2D1GdiInteropRenderTarget::GetDC() failed.");
                goto err_GetDC;
            }

            DrawThemeParentBackground(mdview->win, dc, NULL);

            c_ID2D1GdiInteropRenderTarget_ReleaseDC(gdi_interop, NULL);
err_GetDC:
            c_ID2D1GdiInteropRenderTarget_Release(gdi_interop);
err_QueryInterface:
            ;
        } else {
            xd2d_color_set_cref(&c, cref);
            c_ID2D1RenderTarget_Clear(rt, &c);
        }
    }

    mdtext = mdview_mdtext(mdview);
    if(mdtext != NULL) {
        mdtext_paint(mdtext, rt, -mdview->scroll_x, -mdview->scroll_y,
                    ctx->dirty_rect.top, ctx->dirty_rect.bottom);
    }
}

static xd2d_vtable_t mdview_xd2d_vtable = XD2D_CTX_SIMPLE(mdview_paint);


static int
mdview_set_text(mdview_t* mdview, const char* input, UINT32 size, BOOL convert)
{
    TCHAR* text;
    DWORD text_len;

    if(convert) {
#ifdef UNICODE
        text_len = MultiByteToWideChar(mdview->input_cp, 0, input, size, NULL, 0);
        if(text_len == 0) {
            MC_TRACE_ERR("mdview_set_text: MultiByteToWideChar() failed.");
            return -1;
        }

        text = (TCHAR*) malloc(text_len * sizeof(TCHAR));
        if(MC_ERR(text == NULL)) {
            MC_TRACE("mdview_set_text: malloc() failed.");
            return -1;
        }

        MultiByteToWideChar(mdview->input_cp, 0, input, size, text, text_len);
#else
        #error mdview_set_text() is not (yet?) implemented for ANSI build.
#endif
    } else {
        text = (TCHAR*) malloc(size * sizeof(TCHAR));
        if(MC_ERR(text == NULL)) {
            MC_TRACE("mdview_set_text: malloc() failed.");
            return -1;
        }

        memcpy(text, input, size * sizeof(TCHAR));
        text_len = size;
    }

    if(mdview->text != NULL)
        free(mdview->text);
    mdview->text = text;
    mdview->text_len = text_len;

    if(mdview->mdtext != NULL)
        mdtext_destroy(mdview->mdtext);
    mdview->mdtext = NULL;

    return 0;
}


static int
mdview_goto_file(mdview_t* mdview, const TCHAR* file_path, BOOL is_unicode)
{
    HANDLE file;
    HANDLE (*fn_CreateFile)(const void*, DWORD, DWORD, SECURITY_ATTRIBUTES*, DWORD, DWORD, HANDLE);
    LARGE_INTEGER file_size;
    char* buffer = NULL;
    UINT32 alloc = 0;
    UINT32 used = 0;
    int ret = -1;

    if(is_unicode)
        fn_CreateFile = (HANDLE (*)(const void*, DWORD, DWORD, SECURITY_ATTRIBUTES*, DWORD, DWORD, HANDLE)) CreateFileW;
    else
        fn_CreateFile = (HANDLE (*)(const void*, DWORD, DWORD, SECURITY_ATTRIBUTES*, DWORD, DWORD, HANDLE)) CreateFileA;

    file = fn_CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if(MC_ERR(file == INVALID_HANDLE_VALUE)) {
        MC_TRACE_ERR("mdview_goto_file: CreateFile() failed.");
        return -1;
    }

    /* Get the actual file size as a hint for the size of the growing buffer. */
    if(GetFileSizeEx(file, &file_size))
        file_size.QuadPart++;   /* To prevent extra realloc at eof below. */
    else
        file_size.QuadPart = 0;

    if(MC_ERR(file_size.QuadPart > (LONGLONG) UINT32_MAX)) {
        MC_TRACE("mdview_goto_file: File too big");
        SetLastError(ERROR_BUFFER_OVERFLOW);
        goto err_too_big;
    }

    while(1) {
        DWORD n;

        if(used >= alloc) {
            char* tmp;

            if(alloc > 0) {
                if(alloc < UINT32_MAX / 2) {
                    alloc *= 2;
                } else if(alloc < UINT32_MAX) {
                    alloc = UINT32_MAX;
                } else {
                    MC_TRACE("mdview_goto_file: File too big");
                    goto err_too_big;
                }
            } else {
                alloc = (file_size.QuadPart > 0) ? file_size.QuadPart : 4096;
            }

            tmp = (char*) realloc(buffer, alloc);
            if(MC_ERR(buffer == NULL)) {
                MC_TRACE("mdview_goto_file: realloc() failed.");
                goto err_realloc;
            }

            buffer = tmp;
        }

        if(MC_ERR(!ReadFile(file, buffer + used, alloc - used, &n, NULL))) {
            MC_TRACE_ERR("mdview_goto_file: ReadFile() failed.");
            goto err_ReadFile;
        }

        if(n == 0)
            break;
    }

    if(MC_ERR(mdview_set_text(mdview, buffer, used, TRUE) != 0)) {
        MC_TRACE("mdview_goto_file: mdview_set_text() failed.");
        goto err_create_doc;
    }

    ret = 0;

err_create_doc:
err_realloc:
err_ReadFile:
err_too_big:
    free(buffer);
    CloseHandle(file);
    return ret;
}

static int
mdview_goto_resource(mdview_t* mdview, HINSTANCE instance, const TCHAR* res_type,
                     const TCHAR* res_id, BOOL is_unicode)
{
    HRSRC res;
    DWORD res_size;
    HGLOBAL res_global;
    void* res_data;

    res = FindResource(instance, res_id, res_type);
    if(MC_ERR(res == NULL)) {
        MC_TRACE_ERR("mdview_goto_resource: FindResource() failed.");
        return -1;
    }

    res_size = SizeofResource(instance, res);
    if(MC_ERR(res_size == 0)) {
        MC_TRACE_ERR("mdview_goto_resource: SizeofResource() failed.");
        return -1;
    }

    res_global = LoadResource(instance, res);
    if(MC_ERR(res_global == NULL)) {
        MC_TRACE_ERR("mdview_goto_resource: LoadResource() failed.");
        return -1;
    }

    res_data = LockResource(res_global);
    if(MC_ERR(res_data == NULL)) {
        MC_TRACE_ERR("mdview_goto_resource: LockResource() failed.");
        return -1;
    }

    if(MC_ERR(mdview_set_text(mdview, res_data, res_size, TRUE) != 0)) {
        MC_TRACE("mdview_goto_resource: mdview_set_text() failed.");
        return -1;
    }

    return 0;
}

static int
mdview_goto_url(mdview_t* mdview, const TCHAR* url, BOOL is_unicode)
{
    size_t len;
    TCHAR* buffer;
    int ret = -1;

    len = (is_unicode ? wcslen((const WCHAR*) url) : strlen((const char*) url));
    buffer = (TCHAR*) _malloca((len + 1) * (is_unicode ? sizeof(WCHAR) : sizeof(char)));
    if(buffer == NULL) {
        MC_TRACE("mdview_goto_url: _malloca() failed.");
        return -1;
    }

    mc_str_inbuf(url, (is_unicode ? MC_STRW : MC_STRA), buffer, MC_STRT, len+1);

    if(_tcsnccmp(buffer, _T("file://"), 7) == 0) {
        TCHAR* file_path = buffer + 7;
        TCHAR* ptr = file_path;

        /* Replace all '/' with '\\'. */
        while(*ptr) {
            if(*ptr == _T('/'))
                *ptr = _T('\\');
            ptr++;
        }

        ret = mdview_goto_file(mdview, file_path, MC_IS_UNICODE);
    } else if(_tcsnccmp(buffer, _T("res://"), 6) == 0) {
        /* See https://docs.microsoft.com/en-us/previous-versions/aa767740(v%3Dvs.85) */
        HINSTANCE instance;
        TCHAR* components[4];
        TCHAR* ptr = buffer + 6;
        int n = 0;

        components[n++] = ptr++;

        while(n < MC_SIZEOF_ARRAY(components)) {
            ptr = _tcschr(ptr, _T('/'));
            if(ptr == NULL)
                break;
            *ptr = '\0';
            components[n++] = ++ptr;
        }

        url_decode(components[0]);
        instance = LoadLibraryEx(components[0], NULL,
                    LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if(instance != NULL) {
            switch(n) {
                case 2:
                    ret = mdview_goto_resource(mdview, instance,
                                RT_HTML, components[1], MC_IS_UNICODE);
                    break;
                case 3:
                    ret = mdview_goto_resource(mdview, instance,
                                components[1], components[2], MC_IS_UNICODE);
                    break;
                default:
                    MC_TRACE("mdview_goto_url: Invalid path for res:// protocol.");
                    break;
            }

            FreeLibrary(instance);
        } else {
            MC_TRACE_ERR("mdview_goto_url: LoadLibraryEx() failed.");
        }
    } else {
        MC_TRACE("mdview_goto_url: Unsupported protocol");
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    _freea(buffer);
    return ret;
}

static mdview_t*
mdview_nccreate(HWND win, CREATESTRUCT* cs)
{
    mdview_t* mdview;

    mdview = (mdview_t*) malloc(sizeof(mdview_t));
    if(MC_ERR(mdview == NULL)) {
        MC_TRACE("mdview_nccreate: malloc() failed.");
        return NULL;
    }

    memset(mdview, 0, sizeof(mdview_t));
    mdview->win = win;
    mdview->input_cp = CP_UTF8;
    mdview->back_color = CLR_DEFAULT;

    return mdview;
}

static int
mdview_create(mdview_t* mdview, CREATESTRUCT* cs)
{
    if(cs->lpszName != NULL) {
        if(MC_ERR(mdview_set_text(mdview, (char*) cs->lpszName, _tcslen(cs->lpszName), FALSE) != 0)) {
            MC_TRACE("mdview_create: mdview_set_text() failed.");
            return -1;
        }
    }

    mdview->theme = OpenThemeData(mdview->win, mdview_tc);
    mdview->text_fmt = xdwrite_create_text_format(mdview->gdi_font, NULL);
    mdview->has_border = MDVIEW_HAS_BORDER(cs->style, cs->dwExStyle);
    mdview->is_transparent = MDVIEW_IS_TRANSPARENT(cs->dwExStyle);

    return 0;
}

static void
mdview_destroy(mdview_t* mdview)
{
    if(mdview->theme != NULL) {
        CloseThemeData(mdview->theme);
        mdview->theme = NULL;
    }

    if(mdview->text_fmt != NULL) {
        c_IDWriteTextFormat_Release(mdview->text_fmt);
        mdview->text_fmt = NULL;
    }
}

static void
mdview_ncdestroy(mdview_t* mdview)
{
    if(mdview->mdtext != NULL)
        mdtext_destroy(mdview->mdtext);
    if(mdview->text != NULL)
        free(mdview->text);
    xd2d_free_cache(&mdview->xd2d_cache);
    free(mdview);
}

static LRESULT CALLBACK
mdview_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    mdview_t* mdview = (mdview_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
            /* Make sure mdview->text exists before we might create the canvas.
             *
             * Otherwise its creation could be deferred to mdview_paint(),
             * that would lead to md_setup_scrollbars(), that could lead to
             * WM_SIZE while we would have the canvas already in use, and
             * wdResizeCanvas() would fail. */
            mdview_mdtext(mdview);

            xd2d_paint(win, mdview->no_redraw, 0,
                    &mdview_xd2d_vtable, (void*) mdview, &mdview->xd2d_cache);
            if(mdview->xd2d_cache != NULL)
                SetTimer(win, MDVIEW_xd2d_cache_tIMER_ID, 30 * 1000, NULL);
            return 0;

        case WM_PRINTCLIENT:
            xd2d_printclient(win, (HDC) wp, 0, &mdview_xd2d_vtable, (void*) mdview);
            return 0;

        case WM_NCPAINT:
            return generic_ncpaint(win, mdview->theme, (HRGN) wp);

        case WM_ERASEBKGND:
            /* Keep it on WM_PAINT */
            return FALSE;

        case WM_SIZE:
            if(mdview->xd2d_cache != NULL) {
                c_D2D1_SIZE_U size = { LOWORD(lp), HIWORD(lp) };
                c_ID2D1HwndRenderTarget_Resize((c_ID2D1HwndRenderTarget*) mdview->xd2d_cache->rt, &size);
            }
            if(mdview->mdtext != NULL)
                mdview_setup_text_width_and_scrollbars(mdview);
            return 0;

        case WM_DISPLAYCHANGE:
            xd2d_free_cache(&mdview->xd2d_cache);
            if(!mdview->no_redraw)
                xd2d_invalidate(win, NULL, TRUE, &mdview->xd2d_cache);
            return 0;

        case WM_THEMECHANGED:
            if(mdview->theme != NULL)
                CloseThemeData(mdview->theme);
            if(!mdview->no_redraw)
                mdview->theme = OpenThemeData(win, mdview_tc);
            break;

        case WM_TIMER:
            if(wp == MDVIEW_xd2d_cache_tIMER_ID) {
                xd2d_free_cache(&mdview->xd2d_cache);
                KillTimer(win, MDVIEW_xd2d_cache_tIMER_ID);
                return 0;
            }
            break;

        case MC_MDM_GOTOFILEW:
        case MC_MDM_GOTOFILEA:
            return (mdview_goto_file(mdview, (const TCHAR*) lp, (msg == MC_MDM_GOTOFILEW)) == 0);

        case MC_MDM_GOTOURLW:
        case MC_MDM_GOTOURLA:
            return (mdview_goto_url(mdview, (const TCHAR*) lp, (msg == MC_MDM_GOTOURLW)) == 0);

        case MC_MDM_SETINPUTENCODING:
            mdview->input_cp = (UINT) wp;
            return 0;

        case MC_MDM_GETINPUTENCODING:
            return (LRESULT) mdview->input_cp;

        case WM_VSCROLL:
        case WM_HSCROLL:
            mdview_scroll(mdview, (msg == WM_VSCROLL), LOWORD(wp), 1);
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            mdview_mouse_wheel(mdview, (msg == WM_MOUSEWHEEL), (int)(SHORT)HIWORD(wp));
            return 0;

        case WM_KEYDOWN:
            mdview_key_down(mdview, wp);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;

        case WM_SETTEXT:
            return (mdview_set_text(mdview, (const char*) lp, _tcslen((TCHAR*) lp), FALSE) == 0 ? TRUE : FALSE);

        case WM_GETTEXT:
            if(wp > 0) {
                TCHAR* buf = (TCHAR*) lp;

                if(mdview->text != NULL) {
                    DWORD buf_size = (DWORD) wp;
                    DWORD n = MC_MIN(buf_size-1, mdview->text_len);

                    memcpy(buf, mdview->text, n * sizeof(TCHAR));
                    buf[n] = _T('\0');
                    return n;
                } else {
                    buf[0] = _T('\0');
                }
            }
            return 0;

        case WM_GETTEXTLENGTH:
            return mdview->text_len;

        case WM_SETREDRAW:
            mdview->no_redraw = !wp;
            if(!mdview->no_redraw)
                xd2d_invalidate(mdview->win, NULL, TRUE, &mdview->xd2d_cache);
            return 0;

        case WM_STYLECHANGED:
        {
            DWORD style = GetWindowLong(win, GWL_STYLE);
            DWORD exstyle = GetWindowLong(win, GWL_EXSTYLE);
            mdview->has_border = MDVIEW_HAS_BORDER(style, exstyle);
            mdview->is_transparent = MDVIEW_IS_TRANSPARENT(exstyle);
            if(!mdview->no_redraw)
                xd2d_invalidate(mdview->win, NULL, TRUE, &mdview->xd2d_cache);
            return 0;
        }

        case CCM_SETWINDOWTHEME:
            SetWindowTheme(win, (const WCHAR*) lp, NULL);
            return 0;

        case CCM_SETBKCOLOR:
        {
            COLORREF old = mdview->back_color;
            mdview->back_color = (COLORREF) lp;
            if(!mdview->no_redraw)
                xd2d_invalidate(mdview->win, NULL, TRUE, &mdview->xd2d_cache);
            return old;
        }

        case WM_GETFONT:
            return (LRESULT) mdview->gdi_font;

        case WM_SETFONT:
            mdview->gdi_font = (HFONT) wp;
            if(mdview->text_fmt != NULL)
                c_IDWriteTextFormat_Release(mdview->text_fmt);
            mdview->text_fmt = xdwrite_create_text_format(mdview->gdi_font, NULL);
            if(mdview->mdtext != NULL) {
                mdtext_destroy(mdview->mdtext);
                mdview->mdtext = NULL;
            }
            if((BOOL) lp  &&  !mdview->no_redraw)
                xd2d_invalidate(mdview->win, NULL, TRUE, &mdview->xd2d_cache);
            return 0;

        case WM_NCCREATE:
            mdview = mdview_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(mdview == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)mdview);
            return TRUE;

        case WM_CREATE:
            return (mdview_create(mdview, (CREATESTRUCT*)lp) == 0 ? 0 : -1);

        case WM_DESTROY:
            mdview_destroy(mdview);
            return 0;

        case WM_NCDESTROY:
            if(mdview != NULL)
                mdview_ncdestroy(mdview);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}


int
mdview_init_module(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = mdview_proc;
    wc.cbWndExtra = sizeof(mdview_t*);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = mdview_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("mdview_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
mdview_fini_module(void)
{
    UnregisterClass(mdview_wc, NULL);
}
