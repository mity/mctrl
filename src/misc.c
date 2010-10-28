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

#include "misc.h"
#include "module.h"


/***************
 *** Globals ***
 ***************/

HINSTANCE mc_instance_exe;
HINSTANCE mc_instance_dll;

DWORD mc_win_version;
DWORD mc_comctl32_version;

HFONT mc_font;

HIMAGELIST mc_bmp_glyphs;


/**************************
 *** String converisons ***
 **************************/

void* 
mc_str_n(void* from_str, int from_type, int from_len, 
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
            from_len = (wcslen((WCHAR*)from_str)) * sizeof(WCHAR);
        else
            from_len = strlen((char*)from_str);
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
            to_len = WideCharToMultiByte(CP_ACP, WC_DEFAULTCHAR, 
                                from_str, from_len, NULL, 0, NULL, NULL);
            to_str = malloc(to_len + 1);
            if(MC_ERR(to_str == NULL)) {
                MC_TRACE("mc_str_n: malloc() failed.");
                return NULL;
            }
            WideCharToMultiByte(CP_ACP, WC_DEFAULTCHAR, 
                                from_str, from_len, to_str, to_len, NULL, NULL);
            ((char*)to_str)[to_len] = '\0';
        }

        if(ptr_to_len != NULL)
            *ptr_to_len = to_len;
        return to_str;
    }
}

void 
mc_str_inbuf(void* from_str, int from_type, 
             void* to_str, int to_type, int max_len)
{
    MC_ASSERT(max_len > 0);
    MC_ASSERT(from_type == MC_STRA  ||  from_type == MC_STRW);
    MC_ASSERT(from_type == MC_STRA  ||  from_type == MC_STRW);

    if(from_str == NULL)
        from_str = (void*)L"";

    /* No conversion cases */
    if(from_type == to_type) {
        if(from_type == MC_STRW)  /* W->W */            
            wcsncpy((WCHAR*)to_str, (WCHAR*)from_str, max_len);
        else                      /* A->A */
            strncpy((char*)to_str, (char*)from_str, max_len);
    } else {
        if(from_type == MC_STRA)  /* A->W */
            MultiByteToWideChar(CP_ACP, 0, from_str, -1, to_str, max_len);
        else                      /* W->A */
            WideCharToMultiByte(CP_ACP, 0, from_str, -1, to_str, max_len, NULL, NULL);
    }
}


/**********************
 *** Initialization ***
 **********************/

#ifdef DEBUG
static const char*
get_win_name(void)
{
    switch(mc_win_version) {
        case MC_WIN_95:     return "Windows 95";
        case MC_WIN_98:     return "Windows 98";
        case MC_WIN_ME:     return "Windows ME";
        case MC_WIN_NT4:    return "Windows NT 4.0";
        case MC_WIN_2000:   return "Windows 2000";
        case MC_WIN_XP:     return "Windows XP";
        case MC_WIN_S2003:  return "Windows Server 2003";
        case MC_WIN_VISTA: /* == MC_OS_S2008 */
                            return "Windows Vista / Server 2008";
        case MC_WIN_7:     /* == MC_OS_S2008R2 */
                            return "Windows 7 / Server 2008R2";
        default:            return "Windows ???";
    }
}
#endif  /* #ifdef DEBUG */

static void
setup_win_version(void)
{
    OSVERSIONINFO os_version;

    os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os_version);
    mc_win_version = MC_WIN_VER(os_version.dwPlatformId,
            os_version.dwMajorVersion, os_version.dwMinorVersion);
            
    MC_TRACE(
#ifdef UNICODE
        "setup_win_version: Detected %hs %ls (%u.%u.%u.%u)",
#else
        "setup_win_version: Detected %hs %hs (%u.%u.%u.%u)",
#endif            
        get_win_name(), os_version.szCSDVersion,
        os_version.dwPlatformId, os_version.dwMajorVersion,
        os_version.dwMinorVersion, os_version.dwBuildNumber
    );
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

    /* Rember InitCommonControlsEx address for mc_init_common_controls(). */
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
    MC_TRACE("setup_comctl32_version: Detected COMCTL32.DLL %u.%u.%u.%u",
             (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion,
             (unsigned)vi.dwBuildNumber, (unsigned)vi.dwPlatformID);
    mc_comctl32_version = MC_DLL_VER(vi.dwMajorVersion, vi.dwMinorVersion);
}

void
mc_init_common_controls(DWORD icc)
{
    if(fn_InitCommonControlsEx != NULL) {
        INITCOMMONCONTROLSEX icce;
        icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icce.dwICC = icc;
        fn_InitCommonControlsEx(&icce);
    } else {
        InitCommonControls();
    }
}

int
mc_init(void)
{
    int pixels_per_inch;
    
    /* Retrieve version of Windows and COMCTL32.DLL */
    setup_win_version();
    setup_comctl32_version();

    /* Get default font */
    pixels_per_inch = GetDeviceCaps(GetDC(NULL), LOGPIXELSY);
    mc_font = CreateFont(-MulDiv(8, pixels_per_inch, 72), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
                         mc_win_version < MC_WIN_2000 ? _T("MS Shell Dlg") : _T("MS Shell Dlg 2"));
    if(MC_ERR(mc_font == NULL)) {
        MC_TRACE("mc_init: CreateFont() failed [%lu]", GetLastError());
        goto err_CreateFont;
    }

    /* Load set of helper symbols used for helper buttons of more complex 
     * controls */
    mc_bmp_glyphs = ImageList_LoadBitmap(mc_instance_dll, MAKEINTRESOURCE(
                           MCR_BMP_GLYPHS), MC_BMP_GLYPH_W, 1, RGB(255,0,255));
    if(MC_ERR(mc_bmp_glyphs == NULL)) {
        MC_TRACE("mc_init: ImageList_LoadBitmap() failed [%lu]", GetLastError());
        goto err_ImageList_LoadBitmap;
    }
    
    /* Success */
    return 0;

    /* Error unwinding */
err_ImageList_LoadBitmap:
    DeleteObject(mc_font);
err_CreateFont:
    return -1;
}

void
mc_fini(void)
{
    DeleteObject(mc_font);
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
#if defined DEBUG && DEBUG >= 2
            debug_init();
#endif
            MC_TRACE("****************************************");
            MC_TRACE("DllMain(DLL_PROCESS_ATTACH): MCTRL.DLL version %hs",
                     MC_VERSION_STR);

            /* Remember instance of MCTRL.DLL */
            mc_instance_dll = instance;
            /* Remember instance of the application we are in */
            mc_instance_exe = GetModuleHandle(NULL);

            /* We are not interested in DLL_THREAD_ATTACH/DETACH so 
             * lets save some CPU cycles in multithreaded applications */
            DisableThreadLibraryCalls(mc_instance_dll);            
            
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
