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

#ifndef MC_THEME_H
#define MC_THEME_H

#include "misc.h"
#include "mCtrl/theme.h"


extern HANIMATIONBUFFER (WINAPI* theme_BeginBufferedAnimation)(HWND,HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,BP_ANIMATIONPARAMS*,HDC*,HDC*);
extern HPAINTBUFFER (WINAPI* theme_BeginBufferedPaint)(HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,HDC*);
extern BOOL     (WINAPI* theme_BeginPanningFeedback)(HWND);
extern HRESULT  (WINAPI* theme_BufferedPaintClear)(HPAINTBUFFER,const RECT*);
extern HRESULT  (WINAPI* theme_BufferedPaintInit)(void);
extern BOOL     (WINAPI* theme_BufferedPaintRenderAnimation)(HWND,HDC);
extern HRESULT  (WINAPI* theme_BufferedPaintSetAlpha)(HPAINTBUFFER,const RECT*,BYTE);
extern HRESULT  (WINAPI* theme_BufferedPaintStopAllAnimations)(HWND);
extern HRESULT  (WINAPI* theme_BufferedPaintUnInit)(void);
extern HRESULT  (WINAPI* theme_CloseThemeData)(HTHEME);
extern HRESULT  (WINAPI* theme_DrawThemeBackground)(HTHEME,HDC,int,int,const RECT*,const RECT*);
extern HRESULT  (WINAPI* theme_DrawThemeBackgroundEx)(HTHEME,HDC,int,int,const RECT*,const DTBGOPTS*);
extern HRESULT  (WINAPI* theme_DrawThemeEdge)(HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*);
extern HRESULT  (WINAPI* theme_DrawThemeIcon)(HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int);
extern HRESULT  (WINAPI* theme_DrawThemeParentBackground)(HWND,HDC,RECT*);
extern HRESULT  (WINAPI* theme_DrawThemeParentBackgroundEx)(HWND,HDC,DWORD,RECT*);
extern HRESULT  (WINAPI* theme_DrawThemeText)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,DWORD,const RECT*);
extern HRESULT  (WINAPI* theme_DrawThemeTextEx)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,RECT*,const DTTOPTS*);
extern HRESULT  (WINAPI* theme_EnableThemeDialogTexture)(HWND,DWORD);
extern HRESULT  (WINAPI* theme_EndBufferedAnimation)(HANIMATIONBUFFER,BOOL);
extern HRESULT  (WINAPI* theme_EndBufferedPaint)(HPAINTBUFFER,BOOL);
extern BOOL     (WINAPI* theme_EndPanningFeedback)(HWND,BOOL);
extern HRESULT  (WINAPI* theme_GetBufferedPaintBits)(HPAINTBUFFER,RGBQUAD**,int*);
extern HDC      (WINAPI* theme_GetBufferedPaintDC)(HPAINTBUFFER);
extern HDC      (WINAPI* theme_GetBufferedPaintTargetDC)(HPAINTBUFFER);
extern HRESULT  (WINAPI* theme_GetBufferedPaintTargetRect)(HPAINTBUFFER,RECT*);
extern HRESULT  (WINAPI* theme_GetCurrentThemeName)(WCHAR*,int,WCHAR*,int,WCHAR*,int);
extern DWORD    (WINAPI* theme_GetThemeAppProperties)(void);
extern HRESULT  (WINAPI* theme_GetThemeBackgroundContentRect)(HTHEME,HDC,int,int,const RECT*,RECT*);
extern HRESULT  (WINAPI* theme_GetThemeBackgroundExtent)(HTHEME,HDC,int,int,const RECT*,RECT*);
extern HRESULT  (WINAPI* theme_GetThemeBackgroundRegion)(HTHEME,HDC,int,int,const RECT*,HRGN*);
extern HRESULT  (WINAPI* theme_GetThemeBitmap)(HTHEME,int,int,int,ULONG,HBITMAP*);
extern HRESULT  (WINAPI* theme_GetThemeBool)(HTHEME,int,int,int,BOOL*);
extern HRESULT  (WINAPI* theme_GetThemeColor)(HTHEME,int,int,int,COLORREF*);
extern HRESULT  (WINAPI* theme_GetThemeDocumentationProperty)(const WCHAR*,const WCHAR*,WCHAR*,int);
extern HRESULT  (WINAPI* theme_GetThemeEnumValue)(HTHEME,int,int,int,int*);
extern HRESULT  (WINAPI* theme_GetThemeFilename)(HTHEME,int,int,int,WCHAR*,int);
extern HRESULT  (WINAPI* theme_GetThemeFont)(HTHEME,HDC,int,int,int,LOGFONTW*);
extern HRESULT  (WINAPI* theme_GetThemeInt)(HTHEME,int,int,int,int*);
extern HRESULT  (WINAPI* theme_GetThemeIntList)(HTHEME,int,int,int,INTLIST*);
extern HRESULT  (WINAPI* theme_GetThemeMargins)(HTHEME,HDC,int,int,int,RECT*,MARGINS*);
extern HRESULT  (WINAPI* theme_GetThemeMetric)(HTHEME,HDC,int,int,int,int*);
extern HRESULT  (WINAPI* theme_GetThemePartSize)(HTHEME,HDC,int,int,const RECT*,enum THEMESIZE,SIZE*);
extern HRESULT  (WINAPI* theme_GetThemePosition)(HTHEME,int,int,int,POINT*);
extern HRESULT  (WINAPI* theme_GetThemePropertyOrigin)(HTHEME,int,int,int,PROPERTYORIGIN*);
extern HRESULT  (WINAPI* theme_GetThemeRect)(HTHEME,int,int,int,RECT*);
extern HRESULT  (WINAPI* theme_GetThemeStream)(HTHEME,int,int,int,void**,DWORD*,HINSTANCE);
extern HRESULT  (WINAPI* theme_GetThemeString)(HTHEME,int,int,int,WCHAR*,int);
extern BOOL     (WINAPI* theme_GetThemeSysBool)(HTHEME,int);
extern COLORREF (WINAPI* theme_GetThemeSysColor)(HTHEME,int);
extern HBRUSH   (WINAPI* theme_GetThemeSysColorBrush)(HTHEME,int);
extern HRESULT  (WINAPI* theme_GetThemeSysFont)(HTHEME,int,LOGFONTW*);
extern HRESULT  (WINAPI* theme_GetThemeSysInt)(HTHEME,int,int*);
extern int      (WINAPI* theme_GetThemeSysSize)(HTHEME,int);
extern HRESULT  (WINAPI* theme_GetThemeSysString)(HTHEME,int,WCHAR*,int);
extern HRESULT  (WINAPI* theme_GetThemeTextExtent)(HTHEME,HDC,int,int,const TCHAR*,int,DWORD,const RECT*,RECT*);
extern HRESULT  (WINAPI* theme_GetThemeTextMetrics)(HTHEME,HDC,int,int,TEXTMETRIC*);
extern HRESULT  (WINAPI* theme_GetThemeTransitionDuration)(HTHEME,int,int,int,int,DWORD*);
extern HTHEME   (WINAPI* theme_GetWindowTheme)(HWND);
extern HRESULT  (WINAPI* theme_HitTestThemeBackground)(HTHEME,HDC,int,int,DWORD,const RECT*,HRGN,POINT,WORD*);
extern BOOL     (WINAPI* theme_IsAppThemed)(void);
extern BOOL     (WINAPI* theme_IsCompositionActive)(void);
extern BOOL     (WINAPI* theme_IsThemeActive)(void);
extern BOOL     (WINAPI* theme_IsThemeBackgroundPartiallyTransparent)(HTHEME,int,int);
extern BOOL     (WINAPI* theme_IsThemeDialogTextureEnabled)(HWND);
extern BOOL     (WINAPI* theme_IsThemePartDefined)(HTHEME,int,int);
extern HTHEME   (WINAPI* theme_OpenThemeData)(HWND,const WCHAR*);
extern HTHEME   (WINAPI* theme_OpenThemeDataEx)(HWND,const WCHAR*,DWORD);
extern void     (WINAPI* theme_SetThemeAppProperties)(DWORD);
extern HRESULT  (WINAPI* theme_SetWindowTheme)(HWND,const WCHAR*,const WCHAR*);
extern HRESULT  (WINAPI* theme_SetWindowThemeAttribute)(HWND,enum WINDOWTHEMEATTRIBUTETYPE,void*,DWORD);
extern BOOL     (WINAPI* theme_UpdatePanningFeedback)(HWND,LONG,LONG,BOOL);


int theme_init(void);
void theme_fini(void);


#endif  /* MC_THEME_H */
