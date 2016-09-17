/*
 * Copyright (c) 2008-2015 Martin Mitas
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


static HMODULE uxtheme_dll = NULL;

HANIMATIONBUFFER (WINAPI* theme_BeginBufferedAnimation)(HWND,HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,BP_ANIMATIONPARAMS*,HDC*,HDC*) = NULL;
HPAINTBUFFER (WINAPI* theme_BeginBufferedPaint)(HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,HDC*) = NULL;
BOOL     (WINAPI* theme_BeginPanningFeedback)(HWND) = NULL;
HRESULT  (WINAPI* theme_BufferedPaintClear)(HPAINTBUFFER,const RECT*) = NULL;
HRESULT  (WINAPI* theme_BufferedPaintInit)(void) = NULL;
BOOL     (WINAPI* theme_BufferedPaintRenderAnimation)(HWND,HDC) = NULL;
HRESULT  (WINAPI* theme_BufferedPaintSetAlpha)(HPAINTBUFFER,const RECT*,BYTE) = NULL;
HRESULT  (WINAPI* theme_BufferedPaintStopAllAnimations)(HWND) = NULL;
HRESULT  (WINAPI* theme_BufferedPaintUnInit)(void) = NULL;
HRESULT  (WINAPI* theme_CloseThemeData)(HTHEME) = NULL;
HRESULT  (WINAPI* theme_DrawThemeBackground)(HTHEME,HDC,int,int,const RECT*,const RECT*) = NULL;
HRESULT  (WINAPI* theme_DrawThemeBackgroundEx)(HTHEME,HDC,int,int,const RECT*,const DTBGOPTS*) = NULL;
HRESULT  (WINAPI* theme_DrawThemeEdge)(HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*) = NULL;
HRESULT  (WINAPI* theme_DrawThemeIcon)(HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int) = NULL;
HRESULT  (WINAPI* theme_DrawThemeParentBackground)(HWND,HDC,RECT*) = NULL;
HRESULT  (WINAPI* theme_DrawThemeParentBackgroundEx)(HWND,HDC,DWORD,RECT*) = NULL;
HRESULT  (WINAPI* theme_DrawThemeText)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,DWORD,const RECT*) = NULL;
HRESULT  (WINAPI* theme_DrawThemeTextEx)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,RECT*,const DTTOPTS*) = NULL;
HRESULT  (WINAPI* theme_EnableThemeDialogTexture)(HWND,DWORD) = NULL;
HRESULT  (WINAPI* theme_EndBufferedAnimation)(HANIMATIONBUFFER,BOOL) = NULL;
HRESULT  (WINAPI* theme_EndBufferedPaint)(HPAINTBUFFER,BOOL) = NULL;
BOOL     (WINAPI* theme_EndPanningFeedback)(HWND,BOOL) = NULL;
HRESULT  (WINAPI* theme_GetBufferedPaintBits)(HPAINTBUFFER,RGBQUAD**,int*) = NULL;
HDC      (WINAPI* theme_GetBufferedPaintDC)(HPAINTBUFFER) = NULL;
HDC      (WINAPI* theme_GetBufferedPaintTargetDC)(HPAINTBUFFER) = NULL;
HRESULT  (WINAPI* theme_GetBufferedPaintTargetRect)(HPAINTBUFFER,RECT*) = NULL;
HRESULT  (WINAPI* theme_GetCurrentThemeName)(WCHAR*,int,WCHAR*,int,WCHAR*,int) = NULL;
DWORD    (WINAPI* theme_GetThemeAppProperties)(void) = NULL;
HRESULT  (WINAPI* theme_GetThemeBackgroundContentRect)(HTHEME,HDC,int,int,const RECT*,RECT*) = NULL;
HRESULT  (WINAPI* theme_GetThemeBackgroundExtent)(HTHEME,HDC,int,int,const RECT*,RECT*) = NULL;
HRESULT  (WINAPI* theme_GetThemeBackgroundRegion)(HTHEME,HDC,int,int,const RECT*,HRGN*) = NULL;
HRESULT  (WINAPI* theme_GetThemeBitmap)(HTHEME,int,int,int,ULONG,HBITMAP*) = NULL;
HRESULT  (WINAPI* theme_GetThemeBool)(HTHEME,int,int,int,BOOL*) = NULL;
HRESULT  (WINAPI* theme_GetThemeColor)(HTHEME,int,int,int,COLORREF*) = NULL;
HRESULT  (WINAPI* theme_GetThemeDocumentationProperty)(const WCHAR*,const WCHAR*,WCHAR*,int) = NULL;
HRESULT  (WINAPI* theme_GetThemeEnumValue)(HTHEME,int,int,int,int*) = NULL;
HRESULT  (WINAPI* theme_GetThemeFilename)(HTHEME,int,int,int,WCHAR*,int) = NULL;
HRESULT  (WINAPI* theme_GetThemeFont)(HTHEME,HDC,int,int,int,LOGFONTW*) = NULL;
HRESULT  (WINAPI* theme_GetThemeInt)(HTHEME,int,int,int,int*) = NULL;
HRESULT  (WINAPI* theme_GetThemeIntList)(HTHEME,int,int,int,INTLIST*) = NULL;
HRESULT  (WINAPI* theme_GetThemeMargins)(HTHEME,HDC,int,int,int,RECT*,MARGINS*) = NULL;
HRESULT  (WINAPI* theme_GetThemeMetric)(HTHEME,HDC,int,int,int,int*) = NULL;
HRESULT  (WINAPI* theme_GetThemePartSize)(HTHEME,HDC,int,int,const RECT*,enum THEMESIZE,SIZE*) = NULL;
HRESULT  (WINAPI* theme_GetThemePosition)(HTHEME,int,int,int,POINT*) = NULL;
HRESULT  (WINAPI* theme_GetThemePropertyOrigin)(HTHEME,int,int,int,enum PROPERTYORIGIN*) = NULL;
HRESULT  (WINAPI* theme_GetThemeRect)(HTHEME,int,int,int,RECT*) = NULL;
HRESULT  (WINAPI* theme_GetThemeStream)(HTHEME,int,int,int,void**,DWORD*,HINSTANCE) = NULL;
HRESULT  (WINAPI* theme_GetThemeString)(HTHEME,int,int,int,WCHAR*,int) = NULL;
BOOL     (WINAPI* theme_GetThemeSysBool)(HTHEME,int) = NULL;
COLORREF (WINAPI* theme_GetThemeSysColor)(HTHEME,int) = NULL;
HBRUSH   (WINAPI* theme_GetThemeSysColorBrush)(HTHEME,int) = NULL;
HRESULT  (WINAPI* theme_GetThemeSysFont)(HTHEME,int,LOGFONTW*) = NULL;
HRESULT  (WINAPI* theme_GetThemeSysInt)(HTHEME,int,int*) = NULL;
int      (WINAPI* theme_GetThemeSysSize)(HTHEME,int) = NULL;
HRESULT  (WINAPI* theme_GetThemeSysString)(HTHEME,int,WCHAR*,int) = NULL;
HRESULT  (WINAPI* theme_GetThemeTextExtent)(HTHEME,HDC,int,int,const WCHAR*,int,DWORD,const RECT*,RECT*) = NULL;
HRESULT  (WINAPI* theme_GetThemeTextMetrics)(HTHEME,HDC,int,int,TEXTMETRIC*) = NULL;
HRESULT  (WINAPI* theme_GetThemeTransitionDuration)(HTHEME,int,int,int,int,DWORD*) = NULL;
HTHEME   (WINAPI* theme_GetWindowTheme)(HWND) = NULL;
HRESULT  (WINAPI* theme_HitTestThemeBackground)(HTHEME,HDC,int,int,DWORD,const RECT*,HRGN,POINT,WORD*) = NULL;
BOOL     (WINAPI* theme_IsAppThemed)(void) = NULL;
BOOL     (WINAPI* theme_IsCompositionActive)(void) = NULL;
BOOL     (WINAPI* theme_IsThemeActive)(void) = NULL;
BOOL     (WINAPI* theme_IsThemeBackgroundPartiallyTransparent)(HTHEME,int,int) = NULL;
BOOL     (WINAPI* theme_IsThemeDialogTextureEnabled)(HWND) = NULL;
BOOL     (WINAPI* theme_IsThemePartDefined)(HTHEME,int,int) = NULL;
HTHEME   (WINAPI* theme_OpenThemeData)(HWND,const WCHAR*) = NULL;
HTHEME   (WINAPI* theme_OpenThemeDataEx)(HWND,const WCHAR*,DWORD) = NULL;
void     (WINAPI* theme_SetThemeAppProperties)(DWORD) = NULL;
HRESULT  (WINAPI* theme_SetWindowTheme)(HWND,const WCHAR*,const WCHAR*) = NULL;
HRESULT  (WINAPI* theme_SetWindowThemeAttribute)(HWND,enum WINDOWTHEMEATTRIBUTETYPE,void*,DWORD) = NULL;
BOOL     (WINAPI* theme_UpdatePanningFeedback)(HWND,LONG,LONG,BOOL) = NULL;


/**************************************
 *** Fallbacks for double buffering ***
 **************************************/

typedef struct dummy_HPAINTBUFFER_tag dummy_HPAINTBUFFER_t;
struct dummy_HPAINTBUFFER_tag {
    HDC dc_target;
    HDC dc_buffered;
    HBITMAP old_bmp;
    POINT old_origin;
    RECT rect;
};

typedef dummy_HPAINTBUFFER_t* dummy_HPAINTBUFFER;


static HRESULT WINAPI
dummy_BufferedPaintInit(void)
{
    /* noop */
    return S_OK;
}

static HRESULT WINAPI
dummy_BufferedPaintUnInit(void)
{
    /* noop */
    return S_OK;
}

static dummy_HPAINTBUFFER WINAPI
dummy_BeginBufferedPaint(HDC dc_target, const RECT* rect, BP_BUFFERFORMAT fmt,
                         BP_PAINTPARAMS* params, HDC* dc_buffered)
{
    dummy_HPAINTBUFFER_t* pb;
    HBITMAP bmp;

    pb = (dummy_HPAINTBUFFER_t*) malloc(sizeof(dummy_HPAINTBUFFER_t));
    if(MC_ERR(pb == NULL)) {
        MC_TRACE("dummy_BeginBufferedPaint: malloc() failed.");
        goto err_malloc;
    }

    pb->dc_target = dc_target;
    pb->dc_buffered = CreateCompatibleDC(dc_target);
    if(MC_ERR(pb->dc_buffered == NULL)) {
        MC_TRACE_ERR("dummy_BeginBufferedPaint: CreateCompatibleDC() failed.");
        goto err_CreateCompatibleDC;
    }

    bmp = CreateCompatibleBitmap(dc_target, mc_width(rect), mc_height(rect));
    if(MC_ERR(bmp == NULL)) {
        MC_TRACE_ERR("dummy_BeginBufferedPaint: CreateCompatibleBitmap() failed.");
        goto err_CreateCompatibleBitmap;
    }

    mc_rect_copy(&pb->rect, rect);
    pb->old_bmp = SelectObject(pb->dc_buffered, bmp);
    OffsetViewportOrgEx(pb->dc_buffered, -rect->left, -rect->top, &pb->old_origin);

    if(dc_buffered != NULL)
        *dc_buffered = pb->dc_buffered;

    /* Success */
    return pb;

    /* Error path */
err_CreateCompatibleBitmap:
    DeleteDC(pb->dc_buffered);
err_CreateCompatibleDC:
    free(pb);
err_malloc:
    if(dc_buffered != NULL)
        *dc_buffered = NULL;
    return NULL;
}

static HRESULT WINAPI
dummy_EndBufferedPaint(dummy_HPAINTBUFFER pb, BOOL update_target)
{
    HBITMAP bmp;

    if(update_target) {
        SetViewportOrgEx(pb->dc_buffered, pb->old_origin.x, pb->old_origin.y, NULL);
        BitBlt(pb->dc_target, pb->rect.left, pb->rect.top, mc_width(&pb->rect),
               mc_height(&pb->rect), pb->dc_buffered, 0, 0, SRCCOPY);
    }

    bmp = SelectObject(pb->dc_buffered, pb->old_bmp);
    DeleteObject(bmp);
    DeleteDC(pb->dc_buffered);
    free(pb);

    return S_OK;
}


/****************
 *** Wrappers ***
 ****************/

HANIMATIONBUFFER MCTRL_API
mcBeginBufferedAnimation(HWND hwnd, HDC hdcTarget, const RECT* rcTarget,
            BP_BUFFERFORMAT dwFormat, BP_PAINTPARAMS* pPaintParams,
            BP_ANIMATIONPARAMS* pAnimationParams, HDC* phdcFrom, HDC* phdcTo)
{
    if(theme_BeginBufferedAnimation != NULL) {
        return theme_BeginBufferedAnimation(hwnd, hdcTarget, rcTarget, dwFormat,
                    pPaintParams, pAnimationParams, phdcFrom, phdcTo);
    }

    if(phdcFrom != NULL)  *phdcFrom = NULL;
    if(phdcTo != NULL)    *phdcTo = NULL;

    MC_TRACE("mcBeginBufferedAnimation: Stub [NULL]");
    return NULL;
}

HPAINTBUFFER MCTRL_API
mcBeginBufferedPaint(HDC hdcTarget, const RECT* prcTarget,
            BP_BUFFERFORMAT dwFormat, BP_PAINTPARAMS* pPaintParams, HDC* phdc)
{
    if(theme_BeginBufferedPaint != NULL) {
        return theme_BeginBufferedPaint(hdcTarget, prcTarget, dwFormat,
                    pPaintParams, phdc);
    }

    return (HPAINTBUFFER) dummy_BeginBufferedPaint(hdcTarget, prcTarget,
                                    dwFormat, pPaintParams, phdc);
}

BOOL MCTRL_API
mcBeginPanningFeedback(HWND hwnd)
{
    if(theme_BeginPanningFeedback != NULL)
        return theme_BeginPanningFeedback(hwnd);

    MC_TRACE("mcBeginPanningFeedback: Stub [FALSE]");
    return FALSE;
}

HRESULT MCTRL_API
mcBufferedPaintClear(HPAINTBUFFER hBufferedPaint, const RECT* prc)
{
    if(theme_BufferedPaintClear != NULL)
        return theme_BufferedPaintClear(hBufferedPaint, prc);

    MC_TRACE("mcBufferedPaintClear: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcBufferedPaintInit(void)
{
    if(theme_BufferedPaintInit != NULL)
        return theme_BufferedPaintInit();

    return dummy_BufferedPaintInit();
}

BOOL MCTRL_API
mcBufferedPaintRenderAnimation(HWND hwnd, HDC hdcTarget)
{
    if(theme_BufferedPaintRenderAnimation != NULL)
        return theme_BufferedPaintRenderAnimation(hwnd, hdcTarget);

    MC_TRACE("mcBufferedPaintRenderAnimation: Stub [FALSE]");
    return FALSE;
}

HRESULT MCTRL_API
mcBufferedPaintSetAlpha(HPAINTBUFFER hBufferedPaint, const RECT* prc, BYTE alpha)
{
    if(theme_BufferedPaintSetAlpha != NULL)
        return theme_BufferedPaintSetAlpha(hBufferedPaint, prc, alpha);

    MC_TRACE("mcBufferedPaintSetAlpha: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcBufferedPaintStopAllAnimations(HWND hwnd)
{
    if(theme_BufferedPaintStopAllAnimations != NULL)
        return theme_BufferedPaintStopAllAnimations(hwnd);

    MC_TRACE("mcBufferedPaintStopAllAnimations: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcBufferedPaintUnInit(void)
{
    if(theme_BufferedPaintUnInit != NULL)
        return theme_BufferedPaintUnInit();

    return dummy_BufferedPaintUnInit();
}

HRESULT MCTRL_API
mcCloseThemeData(HTHEME hTheme)
{
    if(theme_CloseThemeData != NULL)
        return theme_CloseThemeData(hTheme);

    MC_TRACE("mcCloseThemeData: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prc, const RECT* prcClip)
{
    if(theme_DrawThemeBackground != NULL) {
        return theme_DrawThemeBackground(hTheme, hdc, iPartId, iStateId,
                                         prc, prcClip);
    }

    MC_TRACE("mcDrawThemeBackground: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeBackgroundEx(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prc, const DTBGOPTS* pOptions)
{
    if(theme_DrawThemeBackgroundEx != NULL) {
        return theme_DrawThemeBackgroundEx(hTheme, hdc, iPartId, iStateId,
                                           prc, pOptions);
    }

    MC_TRACE("mcDrawThemeBackgroundEx: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeEdge(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prcDest, UINT uEdge, UINT uFlags, RECT* prcContent)
{
    if(theme_DrawThemeEdge != NULL) {
        return theme_DrawThemeEdge(hTheme, hdc, iPartId, iStateId, prcDest,
                                   uEdge, uFlags, prcContent);
    }

    MC_TRACE("mcDrawThemeEdge: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeIcon(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prc, HIMAGELIST himl, int iImageIndex)
{
    if(theme_DrawThemeIcon != NULL) {
        return theme_DrawThemeIcon(hTheme, hdc, iPartId, iStateId, prc,
                                   himl, iImageIndex);
    }

    MC_TRACE("mcDrawThemeEdge: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeParentBackground(HWND hwnd, HDC hdc, RECT* prc)
{
    RECT r;
    POINT old_origin;
    HWND parent;
    HRGN clip = NULL;
    int clip_state;

    if(theme_DrawThemeParentBackground != NULL)
        return theme_DrawThemeParentBackground(hwnd, hdc, prc);

    parent = GetAncestor(hwnd, GA_PARENT);
    if(parent == NULL)
        parent = hwnd;

    mc_rect_copy(&r, prc);
    MapWindowPoints(hwnd, parent, (POINT*) &r, 2);
    clip = CreateRectRgn(0, 0, 1, 1);
    clip_state = GetClipRgn(hdc, clip);
    if(clip_state != -1)
        IntersectClipRect(hdc, prc->left, prc->top, prc->right, prc->bottom);

    OffsetViewportOrgEx(hdc, -r.left, -r.top, &old_origin);

    MC_SEND(parent, WM_ERASEBKGND, hdc, 0);
    MC_SEND(parent, WM_PRINTCLIENT, hdc, PRF_CLIENT);

    SetViewportOrgEx(hdc, old_origin.x, old_origin.y, NULL);

    if(clip_state == 0)
        SelectClipRgn(hdc, NULL);
    else if(clip_state == 1)
        SelectClipRgn(hdc, clip);
    DeleteObject(clip);

    return S_OK;
}

HRESULT MCTRL_API
mcDrawThemeParentBackgroundEx(HWND hwnd, HDC hdc, DWORD dwFlags, RECT* prc)
{
    if(theme_DrawThemeParentBackgroundEx != NULL)
        return theme_DrawThemeParentBackgroundEx(hwnd, hdc, dwFlags, prc);

    MC_TRACE("mcDrawThemeParentBackgroundEx: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeText(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const WCHAR* pszText, int iCharCount, DWORD dwFlags, DWORD dwFlags2,
            const RECT* prc)
{
    if(theme_DrawThemeText != NULL) {
        return theme_DrawThemeText(hTheme, hdc, iPartId, iStateId,
                                   pszText, iCharCount, dwFlags, dwFlags2, prc);
    }

    MC_TRACE("mcDrawThemeText: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcDrawThemeTextEx(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const WCHAR* pszText, int iCharCount, DWORD dwFlags, RECT* prc,
            const DTTOPTS* pOptions)
{
    if(theme_DrawThemeTextEx != NULL) {
        return theme_DrawThemeTextEx(hTheme, hdc, iPartId, iStateId, pszText,
                                     iCharCount, dwFlags, prc, pOptions);
    }

    MC_TRACE("mcDrawThemeTextEx: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcEnableThemeDialogTexture(HWND hwnd, DWORD dwFlags)
{
    if(theme_EnableThemeDialogTexture != NULL)
        return theme_EnableThemeDialogTexture(hwnd, dwFlags);

    MC_TRACE("mcEnableThemeDialogTexture: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcEndBufferedAnimation(HANIMATIONBUFFER hbpAnimation, BOOL fUpdateTarget)
{
    if(theme_EndBufferedAnimation != NULL)
        return theme_EndBufferedAnimation(hbpAnimation, fUpdateTarget);

    MC_TRACE("mcEndBufferedAnimation: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcEndBufferedPaint(HPAINTBUFFER hBufferedPaint, BOOL fUpdateTarget)
{
    if(theme_EndBufferedPaint != NULL)
        return theme_EndBufferedPaint(hBufferedPaint, fUpdateTarget);

    return dummy_EndBufferedPaint((dummy_HPAINTBUFFER) hBufferedPaint, fUpdateTarget);
}

BOOL MCTRL_API
mcEndPanningFeedback(HWND hwnd, BOOL fAnimateBack)
{
    if(theme_EndPanningFeedback != NULL)
        return theme_EndPanningFeedback(hwnd, fAnimateBack);

    MC_TRACE("mcEndPanningFeedback: Stub [FALSE]");
    return FALSE;
}

HRESULT MCTRL_API
mcGetBufferedPaintBits(HPAINTBUFFER hBufferedPaint, RGBQUAD** ppbBuffer,
            int* pcxRow)
{
    if(theme_GetBufferedPaintBits != NULL)
        return theme_GetBufferedPaintBits(hBufferedPaint, ppbBuffer, pcxRow);

    if(ppbBuffer)  *ppbBuffer = NULL;
    if(pcxRow)     *pcxRow = 0;

    MC_TRACE("mcGetBufferedPaintBits: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HDC MCTRL_API
mcGetBufferedPaintDC(HPAINTBUFFER hBufferedPaint)
{
    if(theme_GetBufferedPaintDC != NULL)
        return theme_GetBufferedPaintDC(hBufferedPaint);

    MC_TRACE("mcGetBufferedPaintDC: Stub [NULL]");
    return NULL;
}

HDC MCTRL_API
mcGetBufferedPaintTargetDC(HPAINTBUFFER hBufferedPaint)
{
    if(theme_GetBufferedPaintTargetDC != NULL)
        return theme_GetBufferedPaintTargetDC(hBufferedPaint);

    MC_TRACE("mcGetBufferedPaintTargetDC: Stub [NULL]");
    return NULL;
}

HRESULT MCTRL_API
mcGetBufferedPaintTargetRect(HPAINTBUFFER hBufferedPaint, RECT* prc)
{
    if(theme_GetBufferedPaintTargetRect != NULL)
        return theme_GetBufferedPaintTargetRect(hBufferedPaint, prc);

    if(prc != NULL)  mc_rect_set(prc, 0, 0, 0, 0);

    MC_TRACE("mcGetBufferedPaintTargetRect: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetCurrentThemeName(WCHAR* pszThemeFilename, int cchMaxFilenameChars,
                      WCHAR* pszColorBuff, int cchMaxColorChars,
                      WCHAR* pszSizeBuff, int cchMaxSizeChars)
{
    if(theme_GetCurrentThemeName != NULL) {
        return theme_GetCurrentThemeName(pszThemeFilename, cchMaxFilenameChars,
                                         pszColorBuff, cchMaxColorChars,
                                         pszSizeBuff, cchMaxSizeChars);
    }

    if(cchMaxFilenameChars > 0)  pszThemeFilename[0] = L'\0';
    if(cchMaxColorChars > 0)     pszColorBuff[0] = L'\0';
    if(cchMaxSizeChars > 0)      pszSizeBuff[0] = L'\0';

    MC_TRACE("mcGetCurrentThemeName: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

DWORD MCTRL_API
mcGetThemeAppProperties(void)
{
    if(theme_GetThemeAppProperties != NULL)
        return theme_GetThemeAppProperties();

    MC_TRACE("mcGetThemeAppProperties: Stub [0]");
    return 0;
}

HRESULT MCTRL_API
mcGetThemeBackgroundContentRect(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prcBounding, RECT* prcContent)
{
    if(theme_GetThemeBackgroundContentRect != NULL) {
        return theme_GetThemeBackgroundContentRect(hTheme, hdc, iPartId,
                                            iStateId, prcBounding, prcContent);
    }

    if(prcContent != NULL)  mc_rect_set(prcContent, 0, 0, 0, 0);

    MC_TRACE("mcGetThemeBackgroundContentRect: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeBackgroundExtent(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prcContent, RECT* prcExtent)
{
    if(theme_GetThemeBackgroundExtent != NULL) {
        return theme_GetThemeBackgroundExtent(hTheme, hdc, iPartId, iStateId,
                                              prcContent, prcExtent);
    }

    if(prcExtent != NULL)  mc_rect_set(prcExtent, 0, 0, 0, 0);

    MC_TRACE("mcGetThemeBackgroundExtent: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeBackgroundRegion(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prc, HRGN* phRegion)
{
    if(theme_GetThemeBackgroundRegion != NULL) {
        return theme_GetThemeBackgroundRegion(hTheme, hdc, iPartId, iStateId,
                                       prc, phRegion);
    }

    if(phRegion != NULL)  *phRegion = NULL;

    MC_TRACE("mcGetThemeBackgroundRegion: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeBitmap(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            ULONG uFlags, HBITMAP* phBitmap)
{
    if(theme_GetThemeBitmap != NULL) {
        return theme_GetThemeBitmap(hTheme, iPartId, iStateId, iPropId,
                                    uFlags, phBitmap);
    }

    if(phBitmap != NULL)  *phBitmap = NULL;

    MC_TRACE("mcGetThemeBitmap: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeBool(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            BOOL* pfValue)
{
    if(theme_GetThemeBool != NULL)
        return theme_GetThemeBool(hTheme, iPartId, iStateId, iPropId, pfValue);

    if(pfValue != NULL)  *pfValue = FALSE;

    MC_TRACE("mcGetThemeBool: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            COLORREF* pColor)
{
    if(theme_GetThemeColor != NULL)
        return theme_GetThemeColor(hTheme, iPartId, iStateId, iPropId, pColor);

    if(pColor != NULL)  *pColor = RGB(0,0,0);

    MC_TRACE("mcGetThemeColor: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeDocumentationProperty(const WCHAR* pszThemeName,
            const WCHAR* pszPropName, WCHAR* pszValueBuf, int cchMaxValChars)
{
    if(theme_GetThemeDocumentationProperty != NULL) {
        return theme_GetThemeDocumentationProperty(pszThemeName, pszPropName,
                                                   pszValueBuf, cchMaxValChars);
    }

    if(cchMaxValChars > 0)  pszValueBuf[0] = L'\0';

    MC_TRACE("mcGetThemeDocumentationProperty: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeEnumValue(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            int* piValue)
{
    if(theme_GetThemeEnumValue != NULL)
        return theme_GetThemeEnumValue(hTheme, iPartId, iStateId, iPropId, piValue);

    if(piValue != NULL)  *piValue = 0;

    MC_TRACE("mcGetThemeEnumValue: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeFilename(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            WCHAR* pszThemeFilename, int cchMaxBuffChars)
{
    if(theme_GetThemeFilename != NULL) {
        return theme_GetThemeFilename(hTheme, iPartId, iStateId, iPropId,
                                       pszThemeFilename, cchMaxBuffChars);
    }

    if(cchMaxBuffChars > 0)  pszThemeFilename[0] = L'\0';

    MC_TRACE("mcGetThemeFilename: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeFont(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId,
            LOGFONTW* pLogFont)
{
    if(theme_GetThemeFont != NULL)
        return theme_GetThemeFont(hTheme, hdc, iPartId, iStateId, iPropId, pLogFont);

    MC_TRACE("mcGetThemeFont: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeInt(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            int* piValue)
{
    if(theme_GetThemeInt != NULL)
        return theme_GetThemeInt(hTheme, iPartId, iStateId, iPropId, piValue);

    if(piValue != NULL)  *piValue = 0;

    MC_TRACE("mcGetThemeInt: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeIntList(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            INTLIST* pIntList)
{
    if(theme_GetThemeIntList != NULL)
        return theme_GetThemeIntList(hTheme, iPartId, iStateId, iPropId, pIntList);

    if(pIntList != NULL)   pIntList->iValueCount = 0;

    MC_TRACE("mcGetThemeIntList: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeMargins(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            int iPropId, RECT* prc, MARGINS* pMargins)
{
    if(theme_GetThemeMargins != NULL) {
        return theme_GetThemeMargins(hTheme, hdc, iPartId, iStateId, iPropId,
                                     prc, pMargins);
    }

    if(pMargins != NULL)  memset(pMargins, 0, sizeof(MARGINS));

    MC_TRACE("mcGetThemeMargins: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeMetric(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            int iPropId, int* piValue)
{
    if(theme_GetThemeMetric != NULL)
        return theme_GetThemeMetric(hTheme, hdc, iPartId, iStateId, iPropId, piValue);

    if(piValue != NULL)  *piValue = 0;

    MC_TRACE("mcGetThemeMetric: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemePartSize(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const RECT* prc, enum THEMESIZE eSize, SIZE* psz)
{
    if(theme_GetThemePartSize != NULL) {
        return theme_GetThemePartSize(hTheme, hdc, iPartId, iStateId, prc,
                                      eSize, psz);
    }

    if(psz != NULL)  { psz->cx = 0; psz->cy = 0; }

    MC_TRACE("mcGetThemePartSize: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemePosition(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            POINT* pPoint)
{
    if(theme_GetThemePosition != NULL)
        return theme_GetThemePosition(hTheme, iPartId, iStateId, iPropId, pPoint);

    if(pPoint != NULL)  { pPoint->x = 0; pPoint->y = 0; }

    MC_TRACE("mcGetThemePosition: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemePropertyOrigin(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            enum PROPERTYORIGIN* pOrigin)
{
    if(theme_GetThemePropertyOrigin != NULL) {
        return theme_GetThemePropertyOrigin(hTheme, iPartId, iStateId, iPropId,
                                            pOrigin);
    }

    if(pOrigin != NULL)  *pOrigin = PO_NOTFOUND;

    MC_TRACE("mcGetThemePropertyOrigin: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeRect(HTHEME hTheme, int iPartId, int iStateId, int iPropId, RECT* prc)
{
    if(theme_GetThemeRect != NULL)
        return theme_GetThemeRect(hTheme, iPartId, iStateId, iPropId, prc);

    if(prc != NULL)  mc_rect_set(prc, 0, 0, 0, 0);

    MC_TRACE("mcGetThemeRect: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeStream(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            void** ppvStream, DWORD* pcbStream, HINSTANCE hInst)
{
    if(theme_GetThemeStream != NULL) {
        return theme_GetThemeStream(hTheme, iPartId, iStateId, iPropId,
                                    ppvStream, pcbStream, hInst);
    }

    if(ppvStream != NULL)  *ppvStream = NULL;
    if(pcbStream != NULL)  *pcbStream = 0;

    MC_TRACE("mcGetThemeStream: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeString(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
            WCHAR* pszBuff, int cchMaxBuffChars)
{
    if(theme_GetThemeString != NULL) {
        return theme_GetThemeString(hTheme, iPartId, iStateId, iPropId,
                                    pszBuff, cchMaxBuffChars);
    }

    if(cchMaxBuffChars > 0)  pszBuff[0] = L'\0';

    MC_TRACE("mcGetThemeString: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

BOOL MCTRL_API
mcGetThemeSysBool(HTHEME hTheme, int iBoolId)
{
    if(theme_GetThemeSysBool != NULL)
        return theme_GetThemeSysBool(hTheme, iBoolId);

    if(iBoolId == TMT_FLATMENUS) {
        BOOL ret;
        if(SystemParametersInfo(SPI_GETFLATMENU, 0, &ret, 0))
            return ret;
        else
            return FALSE;   /* Win2k does not know SPI_GETFLATMENU */
    }

    MC_TRACE("mcGetThemeSysBool: Stub [FALSE]");
    return FALSE;
}

COLORREF MCTRL_API
mcGetThemeSysColor(HTHEME hTheme, int iColorId)
{
    if(theme_GetThemeSysColor != NULL)
        return theme_GetThemeSysColor(hTheme, iColorId);

    return GetSysColor(iColorId);
}

HBRUSH MCTRL_API
mcGetThemeSysColorBrush(HTHEME hTheme, int iColorId)
{
    if(theme_GetThemeSysColorBrush != NULL)
        return theme_GetThemeSysColorBrush(hTheme, iColorId);

    return GetSysColorBrush(iColorId);
}

HRESULT MCTRL_API
mcGetThemeSysFont(HTHEME hTheme, int iFontId, LOGFONTW* pLogFont)
{
    if(theme_GetThemeSysFont != NULL)
        return theme_GetThemeSysFont(hTheme, iFontId, pLogFont);

    if(iFontId == TMT_ICONTITLEFONT) {
        if(MC_ERR(!SystemParametersInfoW(SPI_GETICONTITLELOGFONT,
                                         sizeof(LOGFONTW), pLogFont, 0))) {
            MC_TRACE_ERR("SystemParametersInfoW(SPI_GETICONTITLELOGFONT) failed");
            return HRESULT_FROM_WIN32(GetLastError());
        }
        return S_OK;
    } else {
        NONCLIENTMETRICSW ncm;
        LOGFONTW* lf;

        ncm.cbSize = sizeof(NONCLIENTMETRICSW);
        if(MC_ERR(!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                         sizeof(NONCLIENTMETRICSW), &ncm, 0))) {
            MC_TRACE_ERR("SystemParametersInfoW(NONCLIENTMETRICSW) failed");
            return HRESULT_FROM_WIN32(GetLastError());
        }

        switch(iFontId) {
            case TMT_CAPTIONFONT:       lf = &ncm.lfCaptionFont; break;
            case TMT_SMALLCAPTIONFONT:  lf = &ncm.lfSmCaptionFont; break;
            case TMT_MENUFONT:          lf = &ncm.lfMenuFont; break;
            case TMT_STATUSFONT:        lf = &ncm.lfStatusFont; break;
            case TMT_MSGBOXFONT:        lf = &ncm.lfMessageFont; break;
            default:
                MC_TRACE("mcGetThemeSysFont: Unknown iFontId %d", iFontId);
                return STG_E_INVALIDPARAMETER;
        }

        memcpy(pLogFont, lf, sizeof(LOGFONTW));
        return S_OK;
    }
}

HRESULT MCTRL_API
mcGetThemeSysInt(HTHEME hTheme, int iIntId, int* piValue)
{
    if(theme_GetThemeSysInt != NULL)
        return theme_GetThemeSysInt(hTheme, iIntId, piValue);

    if(piValue != NULL)  *piValue = 0;

    MC_TRACE("mcGetThemeSysInt: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

int MCTRL_API
mcGetThemeSysSize(HTHEME hTheme, int iSizeId)
{
    if(theme_GetThemeSysSize != NULL)
        return theme_GetThemeSysSize(hTheme, iSizeId);

    return GetSystemMetrics(iSizeId);
}

HRESULT MCTRL_API
mcGetThemeSysString(HTHEME hTheme, int iStringId, WCHAR* pszBuff,
            int cchMaxBuffChars)
{
    if(theme_GetThemeSysString != NULL)
        return theme_GetThemeSysString(hTheme, iStringId, pszBuff, cchMaxBuffChars);

    if(cchMaxBuffChars > 0)  pszBuff[0] = L'\0';

    MC_TRACE("mcGetThemeSysString: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeTextExtent(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            const WCHAR* pszText, int cchTextMax, DWORD dwFlags,
            const RECT* prcBounding, RECT* prcExtent)
{
    if(theme_GetThemeTextExtent != NULL) {
        return theme_GetThemeTextExtent(hTheme, hdc, iPartId, iStateId,
                                        pszText, cchTextMax, dwFlags,
                                        prcBounding, prcExtent);
    }

    if(prcExtent != NULL)  mc_rect_set(prcExtent, 0, 0, 0, 0);

    MC_TRACE("mcGetThemeTextExtent: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeTextMetrics(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            TEXTMETRIC* pTextMetric)
{
    if(theme_GetThemeTextMetrics != NULL)
        theme_GetThemeTextMetrics(hTheme, hdc, iPartId, iStateId, pTextMetric);

    if(pTextMetric)  memset(pTextMetric, 0, sizeof(TEXTMETRIC));

    MC_TRACE("mcGetThemeTextMetrics: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcGetThemeTransitionDuration(HTHEME hTheme, int iPartId, int iStateIdFrom,
            int iStateIdTo, int iPropId, DWORD* pdwDuration)
{
    if(theme_GetThemeTransitionDuration != NULL) {
        return theme_GetThemeTransitionDuration(hTheme, iPartId, iStateIdFrom,
                                    iStateIdTo, iPropId, pdwDuration);
    }

    if(pdwDuration)  *pdwDuration = 0;

    MC_TRACE("mcGetThemeTransitionDuration: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HTHEME MCTRL_API
mcGetWindowTheme(HWND hwnd)
{
    if(theme_GetWindowTheme != NULL)
        return theme_GetWindowTheme(hwnd);

    MC_TRACE("mcGetWindowTheme: Stub [NULL]");
    return NULL;
}

HRESULT MCTRL_API
mcHitTestThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
            DWORD dwOptions, const RECT* prc, HRGN hrgn, POINT ptTest,
            WORD* pwHitTestCode)
{
    if(theme_HitTestThemeBackground != NULL) {
        return theme_HitTestThemeBackground(hTheme, hdc, iPartId, iStateId,
                                dwOptions, prc, hrgn, ptTest, pwHitTestCode);
    }

    if(pwHitTestCode != NULL)  *pwHitTestCode = HTNOWHERE;

    MC_TRACE("mcHitTestThemeBackground: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

BOOL MCTRL_API
mcIsAppThemed(void)
{
    if(theme_IsAppThemed != NULL)
        return theme_IsAppThemed();

    MC_TRACE("mcIsAppThemed: Stub [FALSE]");
    return FALSE;
}

BOOL MCTRL_API
mcIsCompositionActive(void)
{
    if(theme_IsCompositionActive != NULL)
        return theme_IsCompositionActive();

    MC_TRACE("mcIsCompositionActive: Stub [FALSE]");
    return FALSE;
}

BOOL MCTRL_API
mcIsThemeActive(void)
{
    if(theme_IsThemeActive != NULL)
        return theme_IsThemeActive();

    MC_TRACE("mcIsThemeActive: Stub [FALSE]");
    return FALSE;
}

BOOL MCTRL_API
mcIsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId)
{
    if(theme_IsThemeBackgroundPartiallyTransparent != NULL)
        return theme_IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId);

    MC_TRACE("mcIsThemeBackgroundPartiallyTransparent: Stub [FALSE]");
    return FALSE;
}

BOOL MCTRL_API
mcIsThemeDialogTextureEnabled(HWND hwnd)
{
    if(theme_IsThemeDialogTextureEnabled != NULL)
        return theme_IsThemeDialogTextureEnabled(hwnd);

    MC_TRACE("mcIsThemeDialogTextureEnabled: Stub [FALSE]");
    return FALSE;
}

BOOL MCTRL_API
mcIsThemePartDefined(HTHEME hTheme, int iPartId, int iStateId)
{
    if(theme_IsThemePartDefined != NULL)
        return theme_IsThemePartDefined(hTheme, iPartId, iStateId);

    MC_TRACE("mcIsThemePartDefined: Stub [FALSE]");
    return FALSE;
}

HTHEME MCTRL_API
mcOpenThemeData(HWND hwnd, const WCHAR* pszClassList)
{
    if(theme_OpenThemeData != NULL)
        return theme_OpenThemeData(hwnd, pszClassList);

    MC_TRACE("mcOpenThemeData: Stub [NULL]");
    return NULL;
}

HTHEME MCTRL_API
mcOpenThemeDataEx(HWND hwnd, const WCHAR* pszClassList, DWORD dwFlags)
{
    if(theme_OpenThemeDataEx != NULL)
        return theme_OpenThemeDataEx(hwnd, pszClassList, dwFlags);

    MC_TRACE("mcOpenThemeDataEx: Stub [NULL]");
    return NULL;
}

void MCTRL_API
mcSetThemeAppProperties(DWORD dwFlags)
{
    if(theme_SetThemeAppProperties != NULL)
        theme_SetThemeAppProperties(dwFlags);
}

HRESULT MCTRL_API
mcSetWindowTheme(HWND hwnd, const WCHAR* pszSubAppName, const WCHAR* pszSubIdList)
{
    if(theme_SetWindowTheme != NULL)
        return theme_SetWindowTheme(hwnd, pszSubAppName, pszSubIdList);

    MC_TRACE("mcSetWindowTheme: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

HRESULT MCTRL_API
mcSetWindowThemeAttribute(HWND hwnd, enum WINDOWTHEMEATTRIBUTETYPE eAttribute,
            void* pvAttribute, DWORD cbAttribute)
{
    if(theme_SetWindowThemeAttribute != NULL) {
        return theme_SetWindowThemeAttribute(hwnd, eAttribute, pvAttribute,
                                             cbAttribute);
    }

    MC_TRACE("mcSetWindowThemeAttribute: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

BOOL MCTRL_API
mcUpdatePanningFeedback(HWND hwnd, LONG lTotalOverpanOffsetX,
            LONG lTotalOverpanOffsetY, BOOL fInInertia)
{
    if(theme_UpdatePanningFeedback != NULL) {
        return theme_UpdatePanningFeedback(hwnd, lTotalOverpanOffsetX,
                                           lTotalOverpanOffsetY, fInInertia);
    }

    MC_TRACE("mcUpdatePanningFeedback: Stub [FALSE]");
    return FALSE;
}


/**********************
 *** Initialization ***
 **********************/

int
theme_init_module(void)
{
    /* WinXP with COMCL32.DLL version 6.0 or newer is required for theming. */
    if(mc_win_version < MC_WIN_XP) {
        MC_TRACE("theme_init_module: UXTHEME.DLL not used (old Windows)");
        goto no_theming;
    }

    uxtheme_dll = mc_load_sys_dll(_T("UXTHEME.DLL"));
    if(MC_ERR(uxtheme_dll == NULL)) {
        MC_TRACE_ERR("theme_init_module: LoadLibrary(UXTHEME.DLL) failed");
        goto no_theming;
    }

#define GPA(rettype, name, params)                                            \
        do {                                                                  \
            theme_##name = (rettype (WINAPI*)params)                          \
                        GetProcAddress(uxtheme_dll, #name);                   \
            if(MC_ERR(theme_##name == NULL)) {                                \
                MC_TRACE_ERR("theme_init_module: "                            \
                             "GetProcAddress("#name") failed");               \
            }                                                                 \
        } while(0)

    GPA(HTHEME,   OpenThemeData, (HWND,const WCHAR*));
    /* OpenThemeDataEx() on WinXP is only exported as the ordinal #61. */
    if(mc_win_version > MC_WIN_XP)
        GPA(HTHEME,   OpenThemeDataEx, (HWND,const WCHAR*,DWORD));
    else
        theme_OpenThemeDataEx = (HTHEME (WINAPI*)(HWND,const WCHAR*,DWORD))
                    GetProcAddress(uxtheme_dll, MAKEINTRESOURCEA(61));
    GPA(HRESULT,  CloseThemeData, (HTHEME));
    GPA(HRESULT,  DrawThemeBackground, (HTHEME,HDC,int,int,const RECT*,const RECT*));
    GPA(HRESULT,  DrawThemeEdge, (HTHEME,HDC,int,int,const RECT*,UINT,UINT,RECT*));
    GPA(HRESULT,  DrawThemeIcon, (HTHEME,HDC,int,int,const RECT*,HIMAGELIST,int));
    GPA(HRESULT,  DrawThemeParentBackground, (HWND,HDC,RECT*));
    GPA(HRESULT,  DrawThemeText, (HTHEME,HDC,int,int,const WCHAR*,int,DWORD,DWORD,const RECT*));
    GPA(HRESULT,  EnableThemeDialogTexture, (HWND,DWORD));
    GPA(HRESULT,  GetCurrentThemeName, (WCHAR*,int,WCHAR*,int,WCHAR*,int));
    GPA(DWORD,    GetThemeAppProperties, (void));
    GPA(HRESULT,  GetThemeBackgroundContentRect, (HTHEME,HDC,int,int,const RECT*,RECT*));
    GPA(HRESULT,  GetThemeBackgroundExtent, (HTHEME,HDC,int,int,const RECT*,RECT*));
    GPA(HRESULT,  GetThemeBackgroundRegion, (HTHEME,HDC,int,int,const RECT*,HRGN*));
    GPA(HRESULT,  GetThemeBool, (HTHEME,int,int,int,BOOL*));
    GPA(HRESULT,  GetThemeColor, (HTHEME,int,int,int,COLORREF*));
    GPA(HRESULT,  GetThemeDocumentationProperty, (const WCHAR*,const WCHAR*,WCHAR*,int));
    GPA(HRESULT,  GetThemeEnumValue, (HTHEME,int,int,int,int*));
    GPA(HRESULT,  GetThemeFilename, (HTHEME,int,int,int,WCHAR*,int));
    GPA(HRESULT,  GetThemeFont, (HTHEME,HDC,int,int,int,LOGFONTW*));
    GPA(HRESULT,  GetThemeInt, (HTHEME,int,int,int,int*));
    GPA(HRESULT,  GetThemeIntList, (HTHEME,int,int,int,INTLIST*));
    GPA(HRESULT,  GetThemeMargins, (HTHEME,HDC,int,int,int,RECT*,MARGINS*));
    GPA(HRESULT,  GetThemeMetric, (HTHEME,HDC,int,int,int,int*));
    GPA(HRESULT,  GetThemePartSize, (HTHEME,HDC,int,int,const RECT*,enum THEMESIZE,SIZE*));
    GPA(HRESULT,  GetThemePosition, (HTHEME,int,int,int,POINT*));
    GPA(HRESULT,  GetThemePropertyOrigin, (HTHEME,int,int,int,enum PROPERTYORIGIN*));
    GPA(HRESULT,  GetThemeRect, (HTHEME,int,int,int,RECT*));
    GPA(HRESULT,  GetThemeString, (HTHEME,int,int,int,WCHAR*,int));
    GPA(BOOL,     GetThemeSysBool, (HTHEME,int));
    GPA(COLORREF, GetThemeSysColor, (HTHEME,int));
    GPA(HBRUSH,   GetThemeSysColorBrush, (HTHEME,int));
    GPA(HRESULT,  GetThemeSysFont, (HTHEME,int,LOGFONTW*));
    GPA(HRESULT,  GetThemeSysInt, (HTHEME,int,int*));
    GPA(int,      GetThemeSysSize, (HTHEME,int));
    GPA(HRESULT,  GetThemeSysString, (HTHEME,int,WCHAR*,int));
    GPA(HRESULT,  GetThemeTextExtent, (HTHEME,HDC,int,int,const WCHAR*,int,DWORD,const RECT*,RECT*));
    GPA(HRESULT,  GetThemeTextMetrics, (HTHEME,HDC,int,int,TEXTMETRIC*));
    GPA(HTHEME,   GetWindowTheme, (HWND));
    GPA(HRESULT,  HitTestThemeBackground, (HTHEME,HDC,int,int,DWORD,const RECT*,HRGN,POINT,WORD*));
    GPA(BOOL,     IsAppThemed, (void));
    GPA(BOOL,     IsThemeActive, (void));
    GPA(BOOL,     IsThemeBackgroundPartiallyTransparent, (HTHEME,int,int));
    GPA(BOOL,     IsThemeDialogTextureEnabled, (HWND));
    GPA(BOOL,     IsThemePartDefined, (HTHEME,int,int));
    GPA(void,     SetThemeAppProperties, (DWORD));
    GPA(HRESULT,  SetWindowTheme, (HWND,const WCHAR*,const WCHAR*));

    if(mc_win_version > MC_WIN_XP) {
        GPA(BOOL,     IsCompositionActive, (void));
        GPA(HRESULT,  DrawThemeBackgroundEx, (HTHEME,HDC,int,int,const RECT*,const DTBGOPTS*));
        GPA(HRESULT,  DrawThemeParentBackgroundEx, (HWND,HDC,DWORD,RECT*));
        GPA(HRESULT,  DrawThemeTextEx, (HTHEME,HDC,int,int,const WCHAR*,int,DWORD,RECT*,const DTTOPTS*));
        GPA(HRESULT,  GetThemeBitmap, (HTHEME,int,int,int,ULONG,HBITMAP*));
        GPA(HRESULT,  GetThemeStream, (HTHEME,int,int,int,void**,DWORD*,HINSTANCE));
        GPA(HRESULT,  GetThemeTransitionDuration, (HTHEME,int,int,int,int,DWORD*));
        GPA(HRESULT,  SetWindowThemeAttribute, (HWND,enum WINDOWTHEMEATTRIBUTETYPE,void*,DWORD));

        /* Buffered paint & animations */
        GPA(HANIMATIONBUFFER, BeginBufferedAnimation, (HWND,HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,BP_ANIMATIONPARAMS*,HDC*,HDC*));
        GPA(HPAINTBUFFER, BeginBufferedPaint, (HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,HDC*));
        GPA(BOOL,     BeginPanningFeedback, (HWND));
        GPA(HRESULT,  BufferedPaintClear, (HPAINTBUFFER,const RECT*));
        GPA(HRESULT,  BufferedPaintInit, (void));
        GPA(BOOL,     BufferedPaintRenderAnimation, (HWND,HDC));
        GPA(HRESULT,  BufferedPaintSetAlpha, (HPAINTBUFFER,const RECT*,BYTE));
        GPA(HRESULT,  BufferedPaintStopAllAnimations, (HWND));
        GPA(HRESULT,  BufferedPaintUnInit, (void));
        GPA(HRESULT,  EndBufferedAnimation, (HANIMATIONBUFFER,BOOL));
        GPA(HRESULT,  EndBufferedPaint, (HPAINTBUFFER,BOOL));
        GPA(BOOL,     EndPanningFeedback, (HWND,BOOL));
        GPA(HRESULT,  GetBufferedPaintBits, (HPAINTBUFFER,RGBQUAD**,int*));
        GPA(HDC,      GetBufferedPaintDC, (HPAINTBUFFER));
        GPA(HDC,      GetBufferedPaintTargetDC, (HPAINTBUFFER));
        GPA(HRESULT,  GetBufferedPaintTargetRect, (HPAINTBUFFER,RECT*));
        GPA(BOOL,     UpdatePanningFeedback, (HWND,LONG,LONG,BOOL));
    }

    if(mc_comctl32_version < MC_DLL_VER(6, 0)) {
        MC_TRACE("theme_init_module: Disabling themes (COMCTL32.DLL version < 6.0)");
        theme_OpenThemeData = NULL;
        theme_OpenThemeDataEx = NULL;
        theme_IsAppThemed = NULL;
    }

    /* Workaround: It seems that IsAppThemed() and IsCompositionActive()
     * always return FALSE initially until 1st window is created. As we do not
     * have any guaranty when we are called in application's flow, we create
     * a dummy window to get always the expected results. */
    DestroyWindow(CreateWindow(_T("STATIC"), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE,
                  0, mc_instance, NULL));

#ifdef DEBUG
    {
        DWORD app_props = mcGetThemeAppProperties();
        char app_props_str[64] = "";

        if(app_props & STAP_ALLOW_NONCLIENT)
            strcat(app_props_str, ", nonclient");
        if(app_props & STAP_ALLOW_CONTROLS)
            strcat(app_props_str, ", controls");
        if(app_props & STAP_ALLOW_WEBCONTENT)
            strcat(app_props_str, ", webcontent");
        if(app_props_str[0] == '\0')
            strcat(app_props_str, ", none");

        MC_TRACE("theme_init_module: IsThemeActive() -> %s",
                                (mcIsThemeActive() ? "yes" : "no"));
        MC_TRACE("theme_init_module: IsAppThemed() -> %s",
                                (mcIsAppThemed() ? "yes" : "no"));
        MC_TRACE("theme_init_module: GetThemeAppProperties() -> 0x%x (%s)",
                                app_props, app_props_str+2);
        MC_TRACE("theme_init_module: IsCompositionActive() -> %s",
                                (mcIsCompositionActive() ? "yes" : "no"));
    }
#endif

no_theming:
    /* We always "succeed". */
    return 0;
}

void
theme_fini_module(void)
{
    if(uxtheme_dll == NULL)
        return;

    theme_BeginBufferedAnimation = NULL;
    theme_BeginBufferedPaint = NULL;
    theme_BeginPanningFeedback = NULL;
    theme_BufferedPaintClear = NULL;
    theme_BufferedPaintInit = NULL;
    theme_BufferedPaintRenderAnimation = NULL;
    theme_BufferedPaintSetAlpha = NULL;
    theme_BufferedPaintStopAllAnimations = NULL;
    theme_BufferedPaintUnInit = NULL;
    theme_CloseThemeData = NULL;
    theme_DrawThemeBackground = NULL;
    theme_DrawThemeBackgroundEx = NULL;
    theme_DrawThemeEdge = NULL;
    theme_DrawThemeIcon = NULL;
    theme_DrawThemeParentBackground = NULL;
    theme_DrawThemeParentBackgroundEx = NULL;
    theme_DrawThemeText = NULL;
    theme_DrawThemeTextEx = NULL;
    theme_EnableThemeDialogTexture = NULL;
    theme_EndBufferedAnimation = NULL;
    theme_EndBufferedPaint = NULL;
    theme_EndPanningFeedback = NULL;
    theme_GetBufferedPaintBits = NULL;
    theme_GetBufferedPaintDC = NULL;
    theme_GetBufferedPaintTargetDC = NULL;
    theme_GetBufferedPaintTargetRect = NULL;
    theme_GetCurrentThemeName = NULL;
    theme_GetThemeAppProperties = NULL;
    theme_GetThemeBackgroundContentRect = NULL;
    theme_GetThemeBackgroundExtent = NULL;
    theme_GetThemeBackgroundRegion = NULL;
    theme_GetThemeBitmap = NULL;
    theme_GetThemeBool = NULL;
    theme_GetThemeColor = NULL;
    theme_GetThemeDocumentationProperty = NULL;
    theme_GetThemeEnumValue = NULL;
    theme_GetThemeFilename = NULL;
    theme_GetThemeFont = NULL;
    theme_GetThemeInt = NULL;
    theme_GetThemeIntList = NULL;
    theme_GetThemeMargins = NULL;
    theme_GetThemeMetric = NULL;
    theme_GetThemePartSize = NULL;
    theme_GetThemePosition = NULL;
    theme_GetThemePropertyOrigin = NULL;
    theme_GetThemeRect = NULL;
    theme_GetThemeStream = NULL;
    theme_GetThemeString = NULL;
    theme_GetThemeSysBool = NULL;
    theme_GetThemeSysColor = NULL;
    theme_GetThemeSysColorBrush = NULL;
    theme_GetThemeSysFont = NULL;
    theme_GetThemeSysInt = NULL;
    theme_GetThemeSysSize = NULL;
    theme_GetThemeSysString = NULL;
    theme_GetThemeTextExtent = NULL;
    theme_GetThemeTextMetrics = NULL;
    theme_GetThemeTransitionDuration = NULL;
    theme_GetWindowTheme = NULL;
    theme_HitTestThemeBackground = NULL;
    theme_IsAppThemed = NULL;
    theme_IsCompositionActive = NULL;
    theme_IsThemeActive = NULL;
    theme_IsThemeBackgroundPartiallyTransparent = NULL;
    theme_IsThemeDialogTextureEnabled = NULL;
    theme_IsThemePartDefined = NULL;
    theme_OpenThemeData = NULL;
    theme_OpenThemeDataEx = NULL;
    theme_SetThemeAppProperties = NULL;
    theme_SetWindowTheme = NULL;
    theme_SetWindowThemeAttribute = NULL;
    theme_UpdatePanningFeedback = NULL;

    FreeLibrary(uxtheme_dll);
    uxtheme_dll = NULL;
}

