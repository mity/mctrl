/*
 * Copyright (c) 2008-2011 Martin Mitas
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


/* Note we use this header for horrible preprocessor magic, therefore there
 * is no #ifdef MC_THEME_MAP_H guarding the header. This header is #included
 * several times from theme.h and theme.c and no other file should use this
 * header directly.
 *
 * We use it to automate GetProcAddress() machinery with the casting
 * and error checking for each function.
 */


/* Before attempting to #include this, THEME_FN(ret_value, fn_name, params)
 * must be #defined accordingly, which defines what to do with each of the
 * listed UXTHEME.DLL function.
 */
#ifndef THEME_FN
    #error Macro THEME_FN is not defined.
#endif


THEME_FN(HRESULT, CloseThemeData,      (HTHEME))
THEME_FN(HRESULT, DrawThemeBackground, (HTHEME,HDC,int,int,const RECT*,const RECT*))
THEME_FN(HRESULT, DrawThemeEdge,       (HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*))
THEME_FN(HRESULT, DrawThemeIcon,       (HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int))
THEME_FN(HRESULT, DrawThemeParentBackground, (HWND,HDC,RECT*))
THEME_FN(HRESULT, DrawThemeText,       (HTHEME,HDC,int,int,LPCWSTR,int,DWORD,DWORD,const RECT*))
THEME_FN(HRESULT, GetThemeBackgroundContentRect, (HTHEME,HDC,int,int,const RECT*,RECT*))
THEME_FN(HRESULT, GetThemeColor,       (HTHEME,int,int,int,COLORREF*))
THEME_FN(HRESULT, GetThemeTextExtent,  (HTHEME,HDC,int,int,LPCWSTR,int,DWORD,const RECT*,RECT*))
THEME_FN(BOOL,    IsThemeActive,       (void))
THEME_FN(BOOL,    IsThemeBackgroundPartiallyTransparent, (HTHEME,int,int))
THEME_FN(HTHEME,  OpenThemeData,       (HWND,LPCWSTR))
