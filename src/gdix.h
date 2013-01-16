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

#include <gdiplus.h>


extern GpStatus (WINAPI* gdix_CreateFromHDC)(HDC,GpGraphics**);
extern GpStatus (WINAPI* gdix_DeleteGraphics)(GpGraphics*);
extern GpStatus (WINAPI* gdix_SetCompositingMode)(GpGraphics*,CompositingMode);
extern GpStatus (WINAPI* gdix_SetSmoothingMode)(GpGraphics*,SmoothingMode);

/* Pen management */
extern GpStatus (WINAPI* gdix_CreatePen1)(ARGB,REAL,GpUnit,GpPen**);
extern GpStatus (WINAPI* gdix_DeletePen)(GpPen*);
extern GpStatus (WINAPI* gdix_SetPenWidth)(GpPen*,REAL);
extern GpStatus (WINAPI* gdix_SetPenColor)(GpPen*,ARGB);

/* Brush management */
extern GpStatus (WINAPI* gdix_CreateSolidFill)(ARGB,GpSolidFill**);
extern GpStatus (WINAPI* gdix_DeleteBrush)(GpBrush*);
extern GpStatus (WINAPI* gdix_SetSolidFillColor)(GpSolidFill*,ARGB);

/* Font management */
extern GpStatus (WINAPI* gdix_CreateFontFromDC)(HDC,GpFont**);
extern GpStatus (WINAPI* gdix_DeleteFont)(GpFont*);

/* String format management */
extern GpStatus (WINAPI* gdix_CreateStringFormat)(INT,LANGID,GpStringFormat**);
extern GpStatus (WINAPI* gdix_DeleteStringFormat)(GpStringFormat*);
extern GpStatus (WINAPI* gdix_SetStringFormatFlags)(GpStringFormat*,INT);
extern GpStatus (WINAPI* gdix_SetStringFormatAlign)(GpStringFormat*,StringAlignment);

/* Path management */
extern GpStatus (WINAPI* gdix_CreatePath)(GpFillMode,GpPath**);
extern GpStatus (WINAPI* gdix_DeletePath)(GpPath*);
extern GpStatus (WINAPI* gdix_ResetPath)(GpPath*);
extern GpStatus (WINAPI* gdix_AddPathArc)(GpPath*,REAL,REAL,REAL,REAL,REAL,REAL);
extern GpStatus (WINAPI* gdix_AddPathLine)(GpPath*,REAL,REAL,REAL,REAL);
extern GpStatus (WINAPI* gdix_AddPathRectangle)(GpPath*,REAL,REAL,REAL,REAL);

/* Draw methods */
extern GpStatus (WINAPI* gdix_DrawLine)(GpGraphics*,GpPen*,REAL,REAL,REAL,REAL);
extern GpStatus (WINAPI* gdix_DrawLines)(GpGraphics*,GpPen*,GDIPCONST GpPointF*,INT);
extern GpStatus (WINAPI* gdix_DrawPie)(GpGraphics*,GpPen*,REAL,REAL,REAL,REAL,REAL,REAL);

/* Fill methods */
extern GpStatus (WINAPI* gdix_FillRectangle)(GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL);
extern GpStatus (WINAPI* gdix_FillPolygon)(GpGraphics*,GpBrush*,GDIPCONST GpPointF*,INT,GpFillMode);
extern GpStatus (WINAPI* gdix_FillPolygon2)(GpGraphics*,GpBrush*,GDIPCONST GpPointF*,INT);
extern GpStatus (WINAPI* gdix_FillEllipse)(GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL);
extern GpStatus (WINAPI* gdix_FillPie)(GpGraphics*,GpBrush*,REAL,REAL,REAL,REAL,REAL,REAL);
extern GpStatus (WINAPI* gdix_FillPath)(GpGraphics*,GpBrush*,GpPath*);

/* String methods */
extern GpStatus (WINAPI* gdix_DrawString)(GpGraphics*,GDIPCONST WCHAR*,INT,GDIPCONST GpFont*,GDIPCONST RectF*,GDIPCONST GpStringFormat*,GDIPCONST GpBrush*);
extern GpStatus (WINAPI* gdix_MeasureString)(GpGraphics*,GDIPCONST WCHAR*,INT,GDIPCONST GpFont*,GDIPCONST RectF*,GDIPCONST GpStringFormat*,RectF*,INT*,INT*);


static inline ARGB
gdix_ARGB(BYTE a, BYTE r, BYTE g, BYTE b)
{
    return (ARGB) ((((DWORD) a) << 24) | (((DWORD) r) << 16) |
                   (((DWORD) g) <<  8) | (((DWORD) b) <<  0));
}

static inline ARGB
gdix_RGB(BYTE r, BYTE g, BYTE b)
{
    return gdix_ARGB(0xff, r, g, b);
}

static inline ARGB
gdix_Color(COLORREF color)
{
    return gdix_ARGB(0xff, GetRValue(color), GetGValue(color), GetBValue(color));
}

static inline ARGB
gdix_AColor(BYTE a, COLORREF color)
{
    return gdix_ARGB(a, GetRValue(color), GetGValue(color), GetBValue(color));
}


int gdix_init(void);
void gdix_fini(void);


#endif  /* MC_GDIX_H */
