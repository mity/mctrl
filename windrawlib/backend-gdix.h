/*
 * Copyright (c) 2015-2016 Martin Mitas
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

#ifndef WD_BACKEND_GDIX_H
#define WD_BACKEND_GDIX_H

#include "misc.h"
#include "dummy/gdiplus.h"


typedef struct gdix_canvas_tag gdix_canvas_t;
struct gdix_canvas_tag {
    HDC dc;
    dummy_GpGraphics* graphics;
    dummy_GpPen* pen;
    dummy_GpStringFormat* string_format;

    HDC real_dc;        /* non-NULL if double buffering is enabled. */
    HBITMAP orig_bmp;
    int x;
    int y;
    int cx;
    int cy;
};


/* Graphics functions */
extern int (WINAPI* gdix_CreateFromHDC)(HDC, dummy_GpGraphics**);
extern int (WINAPI* gdix_DeleteGraphics)(dummy_GpGraphics*);
extern int (WINAPI* gdix_GraphicsClear)(dummy_GpGraphics*, dummy_ARGB);
extern int (WINAPI* gdix_GetDC)(dummy_GpGraphics*, HDC*);
extern int (WINAPI* gdix_ReleaseDC)(dummy_GpGraphics*, HDC);
extern int (WINAPI* gdix_ResetWorldTransform)(dummy_GpGraphics*);
extern int (WINAPI* gdix_RotateWorldTransform)(dummy_GpGraphics*, float, dummy_GpMatrixOrder);
extern int (WINAPI* gdix_SetPixelOffsetMode)(dummy_GpGraphics*, dummy_GpPixelOffsetMode);
extern int (WINAPI* gdix_SetSmoothingMode)(dummy_GpGraphics*, dummy_GpSmoothingMode);
extern int (WINAPI* gdix_TranslateWorldTransform)(dummy_GpGraphics*, float, float, dummy_GpMatrixOrder);
extern int (WINAPI* gdix_SetClipRect)(dummy_GpGraphics*, float, float, float, float, dummy_GpCombineMode);
extern int (WINAPI* gdix_SetClipPath)(dummy_GpGraphics*, dummy_GpPath*, dummy_GpCombineMode);
extern int (WINAPI* gdix_ResetClip)(dummy_GpGraphics*);

/* Brush functions */
extern int (WINAPI* gdix_CreateSolidFill)(dummy_ARGB, dummy_GpSolidFill**);
extern int (WINAPI* gdix_DeleteBrush)(dummy_GpBrush*);
extern int (WINAPI* gdix_SetSolidFillColor)(dummy_GpSolidFill*, dummy_ARGB);

/* Pen functions */
extern int (WINAPI* gdix_CreatePen1)(dummy_ARGB, float, dummy_GpUnit, dummy_GpPen**);
extern int (WINAPI* gdix_DeletePen)(dummy_GpPen*);
extern int (WINAPI* gdix_SetPenBrushFill)(dummy_GpPen*, dummy_GpBrush*);
extern int (WINAPI* gdix_SetPenWidth)(dummy_GpPen*, float);

/* Path functions */
extern int (WINAPI* gdix_CreatePath)(dummy_GpFillMode, dummy_GpPath**);
extern int (WINAPI* gdix_DeletePath)(dummy_GpPath*);
extern int (WINAPI* gdix_ClosePathFigure)(dummy_GpPath*);
extern int (WINAPI* gdix_StartPathFigure)(dummy_GpPath*);
extern int (WINAPI* gdix_GetPathLastPoint)(dummy_GpPath*, dummy_GpPointF*);
extern int (WINAPI* gdix_AddPathArc)(dummy_GpPath*, float, float, float, float, float, float);
extern int (WINAPI* gdix_AddPathLine)(dummy_GpPath*, float, float, float, float);

/* Font functions */
extern int (WINAPI* gdix_CreateFontFromLogfontW)(HDC, const LOGFONTW*, dummy_GpFont**);
extern int (WINAPI* gdix_DeleteFont)(dummy_GpFont*);
extern int (WINAPI* gdix_DeleteFontFamily)(dummy_GpFont*);
extern int (WINAPI* gdix_GetCellAscent)(const dummy_GpFont*, int, UINT16*);
extern int (WINAPI* gdix_GetCellDescent)(const dummy_GpFont*, int, UINT16*);
extern int (WINAPI* gdix_GetEmHeight)(const dummy_GpFont*, int, UINT16*);
extern int (WINAPI* gdix_GetFamily)(dummy_GpFont*, void**);
extern int (WINAPI* gdix_GetFontSize)(dummy_GpFont*, float*);
extern int (WINAPI* gdix_GetFontStyle)(dummy_GpFont*, int*);
extern int (WINAPI* gdix_GetLineSpacing)(const dummy_GpFont*, int, UINT16*);

/* Image & bitmap functions */
extern int (WINAPI* gdix_LoadImageFromFile)(const WCHAR*, dummy_GpImage**);
extern int (WINAPI* gdix_LoadImageFromStream)(IStream*, dummy_GpImage**);
extern int (WINAPI* gdix_CreateBitmapFromHBITMAP)(HBITMAP, HPALETTE, dummy_GpBitmap**);
extern int (WINAPI* gdix_CreateBitmapFromHICON)(HICON, dummy_GpBitmap**);
extern int (WINAPI* gdix_DisposeImage)(dummy_GpImage*);
extern int (WINAPI* gdix_GetImageWidth)(dummy_GpImage*, UINT*);
extern int (WINAPI* gdix_GetImageHeight)(dummy_GpImage*, UINT*);

/* String format functions */
extern int (WINAPI* gdix_CreateStringFormat)(int, LANGID, dummy_GpStringFormat**);
extern int (WINAPI* gdix_DeleteStringFormat)(dummy_GpStringFormat*);
extern int (WINAPI* gdix_SetStringFormatAlign)(dummy_GpStringFormat*, dummy_GpStringAlignment);
extern int (WINAPI* gdix_SetStringFormatFlags)(dummy_GpStringFormat*, int);
extern int (WINAPI* gdix_SetStringFormatTrimming)(dummy_GpStringFormat*, dummy_GpStringTrimming);

/* Draw/fill functions */
extern int (WINAPI* gdix_DrawArc)(dummy_GpGraphics*, dummy_GpPen*, float, float, float, float, float, float);
extern int (WINAPI* gdix_DrawImageRectRect)(dummy_GpGraphics*, dummy_GpImage*, float, float, float, float, float, float, float, float, dummy_GpUnit, const void*, void*, void*);
extern int (WINAPI* gdix_DrawLine)(dummy_GpGraphics*, dummy_GpPen*, float, float, float, float);
extern int (WINAPI* gdix_DrawPath)(dummy_GpGraphics*, dummy_GpPen*, dummy_GpPath*);
extern int (WINAPI* gdix_DrawPie)(dummy_GpGraphics*, dummy_GpPen*, float, float, float, float, float, float);
extern int (WINAPI* gdix_DrawRectangle)(dummy_GpGraphics*, void*, float, float, float, float);
extern int (WINAPI* gdix_DrawString)(dummy_GpGraphics*, const WCHAR*, int, const dummy_GpFont*, const dummy_GpRectF*, const dummy_GpStringFormat*, const dummy_GpBrush*);
extern int (WINAPI* gdix_FillEllipse)(dummy_GpGraphics*, dummy_GpBrush*, float, float, float, float);
extern int (WINAPI* gdix_FillPath)(dummy_GpGraphics*, dummy_GpBrush*, dummy_GpPath*);
extern int (WINAPI* gdix_FillPie)(dummy_GpGraphics*, dummy_GpBrush*, float, float, float, float, float, float);
extern int (WINAPI* gdix_FillRectangle)(dummy_GpGraphics*, void*, float, float, float, float);
extern int (WINAPI* gdix_MeasureString)(dummy_GpGraphics*, const WCHAR*, int, const dummy_GpFont*, const dummy_GpRectF*, const dummy_GpStringFormat*, dummy_GpRectF*, int*, int*);


int gdix_init(void);
void gdix_fini(void);


/* Helpers */
gdix_canvas_t* gdix_canvas_alloc(HDC dc, const RECT* doublebuffer_rect);
void gdix_canvas_apply_string_flags(gdix_canvas_t* c, DWORD flags);


#endif  /* WD_BACKEND_GDIX_H */
