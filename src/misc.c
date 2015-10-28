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

#include "misc.h"
#include "compat.h"
#include "module.h"
#include "theme.h"
#include "xcom.h"


/***************
 *** Globals ***
 ***************/

HINSTANCE mc_instance;
HINSTANCE mc_instance_kernel32;
DWORD mc_win_version;
DWORD mc_comctl32_version;

HIMAGELIST mc_bmp_glyphs;


/************************
 *** String Utilities ***
 ************************/

/* We dig into the raw resources instead of using LoadStringW() with nBufferMax
 * set to zero.
 *
 * See http://blogs.msdn.com/b/oldnewthing/archive/2004/01/30/65013.aspx.
 *
 * This allows us to do two useful things:
 *  -- Verify easily the string is zero-terminated (the assertion).
 *  -- Implement a fall-back to English, as translations can be potentially
 *     incomplete.
 */
const TCHAR*
mc_str_load(UINT id)
{
#ifndef UNICODE
    #error mc_str_load() is not (yet?) implemented for ANSI build.
#endif

    const UINT lang_id[2] = { MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                              MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT) };
    TCHAR* rsrc_id = MAKEINTRESOURCE(id/16 + 1);
    int str_num = (id & 15);
    HRSRC rsrc;
    HGLOBAL handle;
    WCHAR* str;
    UINT len;
    int i, j;

    for(i = 0; i < MC_SIZEOF_ARRAY(lang_id); i++) {
        rsrc = FindResourceEx(mc_instance, RT_STRING, rsrc_id, lang_id[i]);
        if(MC_ERR(rsrc == NULL))
            goto not_found;
        handle = LoadResource(mc_instance, rsrc);
        if(MC_ERR(handle == NULL))
            goto not_found;
        str = (WCHAR*) LockResource(handle);
        if(MC_ERR(str == NULL))
            goto not_found;

        for(j = 0; j < str_num; j++)
            str += 1 + (UINT) *str;

        len = (UINT) *str;
        if(MC_ERR(len == 0))
            goto not_found;
        str++;

        /* Verify string resources are '\0'-terminated. This is not default
         * behavior of RC.EXE as well as windres.exe. For windres.exe we need
         * to have resources in the form "foo bar\0". For RC.EXE, we need to
         * use option '/n' to terminate the strings as RC.EXE even strips final
         * '\0' from the string even when explicitly specified. */
        MC_ASSERT(str[len - 1] == L'\0');

        return str;

not_found:
        MC_TRACE("mc_str_load: String %u missing [language 0x%x].", id,
                 (DWORD)(lang_id[i] == MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)
                    ? LANGIDFROMLCID(GetThreadLocale()) : lang_id[i]));
    }

    return _T("");
}

void
mc_str_inbuf_A2A(const char* restrict from_str, char* restrict to_str, int to_str_bufsize)
{
    if(to_str_bufsize <= 0)
        return;

    strncpy(to_str, from_str ? from_str : "", to_str_bufsize);
    to_str[to_str_bufsize-1] = '\0';
}

void
mc_str_inbuf_W2W(const WCHAR* restrict from_str, WCHAR* restrict to_str, int to_str_bufsize)
{
    if(to_str_bufsize <= 0)
        return;

    wcsncpy(to_str, from_str ? from_str : L"", to_str_bufsize);
    to_str[to_str_bufsize-1] = L'\0';
}

void
mc_str_inbuf_A2W(const char* restrict from_str, WCHAR* restrict to_str, int to_str_bufsize)
{
    int n;

    if(to_str_bufsize <= 0)
        return;
    if(from_str == NULL)
        from_str = "";

    n = MultiByteToWideChar(CP_ACP, 0, from_str, -1, to_str, to_str_bufsize);
    if(MC_ERR(n == 0  &&  from_str[0] != '\0'))
        MC_TRACE_ERR("mc_str_inbuf_A2W: MultiByteToWideChar() failed.");
    to_str[to_str_bufsize-1] = L'\0';
}

void
mc_str_inbuf_W2A(const WCHAR* restrict from_str, char* restrict to_str, int to_str_bufsize)
{
    int n;

    if(to_str_bufsize <= 0)
        return;
    if(from_str == NULL)
        from_str = L"";

    n = WideCharToMultiByte(CP_ACP, 0, from_str, -1, to_str, to_str_bufsize, NULL, NULL);
    if(MC_ERR(n == 0  &&  from_str[0] != '\0'))
        MC_TRACE_ERR("mc_str_inbuf_W2A: WideCharToMultiByte() failed.");
    to_str[to_str_bufsize-1] = '\0';
}

char*
mc_str_n_A2A(const char* restrict from_str, int from_len, int* restrict ptr_to_len)
{
    char* to_str = NULL;
    int to_len = 0;

    if(from_str == NULL)
        goto out;

    if(from_len < 0)
        from_len = (int)strlen((char*)from_str);
    to_len = from_len;

    to_str = (char*) malloc(to_len + 1);
    if(MC_ERR(to_str == NULL)) {
        MC_TRACE("mc_str_n_A2A: malloc() failed.");
        return NULL;
    }

    memcpy(to_str, from_str, to_len);
    to_str[to_len] = '\0';

out:
    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;
    return to_str;
}

WCHAR*
mc_str_n_W2W(const WCHAR* restrict from_str, int from_len, int* restrict ptr_to_len)
{
    WCHAR* to_str = NULL;
    int to_len = 0;

    if(from_str == NULL)
        goto out;

    if(from_len < 0)
        from_len = wcslen(from_str);
    to_len = from_len;

    to_str = (WCHAR*) malloc((to_len + 1) * sizeof(WCHAR));
    if(MC_ERR(to_str == NULL)) {
        MC_TRACE("mc_str_n_W2W: malloc() failed.");
        return NULL;
    }

    memcpy(to_str, from_str, to_len * sizeof(WCHAR));
    to_str[to_len] = L'\0';

out:
    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;
    return to_str;
}

WCHAR*
mc_str_n_A2W(const char* restrict from_str, int from_len, int* restrict ptr_to_len)
{
    WCHAR* to_str = NULL;
    int to_len = 0;

    if(from_str == NULL)
        goto out;

    if(from_len < 0)
        from_len = (int)strlen((char*)from_str);

    to_len = MultiByteToWideChar(CP_ACP, 0, from_str, from_len, NULL, 0);
    if(MC_ERR(to_len == 0  &&  from_len > 0)) {
        MC_TRACE_ERR("mc_str_n_A2W: MultiByteToWideChar() check length failed.");
        return NULL;
    }

    to_str = (WCHAR*) malloc((to_len + 1) * sizeof(WCHAR));
    if(MC_ERR(to_str == NULL)) {
        MC_TRACE("mc_str_n_A2W: malloc() failed.");
        return NULL;
    }

    if(MC_ERR(MultiByteToWideChar(CP_ACP, 0, from_str, from_len, to_str, to_len) != to_len)) {
        MC_TRACE_ERR("mc_str_n_A2W: MultiByteToWideChar() conversion failed.");
        free(to_str);
        return NULL;
    }
    to_str[to_len] = L'\0';

out:
    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;
    return to_str;
}

char*
mc_str_n_W2A(const WCHAR* restrict from_str, int from_len, int* restrict ptr_to_len)
{
    char* to_str = NULL;
    int to_len = 0;

    if(from_str == NULL)
        goto out;

    if(from_len < 0)
        from_len = (int)(wcslen((WCHAR*)from_str));

    to_len = WideCharToMultiByte(CP_ACP, 0, from_str, from_len,
                                 NULL, 0, NULL, NULL);
    if(MC_ERR(to_len == 0  &&  from_len > 0)) {
        MC_TRACE_ERR("mc_str_n_W2A: WideCharToMultiByte() check length failed.");
        return NULL;
    }

    to_str = (char*) malloc((to_len+1) * sizeof(WCHAR));
    if(MC_ERR(to_str == NULL)) {
        MC_TRACE("mc_str_n_W2A: malloc() failed.");
        return NULL;
    }

    if(MC_ERR(WideCharToMultiByte(CP_ACP, 0, from_str, from_len,
                                  to_str, to_len, NULL, NULL) != to_len)) {
        MC_TRACE_ERR("mc_str_n_W2A: WideCharToMultiByte() conversion failed.");
        free(to_str);
        return NULL;
    }
    to_str[to_len] = '\0';

out:
    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;
    return to_str;
}


/***************************
 *** Loading System DLLs ***
 ***************************/

static BOOL mc_use_LOAD_LIBRARY_SEARCH_SYSTEM32 = FALSE;

static void
setup_load_sys_dll(void)
{
    /* Win 2000 and XP does not support LOAD_LIBRARY_SEARCH_SYSTEM32. */
    if(mc_win_version <= MC_WIN_XP)
        return;

    /* Win Vista/7 prior the security update KB2533623 does not support
     * the flag LOAD_LIBRARY_SEARCH_SYSTEM32 either. The update KB2533623
     * also added AddDllDirectory() so we use that as the indicator whether
     * we can use the flag. */
    if(mc_win_version < MC_WIN_8) {
        if(GetProcAddress(mc_instance_kernel32, "AddDllDirectory") == NULL)
            return;
    }

    mc_use_LOAD_LIBRARY_SEARCH_SYSTEM32 = TRUE;
}

HMODULE
mc_load_sys_dll(const TCHAR* dll_name)
{
    if(mc_use_LOAD_LIBRARY_SEARCH_SYSTEM32) {
        return LoadLibraryEx(dll_name, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    } else {
        TCHAR path[MAX_PATH];
        UINT dllname_len;
        UINT sysdir_len;

        dllname_len = _tcslen(dll_name);
        sysdir_len = GetSystemDirectory(path, MAX_PATH);
        if(MC_ERR(sysdir_len + 1 + dllname_len >= MAX_PATH)) {
            MC_TRACE("mc_load_sys_dll: Buffer too small.");
            return NULL;
        }

        path[sysdir_len] = _T('\\');
        memcpy(path + sysdir_len + 1, dll_name, (dllname_len + 1) * sizeof(TCHAR));

        return LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
}

HMODULE
mc_load_redist_dll(const TCHAR* dll_name)
{
    return LoadLibrary(dll_name);
}


/***********************
 *** Mouse Utilities ***
 ***********************/

static CRITICAL_SECTION mc_wheel_lock;

int
mc_wheel_scroll(HWND win, int delta, int page, BOOL is_vertical)
{
    /* We accumulate the wheel_delta until there is enough to scroll for
     * at least a single line. This improves the feel for strange values
     * of SPI_GETWHEELSCROLLLINES and for some mouses. */
    static HWND last_win = NULL;
    static DWORD last_time[2] = { 0, 0 };   /* horizontal, vertical */
    static int accum_delta[2] = { 0, 0 };   /* horizontal, vertical */

    DWORD now;
    UINT param;
    int dir = (is_vertical ? 1 : 0); /* index into accum_delta[], last_time[] */
    UINT lines_per_WHEEL_DELTA;
    int lines;

    now = GetTickCount();

    if(page < 1)
        page = 1;

    /* Ask the system for scroll speed. */
    param = (is_vertical ? SPI_GETWHEELSCROLLLINES : SPI_GETWHEELSCROLLCHARS);
    if(MC_ERR(!SystemParametersInfo(param, 0, &lines_per_WHEEL_DELTA, 0)))
        lines_per_WHEEL_DELTA = 3;

    /* But never scroll more then complete page. */
    if(lines_per_WHEEL_DELTA == WHEEL_PAGESCROLL  ||  lines_per_WHEEL_DELTA >= page)
        lines_per_WHEEL_DELTA = page;

    EnterCriticalSection(&mc_wheel_lock);

    /* Reset the accumulated value(s) when switching to another window, when
     * changing scrolling direction, or when the wheel was not used for some
     * time. */
    if(win != last_win) {
        last_win = win;
        accum_delta[0] = 0;
        accum_delta[1] = 0;
    } else if((now - last_time[dir] > GetDoubleClickTime() * 2)  ||
              ((delta > 0) == (accum_delta[dir] < 0))) {
        accum_delta[dir] = 0;
    }

    /* Compute lines to scroll. */
    accum_delta[dir] += delta;
    lines = (accum_delta[dir] * (int)lines_per_WHEEL_DELTA) / WHEEL_DELTA;
    accum_delta[dir] -= (lines * WHEEL_DELTA) / (int)lines_per_WHEEL_DELTA;
    last_time[dir] = now;

    LeaveCriticalSection(&mc_wheel_lock);
    return (is_vertical ? -lines : lines);
}


static CRITICAL_SECTION mc_drag_lock;

static BOOL mc_drag_running = FALSE;
static HWND mc_drag_win = NULL;
int mc_drag_start_x;
int mc_drag_start_y;
int mc_drag_hotspot_x;
int mc_drag_hotspot_y;
int mc_drag_index;
UINT_PTR mc_drag_extra;

BOOL
mc_drag_set_candidate(HWND win, int start_x, int start_y,
                      int hotspot_x, int hotspot_y, int index, UINT_PTR extra)
{
    BOOL ret = FALSE;

    EnterCriticalSection(&mc_drag_lock);
    if(!mc_drag_running) {
        mc_drag_win = win;
        mc_drag_start_x = start_x;
        mc_drag_start_y = start_y;
        mc_drag_hotspot_x = hotspot_x;
        mc_drag_hotspot_y = hotspot_y;
        mc_drag_index = index;
        mc_drag_extra = extra;
        ret = TRUE;
    } else {
        /* Dragging (of different window?) is already running. Actually this
         * should happen normally only if the two windows live in different
         * threads, because the mc_drag_win should have the mouse captured. */
        MC_ASSERT(mc_drag_win != NULL);
        MC_ASSERT(GetWindowThreadProcessId(win, NULL) != GetWindowThreadProcessId(mc_drag_win, NULL));
    }
    LeaveCriticalSection(&mc_drag_lock);

    return ret;
}

mc_drag_state_t
mc_drag_consider_start(HWND win, int x, int y)
{
    mc_drag_state_t ret;

    EnterCriticalSection(&mc_drag_lock);
    if(!mc_drag_running  &&  win == mc_drag_win) {
        int drag_cx, drag_cy;
        RECT rect;

        drag_cx = GetSystemMetrics(SM_CXDRAG);
        drag_cy = GetSystemMetrics(SM_CYDRAG);

        rect.left = mc_drag_start_x - drag_cx;
        rect.top = mc_drag_start_y - drag_cy;
        rect.right = mc_drag_start_x + drag_cx + 1;
        rect.bottom = mc_drag_start_y + drag_cy + 1;

        if(!mc_rect_contains_xy(&rect, x, y)) {
            mc_drag_running = TRUE;
            ret = MC_DRAG_STARTED;
        } else {
            /* Still not clear whether we should start the dragging. Maybe
             * next WM_MOUSEMOVE will finally decide it. */
            ret = MC_DRAG_CONSIDERING;
        }
    } else {
        ret = MC_DRAG_CANCELED;
    }
    LeaveCriticalSection(&mc_drag_lock);

    return ret;
}

mc_drag_state_t
mc_drag_start(HWND win, int start_x, int start_y)
{
    mc_drag_state_t ret;

    EnterCriticalSection(&mc_drag_lock);
    if(!mc_drag_running) {
        mc_drag_running = TRUE;
        mc_drag_win = win;
        mc_drag_start_x = start_x;
        mc_drag_start_y = start_y;
        ret = MC_DRAG_STARTED;
    } else {
        ret = MC_DRAG_CANCELED;
    }
    LeaveCriticalSection(&mc_drag_lock);

    return ret;
}

void
mc_drag_stop(HWND win)
{
    EnterCriticalSection(&mc_drag_lock);
    MC_ASSERT(mc_drag_running);
    MC_ASSERT(win == mc_drag_win);
    mc_drag_running = FALSE;
    LeaveCriticalSection(&mc_drag_lock);
}


/**************************
 *** Assorted Utilities ***
 **************************/

int
mc_init_comctl32(DWORD icc)
{
    INITCOMMONCONTROLSEX icce = { 0 };

    icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icce.dwICC = icc;

    if(MC_ERR(!InitCommonControlsEx(&icce))) {
        MC_TRACE_ERR("mc_init_comctl32: InitCommonControlsEx() failed.");
        return -1;
    }

    return 0;
}

void
mc_icon_size(HICON icon, SIZE* size)
{
    ICONINFO ii;
    BITMAP bmp;

    if(icon == NULL) {
        size->cx = 0;
        size->cy = 0;
        return;
    }

    GetIconInfo(icon, &ii);
    GetObject(ii.hbmMask, sizeof(BITMAP), &bmp);

    size->cx = bmp.bmWidth;
    size->cy = bmp.bmHeight;

    /* In cases the HICON is monochromatic both the icon and its mask
     * are stored in the hbmMask member (upper half is the icon, the
     * lower half is the mask). */
    if(ii.hbmColor == NULL)
        size->cy /= 2;
    else
        DeleteObject(ii.hbmColor);

    DeleteObject(ii.hbmMask);
}

void
mc_font_size(HFONT font, SIZE* size, BOOL include_external_leading)
{
    /* See http://support.microsoft.com/kb/125681 */
    static const TCHAR canary_str[] = _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    static const int canary_len = MC_SIZEOF_ARRAY(canary_str) - 1;
    HDC dc;
    HFONT old_font;
    SIZE s;
    TEXTMETRIC tm;

    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    old_font = SelectObject(dc, font);
    GetTextExtentPoint32(dc, canary_str, canary_len, &s);
    GetTextMetrics(dc, &tm);
    SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);

    size->cx = (s.cx / (canary_len/2) + 1) / 2;
    size->cy = tm.tmHeight;

    if(include_external_leading)
        size->cy += tm.tmExternalLeading;
}

void
mc_string_size(const TCHAR* str, HFONT font, SIZE* size)
{
    HDC dc;
    HFONT old_font;

    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    old_font = SelectObject(dc, font);
    GetTextExtentPoint32(dc, str, _tcslen(str), size);
    SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);
}

int
mc_pixels_from_dlus(HFONT font, int dlus, BOOL vert)
{
    SIZE font_size;
    mc_font_size(font, &font_size, FALSE);
    if(vert)
        return (dlus * font_size.cy + 2) / 8;
    else
        return (dlus * font_size.cx + 2) / 4;
}

int
mc_dlus_from_pixels(HFONT font, int pixels, BOOL vert)
{
    SIZE font_size;
    mc_font_size(font, &font_size, FALSE);
    if(vert)
        return (16 * pixels + font_size.cy) / (2 * font_size.cy);
    else
        return (8 * pixels + font_size.cx) / (2 * font_size.cx);
}


/**********************
 *** Initialization ***
 **********************/

#if DEBUG >= 2

/* Unit testing forward declarations. */
void rgn16_test(void);

static void
perform_unit_tests(void)
{
    rgn16_test();
}

#endif  /* #if DEBUG >= 2 */


#ifdef DEBUG
    #define DEFINE_WIN_VERSION(id, workstation_name, server_name)           \
                { id, workstation_name, server_name }
#else
    #define DEFINE_WIN_VERSION(id, workstation_name, server_name)           \
                { id }
#endif

static const struct {
    DWORD version;
#ifdef DEBUG
    const char* name;
    const char* server_name;    /* optional */
#endif
} win_versions[] = {
    DEFINE_WIN_VERSION( MC_WIN_10,        "Windows 10",         "Windows Server 2016" ),
    DEFINE_WIN_VERSION( MC_WIN_8_1,       "Windows 8.1",        "Windows Server 2012R2" ),
    DEFINE_WIN_VERSION( MC_WIN_8,         "Windows 8",          "Windows Server 2012" ),
    DEFINE_WIN_VERSION( MC_WIN_7_SP1,     "Windows 7 SP1",      "Windows Server 2008R2 SP1" ),
    DEFINE_WIN_VERSION( MC_WIN_7,         "Windows 7",          "Windows Server 2008R2" ),
    DEFINE_WIN_VERSION( MC_WIN_VISTA_SP2, "Windows Vista SP2",  "Windows Server 2008 SP2" ),
    DEFINE_WIN_VERSION( MC_WIN_VISTA_SP1, "Windows Vista SP1",  "Windows Server 2008 SP1" ),
    DEFINE_WIN_VERSION( MC_WIN_VISTA,     "Windows Vista",      "Windows Server 2008" ),
    DEFINE_WIN_VERSION( MC_WIN_S2003_SP2, "Windows XP x64 SP2", "Windows Server 2003 SP2" ),
    DEFINE_WIN_VERSION( MC_WIN_S2003_SP1, "Windows XP x64 SP1", "Windows Server 2003 SP1" ),
    DEFINE_WIN_VERSION( MC_WIN_S2003,     "Windows XP x64",     "Windows Server 2003" ),
    DEFINE_WIN_VERSION( MC_WIN_XP_SP3,    "Windows XP SP3",     NULL ),
    DEFINE_WIN_VERSION( MC_WIN_XP_SP2,    "Windows XP SP2",     NULL ),
    DEFINE_WIN_VERSION( MC_WIN_XP_SP1,    "Windows XP SP1",     NULL ),
    DEFINE_WIN_VERSION( MC_WIN_XP,        "Windows XP",         NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP4,  "Windows 2000 SP4",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP3,  "Windows 2000 SP3",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP2,  "Windows 2000 SP2",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP1,  "Windows 2000 SP1",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000,      "Windows 2000",       NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP6,   "Windows NT4 SP6",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP5,   "Windows NT4 SP5",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP4,   "Windows NT4 SP4",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP3,   "Windows NT4 SP3",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP2,   "Windows NT4 SP2",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP1,   "Windows NT4 SP1",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4,       "Windows NT4",        NULL )
};

static void
setup_win_version(void)
{
    OSVERSIONINFOEX ver;
    DWORDLONG cond_mask;
    int i;

    cond_mask = 0;
    cond_mask = VerSetConditionMask(cond_mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond_mask = VerSetConditionMask(cond_mask, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond_mask = VerSetConditionMask(cond_mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    for(i = 0; i < MC_SIZEOF_ARRAY(win_versions); i++) {
        ver.dwMajorVersion = (win_versions[i].version & 0x00ff0000) >> 16;
        ver.dwMinorVersion = (win_versions[i].version & 0x0000ff00) >> 8;
        ver.wServicePackMajor = (win_versions[i].version & 0x000000ff) >> 0;
        if(VerifyVersionInfo(&ver, VER_MAJORVERSION | VER_MINORVERSION |
                             VER_SERVICEPACKMAJOR, cond_mask)) {
#ifdef DEBUG
            BOOL is_server;
            const char* name;
            const char* extra;

            cond_mask = VerSetConditionMask(0, VER_PRODUCT_TYPE, VER_EQUAL);
            ver.wProductType = VER_NT_WORKSTATION;
            is_server = !VerifyVersionInfo(&ver, VER_PRODUCT_TYPE, cond_mask);

            name = win_versions[i].name;
            extra = "";
            if(is_server) {
                if(win_versions[i].server_name != NULL)
                    name = win_versions[i].server_name;
                else
                    extra = " (server)";
            }

            MC_TRACE("setup_win_version: Detected %s%s.", name, extra);
#endif
            mc_win_version = win_versions[i].version;
            return;
        }
    }

    MC_TRACE("setup_win_version: Failed to detect Windows version.");
    mc_win_version = 0;
}

static void
setup_comctl32_version(HMODULE dll)
{
    DLLGETVERSIONPROC fn_DllGetVersion;
    DLLVERSIONINFO vi;

    /* Detect COMCTL32.DLL version */
    fn_DllGetVersion = (DLLGETVERSIONPROC) GetProcAddress(dll, "DllGetVersion");
    if(MC_ERR(fn_DllGetVersion == NULL)) {
        MC_TRACE("setup_comctl32_version: DllGetVersion not found. "
                 "Assuming COMCTL32.DLL 4.0");
        mc_comctl32_version = MC_DLL_VER(4, 0);
        return;
    }

    vi.cbSize = sizeof(DLLVERSIONINFO);
    fn_DllGetVersion(&vi);
    MC_TRACE("setup_comctl32_version: Detected COMCTL32.DLL %u.%u (build %u)",
             (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
             (unsigned)vi.dwBuildNumber);

    mc_comctl32_version = MC_DLL_VER(vi.dwMajorVersion, vi.dwMinorVersion);
}

int
mc_init_module(void)
{
    HMODULE dll_comctl32;

    /* GetModuleHandle() is safe here instead of LoadLibrary() because
     * MCTRL.DLL is linked with COMCTL32.DLL. Hence it already has been
     * mapped in memory of the process when we come here.
     */
    dll_comctl32 = GetModuleHandle(_T("COMCTL32.DLL"));
    MC_ASSERT(dll_comctl32 != NULL);

    /* Init common controls. */
    if(MC_ERR(mc_init_comctl32(ICC_STANDARD_CLASSES) != 0)) {
        MC_TRACE("mc_init_module: mc_init_comctl32() failed.");
        return -1;
    }

    /* Load set of helper symbols used for helper buttons of more complex
     * controls */
    mc_bmp_glyphs = ImageList_LoadBitmap(mc_instance, MAKEINTRESOURCE(
                           IDR_GLYPHS), MC_BMP_GLYPH_W, 1, RGB(255,0,255));
    if(MC_ERR(mc_bmp_glyphs == NULL)) {
        MC_TRACE_ERR("mc_init_module: ImageList_LoadBitmap() failed");
        return -1;
    }

    /* Retrieve version of Windows and COMCTL32.DLL */
    setup_win_version();
    setup_load_sys_dll();
    setup_comctl32_version(dll_comctl32);

    InitializeCriticalSection(&mc_wheel_lock);
    InitializeCriticalSection(&mc_drag_lock);

#if DEBUG >= 2
    /* In debug builds, we may want to run few basic unit tests. */
    perform_unit_tests();
#endif

    /* Success */
    return 0;
}

void
mc_fini_module(void)
{
    DeleteCriticalSection(&mc_drag_lock);
    DeleteCriticalSection(&mc_wheel_lock);

    ImageList_Destroy(mc_bmp_glyphs);
}


/*****************
 *** DllMain() ***
 *****************/

BOOL WINAPI
DllMain(HINSTANCE instance, DWORD reason, VOID* ignored)
{
    switch(reason) {
        case DLL_PROCESS_ATTACH:
            MC_TRACE("***************************************"
                     "***************************************");
            MC_TRACE("DllMain(DLL_PROCESS_ATTACH): MCTRL.DLL version %hs (%d bit)",
                     MC_VERSION_STR, 8 * sizeof(void*));

            mc_instance = instance;
            DisableThreadLibraryCalls(instance);

            /* This is somewhat scary to do within DllMain() context because of
             * all the limitations DllMain() imposes. Unfortunately we need it
             * this early for compat_init() below.
             *
             * I believe it's valid to do here because system DLL loader lock
             * is reentrant and KERNEL32.DLL has already be mapped into our
             * process memory and initialized.
             *
             * (Otherwise InitializeCriticalSection() would not be usable in
             * DllMain() but creation of synchronization objects is explicitly
             * allowed by MSDN docs.)
             */
            mc_instance_kernel32 = GetModuleHandle(_T("KERNEL32.DLL"));
            if(MC_ERR(mc_instance_kernel32 == NULL)) {
                MC_TRACE_ERR("DllMain: GetModuleHandle(KERNEL32.DLL) failed.");
                return FALSE;
            }

            /* Perform very minimal initialization. Most complex stuff,
             * especially any registration of window classes, is deferred into
             * public functions exposed from the DLL. (See module.c)
             *
             * BEWARE when changing this: All these functions are very limited
             * in what they can do (because of DllMain() context) and their
             * order is very important (see the comments) because they may
             * do some cooperation with preprocessor magic replacing some
             * WinAPI function with our wrapper.
             */
            compat_init();  /* <-- must precede any InitializeCriticalSection(). */
            debug_init();   /* <-- must precede any malloc(). */
            module_init();
            xcom_init();
            break;

        case DLL_PROCESS_DETACH:
        {
            xcom_fini();
            module_fini();
            debug_fini();
            compat_fini();
            MC_TRACE("DllMain(DLL_PROCESS_DETACH)");
            break;
        }
    }

    return TRUE;
}


/* Include the main public header file as we actually never do this elsewhere.
 * This at least verifies there is no compilation problem with it,
 * e.g. some idendifier clash between some of the public headers etc. */
#include "mctrl.h"

