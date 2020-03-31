/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2008-2020 Martin Mitas
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
#include "xcom.h"


/***************
 *** Globals ***
 ***************/

HINSTANCE mc_instance;
HINSTANCE mc_instance_kernel32;
DWORD mc_win_version;
DWORD mc_comctl32_version;


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
    TCHAR* rsrc_id = MC_RES_ID(id/16 + 1);
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
    /* Win Vista/7 prior the security update KB2533623 does not support
     * the flag LOAD_LIBRARY_SEARCH_SYSTEM32. The update KB2533623 also added
     * AddDllDirectory() so we use that as the indicator whether we can use
     * the flag. */
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


/**************************
 *** Clipping Utilities ***
 **************************/

HRGN
mc_clip_get(HDC dc)
{
    HRGN old_clip;

    old_clip = CreateRectRgn(0, 0, 0, 0);
    if(GetClipRgn(dc, old_clip) != 1) {
        DeleteObject(old_clip);
        return NULL;
    }

    return old_clip;
}

void
mc_clip_set(HDC dc, LONG left, LONG top, LONG right, LONG bottom)
{
    HRGN clip;
    POINT origin;

    /* SelectClipRgn() uses physical coordinates. We need to use logical ones
     * as we may change origin in case of double-buffered painting.
     *
     * See https://github.com/mity/mctrl/issues/45
     */
    GetWindowOrgEx(dc, &origin);

    clip = CreateRectRgn(left - origin.x, top - origin.y,
                         right - origin.x, bottom - origin.y);
    if(MC_ERR(clip == NULL)) {
        MC_TRACE("mc_clip_set: CreateRectRgn() failed.");
        return;
    }

    SelectClipRgn(dc, clip);
    DeleteObject(clip);
}

void
mc_clip_reset(HDC dc, HRGN old_clip)
{
    SelectClipRgn(dc, old_clip);
    if(old_clip != NULL)
        DeleteObject(old_clip);
}


/***********************
 *** Blocker Utility ***
 ***********************/

static mc_mutex_t mc_blocker_mutex = MC_MUTEX_INIT;
static HWND mc_blocker_win;
static UINT_PTR mc_blocker_id;
static DWORD mc_blocker_start = 0;

void
mc_msgblocker_start(HWND win, UINT_PTR id)
{
    mc_mutex_lock(&mc_blocker_mutex);
    mc_blocker_win = win;
    mc_blocker_id = id;
    mc_blocker_start = GetTickCount();
    mc_mutex_unlock(&mc_blocker_mutex);
}

BOOL
mc_msgblocker_query(HWND win, UINT_PTR id)
{
    BOOL ret = TRUE;

    mc_mutex_lock(&mc_blocker_mutex);
    if(win == mc_blocker_win  &&  id == mc_blocker_id) {
        LONG period = GetMessageTime() - mc_blocker_start;
        ret = !(-200 <= period  &&  period <= 0);
    }
    mc_mutex_unlock(&mc_blocker_mutex);

    return ret;
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

BOOL
mc_is_rtl_exstyle(DWORD exstyle)
{
    BOOL rtl = FALSE;

    /* If both are set, they cancel each other. */
    if(exstyle & WS_EX_LAYOUTRTL)
        rtl = !rtl;
    if(exstyle & WS_EX_RTLREADING)
        rtl = !rtl;

    return rtl;
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
    DEFINE_WIN_VERSION( MC_WIN_10,        "10",         "Server 2016" ),
    DEFINE_WIN_VERSION( MC_WIN_8_1,       "8.1",        "Server 2012R2" ),
    DEFINE_WIN_VERSION( MC_WIN_8,         "8",          "Server 2012" ),
    DEFINE_WIN_VERSION( MC_WIN_7_SP1,     "7 SP1",      "Server 2008R2 SP1" ),
    DEFINE_WIN_VERSION( MC_WIN_7,         "7",          "Server 2008R2" ),
    DEFINE_WIN_VERSION( MC_WIN_VISTA_SP2, "Vista SP2",  "Server 2008 SP2" ),
    DEFINE_WIN_VERSION( MC_WIN_VISTA_SP1, "Vista SP1",  "Server 2008 SP1" ),
    DEFINE_WIN_VERSION( MC_WIN_VISTA,     "Vista",      "Server 2008" ),
    DEFINE_WIN_VERSION( MC_WIN_S2003_SP2, "XP x64 SP2", "Server 2003 SP2" ),
    DEFINE_WIN_VERSION( MC_WIN_S2003_SP1, "XP x64 SP1", "Server 2003 SP1" ),
    DEFINE_WIN_VERSION( MC_WIN_S2003,     "XP x64",     "Server 2003" ),
    DEFINE_WIN_VERSION( MC_WIN_XP_SP3,    "XP SP3",     NULL ),
    DEFINE_WIN_VERSION( MC_WIN_XP_SP2,    "XP SP2",     NULL ),
    DEFINE_WIN_VERSION( MC_WIN_XP_SP1,    "XP SP1",     NULL ),
    DEFINE_WIN_VERSION( MC_WIN_XP,        "XP",         NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP4,  "2000 SP4",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP3,  "2000 SP3",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP2,  "2000 SP2",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000_SP1,  "2000 SP1",   NULL ),
    DEFINE_WIN_VERSION( MC_WIN_2000,      "2000",       NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP6,   "NT4 SP6",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP5,   "NT4 SP5",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP4,   "NT4 SP4",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP3,   "NT4 SP3",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP2,   "NT4 SP2",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4_SP1,   "NT4 SP1",    NULL ),
    DEFINE_WIN_VERSION( MC_WIN_NT4,       "NT4",        NULL )
};

static void
setup_win_version(void)
{
    OSVERSIONINFOEX info;
    int i;

    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((OSVERSIONINFO*) &info);
    mc_win_version = MC_WIN_VER(info.dwMajorVersion, info.dwMinorVersion, info.wServicePackMajor);

    /* GetVersion() on Windows 8.1 or newer still report as Windows 8.0 unless
     * .exe manifest explicitly declares compatibility with newer Windows.
     * So we use the machinery with VerifyVersionInfo() to get the real info. */
    if(mc_win_version >= MC_WIN_8) {
        const DWORD ver_mask =  VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
        DWORDLONG cond_mask = 0;

        cond_mask = VerSetConditionMask(cond_mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        cond_mask = VerSetConditionMask(cond_mask, VER_MINORVERSION, VER_GREATER_EQUAL);
        cond_mask = VerSetConditionMask(cond_mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

        for(i = 0; i < MC_SIZEOF_ARRAY(win_versions); i++) {
            info.dwMajorVersion = (win_versions[i].version & 0x00ff0000) >> 16;
            info.dwMinorVersion = (win_versions[i].version & 0x0000ff00) >> 8;
            info.wServicePackMajor = (win_versions[i].version & 0x000000ff) >> 0;

            if(VerifyVersionInfo(&info, ver_mask, cond_mask)) {
                mc_win_version = win_versions[i].version;
                break;
            }
        }
    }

#ifdef DEBUG
    /* Log the detected Windows version. */
    {
#ifndef IMAGE_FILE_MACHINE_ARM64
    #define IMAGE_FILE_MACHINE_ARM64    0xAA64
#endif
        
        const char* name = "???";
        const char* prefix = "";
        const char* suffix = "";
        BOOL is_64bit = FALSE;
        BOOL is_server = FALSE;

        is_64bit = (sizeof(void*) == 8);
        if(!is_64bit) {
            BOOL (WINAPI *fn_IsWow64Process)(HANDLE, BOOL*);

            fn_IsWow64Process = (BOOL (WINAPI *)(HANDLE, BOOL*))
                        GetProcAddress(mc_instance_kernel32, "IsWow64Process");
            if(fn_IsWow64Process != NULL)
                fn_IsWow64Process(GetCurrentProcess(), &is_64bit);
        }
        if (!is_64bit) {
            BOOL(WINAPI * fn_IsWow64Process2)(HANDLE, USHORT*, USHORT*);
            
            fn_IsWow64Process2 = (BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*))
                        GetProcAddress(mc_instance_kernel32, "IsWow64Process2");
            if (fn_IsWow64Process2 != NULL) {
                USHORT process_machine;
                USHORT native_machine;
                fn_IsWow64Process2(GetCurrentProcess(), &process_machine, &native_machine);
                if (native_machine == IMAGE_FILE_MACHINE_ARM64)
                    is_64bit = TRUE;
            }
        }
        if(is_64bit)
            prefix = "64-bit ";

        for(i = 0; i < MC_SIZEOF_ARRAY(win_versions); i++) {
            if(mc_win_version == win_versions[i].version) {
                info.wProductType = VER_NT_WORKSTATION;
                is_server = !VerifyVersionInfo(&info, VER_PRODUCT_TYPE,
                                VerSetConditionMask(0, VER_PRODUCT_TYPE, VER_EQUAL));

                name = win_versions[i].name;
                if(is_server) {
                    if(win_versions[i].server_name != NULL)
                        name = win_versions[i].server_name;
                    else
                        suffix = " server";
                }
                break;
            }
        }

        MC_TRACE("setup_win_version: Detected %sWindows %s%s (%u.%u SP%u).",
                prefix, name, suffix, (mc_win_version & 0x00ff0000) >> 16,
                (mc_win_version & 0x0000ff00) >> 8, (mc_win_version & 0x000000ff) >> 0);
    }
#endif
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

    /* Retrieve version of Windows and COMCTL32.DLL */
    setup_win_version();
    setup_load_sys_dll();
    setup_comctl32_version(dll_comctl32);

    /* Init ICC_STANDARD_CLASSES if we are using COMCTL32.DLL version 6.0
     * or higher. With older versions, the legacy std. controls from USER32.DLL
     * are used. */
    if(mc_comctl32_version >= MC_DLL_VER(6, 0)) {
        if(MC_ERR(mc_init_comctl32(ICC_STANDARD_CLASSES) != 0)) {
            MC_TRACE("mc_init_module: mc_init_comctl32() failed.");
            return -1;
        }
    }

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
    /* noop */
}


/*****************
 *** DllMain() ***
 *****************/

/* Forward declarations of various initialization routines for some helper
 * modules. (We don't pollute the namespace in headers with these.)
 */
void debug_dllmain_init(void);
void debug_dllmain_fini(void);


static int
dllmain_init(HINSTANCE instance)
{
    MC_TRACE("****************************************************************");
    MC_TRACE("MCTRL.DLL version %hs (%d bit)", MC_VERSION_STR, 8 * sizeof(void*));

    mc_instance = instance;

    mc_instance_kernel32 = GetModuleHandle(_T("KERNEL32.DLL"));
    if(MC_ERR(mc_instance_kernel32 == NULL)) {
        MC_TRACE_ERR("dllmain_init: GetModuleHandle(KERNEL32.DLL) failed.");
        return -1;
    }

    /* BEWARE when changing this: All these functions are very limited in what
     * they can do because of DllMain() context.
     *
     * More complex stuff, especially any registration of window classes, has
     * to be deferred into public functions exposed from the DLL.
     * (That is handled in module.c.)
     */
    debug_dllmain_init();   /* <-- Keep 1st, must precede any malloc(). */

    return 0;
}

static void
dllmain_fini(void)
{
    debug_dllmain_fini();
}

#ifdef MCTRL_BUILD_STATIC
    int mc_resource_id_base = 0;

    /* Building as a static lib. */
    BOOL MCTRL_API
    mcInitialize(HINSTANCE instance, int resource_id_base)
    {
        mc_resource_id_base = resource_id_base;

        if(MC_ERR(dllmain_init(instance) != 0)) {
            MC_TRACE("mcInitialize: dllmain_init() failed.");
            return FALSE;
        }

        return TRUE;
    }

    void MCTRL_API
    mcTerminate(void)
    {
        dllmain_fini();
    }
#else
    /* Building as a DLL. */
    BOOL WINAPI
    DllMain(HINSTANCE instance, DWORD reason, VOID* ignored)
    {
        switch(reason) {
            case DLL_PROCESS_ATTACH:
                DisableThreadLibraryCalls(instance);
                if(MC_ERR(dllmain_init(instance) != 0))
                    return FALSE;
                break;

            case DLL_PROCESS_DETACH:
                dllmain_fini();
                break;
        }

        return TRUE;
    }
#endif


/* Include the main public header file as we actually never do this elsewhere.
 * This at least verifies there is no compilation problem with it,
 * e.g. some identifier clash between some of the public headers etc. */
#include "mctrl.h"

