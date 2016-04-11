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

#ifndef WDL_DRAW_H
#define WDL_DRAW_H

#include <windows.h>
#include <objidl.h>     /* IStream */

#ifdef __cplusplus
extern "C" {
#endif



/***************
 ***  Color  ***
 ***************/

/* 32-bit integer type representing a color.
 *
 * The color is made of four 8-bit components: red, green, blue and also
 * (optionally) alpha.
 *
 * The red, green and blue components range from most intensive (255) to
 * least intensive (0), and the alpha component from fully opaque (255) to
 * fully transparent (0).
 */
typedef DWORD WD_COLOR;

#define WD_ARGB(a,r,g,b)                                                    \
        ((((WD_COLOR)(a) & 0xff) << 24) | (((WD_COLOR)(r) & 0xff) << 16) |  \
         (((WD_COLOR)(g) & 0xff) << 8)  | (((WD_COLOR)(b) & 0xff) << 0))
#define WD_RGB(r,g,b)       WD_ARGB(255,(r),(g),(b))

#define WD_AVALUE(color)    (((WD_COLOR)(color) & 0xff000000U) >> 24)
#define WD_RVALUE(color)    (((WD_COLOR)(color) & 0x00ff0000U) >> 16)
#define WD_GVALUE(color)    (((WD_COLOR)(color) & 0x0000ff00U) >> 8)
#define WD_BVALUE(color)    (((WD_COLOR)(color) & 0x000000ffU) >> 0)


/*****************************
 ***  2D Geometry Objects  ***
 *****************************/

typedef struct WD_POINT_tag WD_POINT;
struct WD_POINT_tag {
    float x;
    float y;
};

typedef struct WD_LINE_tag WD_LINE;
struct WD_LINE_tag {
    float x0;
    float y0;
    float x1;
    float y1;
};

typedef struct WD_RECT_tag WD_RECT;
struct WD_RECT_tag {
    float x0;
    float y0;
    float x1;
    float y1;
};

typedef struct WD_CIRCLE_tag WD_CIRCLE;
struct WD_CIRCLE_tag {
    float x;
    float y;
    float r;
};


/************************
 ***  Initialization  ***
 ************************/

/* If the library is to be used in a context of multiple threads concurrently,
 * application has to provide a critical section to protect its internal
 * globals.
 *
 * Note that even then, object instances (like e.g. canvas, brushes, images)
 * cannot be used concurrently, each thread must work with its own objects.
 *
 * This function may be called only once, prior to any other use of the library,
 * even prior any call to wdInitialize().
 *
 * Additionally wdPreInitialize() allows application to disable certain
 * features:
 *
 * WD_DISABLE_D2D: Disable D2D back-end.
 *
 * WD_DISABLE_GDIPLUS: Disable GDI+ back-end.
 *
 * Note: If all back-ends are disabled, wdInitialize() will subsequently fail.
 *
 * Note 2: wdPreinitialize() can (unlike wdInitialize()) be called from
 * DllMain() context.
 */

#define WD_DISABLE_D2D              0x0001
#define WD_DISABLE_GDIPLUS          0x0002

void wdPreInitialize(CRITICAL_SECTION* pCritSection, DWORD dwFlags);


/* Initialization functions may be called multiple times, even concurrently
 * (assuming a critical section has been provided via wdPreInitialize()).
 *
 * The library maintains a counter for each module and it gets really
 * uninitialized when the respective counter drops back to zero.
 *
 * The corresponding pairs of wdInitialize() and wdTerminate() should be
 * always called with the same flags, otherwise you may cause a resource leak.
 *
 * Note: These functions cannot be called from DllMain().
 */
#define WD_INIT_COREAPI             0x0000
#define WD_INIT_IMAGEAPI            0x0001
#define WD_INIT_STRINGAPI           0x0002

BOOL wdInitialize(DWORD dwFlags);
void wdTerminate(DWORD dwFlags);


/*******************************
 ***  Opaque Object Handles  ***
 *******************************/

typedef struct WD_CANVAS_tag *WD_HCANVAS;
typedef struct WD_IMAGE_tag *WD_HIMAGE;
typedef struct WD_PATH_tag *WD_HPATH;
typedef struct WD_FONT_tag *WD_HFONT;


/***************************
 ***  Canvas Management  ***
 ***************************/

/* Canvas is an abstract object which can be painted with this library. */

#define WD_CANVAS_DOUBLEBUFFER      0x0001
#define WD_CANVAS_NOGDICOMPAT       0x0002      /* Disable GDI compatibility. */

WD_HCANVAS wdCreateCanvasWithPaintStruct(HWND hWnd, PAINTSTRUCT* pPS, DWORD dwFlags);
WD_HCANVAS wdCreateCanvasWithHDC(HDC hDC, const RECT* pRect, DWORD dwFlags);
void wdDestroyCanvas(WD_HCANVAS hCanvas);

/* All drawing, filling and bit-blitting operations to it should be only
 * performed between wdBeginPaint() and wdEndPaint() calls.
 *
 * Note the canvas (and all resource created from it) may be cached for the
 * reuse only in the following circumstances (all conditions have to be met):
 *
 * - The canvas has been created with wdCreateCanvasWithPaintStruct() and
 *   is used strictly for handling WM_PAINT.
 * - wdEndPaint() returns TRUE.
 */
void wdBeginPaint(WD_HCANVAS hCanvas);
BOOL wdEndPaint(WD_HCANVAS hCanvas);

/* This is supposed to be called to resize cached canvas (see above), if it
 * needs to be resized, typically as a response to WM_SIZE message.
 */
BOOL wdResizeCanvas(WD_HCANVAS hCanvas, UINT uWidth, UINT uHeight);

/* Unless you create the canvas with the WD_CANVAS_NOGDICOMPAT flag, you may
 * also use GDI to paint on it. To do so, call wdStartGdi() to acquire HDC.
 * When done, release the HDC with wdEndGdi(). (Note that between those two
 * calls, only GDI can be used for painting on the canvas.)
 */
HDC wdStartGdi(WD_HCANVAS hCanvas, BOOL bKeepContents);
void wdEndGdi(WD_HCANVAS hCanvas, HDC hDC);

/* Clear the whole canvas with the given color. */
void wdClear(WD_HCANVAS hCanvas, WD_COLOR color);

/* If both, pRect and hPath are set, the clipping is set to the intersection
 * of both. If none of them is set, the clipping is reset and painting is not
 * clipped at all. */
void wdSetClip(WD_HCANVAS hCanvas, const WD_RECT* pRect, const WD_HPATH hPath);

/* The painting is by default measured in pixel units: 1.0f corresponds to
 * the pixel width or height, depending on the current axis.
 *
 * Origin (the point [0.0f, 0.0f]) corresponds always the top left pixel of
 * the canvas.
 *
 * Though this can be changed if a transformation is applied on the canvas.
 * Transformation is determined by a matrix which can specify translation,
 * rotation and scaling (in both axes), or any combination of these operations.
 */
void wdRotateWorld(WD_HCANVAS hCanvas, float cx, float cy, float fAngle);
void wdTranslateWorld(WD_HCANVAS hCanvas, float dx, float dy);
void wdResetWorld(WD_HCANVAS hCanvas);


/**************************
 ***  Image Management  ***
 **************************/

/* All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_IMAGEAPI.
 *
 * Note that unlike most other resources, WD_HIMAGE is not canvas-specific
 */

WD_HIMAGE wdCreateImageFromHBITMAP(HBITMAP hBmp);
WD_HIMAGE wdLoadImageFromFile(const WCHAR* pszPath);
WD_HIMAGE wdLoadImageFromIStream(IStream* pStream);
WD_HIMAGE wdLoadImageFromResource(HINSTANCE hInstance,
                const WCHAR* pszResType, const WCHAR* pszResName);
void wdDestroyImage(WD_HIMAGE hImage);

void wdGetImageSize(WD_HIMAGE hImage, UINT* puWidth, UINT* puHeight);


/**************************
 ***  Brush Management  ***
 **************************/

/* Brush is an object used for drawing operations. Note the brush can only
 * be used for the canvas it has been created for. */

typedef struct WD_BRUSH_tag *WD_HBRUSH;

WD_HBRUSH wdCreateSolidBrush(WD_HCANVAS hCanvas, WD_COLOR color);
void wdDestroyBrush(WD_HBRUSH hBrush);

/* Can be only called for brushes created with wdCreateSolidBrush(). */
void wdSetSolidBrushColor(WD_HBRUSH hBrush, WD_COLOR color);


/*************************
 ***  Path Management  ***
 *************************/

/* Path is an object representing more complex and reusable shapes which can
 * be painted at once. Note the path can only be used for the canvas it has
 * been created for. */

WD_HPATH wdCreatePath(WD_HCANVAS hCanvas);
WD_HPATH wdCreatePolygonPath(WD_HCANVAS hCanvas, const WD_POINT* pPoints, UINT uCount);
void wdDestroyPath(WD_HPATH hPath);

typedef struct WD_PATHSINK_tag WD_PATHSINK;
struct WD_PATHSINK_tag {
    void* pData;
    WD_POINT ptEnd;
};

BOOL wdOpenPathSink(WD_PATHSINK* pSink, WD_HPATH hPath);
void wdClosePathSink(WD_PATHSINK* pSink);

void wdBeginFigure(WD_PATHSINK* pSink, const WD_POINT* pStartPoint);
void wdEndFigure(WD_PATHSINK* pSink, BOOL bCloseFigure);

void wdAddLine(WD_PATHSINK* pSink, const WD_POINT* pEndPoint);
void wdAddArc(WD_PATHSINK* pSink, const WD_POINT* pCenter, float fSweepAngle);


/*************************
 ***  Font Management  ***
 *************************/

/* All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_DRAWSTRINGAPI.
 *
 * Also note that usage of non-TrueType fonts is not supported by GDI+
 * so attempt to create such WD_HFONT will fall back to a default GUI font.
 */

WD_HFONT wdCreateFont(const LOGFONTW* pLogFont);
WD_HFONT wdCreateFontWithGdiHandle(HFONT hGdiFont);
void wdDestroyFont(WD_HFONT hFont);


/* Structure describing metrics of the font. */
typedef struct WD_FONT_METRICS_tag WD_FONT_METRICS;
struct WD_FONT_METRICS_tag {
    float fEmHeight;        /* Typically height of letter 'M' or 'H' */
    float fAscent;          /* Height of char cell above the base line. */
    float fDescent;         /* Height of char cell below the base line. */
    float fLeading;         /* Distance of two base lines in multi-line text. */

    /* Usually: fEmHeight < fAscent + fDescent <= fLeading */
};

void wdFontMetrics(WD_HFONT hFont, WD_FONT_METRICS* pMetrics);


/*************************
 ***  Draw Operations  ***
 *************************/

void wdDrawArc(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_CIRCLE* pCircle,
                float fBaseAngle, float fSweepAngle, float fStrokeWidth);
void wdDrawLine(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_LINE* pLine,
                float fStrokeWidth);
void wdDrawPath(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath,
                float fStrokeWidth);
void wdDrawPie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_CIRCLE* pCircle,
                float fBaseAngle, float fSweepAngle, float fStrokeWidth);
void wdDrawRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_RECT* pRect,
                float fStrokeWidth);


/*************************
 ***  Fill Operations  ***
 *************************/

void wdFillCircle(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_CIRCLE* pCircle);
void wdFillPath(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath);
void wdFillPie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_CIRCLE* pCircle,
                float fBaseAngle, float fSweepAngle);
void wdFillRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_RECT* pRect);


/*****************************
 ***  Bit-Blit Operations  ***
 *****************************/

/* All these functions are usable only if the library has been initialized
 * with the flag WD_INIT_IMAGEAPI.
 *
 * These functions are capable of bit-blit operation from some source image
 * to a destination canvas. If source and target rectangles gave different
 * dimensions, the functions scale the image during the operation.
 *
 * Note the destination rectangle has to be always specified. Source rectangle
 * is optional: If NULL, whole source image is taken.
 *
 */
void wdBitBltImage(WD_HCANVAS hCanvas, const WD_HIMAGE hImage,
                const WD_RECT* pDestRect, const WD_RECT* pSourceRect);
void wdBitBltHICON(WD_HCANVAS hCanvas, HICON hIcon,
                const WD_RECT* pDestRect, const WD_RECT* pSourceRect);


/****************************
 ***  Simple Text Output  ***
 ****************************/

/* Functions for basic string output. Note the functions operate strictly with
 * Unicode strings.
 *
 * All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_DRAWSTRINGAPI.
 */

/* Flags specifying alignment and various rendering options. */
#define WD_STR_LEFTALIGN        0x0000
#define WD_STR_CENTERALIGN      0x0001
#define WD_STR_RIGHTALIGN       0x0002
#define WD_STR_NOCLIP           0x0004
#define WD_STR_NOWRAP           0x0008
#define WD_STR_ENDELLIPSIS      0x0010
#define WD_STR_WORDELLIPSIS     0x0020
#define WD_STR_PATHELLIPSIS     0x0040

#define WD_STR_ALIGNMASK        (WD_STR_LEFTALIGN | WD_STR_CENTERALIGN | WD_STR_RIGHTALIGN)
#define WD_STR_ELLIPSISMASK     (WD_STR_ENDELLIPSIS | WD_STR_WORDELLIPSIS | WD_STR_PATHELLIPSIS)

void wdDrawString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect,
                const WCHAR* pszText, int iTextLength, WD_HBRUSH hBrush,
                DWORD dwFlags);

void wdMeasureString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect,
                const WCHAR* pszText, int iTextLength, WD_RECT* pResult,
                DWORD dwFlags);

/* Convenient wdMeasureString() wrapper. */
float wdStringWidth(WD_HCANVAS hCanvas, WD_HFONT hFont, const WCHAR* pszText);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* WDL_DRAW_H */
