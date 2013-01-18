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
#include "module.h"


/***************
 *** Globals ***
 ***************/

HINSTANCE mc_instance;

DWORD mc_win_version;
DWORD mc_comctl32_version;

HIMAGELIST mc_bmp_glyphs;


static void (WINAPI *fn_InitCommonControlsEx)(INITCOMMONCONTROLSEX*) = NULL;


/************************
 *** String Utilities ***
 ************************/

TCHAR*
mc_str_load(UINT id)
{
#ifndef UNICODE
    #error mc_str_load() is not (yet?) implemented for ANSII build.
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

    for(i = 0; i < MC_ARRAY_SIZE(lang_id); i++) {
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

        MC_ASSERT(str[len - 1] == L'\0');
        return str;

not_found:
        MC_TRACE("mc_str_load: String %u missing [language 0x%x].", id,
                 (DWORD)(lang_id[i] == MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)
                    ? LANGIDFROMLCID(GetThreadLocale()) : lang_id[i]));
    }

    return _T("");
}

char*
mc_str_n_A2A(const char* from_str, int from_len, int* ptr_to_len)
{
    char* to_str;
    int to_len;

    if(from_str == NULL)
        return NULL;

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

    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;

    return to_str;
}

WCHAR*
mc_str_n_W2W(const WCHAR* from_str, int from_len, int* ptr_to_len)
{
    WCHAR* to_str;
    int to_len;

    if(from_str == NULL)
        return NULL;

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

    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;

    return to_str;
}

WCHAR*
mc_str_n_A2W(const char* from_str, int from_len, int* ptr_to_len)
{
    WCHAR* to_str;
    int to_len;

    if(from_str == NULL)
        return NULL;

    if(from_len < 0)
        from_len = (int)strlen((char*)from_str);

    to_len = MultiByteToWideChar(CP_ACP, 0, from_str, from_len, NULL, 0);
    to_str = (WCHAR*) malloc((to_len + 1) * sizeof(WCHAR));
    if(MC_ERR(to_str == NULL)) {
        MC_TRACE("mc_str_n_A2W: malloc() failed.");
        return NULL;
    }
    MultiByteToWideChar(CP_ACP, 0, from_str, from_len, to_str, to_len);
    to_str[to_len] = L'\0';

    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;

    return to_str;
}

char*
mc_str_n_W2A(const WCHAR* from_str, int from_len, int* ptr_to_len)
{
    char* to_str;
    int to_len;

    if(from_str == NULL)
        return NULL;

    if(from_len < 0)
        from_len = (int)(wcslen((WCHAR*)from_str) * sizeof(WCHAR));

    to_len = WideCharToMultiByte(CP_ACP, 0, from_str, from_len,
                                 NULL, 0, NULL, NULL);
    to_str = (char*) malloc(to_len + 1);
    if(MC_ERR(to_str == NULL)) {
        MC_TRACE("mc_str_n_W2A: malloc() failed.");
        return NULL;
    }
    WideCharToMultiByte(CP_ACP, 0, from_str, from_len,
                        to_str, to_len, NULL, NULL);
    to_str[to_len] = '\0';

    if(ptr_to_len != NULL)
        *ptr_to_len = to_len;

    return to_str;
}


/*****************************
 *** Mouse Wheel Utilities ***
 *****************************/

int
mc_wheel_scroll(HWND win, BOOL is_vertical, int wheel_delta)
{
    /* We accumulate the wheel_delta until there is enough to scroll for
     * at least a single line. This improves the feel for strange values
     * of SPI_GETWHEELSCROLLLINES and for some mouses. */
    static HWND last_win = NULL;
    static int wheel[2] = { 0, 0 };  /* horizontal, vertical */

    int dir;
    int lines_per_WHEEL_DELTA;
    int lines_to_scroll;

    if(win != last_win) {
        last_win = win;
        wheel[0] = 0;
        wheel[1] = 0;

        if(win == NULL)  /* just reset */
            return 0;
    }

    if(MC_ERR(!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines_per_WHEEL_DELTA, 0)))
        lines_per_WHEEL_DELTA = 3;

    dir = (is_vertical ? 1 : 0);

    if((wheel_delta > 0 && wheel[dir] < 0) || (wheel_delta < 0 && wheel[dir] > 0))
        wheel[dir] = 0;

    wheel[dir] += wheel_delta;
    lines_to_scroll = (wheel[dir] * lines_per_WHEEL_DELTA) / WHEEL_DELTA;
    if(lines_to_scroll != 0)
        wheel[dir] = 0;

    return (is_vertical ? -lines_to_scroll : lines_to_scroll);
}


/********************************
 *** Double-buffered Painting ***
 ********************************/

void
mc_doublebuffer(void* control, PAINTSTRUCT* ps,
                void (*func_paint)(void*, HDC, RECT*, BOOL))
{
    int w, h;
    HDC mem_dc;
    HBITMAP bmp;
    HBITMAP old_bmp;
    POINT old_origin;

    w = mc_width(&ps->rcPaint);
    h = mc_height(&ps->rcPaint);

    mem_dc = CreateCompatibleDC(ps->hdc);
    if(MC_ERR(mem_dc == NULL))
        goto err_CreateCompatibleDC;
    bmp = CreateCompatibleBitmap(ps->hdc, w, h);
    if(MC_ERR(bmp == NULL))
        goto err_CreateCompatibleBitmap;

    old_bmp = SelectObject(mem_dc, bmp);
    OffsetViewportOrgEx(mem_dc, -(ps->rcPaint.left), -(ps->rcPaint.top), &old_origin);
    func_paint(control, mem_dc, &ps->rcPaint, TRUE);
    SetViewportOrgEx(mem_dc, old_origin.x, old_origin.y, NULL);

    BitBlt(ps->hdc, ps->rcPaint.left, ps->rcPaint.top, w, h, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    return;

    /* Error path: */
err_CreateCompatibleBitmap:
    DeleteDC(mem_dc);
err_CreateCompatibleDC:
    func_paint(control, ps->hdc, &ps->rcPaint, ps->fErase);
}


/**************************
 *** Assorted Utilities ***
 **************************/

void
mc_init_common_controls(DWORD icc)
{
    if(fn_InitCommonControlsEx != NULL) {
        INITCOMMONCONTROLSEX icce = { 0 };
        icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icce.dwICC = icc;
        fn_InitCommonControlsEx(&icce);
    } else {
        InitCommonControls();
    }
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
mc_font_size(HFONT font, SIZE* size)
{
    /* See http://support.microsoft.com/kb/125681 */
    static const TCHAR canary_str[] = _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    static const int canary_len = MC_ARRAY_SIZE(canary_str) - 1;
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
}

int
mc_pixels_from_dlus(HFONT font, int dlus, BOOL vert)
{
    SIZE font_size;
    mc_font_size(font, &font_size);
    if(vert)
        return (dlus * font_size.cy + 2) / 8;
    else
        return (dlus * font_size.cx + 2) / 4;
}

int
mc_dlus_from_pixels(HFONT font, int pixels, BOOL vert)
{
    SIZE font_size;
    mc_font_size(font, &font_size);
    if(vert)
        return (16 * pixels + font_size.cy) / (2 * font_size.cy);
    else
        return (8 * pixels + font_size.cx) / (2 * font_size.cx);
}


/**********************
 *** Initialization ***
 **********************/

#ifdef DEBUG
static const char*
get_win_name(BYTE type)
{
    BOOL is_server = (type == VER_NT_DOMAIN_CONTROLLER || type == VER_NT_SERVER);

    if(!is_server) {
        switch(mc_win_version) {
            case MC_WIN_95:         return "Windows 95";
            case MC_WIN_98:         return "Windows 98";
            case MC_WIN_ME:         return "Windows ME";
            case MC_WIN_NT4:        return "Windows NT 4.0";
            case MC_WIN_2000:       return "Windows 2000";
            case MC_WIN_XP:         return "Windows XP";
            case MC_WIN_VISTA:      return "Windows Vista";
            case MC_WIN_7:          return "Windows 7";
            case MC_WIN_8:          return "Windows 8";
            default:                return "Windows ???";
        }
    } else {
        switch(mc_win_version) {
            case MC_WIN_S2003:      return "Windows Server 2003";
            case MC_WIN_S2008:      return "Windows Server 2008";
            case MC_WIN_S2008R2:    return "Windows Server 2008 R2";
            default:                return "Windows Server ???";
        }
    }
}
#endif  /* #ifdef DEBUG */

static void
setup_win_version(void)
{
    OSVERSIONINFOEX ver;

    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((OSVERSIONINFO*) &ver);
    mc_win_version = MC_WIN_VER(ver.dwPlatformId,
                                ver.dwMajorVersion, ver.dwMinorVersion);

    MC_TRACE("setup_win_version: Detected %hs "
             "(%u.%u.%u, service pack %u.%u, build %u)",
            get_win_name(ver.wProductType),
            ver.dwPlatformId, ver.dwMajorVersion, ver.dwMinorVersion,
            ver.wServicePackMajor, ver.wServicePackMinor,
            ver.dwBuildNumber);
}

static void
setup_comctl32_version(void)
{
    HMODULE dll;
    DLLGETVERSIONPROC fn_DllGetVersion;
    DLLVERSIONINFO vi;

    /* GetModuleHandle() is safe here instead of LoadLibrary() because
     * MCTRL.DLL is linked with COMCTL32.DLL. Hence it already has been
     * mapped in memory of the process when we come here.
     */
    dll = GetModuleHandle(_T("COMCTL32.DLL"));
    MC_ASSERT(dll != NULL);

    /* Remember InitCommonControlsEx address for mc_init_common_controls(). */
    fn_InitCommonControlsEx = (void (WINAPI *)(INITCOMMONCONTROLSEX*))
                GetProcAddress(dll, "InitCommonControlsEx");

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
mc_init(void)
{
    /* Load set of helper symbols used for helper buttons of more complex
     * controls */
    mc_bmp_glyphs = ImageList_LoadBitmap(mc_instance, MAKEINTRESOURCE(
                           IDR_GLYPHS), MC_BMP_GLYPH_W, 1, RGB(255,0,255));
    if(MC_ERR(mc_bmp_glyphs == NULL)) {
        MC_TRACE_ERR("mc_init: ImageList_LoadBitmap() failed");
        return -1;
    }

    /* Retrieve version of Windows and COMCTL32.DLL */
    setup_win_version();
    setup_comctl32_version();

    /* Success */
    return 0;
}

void
mc_fini(void)
{
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

            debug_init();
            mc_instance = instance;
            DisableThreadLibraryCalls(mc_instance);
            module_init();
            break;

        case DLL_PROCESS_DETACH:
        {
            module_fini();
            debug_fini();
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

