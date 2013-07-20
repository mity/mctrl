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

#ifndef MC_GDIX_H
#define MC_GDIX_H

#include "misc.h"


/************************
 *** Fake <gdiplus.h> ***
 ************************/

/* Microsoft (and their SDK) does not support using GDI+ from C and C++ is
 * required. Their headers simply are incompatible with C. We load GDIPLUS.DLL
 * with LoadLibrary() so the C++ wrapper sugery in their headers cannot be
 * used. Hence we do not include any GDI+ related header and instead use our
 * own types.
 */

typedef void* gdix_Graphics;
typedef void* gdix_SolidFill;
typedef void* gdix_Brush;
typedef void* gdix_Pen;
typedef void* gdix_Font;
typedef void* gdix_StringFormat;
typedef void* gdix_Path;

typedef float gdix_Real;
typedef DWORD gdix_ARGB;

typedef enum gdix_Status_tag gdix_Status;
enum gdix_Status_tag {
    gdix_Ok = 0
};

typedef struct gdix_PointF_tag gdix_PointF;
struct gdix_PointF_tag {
    gdix_Real x;
    gdix_Real y;
};

typedef struct gdix_RectF_tag gdix_RectF;
struct gdix_RectF_tag {
    gdix_Real x;
    gdix_Real y;
    gdix_Real w;
    gdix_Real h;
};

typedef enum gdix_Unit_tag gdix_Unit;
enum gdix_Unit_tag {
    gdix_UnitWorld = 0
};

typedef enum gdix_SmoothingMode_tag gdix_SmoothingMode;
enum gdix_SmoothingMode_tag {
    gdix_SmoothingModeHighQuality = 2
};

typedef enum gdix_StringAlignment_tag gdix_StringAlignment;
enum gdix_StringAlignment_tag {
    gdix_StringAlignmentCenter = 1,
    gdix_StringAlignmentFar = 2
};

typedef enum gdix_FillMode_tag gdix_FillMode;
enum gdix_FillMode_tag  {
    gdix_FillModeAlternate = 0
} ;

#define gdix_StringFormatFlagsNoWrap   0x00001000
#define gdix_StringFormatFlagsNoClip   0x00004000


/**********************************
 *** Pointers to GDI+ functions ***
 **********************************/

/* Graphics management */
extern gdix_Status (WINAPI* gdix_CreateFromHDC)(HDC,gdix_Graphics**);
extern gdix_Status (WINAPI* gdix_DeleteGraphics)(gdix_Graphics*);
extern gdix_Status (WINAPI* gdix_SetSmoothingMode)(gdix_Graphics*,gdix_SmoothingMode);

/* Pen management */
extern gdix_Status (WINAPI* gdix_CreatePen1)(gdix_ARGB,gdix_Real,gdix_Unit,gdix_Pen**);
extern gdix_Status (WINAPI* gdix_DeletePen)(gdix_Pen*);
extern gdix_Status (WINAPI* gdix_SetPenWidth)(gdix_Pen*,gdix_Real);
extern gdix_Status (WINAPI* gdix_SetPenColor)(gdix_Pen*,gdix_ARGB);

/* Brush management */
extern gdix_Status (WINAPI* gdix_CreateSolidFill)(gdix_ARGB,gdix_SolidFill**);
extern gdix_Status (WINAPI* gdix_DeleteBrush)(gdix_Brush*);
extern gdix_Status (WINAPI* gdix_SetSolidFillColor)(gdix_SolidFill*,gdix_ARGB);

/* Font management */
extern gdix_Status (WINAPI* gdix_CreateFontFromDC)(HDC,gdix_Font**);
extern gdix_Status (WINAPI* gdix_CreateFontFromLogfontW)(HDC,const LOGFONTW*,gdix_Font**);
extern gdix_Status (WINAPI* gdix_DeleteFont)(gdix_Font*);

/* String format management */
extern gdix_Status (WINAPI* gdix_CreateStringFormat)(INT,LANGID,gdix_StringFormat**);
extern gdix_Status (WINAPI* gdix_DeleteStringFormat)(gdix_StringFormat*);
extern gdix_Status (WINAPI* gdix_SetStringFormatFlags)(gdix_StringFormat*,INT);
extern gdix_Status (WINAPI* gdix_SetStringFormatAlign)(gdix_StringFormat*,gdix_StringAlignment);

/* Path management */
extern gdix_Status (WINAPI* gdix_CreatePath)(gdix_FillMode,gdix_Path**);
extern gdix_Status (WINAPI* gdix_DeletePath)(gdix_Path*);
extern gdix_Status (WINAPI* gdix_ResetPath)(gdix_Path*);
extern gdix_Status (WINAPI* gdix_AddPathArc)(gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
extern gdix_Status (WINAPI* gdix_AddPathLine)(gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
extern gdix_Status (WINAPI* gdix_AddPathRectangle)(gdix_Path*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);

/* Draw methods */
extern gdix_Status (WINAPI* gdix_DrawLine)(gdix_Graphics*,gdix_Pen*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
extern gdix_Status (WINAPI* gdix_DrawLines)(gdix_Graphics*,gdix_Pen*,const gdix_PointF*,INT);
extern gdix_Status (WINAPI* gdix_DrawPie)(gdix_Graphics*,gdix_Pen*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real);

/* Fill methods */
extern gdix_Status (WINAPI* gdix_FillRectangle)(gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
extern gdix_Status (WINAPI* gdix_FillPolygon)(gdix_Graphics*,gdix_Brush*,const gdix_PointF*,INT,gdix_FillMode);
extern gdix_Status (WINAPI* gdix_FillPolygon2)(gdix_Graphics*,gdix_Brush*,const gdix_PointF*,INT);
extern gdix_Status (WINAPI* gdix_FillEllipse)(gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
extern gdix_Status (WINAPI* gdix_FillPie)(gdix_Graphics*,gdix_Brush*,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real,gdix_Real);
extern gdix_Status (WINAPI* gdix_FillPath)(gdix_Graphics*,gdix_Brush*,gdix_Path*);

/* String methods */
extern gdix_Status (WINAPI* gdix_DrawString)(gdix_Graphics*,const WCHAR*,INT,const gdix_Font*,const gdix_RectF*,const gdix_StringFormat*,const gdix_Brush*);
extern gdix_Status (WINAPI* gdix_MeasureString)(gdix_Graphics*,const WCHAR*,INT,const gdix_Font*,const gdix_RectF*,const gdix_StringFormat*,gdix_RectF*,INT*,INT*);


/*************************
 *** Wrapper functions ***
 *************************/

/* Safer function for getting a reasonable font. GDI+ supports only true-type
 * fonts and this does a reasonable fallback. */
gdix_Status gdix_CreateFontFromHFONT(HDC dc, HFONT font, gdix_Font** gdix_font);


/**********************
 *** ARGB functions ***
 **********************/

static inline gdix_ARGB
gdix_ARGB_from_argb(BYTE a, BYTE r, BYTE g, BYTE b)
{
    return ((((DWORD) a) << 24) | (((DWORD) r) << 16) |
            (((DWORD) g) <<  8) | (((DWORD) b) <<  0));
}

static inline gdix_ARGB
gdix_ARGB_from_rgb(BYTE r, BYTE g, BYTE b)
{
    return gdix_ARGB_from_argb(0xff, r, g, b);
}

static inline gdix_ARGB
gdix_ARGB_from_cr(COLORREF cr)
{
    return gdix_ARGB_from_argb(0xff, GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

static inline gdix_ARGB
gdix_ARGB_from_acr(BYTE a, COLORREF cr)
{
    return gdix_ARGB_from_argb(a, GetRValue(cr), GetGValue(cr), GetBValue(cr));
}


#endif  /* MC_GDIX_H */
