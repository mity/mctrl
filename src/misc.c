/*
 * Copyright (c) 2008-2012 Martin Mitas
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


/**************************
 *** String conversions ***
 **************************/

void*
mc_str_n(const void* from_str, int from_type, int from_len,
         int to_type, int* ptr_to_len)
{
    int to_len;
    void* to_str;

    MC_ASSERT(from_type == MC_STRA || from_type == MC_STRW);
    MC_ASSERT(to_type == MC_STRA || to_type == MC_STRW);

    if(from_str == NULL)
        return NULL;

    if(from_len < 0) {
        if(from_type == MC_STRW)
            from_len = (int)(wcslen((WCHAR*)from_str) * sizeof(WCHAR));
        else
            from_len = (int)strlen((char*)from_str);
    }

    if(from_type == to_type) {
        to_len = from_len;
        if(from_type == MC_STRW) {
            /* W->W */
            to_str = malloc((to_len  + 1) * sizeof(WCHAR));
            if(MC_ERR(to_str == NULL)) {
                MC_TRACE("mc_str_n: malloc() failed.");
                return NULL;
            }
            memcpy(to_str, from_str, to_len * sizeof(WCHAR));
            ((WCHAR*)to_str)[to_len] = L'\0';
        } else {
            /* A->A */
            to_str = malloc(to_len  + 1);
            if(MC_ERR(to_str == NULL)) {
                MC_TRACE("mc_str_n: malloc() failed.");
                return NULL;
            }
            memcpy(to_str, from_str, to_len);
            ((char*)to_str)[to_len] = '\0';
        }

        if(ptr_to_len != NULL)
            *ptr_to_len = to_len;
        return to_str;
    } else {
        if(to_type == MC_STRW) {
            /* A->W */
            to_len = MultiByteToWideChar(CP_ACP, 0, from_str, from_len, NULL, 0);
            to_str = malloc((to_len + 1) * sizeof(WCHAR));
            if(MC_ERR(to_str == NULL)) {
                MC_TRACE("mc_str_n: malloc() failed.");
                return NULL;
            }
            MultiByteToWideChar(CP_ACP, 0, from_str, from_len, to_str, to_len);
            ((WCHAR*)to_str)[to_len] = L'\0';
        } else {
            /* W->A */
            to_len = WideCharToMultiByte(CP_ACP, 0, from_str, from_len,
                                         NULL, 0, NULL, NULL);
            to_str = malloc(to_len + 1);
            if(MC_ERR(to_str == NULL)) {
                MC_TRACE("mc_str_n: malloc() failed.");
                return NULL;
            }
            WideCharToMultiByte(CP_ACP, 0, from_str, from_len,
                                to_str, to_len, NULL, NULL);
            ((char*)to_str)[to_len] = '\0';
        }

        if(ptr_to_len != NULL)
            *ptr_to_len = to_len;
        return to_str;
    }
}

void
mc_str_inbuf(const void* from_str, int from_type,
             void* to_str, int to_type, int to_str_bufsize)
{
    MC_ASSERT(to_str_bufsize > 0);
    MC_ASSERT(from_type == MC_STRA  ||  from_type == MC_STRW);
    MC_ASSERT(from_type == MC_STRA  ||  from_type == MC_STRW);

    if(from_str == NULL)
        from_str = (void*)L"";

    /* No conversion cases */
    if(from_type == to_type) {
        if(from_type == MC_STRW) {  /* W->W */
            wcsncpy((WCHAR*)to_str, (WCHAR*)from_str, to_str_bufsize);
            ((WCHAR*)to_str)[to_str_bufsize-1] = L'\0';
        } else {                    /* A->A */
            strncpy((char*)to_str, (char*)from_str, to_str_bufsize);
            ((char*)to_str)[to_str_bufsize-1] = '\0';
        }
    } else {
        if(from_type == MC_STRA)  /* A->W */
            MultiByteToWideChar(CP_ACP, 0, from_str, -1, to_str, to_str_bufsize);
        else                      /* W->A */
            WideCharToMultiByte(CP_ACP, 0, from_str, -1, to_str, to_str_bufsize, NULL, NULL);
    }
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

static void (WINAPI *fn_InitCommonControlsEx)(INITCOMMONCONTROLSEX*) = NULL;

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
                           MCR_BMP_GLYPHS), MC_BMP_GLYPH_W, 1, RGB(255,0,255));
    if(MC_ERR(mc_bmp_glyphs == NULL)) {
        MC_TRACE("mc_init: ImageList_LoadBitmap() failed [%lu]", GetLastError());
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



/*************************
 *** Utility functions ***
 *************************/

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

    /* In cases the HICON is monochromatic both the icon and its mask are
     * stored in the hbmMask member (upper half is the icon, the lower half
     * is the mask). */
    if(ii.hbmColor) {
        GetObject(ii.hbmColor, sizeof(BITMAP), &bmp);
        size->cx = bmp.bmWidth;
        size->cy = bmp.bmHeight;
        DeleteObject(ii.hbmColor);
    } else {
        GetObject(ii.hbmMask, sizeof(BITMAP), &bmp);
        size->cx = bmp.bmWidth / 2;
        size->cy = bmp.bmHeight;
    }
    DeleteObject(ii.hbmMask);
}

void
mc_font_size(HFONT font, SIZE* size)
{
    static const TCHAR canary_str[] = _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    static const int canary_len = MC_ARRAY_SIZE(canary_str) - 1;

    HDC dc;
    HFONT old_font;
    SIZE s;

    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);   // FIXME: is this needed?

    dc = GetDCEx(NULL, NULL, DCX_CACHE);
    old_font = SelectObject(dc, font);
    GetTextExtentPoint(dc, canary_str, canary_len, &s);
    SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);

    size->cx = s.cx / canary_len + 1;  /* +1 to fight rounding */
    size->cy = s.cy;
}

LRESULT
mc_send_notify(HWND parent, HWND win, UINT code)
{
    NMHDR hdr;
    
    hdr.hwndFrom = win;
    hdr.idFrom = GetWindowLong(win, GWL_ID);
    hdr.code = code;
    
    return SendMessage(parent, WM_NOTIFY, hdr.idFrom, (LPARAM)&hdr);
}

int
mc_wheel_scroll(HWND win, BOOL is_vertical, int wheel_delta)
{
    static HWND cached_win = NULL;
    static int cached_delta_v = 0;
    static int cached_delta_h = 0;

    int* cached_delta;
    int lines_per_WHEEL_DELTA;
    int lines_to_scroll = 0;

    if(win != cached_win) {
        cached_win = win;
        cached_delta_v = 0;
        cached_delta_h = 0;

        if(win == NULL)  /* just reset */
            return 0;
    }

    if(MC_ERR(!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines_per_WHEEL_DELTA, 0)))
        lines_per_WHEEL_DELTA = 3;

    cached_delta = (is_vertical ? &cached_delta_v : &cached_delta_h);

    /* On wheel direction change, reset the accumulated delta */
    if(MC_SIGN(wheel_delta) != MC_SIGN(*cached_delta))
        *cached_delta = 0;

    *cached_delta += wheel_delta;

    lines_to_scroll = *cached_delta / (WHEEL_DELTA / lines_per_WHEEL_DELTA);
    *cached_delta = *cached_delta % (WHEEL_DELTA / lines_per_WHEEL_DELTA);

    return (is_vertical ? -lines_to_scroll : lines_to_scroll);
}


/*****************
 *** DllMain() ***
 *****************/

BOOL WINAPI
DllMain(HINSTANCE instance, DWORD reason, VOID* ignored)
{
    switch(reason) {
        case DLL_PROCESS_ATTACH:
#if defined DEBUG && DEBUG >= 2
            debug_init();
#endif
            MC_TRACE("***************************************"
                     "***************************************");
            MC_TRACE("DllMain(DLL_PROCESS_ATTACH): MCTRL.DLL version %hs",
                     MC_VERSION_STR);

            mc_instance = instance;
            DisableThreadLibraryCalls(mc_instance);
            module_init();
            break;

        case DLL_PROCESS_DETACH:
        {
            module_fini();
#if defined DEBUG && DEBUG >= 2
            debug_fini();
#endif
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


