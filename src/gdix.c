/*
 * Copyright (c) 2013 Martin Mitas
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

#include "gdix.h"

/* Uncomment this to have more verbose traces from this module. */
/*#define GDIX_DEBUG     1*/

#ifdef GDIX_DEBUG
    #define GDIX_TRACE         MC_TRACE
#else
    #define GDIX_TRACE(...)    do {} while(0)
#endif


GpStatus (WINAPI* gdix_CreateFromHDC)(HDC,GpGraphics**);
GpStatus (WINAPI* gdix_DeleteGraphics)(GpGraphics*);
GpStatus (WINAPI* gdix_SetCompositingMode)(GpGraphics*,CompositingMode);
GpStatus (WINAPI* gdix_SetSmoothingMode)(GpGraphics*,SmoothingMode);

GpStatus (WINAPI* gdix_CreatePen1)(ARGB,REAL,GpUnit,GpPen**);
GpStatus (WINAPI* gdix_DeletePen)(GpPen*);
GpStatus (WINAPI* gdix_SetPenWidth)(GpPen*,REAL);
GpStatus (WINAPI* gdix_SetPenColor)(GpPen*,ARGB);

GpStatus (WINAPI* gdix_CreateSolidFill)(ARGB,GpSolidFill**);
GpStatus (WINAPI* gdix_DeleteBrush)(GpBrush*);
GpStatus (WINAPI* gdix_SetSolidFillColor)(GpSolidFill*,ARGB);

GpStatus (WINAPI* gdix_CreateFontFromDC)(HDC,GpFont**);
GpStatus (WINAPI* gdix_DeleteFont)(GpFont*);

GpStatus (WINAPI* gdix_CreateStringFormat)(INT,LANGID,GpStringFormat**);
GpStatus (WINAPI* gdix_DeleteStringFormat)(GpStringFormat*);
GpStatus (WINAPI* gdix_SetStringFormatFlags)(GpStringFormat*,INT);
GpStatus (WINAPI* gdix_SetStringFormatAlign)(GpStringFormat*,StringAlignment);

GpStatus (WINAPI* gdix_CreatePath)(GpFillMode,GpPath**);
GpStatus (WINAPI* gdix_DeletePath)(GpPath*);
GpStatus (WINAPI* gdix_AddPathArc)(GpPath*,REAL,REAL,REAL,REAL,REAL,REAL);
GpStatus (WINAPI* gdix_AddPathRectangle)(GpPath*,REAL,REAL,REAL,REAL);

GpStatus (WINAPI* gdix_DrawLine)(GpGraphics*,GpPen*,REAL,REAL,REAL,REAL);
GpStatus (WINAPI* gdix_DrawLines)(GpGraphics*,GpPen*,GDIPCONST GpPointF*,INT);
GpStatus (WINAPI* gdix_DrawPie)(GpGraphics*,GpPen*,REAL,REAL,REAL,REAL,REAL,REAL);

GpStatus (WINAPI* gdix_FillRectangle)(GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL);
GpStatus (WINAPI* gdix_FillPolygon)(GpGraphics*,GpBrush*,GDIPCONST GpPointF*,INT,GpFillMode);
GpStatus (WINAPI* gdix_FillPolygon2)(GpGraphics*,GpBrush*,GDIPCONST GpPointF*,INT);
GpStatus (WINAPI* gdix_FillEllipse)(GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL);
GpStatus (WINAPI* gdix_FillPie)(GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL,REAL,REAL);
GpStatus (WINAPI* gdix_FillPath)(GpGraphics*,GpBrush*,GpPath*);

GpStatus (WINAPI* gdix_DrawString)(GpGraphics*,GDIPCONST WCHAR*,INT,GDIPCONST GpFont*,GDIPCONST RectF*,GDIPCONST GpStringFormat*,GDIPCONST GpBrush*);
GpStatus (WINAPI* gdix_MeasureString)(GpGraphics*,GDIPCONST WCHAR*,INT,GDIPCONST GpFont*,GDIPCONST RectF*,GDIPCONST GpStringFormat*,RectF*,INT*,INT*);


static HMODULE gdix_dll;
static ULONG_PTR gdix_token;
static void (WINAPI* gdix_Shutdown)(ULONG_PTR);


int
gdix_init(void)
{
    GpStatus (WINAPI* gdix_Startup)(ULONG_PTR*,GDIPCONST GdiplusStartupInput*,GdiplusStartupOutput*);
    GpStatus status;
    GdiplusStartupInput input = { 0 };

    gdix_dll = LoadLibrary(_T("GDIPLUS.DLL"));
    if(MC_ERR(gdix_dll == NULL)) {
        MC_TRACE("gdix_init: LoadLibrary(GDIPLUS.DLL) failed [%ld].",
                 GetLastError());
        goto err_LoadLibrary;
    }

    gdix_Startup = (GpStatus (WINAPI*)(ULONG_PTR*,GDIPCONST GdiplusStartupInput*,GdiplusStartupOutput*))
                                                GetProcAddress(gdix_dll, "GdiplusStartup");
    if(MC_ERR(gdix_Startup == NULL)) {
        MC_TRACE("gdix_init: GetProcAddress(GdiplusStartup) failed [%ld].",
                 GetLastError());
        goto err_GetProcAddress;
    }

    gdix_Shutdown = (void (WINAPI*)(ULONG_PTR)) GetProcAddress(gdix_dll, "GdiplusShutdown");
    if(MC_ERR(gdix_Shutdown == NULL)) {
        MC_TRACE("gdix_init: GetProcAddress(GdiplusShutdown) failed [%ld].",
                 GetLastError());
        goto err_GetProcAddress;
    }

#define GPA(name, params)                                                     \
        do {                                                                  \
            gdix_##name = (GpStatus (WINAPI*)params)                          \
                        GetProcAddress(gdix_dll, "Gdip" #name);               \
            if(MC_ERR(gdix_##name == NULL)) {                                 \
                MC_TRACE("gdix_init: GetProcAddress(Gdip%s) failed [%ld].",   \
                         #name, GetLastError());                              \
                goto err_GetProcAddress;                                      \
            }                                                                 \
        } while(0)

    GPA(CreateFromHDC,        (HDC,GpGraphics**));
    GPA(DeleteGraphics,       (GpGraphics*));
    GPA(SetCompositingMode,   (GpGraphics*,CompositingMode));
    GPA(SetSmoothingMode,     (GpGraphics*,SmoothingMode));

    GPA(CreatePen1,           (ARGB,REAL,GpUnit,GpPen**));
    GPA(DeletePen,            (GpPen*));
    GPA(SetPenWidth,          (GpPen*,REAL));
    GPA(SetPenColor,          (GpPen*,ARGB));

    GPA(CreateSolidFill,      (ARGB,GpSolidFill**));
    GPA(DeleteBrush,          (GpBrush*));
    GPA(SetSolidFillColor,    (GpSolidFill*,ARGB));

    GPA(CreateFontFromDC,     (HDC,GpFont**));
    GPA(DeleteFont,           (GpFont*));

    GPA(CreatePath,           (GpFillMode,GpPath**));
    GPA(DeletePath,           (GpPath*));
    GPA(AddPathArc,           (GpPath*,REAL,REAL,REAL,REAL,REAL,REAL));
    GPA(AddPathRectangle,     (GpPath*,REAL,REAL,REAL,REAL));

    GPA(CreateStringFormat,   (INT,LANGID,GpStringFormat**));
    GPA(DeleteStringFormat,   (GpStringFormat*));
    GPA(SetStringFormatFlags, (GpStringFormat*,INT));
    GPA(SetStringFormatAlign, (GpStringFormat*,StringAlignment));

    GPA(DrawLine,             (GpGraphics*,GpPen*,REAL,REAL,REAL,REAL));
    GPA(DrawLines,            (GpGraphics*,GpPen*,GDIPCONST GpPointF*,INT));
    GPA(DrawPie,              (GpGraphics*,GpPen*,REAL,REAL,REAL,REAL,REAL,REAL));

    GPA(FillRectangle,        (GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL));
    GPA(FillPolygon,          (GpGraphics*,GpBrush*,GDIPCONST GpPointF*,INT,GpFillMode));
    GPA(FillPolygon2,         (GpGraphics*,GpBrush*,GDIPCONST GpPointF*,INT));
    GPA(FillEllipse,          (GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL));
    GPA(FillPie,              (GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL,REAL,REAL));
    GPA(FillPath,             (GpGraphics*,GpBrush*,GpPath*));

    GPA(DrawString,           (GpGraphics*,GDIPCONST WCHAR*,INT,GDIPCONST GpFont*,GDIPCONST RectF*,GDIPCONST GpStringFormat*,GDIPCONST GpBrush*));
    GPA(MeasureString,        (GpGraphics*,GDIPCONST WCHAR*,INT,GDIPCONST GpFont*,GDIPCONST RectF*,GDIPCONST GpStringFormat*,RectF*,INT*,INT*));

#undef GPA

    input.GdiplusVersion = 1;
    input.SuppressExternalCodecs = TRUE;
    status = gdix_Startup(&gdix_token, &input, NULL);
    if(MC_ERR(status != Ok)) {
        MC_TRACE("GdiplusStartup() failed. [%d]", (int)status);
        goto err_Startup;
    }

    GDIX_TRACE("gdix_init: Success.");
    return 0;

err_Startup:
err_GetProcAddress:
    FreeLibrary(gdix_dll);
err_LoadLibrary:
    return -1;
}

void
gdix_fini(void)
{
    gdix_Shutdown(gdix_token);
    FreeLibrary(gdix_dll);
}
