/*
 * C-Win32
 * <http://github.com/mity/c-win32>
 *
 * Copyright (c) 2015-2019 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef C_GDIPLUS_H
#define C_GDIPLUS_H

#include <windows.h>


/* MSDN documentation for <gdiplus/gdiplusflat.h> sucks. This one is better:
 * http://www.jose.it-berater.org/gdiplus/iframe/index.htm
 */

/* Note we don't declare any functions here: We load them dynamically anyway.
 */


typedef DWORD c_ARGB;

typedef INT c_GpPixelFormat;
#define c_PixelFormatGDI            0x00020000 /* Is a GDI-supported format */
#define c_PixelFormatAlpha          0x00040000 /* Has an alpha component */
#define c_PixelFormatPAlpha         0x00080000 /* Pre-multiplied alpha */
#define c_PixelFormatCanonical      0x00200000
#define c_PixelFormat24bppRGB       (8 | (24 << 8) | c_PixelFormatGDI)
#define c_PixelFormat32bppARGB      (10 | (32 << 8) | c_PixelFormatAlpha | c_PixelFormatGDI | c_PixelFormatCanonical)
#define c_PixelFormat32bppPARGB     (11 | (32 << 8) | c_PixelFormatAlpha | c_PixelFormatPAlpha | c_PixelFormatGDI)

#define c_ImageLockModeWrite        2


/*****************************
 ***  Helper Enumerations  ***
 *****************************/

typedef enum c_GpMatrixOrder_tag c_GpMatrixOrder;
enum c_GpMatrixOrder_tag {
    c_MatrixOrderPrepend = 0,
    c_MatrixOrderAppend = 1
};

typedef enum c_GpCombineMode_tag c_GpCombineMode;
enum c_GpCombineMode_tag {
    c_CombineModeReplace = 0,
    c_CombineModeIntersect = 1,
    c_CombineModeUnion = 2,
    c_CombineModeXor = 3,
    c_CombineModeExclude = 4,
    c_CombineModeComplement = 5
};

typedef enum c_GpPixelOffsetMode_tag c_GpPixelOffsetMode;
enum c_GpPixelOffsetMode_tag {
    c_PixelOffsetModeInvalid = -1,
    c_PixelOffsetModeDefault = 0,
    c_PixelOffsetModeHighSpeed = 1,
    c_PixelOffsetModeHighQuality = 2,
    c_PixelOffsetModeNone = 3,
    c_PixelOffsetModeHalf = 4
};

typedef enum c_GpSmoothingMode_tag c_GpSmoothingMode;
enum c_GpSmoothingMode_tag {
    c_SmoothingModeInvalid = -1,
    c_SmoothingModeDefault = 0,
    c_SmoothingModeHighSpeed = 1,
    c_SmoothingModeHighQuality = 2,
    c_SmoothingModeNone = 3,
    c_SmoothingModeAntiAlias8x4 = 4,
    c_SmoothingModeAntiAlias = 4,
    c_SmoothingModeAntiAlias8x8 = 5
};

typedef enum c_GpUnit_tag c_GpUnit;
enum c_GpUnit_tag {
    c_UnitWorld = 0,
    c_UnitDisplay = 1,
    c_UnitPixel = 2,
    c_UnitPoint = 3,
    c_UnitInch = 4,
    c_UnitDocument = 5,
    c_UnitMillimeter = 6
};

typedef enum c_GpFillMode_tag c_GpFillMode;
enum c_GpFillMode_tag {
    c_FillModeAlternate = 0,
    c_FillModeWinding = 1
};

typedef enum c_GpStringAlignment_tag c_GpStringAlignment;
enum c_GpStringAlignment_tag {
    c_StringAlignmentNear = 0,
    c_StringAlignmentCenter = 1,
    c_StringAlignmentFar = 2
};

typedef enum c_GpStringFormatFlags_tag c_GpStringFormatFlags;
enum c_GpStringFormatFlags_tag {
    c_StringFormatFlagsDirectionRightToLeft = 0x00000001,
    c_StringFormatFlagsDirectionVertical = 0x00000002,
    c_StringFormatFlagsNoFitBlackBox = 0x00000004,
    c_StringFormatFlagsDisplayFormatControl = 0x00000020,
    c_StringFormatFlagsNoFontFallback = 0x00000400,
    c_StringFormatFlagsMeasureTrailingSpaces = 0x00000800,
    c_StringFormatFlagsNoWrap = 0x00001000,
    c_StringFormatFlagsLineLimit = 0x00002000,
    c_StringFormatFlagsNoClip = 0x00004000
};

typedef enum c_GpStringTrimming_tag c_GpStringTrimming;
enum c_GpStringTrimming_tag {
    c_StringTrimmingNone = 0,
    c_StringTrimmingCharacter = 1,
    c_StringTrimmingWord = 2,
    c_StringTrimmingEllipsisCharacter = 3,
    c_StringTrimmingEllipsisWord = 4,
    c_StringTrimmingEllipsisPath = 5
};

typedef enum c_GpLineCap_tag c_GpLineCap;
enum c_GpLineCap_tag {
    c_LineCapFlat = 0,
    c_LineCapSquare = 1,
    c_LineCapRound = 2,
    c_LineCapTriangle = 3,
};

typedef enum c_GpLineJoin_tag c_GpLineJoin;
enum c_GpLineJoin_tag {
    c_LineJoinMiter = 0,
    c_LineJoinBevel = 1,
    c_LineJoinRound = 2
};

typedef enum c_GpDashStyle_tag c_GpDashStyle;
enum c_GpDashStyle_tag {
    c_DashStyleSolid = 0,
    c_DashStyleDash = 1,
    c_DashStyleDot = 2,
    c_DashStyleDashDot = 3,
    c_DashStyleDashDotDot = 4,
    c_DashStyleCustom = 5
};

typedef enum c_GpWrapMode_tag c_GpWrapMode;
enum c_GpWrapMode_tag {
    c_WrapModeTile = 0,
    c_WrapModeTileFlipX = 1,
    c_WrapModeTileFlipY = 2,
    c_WrapModeTileFlipXY = 3,
    c_WrapModeClamp = 4
};


/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct c_GpStartupInput_tag c_GpStartupInput;
struct c_GpStartupInput_tag {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;  /* DebugEventProc (not used) */
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
};

typedef struct c_GpPointF_tag c_GpPointF;
struct c_GpPointF_tag {
    float x;
    float y;
};

typedef struct c_GpRectF_tag c_GpRectF;
struct c_GpRectF_tag {
    float x;
    float y;
    float w;
    float h;
};

typedef struct c_GpRectI_tag c_GpRectI;
struct c_GpRectI_tag {
    INT x;
    INT y;
    INT w;
    INT h;
};

typedef struct c_GpBitmapData_tag c_GpBitmapData;
struct c_GpBitmapData_tag {
    UINT width;
    UINT height;
    INT Stride;
    c_GpPixelFormat PixelFormat;
    void *Scan0;
    UINT_PTR Reserved;
};


/**********************
 ***  GDI+ Objects  ***
 **********************/

typedef struct c_GpBrush_tag        c_GpBrush;
typedef struct c_GpCachedBitmap_tag c_GpCachedBitmap;
typedef struct c_GpFont_tag         c_GpFont;
typedef struct c_GpGraphics_tag     c_GpGraphics;
typedef struct c_GpImage_tag        c_GpImage;
typedef struct c_GpPath_tag         c_GpPath;
typedef struct c_GpPen_tag          c_GpPen;
typedef struct c_GpStringFormat_tag c_GpStringFormat;
typedef struct c_GpMatrix_tag       c_GpMatrix;

/* These are "derived" from the types aboves (more specialized). */
typedef struct c_GpImage_tag        c_GpBitmap;
typedef struct c_GpBrush_tag        c_GpSolidFill;
typedef struct c_GpBrush_tag        c_GpLineGradient;
typedef struct c_GpBrush_tag        c_GpPathGradient;


#endif  /* C_GDIPLUS_H */
