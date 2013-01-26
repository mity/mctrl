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

#include "theme.h"


HRESULT (WINAPI* theme_CloseThemeData)(HTHEME);
HRESULT (WINAPI* theme_DrawThemeBackground)(HTHEME,HDC,int,int,const RECT*,const RECT*);
HRESULT (WINAPI* theme_DrawThemeEdge)(HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*);
HRESULT (WINAPI* theme_DrawThemeIcon)(HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int);
HRESULT (WINAPI* theme_DrawThemeParentBackground)(HWND,HDC,RECT*);
HRESULT (WINAPI* theme_DrawThemeText)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,DWORD,const RECT*);
HRESULT (WINAPI* theme_GetThemeBackgroundContentRect)(HTHEME,HDC,int,int,const RECT*,RECT*);
HRESULT (WINAPI* theme_GetThemeColor)(HTHEME,int,int,int,COLORREF*);
HRESULT (WINAPI* theme_GetThemeTextExtent)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,const RECT*,RECT*);
BOOL    (WINAPI* theme_IsThemeActive)(void);
BOOL    (WINAPI* theme_IsThemeBackgroundPartiallyTransparent)(HTHEME,int,int);
HTHEME  (WINAPI* theme_OpenThemeData)(HWND,const WCHAR*);
HRESULT (WINAPI* theme_SetWindowTheme)(HWND,const WCHAR*,const WCHAR*);

HANIMATIONBUFFER (WINAPI* theme_BeginBufferedAnimation)(HWND,HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,BP_ANIMATIONPARAMS*,HDC*,HDC*);
HRESULT (WINAPI* theme_BufferedPaintInit)(void);
HRESULT (WINAPI* theme_BufferedPaintUnInit)(void);
BOOL    (WINAPI* theme_BufferedPaintRenderAnimation)(HWND,HDC);
HRESULT (WINAPI* theme_BufferedPaintStopAllAnimations)(HWND);
HRESULT (WINAPI* theme_EndBufferedAnimation)(HANIMATIONBUFFER,BOOL);
HRESULT (WINAPI* theme_GetThemeTransitionDuration)(HTHEME,int,int,int,int,DWORD*);


static HMODULE uxtheme_dll = NULL;


/****************************
 *** Dummy implementation ***
 ****************************/

static HRESULT WINAPI
dummy_DrawThemeParentBackground(HWND win, HDC dc, RECT* rect)
{
    RECT r;
    POINT old_origin;
    HWND parent;
    HRGN clip = NULL;
    int clip_state;

    parent = GetAncestor(win, GA_PARENT);
    if(parent == NULL)
        parent = win;

    mc_rect_copy(&r, rect);
    MapWindowPoints(win, parent, (POINT*) &r, 2);
    clip = CreateRectRgn(0, 0, 1, 1);
    clip_state = GetClipRgn(dc, clip);
    if(clip_state != -1)
        IntersectClipRect(dc, rect->left, rect->top, rect->right, rect->bottom);

    OffsetViewportOrgEx(dc, -r.left, -r.top, &old_origin);

    MC_MSG(parent, WM_ERASEBKGND, dc, 0);
    MC_MSG(parent, WM_PRINTCLIENT, dc, PRF_CLIENT);

    SetViewportOrgEx(dc, old_origin.x, old_origin.y, NULL);

    if(clip_state == 0)
        SelectClipRgn(dc, NULL);
    else if(clip_state == 1)
        SelectClipRgn(dc, clip);
    DeleteObject(clip);

    return S_OK;
}

static BOOL WINAPI
dummy_IsThemeActive(void)
{
    return FALSE;
}

static HTHEME WINAPI
dummy_OpenThemeData(HWND win, const WCHAR* class_id_list)
{
    return NULL;
}

static HRESULT WINAPI
dummy_SetWindowTheme(HWND win, const WCHAR* app_name, const WCHAR* theme_name)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI
dummy_BufferedPaintInit_or_UnInit(void)
{
    return E_NOTIMPL;
}

static BOOL WINAPI
dummy_BufferedPaintRenderAnimation(HWND win, HDC dc)
{
    return FALSE;
}

static HANIMATIONBUFFER WINAPI
dummy_BeginBufferedAnimation(HWND win, HDC dc, const RECT *rect,
                             BP_BUFFERFORMAT format, BP_PAINTPARAMS *paint_params,
                             BP_ANIMATIONPARAMS *anim_params,
                             HDC* dc_from, HDC* hdc_to)
{
    return NULL;
}

static HRESULT WINAPI
dummy_GetThemeTransitionDuration(HTHEME theme, int part, int state1,
                                 int state2, int prop, DWORD* duration)
{
    *duration = 0;
    return E_NOTIMPL;
}

static HRESULT WINAPI
dummy_BufferedPaintStopAllAnimations(HWND win)
{
    return E_NOTIMPL;
}


/***************************
 *** ANSI implementation ***
 ***************************/

#ifndef UNICODE

static HRESULT (WINAPI* theme_DrawThemeTextW)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,DWORD,const RECT*);
static HRESULT (WINAPI* theme_GetThemeTextExtentW)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,const RECT*,RECT*);

static HRESULT WINAPI
theme_DrawThemeTextA(HTHEME theme, HDC dc, int part, int state,
                     const char* str, int str_len, DWORD flags, DWORD flags2,
                     const RECT* rect)
{
    WCHAR* wstr;
    int wstr_len;
    HRESULT hr;

    wstr = mc_str_n(str, MC_STRA, str_len, MC_STRW, &wstr_len);
    hr = theme_DrawThemeTextW(theme, dc, part, state, wstr, wstr_len,
                              flags, flags2, rect);
    free(wstr);
    return hr;
}

static HRESULT WINAPI
theme_GetThemeTextExtentA(HTHEME theme, HDC dc, int part, int state,
                     const char* str, int str_len, DWORD flags,
                     const RECT* bound_rect, RECT* extent_rect)
{
    WCHAR* wstr;
    int wstr_len;
    HRESULT hr;

    wstr = mc_str_n(str, MC_STRA, str_len, MC_STRW, &wstr_len);
    hr = theme_GetThemeTextExtentW(theme, dc, part, state, wstr, wstr_len,
                                   flags, bound_rect, extent_rect);
    free(wstr);
    return hr;
}

#endif  /* #ifndef UNICODE */


/**********************
 *** Initialization ***
 **********************/

int
theme_init(void)
{
    /* WinXP with COMCL32.DLL version 6.0 or newer is required for theming. */
    if(mc_win_version < MC_WIN_XP) {
        MC_TRACE("theme_init: UXTHEME.DLL not used (old Windows)");
        goto err_uxtheme_not_loaded;
    }
    if(mc_comctl32_version < MC_DLL_VER(6, 0)) {
        MC_TRACE("theme_init: UXTHEME.DLL not used (COMCTL32.DLL < 6.0)");
        goto err_uxtheme_not_loaded;
    }

    /* Ok, so lets try to use themes. */
    uxtheme_dll = LoadLibrary(_T("UXTHEME.DLL"));
    if(MC_ERR(uxtheme_dll == NULL)) {
        MC_TRACE("theme_init: LoadLibrary(UXTHEME.DLL) failed [%ld].",
                 GetLastError());
        goto err_uxtheme_not_loaded;
    }

    /* Get core UXTHEME.DLL functions */
    theme_CloseThemeData = (HRESULT (WINAPI*)(HTHEME)) GetProcAddress(uxtheme_dll, "CloseThemeData");
    if(MC_ERR(theme_CloseThemeData == NULL))
        goto err_core;
    theme_DrawThemeBackground = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const RECT*,const RECT*)) GetProcAddress(uxtheme_dll, "DrawThemeBackground");
    if(MC_ERR(theme_DrawThemeBackground == NULL))
        goto err_core;
    theme_DrawThemeEdge = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*)) GetProcAddress(uxtheme_dll, "DrawThemeEdge");
    if(MC_ERR(theme_DrawThemeEdge == NULL))
        goto err_core;
    theme_DrawThemeIcon = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int)) GetProcAddress(uxtheme_dll, "DrawThemeIcon");
    if(MC_ERR(theme_DrawThemeIcon == NULL))
        goto err_core;
    theme_DrawThemeParentBackground = (HRESULT (WINAPI*)(HWND,HDC,RECT*)) GetProcAddress(uxtheme_dll, "DrawThemeParentBackground");
    if(MC_ERR(theme_DrawThemeParentBackground == NULL))
        goto err_core;
#ifdef UNICODE
    theme_DrawThemeText = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,DWORD,const RECT*)) GetProcAddress(uxtheme_dll, "DrawThemeText");
    if(MC_ERR(theme_DrawThemeText == NULL))
        goto err_core;
#else
    theme_DrawThemeTextW = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,DWORD,const RECT*)) GetProcAddress(uxtheme_dll, "DrawThemeText");
    if(MC_ERR(theme_DrawThemeTextW == NULL))
        goto err_core;
    theme_DrawThemeText = theme_DrawThemeTextA;
#endif
    theme_GetThemeBackgroundContentRect = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const RECT*,RECT*)) GetProcAddress(uxtheme_dll, "GetThemeBackgroundContentRect");
    if(MC_ERR(theme_GetThemeBackgroundContentRect == NULL))
        goto err_core;
    theme_GetThemeColor = (HRESULT (WINAPI*)(HTHEME,int,int,int,COLORREF*)) GetProcAddress(uxtheme_dll, "GetThemeColor");
    if(MC_ERR(theme_GetThemeColor == NULL))
        goto err_core;
#ifdef UNICODE
    theme_GetThemeTextExtent = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,const RECT*,RECT*)) GetProcAddress(uxtheme_dll, "GetThemeTextExtent");
    if(MC_ERR(theme_GetThemeTextExtent == NULL))
        goto err_core;
#else
    theme_GetThemeTextExtentW = (HRESULT (WINAPI*)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,const RECT*,RECT*)) GetProcAddress(uxtheme_dll, "GetThemeTextExtent");
    if(MC_ERR(theme_GetThemeTextExtentW == NULL))
        goto err_core;
    theme_GetThemeTextExtent = theme_GetThemeTextExtentA;
#endif
    theme_IsThemeActive = (BOOL (WINAPI*)(void)) GetProcAddress(uxtheme_dll, "IsThemeActive");
    if(MC_ERR(theme_IsThemeActive == NULL))
        goto err_core;
    theme_IsThemeBackgroundPartiallyTransparent = (BOOL (WINAPI*)(HTHEME,int,int)) GetProcAddress(uxtheme_dll, "IsThemeBackgroundPartiallyTransparent");
    if(MC_ERR(theme_IsThemeBackgroundPartiallyTransparent == NULL))
        goto err_core;
    theme_OpenThemeData = (HTHEME (WINAPI*)(HWND,const WCHAR*)) GetProcAddress(uxtheme_dll, "OpenThemeData");
    if(MC_ERR(theme_OpenThemeData == NULL))
        goto err_core;
    theme_SetWindowTheme = (HRESULT (WINAPI*)(HWND,const WCHAR*,const WCHAR*)) GetProcAddress(uxtheme_dll, "SetWindowTheme");
    if(MC_ERR(theme_SetWindowTheme == NULL))
        goto err_core;

    /* Get buffered animation functions */
    theme_BeginBufferedAnimation = (HANIMATIONBUFFER (WINAPI*)(HWND,HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,BP_ANIMATIONPARAMS*,HDC*,HDC*)) GetProcAddress(uxtheme_dll, "BeginBufferedAnimation");
    if(MC_ERR(theme_BeginBufferedAnimation == NULL))
        goto err_anim;
    theme_BufferedPaintInit = (HRESULT (WINAPI*)(void)) GetProcAddress(uxtheme_dll, "BufferedPaintInit");
    if(MC_ERR(theme_BufferedPaintInit == NULL))
        goto err_anim;
    theme_BufferedPaintUnInit = (HRESULT (WINAPI*)(void)) GetProcAddress(uxtheme_dll, "BufferedPaintUnInit");
    if(MC_ERR(theme_BufferedPaintUnInit == NULL))
        goto err_anim;
    theme_BufferedPaintRenderAnimation = (BOOL (WINAPI*)(HWND,HDC)) GetProcAddress(uxtheme_dll, "BufferedPaintRenderAnimation");
    if(MC_ERR(theme_BufferedPaintRenderAnimation == NULL))
        goto err_anim;
    theme_BufferedPaintStopAllAnimations = (HRESULT (WINAPI*)(HWND)) GetProcAddress(uxtheme_dll, "BufferedPaintStopAllAnimations");
    if(MC_ERR(theme_BufferedPaintStopAllAnimations == NULL))
        goto err_anim;
    theme_EndBufferedAnimation = (HRESULT (WINAPI*)(HANIMATIONBUFFER,BOOL)) GetProcAddress(uxtheme_dll, "EndBufferedAnimation");
    if(MC_ERR(theme_EndBufferedAnimation == NULL))
        goto err_anim;
    theme_GetThemeTransitionDuration = (HRESULT (WINAPI*)(HTHEME,int,int,int,int,DWORD*)) GetProcAddress(uxtheme_dll, "GetThemeTransitionDuration");
    if(MC_ERR(theme_GetThemeTransitionDuration == NULL))
        goto err_anim;

    /* Success */
    return 0;

err_core:
    FreeLibrary(uxtheme_dll);
    uxtheme_dll = NULL;

err_uxtheme_not_loaded:
    /* Make the controls fall back to GDI (non-themed) painting. */
    theme_DrawThemeParentBackground = dummy_DrawThemeParentBackground;
    theme_IsThemeActive = dummy_IsThemeActive;
    theme_OpenThemeData = dummy_OpenThemeData;
    theme_SetWindowTheme = dummy_SetWindowTheme;

err_anim:
    /* Disable the UXTHEME.DLL based animations. */
    theme_BufferedPaintInit = dummy_BufferedPaintInit_or_UnInit;
    theme_BufferedPaintUnInit = dummy_BufferedPaintInit_or_UnInit;
    theme_BufferedPaintRenderAnimation = dummy_BufferedPaintRenderAnimation;
    theme_BeginBufferedAnimation = dummy_BeginBufferedAnimation;
    theme_GetThemeTransitionDuration = dummy_GetThemeTransitionDuration;
    theme_BufferedPaintStopAllAnimations = dummy_BufferedPaintStopAllAnimations;

    /* Return 0 anyway. */
    return 0;
}

void
theme_fini(void)
{
    if(uxtheme_dll != NULL) {
        FreeLibrary(uxtheme_dll);
        uxtheme_dll = NULL;
    }
}

