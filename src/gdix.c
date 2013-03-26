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


/* Graphics management */
gdix_Status (WINAPI* gdix_CreateFromHDC)(HDC,gdix_Graphics**);
gdix_Status (WINAPI* gdix_DeleteGraphics)(gdix_Graphics*);
gdix_Status (WINAPI* gdix_SetSmoothingMode)(gdix_Graphics*,gdix_SmoothingMode);

/* Pen management */
gdix_Status (WINAPI* gdix_CreatePen1)(gdix_ARGB,gdix_Real,gdix_Unit,gdix_Pen**);
gdix_Status (WINAPI* gdix_DeletePen)(gdix_Pen*);
gdix_Status (WINAPI* gdix_SetPenWidth)(gdix_Pen*,gdix_Real);
gdix_Status (WINAPI* gdix_SetPenColor)(gdix_Pen*,gdix_ARGB);

/* Brush management */
gdix_Status (WINAPI* gdix_CreateSolidFill)(gdix_ARGB,gdix_SolidFill**);
gdix_Status (WINAPI* gdix_DeleteBrush)(gdix_Brush*);
gdix_Status (WINAPI* gdix_SetSolidFillColor)(gdix_SolidFill*,gdix_ARGB);

/* Font management */
gdix_Status (WINAPI* gdix_CreateFontFromDC)(HDC,gdix_Font**);
gdix_Status (WINAPI* gdix_CreateFontFromLogfontW)(HDC,const LOGFONTW*,gdix_Font**);
gdix_Status (WINAPI* gdix_DeleteFont)(gdix_Font*);

/* String format management */
gdix_Status (WINAPI* gdix_CreateStringFormat)(INT,LANGID,gdix_StringFormat**);
gdix_Status (WINAPI* gdix_DeleteStringFormat)(gdix_StringFormat*);
gdix_Status (WINAPI* gdix_SetStringFormatFlags)(gdix_StringFormat*,INT);
gdix_Status (WINAPI* gdix_SetStringFormatAlign)(gdix_StringFormat*,gdix_StringAlignment);

/* Path management */
gdix_Status (WINAPI* gdix_CreatePath)(gdix_FillMode,gdix_Path**);
gdix_Status (WINAPI* gdix_DeletePath)(gdix_Path*);
gdix_Status (WINAPI* gdix_ResetPath)(gdix_Path*);
gdix_Status (WINAPI* gdix_AddPathArc)(gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
gdix_Status (WINAPI* gdix_AddPathLine)(gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
gdix_Status (WINAPI* gdix_AddPathRectangle)(gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);

/* Draw methods */
gdix_Status (WINAPI* gdix_DrawLine)(gdix_Graphics*,gdix_Pen*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
gdix_Status (WINAPI* gdix_DrawLines)(gdix_Graphics*,gdix_Pen*,const gdix_PointF*,INT);
gdix_Status (WINAPI* gdix_DrawPie)(gdix_Graphics*,gdix_Pen*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real);

/* Fill methods */
gdix_Status (WINAPI* gdix_FillRectangle)(gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
gdix_Status (WINAPI* gdix_FillPolygon)(gdix_Graphics*,gdix_Brush*,const gdix_PointF*,INT,gdix_FillMode);
gdix_Status (WINAPI* gdix_FillPolygon2)(gdix_Graphics*,gdix_Brush*,const gdix_PointF*,INT);
gdix_Status (WINAPI* gdix_FillEllipse)(gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
gdix_Status (WINAPI* gdix_FillPie)(gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
gdix_Status (WINAPI* gdix_FillPath)(gdix_Graphics*,gdix_Brush*,gdix_Path*);

/* String methods */
gdix_Status (WINAPI* gdix_DrawString)(gdix_Graphics*,const WCHAR*,INT,const gdix_Font*,const gdix_RectF*,const gdix_StringFormat*,const gdix_Brush*);
gdix_Status (WINAPI* gdix_MeasureString)(gdix_Graphics*,const WCHAR*,INT,const gdix_Font*,const gdix_RectF*,const gdix_StringFormat*,gdix_RectF*,INT*,INT*);



static HMODULE gdix_dll;
static ULONG_PTR gdix_token;
static void (WINAPI* gdix_Shutdown)(ULONG_PTR);


typedef struct gdix_StartupInput_tag gdix_StartupInput;
struct gdix_StartupInput_tag {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;  /* DebugEventProc */
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

typedef struct gdix_StartupOutput_tag gdix_StartupOutput;
/* We do not use gdix_StartupOutput, so no definition */


gdix_Status
gdix_CreateFontFromHFONT(HDC dc, HFONT font, gdix_Font** gdix_font)
{
    LOGFONTW lf;
    gdix_Status status;
    int retries;

    if(font == NULL)
        font = GetStockObject(SYSTEM_FONT);
    GetObjectW(font, sizeof(LOGFONTW), &lf);

    for(retries = 0; retries < 2; retries++) {
        status = gdix_CreateFontFromLogfontW(dc, &lf, gdix_font);
        if(MC_ERR(status != gdix_Ok)) {
            /* Perhaps it is a font which does not have true-type outlines?
             * Fallback to a font which should have them. */
            wcscpy(lf.lfFaceName,
                   (mc_win_version >= MC_WIN_VISTA ? L"Segoe UI" : L"Tahoma"));
        } else {
            break;
        }
    }

    return status;
}

int
gdix_init_module(void)
{
    gdix_Status (WINAPI* gdix_Startup)(ULONG_PTR*,const gdix_StartupInput*,gdix_StartupOutput*);
    gdix_Status status;
    gdix_StartupInput input = { 0 };

    gdix_dll = LoadLibrary(_T("GDIPLUS.DLL"));
    if(MC_ERR(gdix_dll == NULL)) {
        MC_TRACE_ERR("gdix_init_module: LoadLibrary(GDIPLUS.DLL) failed");
        goto err_LoadLibrary;
    }

    gdix_Startup = (gdix_Status (WINAPI*)(ULONG_PTR*,const gdix_StartupInput*,gdix_StartupOutput*))
                                                GetProcAddress(gdix_dll, "GdiplusStartup");
    if(MC_ERR(gdix_Startup == NULL)) {
        MC_TRACE_ERR("gdix_init_module: GetProcAddress(GdiplusStartup) failed");
        goto err_GetProcAddress;
    }

    gdix_Shutdown = (void (WINAPI*)(ULONG_PTR)) GetProcAddress(gdix_dll, "GdiplusShutdown");
    if(MC_ERR(gdix_Shutdown == NULL)) {
        MC_TRACE_ERR("gdix_init_module: GetProcAddress(GdiplusShutdown) failed");
        goto err_GetProcAddress;
    }

#define GPA(name, params)                                                     \
        do {                                                                  \
            gdix_##name = (gdix_Status (WINAPI*)params)                       \
                        GetProcAddress(gdix_dll, "Gdip" #name);               \
            if(MC_ERR(gdix_##name == NULL)) {                                 \
                MC_TRACE_ERR("gdix_init_module: "                             \
                             "GetProcAddress(Gdip"#name") failed");           \
                goto err_GetProcAddress;                                      \
            }                                                                 \
        } while(0)

    GPA(CreateFromHDC,        (HDC,gdix_Graphics**));
    GPA(DeleteGraphics,       (gdix_Graphics*));
    GPA(SetSmoothingMode,     (gdix_Graphics*,gdix_SmoothingMode));

    GPA(CreatePen1,           (gdix_ARGB,gdix_Real,gdix_Unit,gdix_Pen**));
    GPA(DeletePen,            (gdix_Pen*));
    GPA(SetPenWidth,          (gdix_Pen*,gdix_Real));
    GPA(SetPenColor,          (gdix_Pen*,gdix_ARGB));

    GPA(CreateSolidFill,      (gdix_ARGB,gdix_SolidFill**));
    GPA(DeleteBrush,          (gdix_Brush*));
    GPA(SetSolidFillColor,    (gdix_SolidFill*,gdix_ARGB));

    GPA(CreateFontFromDC,     (HDC,gdix_Font**));
    GPA(CreateFontFromLogfontW, (HDC,const LOGFONTW*,gdix_Font**));
    GPA(DeleteFont,           (gdix_Font*));

    GPA(CreatePath,           (gdix_FillMode,gdix_Path**));
    GPA(DeletePath,           (gdix_Path*));
    GPA(ResetPath,            (gdix_Path*));
    GPA(AddPathArc,           (gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real));
    GPA(AddPathLine,          (gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real));
    GPA(AddPathRectangle,     (gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real));

    GPA(CreateStringFormat,   (INT,LANGID,gdix_StringFormat**));
    GPA(DeleteStringFormat,   (gdix_StringFormat*));
    GPA(SetStringFormatFlags, (gdix_StringFormat*,INT));
    GPA(SetStringFormatAlign, (gdix_StringFormat*,gdix_StringAlignment));

    GPA(DrawLine,             (gdix_Graphics*,gdix_Pen*,gdix_Real,gdix_Real,gdix_Real,gdix_Real));
    GPA(DrawLines,            (gdix_Graphics*,gdix_Pen*,const gdix_PointF*,INT));
    GPA(DrawPie,              (gdix_Graphics*,gdix_Pen*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real));

    GPA(FillRectangle,        (gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real));
    GPA(FillPolygon,          (gdix_Graphics*,gdix_Brush*,const gdix_PointF*,INT,gdix_FillMode));
    GPA(FillPolygon2,         (gdix_Graphics*,gdix_Brush*,const gdix_PointF*,INT));
    GPA(FillEllipse,          (gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real));
    GPA(FillPie,              (gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real));
    GPA(FillPath,             (gdix_Graphics*,gdix_Brush*,gdix_Path*));

    GPA(DrawString,           (gdix_Graphics*,const WCHAR*,INT,const gdix_Font*,const gdix_RectF*,const gdix_StringFormat*,const gdix_Brush*));
    GPA(MeasureString,        (gdix_Graphics*,const WCHAR*,INT,const gdix_Font*,const gdix_RectF*,const gdix_StringFormat*,gdix_RectF*,INT*,INT*));

#undef GPA

    input.GdiplusVersion = 1;
    input.SuppressExternalCodecs = TRUE;
    status = gdix_Startup(&gdix_token, &input, NULL);
    if(MC_ERR(status != gdix_Ok)) {
        MC_TRACE("GdiplusStartup() failed. [%d]", (int)status);
        goto err_Startup;
    }

    GDIX_TRACE("gdix_init_module: Success.");
    return 0;

err_Startup:
err_GetProcAddress:
    FreeLibrary(gdix_dll);
err_LoadLibrary:
    return -1;
}

void
gdix_fini_module(void)
{
    gdix_Shutdown(gdix_token);
    FreeLibrary(gdix_dll);
}
