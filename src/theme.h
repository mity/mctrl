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

#ifndef MC_THEME_H
#define MC_THEME_H

#include "misc.h"

#include <vssym32.h>
#include <vsstyle.h>
#include <uxtheme.h>


extern HRESULT (WINAPI* theme_CloseThemeData)(HTHEME);
extern HRESULT (WINAPI* theme_DrawThemeBackground)(HTHEME,HDC,int,int,const RECT*,const RECT*);
extern HRESULT (WINAPI* theme_DrawThemeEdge)(HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*);
extern HRESULT (WINAPI* theme_DrawThemeIcon)(HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int);
extern HRESULT (WINAPI* theme_DrawThemeParentBackground)(HWND,HDC,RECT*);
extern HRESULT (WINAPI* theme_DrawThemeText)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,DWORD,const RECT*);
extern HRESULT (WINAPI* theme_GetThemeBackgroundContentRect)(HTHEME,HDC,int,int,const RECT*,RECT*);
extern HRESULT (WINAPI* theme_GetThemeColor)(HTHEME,int,int,int,COLORREF*);
extern HRESULT (WINAPI* theme_GetThemeTextExtent)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,const RECT*,RECT*);
extern BOOL    (WINAPI* theme_IsThemeActive)(void);
extern BOOL    (WINAPI* theme_IsThemeBackgroundPartiallyTransparent)(HTHEME,int,int);
extern HTHEME  (WINAPI* theme_OpenThemeData)(HWND,const WCHAR*);
extern HRESULT (WINAPI* theme_SetWindowTheme)(HWND,const WCHAR*,const WCHAR*);


extern HANIMATIONBUFFER (WINAPI* theme_BeginBufferedAnimation)(HWND,HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,BP_ANIMATIONPARAMS*,HDC*,HDC*);
extern HRESULT (WINAPI* theme_BufferedPaintInit)(void);
extern HRESULT (WINAPI* theme_BufferedPaintUnInit)(void);
extern BOOL    (WINAPI* theme_BufferedPaintRenderAnimation)(HWND,HDC);
extern HRESULT (WINAPI* theme_BufferedPaintStopAllAnimations)(HWND);
extern HRESULT (WINAPI* theme_EndBufferedAnimation)(HANIMATIONBUFFER,BOOL);
extern HRESULT (WINAPI* theme_GetThemeTransitionDuration)(HTHEME,int,int,int,int,DWORD*);


int theme_init(void);
void theme_fini(void);


#endif  /* MC_THEME_H */
