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

#include "theme.h"
#include <shlwapi.h>  /* DLLVERSIONINFO */


#ifndef UNICODE
    /* Right now, we don't supprot theming in non-unicode builds. */
    #define DISABLE_UXTHEME     1
#endif


#ifndef DISABLE_UXTHEME
    /* Preprocessor magic to define pointers to UXTHEME.DLL functions */
    #define THEME_FN(rettype, name, params)            \
        rettype (WINAPI* theme_##name)params;
    #include "theme_fn.h"
    #undef THEME_FN

    static HMODULE uxtheme_dll = NULL;
#endif  /* #ifndef DISABLE_UXTHEME */


/* Dummy implementations of some UXTHEME.DLL functions so controls can 
 * comfortably detect when theming cannot be used. */
static HTHEME WINAPI
dummy_OpenThemeData(HWND win, LPCWSTR class_id_list)
{
    return NULL;
}

static BOOL WINAPI
dummy_IsThemeActive(void)
{
    return FALSE;
}


int 
theme_init(void)
{
#ifdef DISABLE_UXTHEME
    MC_TRACE("theme_init: UXTHEME.DLL disabled in this build.");
    goto err_uxtheme_not_available;
#else
    /* WinXP with COMCL32.DLL version 6.0 or newer is required for theming. */
    if(mc_win_version < MC_WIN_XP) {
        MC_TRACE("theme_init: UXTHEME.DLL not used (old Windows)");
        goto err_uxtheme_not_available;
    }
    if(mc_comctl32_version < MC_DLL_VER(6, 0)) {
        MC_TRACE("theme_init: UXTHEME.DLL not used (COMCTL32.DLL < 6.0)");
        goto err_uxtheme_not_available;
    }

    /* Ok, so lets try to use themes. */
    uxtheme_dll = LoadLibrary(_T("UXTHEME.DLL"));
    if(MC_ERR(uxtheme_dll == NULL)) {
        MC_TRACE("theme_init: LoadLibrary(UXTHEME.DLL) failed [%ld].",
                 GetLastError());
        goto err_uxtheme_not_available;
    }

    /* Preprocessor magic to get UXTHEME.DLL functions with GetProcAddress() */
#define THEME_FN(rettype, name, params)                                       \
    theme_##name = (rettype (WINAPI*)params) GetProcAddress(uxtheme_dll, #name); \
    if(MC_ERR(theme_##name == NULL)) {                                        \
        MC_TRACE("theme_init: GetProcAddress(%s) failed [%lu]",               \
                 #name, GetLastError());                                      \
        goto err_GetProcAddress;                                              \
    }
#include "theme_fn.h"
#undef THEME_FN

    /* Success. All mCtrl controls can use the XP themes. */
    return 0;
    
err_GetProcAddress:
    FreeLibrary(uxtheme_dll);
    uxtheme_dll = NULL;
#endif  /* DISABLE_UXTHEME */
    
err_uxtheme_not_available:
    MC_TRACE("theme_init: Switching to dummy implementation.");
    theme_OpenThemeData = dummy_OpenThemeData;
    theme_IsThemeActive = dummy_IsThemeActive;
    /* Failure but return 0 anyway. XP themes won't be used in runtime. */
    return 0;
}

void 
theme_fini(void)
{
#ifndef DISABLE_UXTHEME
    if(uxtheme_dll != NULL) {
        FreeLibrary(uxtheme_dll);
        uxtheme_dll = NULL;
    }
#endif  /* #ifndef DISABLE_UXTHEME */
}

