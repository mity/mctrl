/*
 * C-Win32
 * <http://github.com/mity/c-win32>
 *
 * Copyright (c) 2017-2019 Martin Mitas
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

#ifndef C_D2D1_H
#define C_D2D1_H

#include <windows.h>
#include <unknwn.h>

#include <dcommon.h>
#include <d2dbasetypes.h>
#include <d2derr.h>

#include <wincodec.h>   /* IWICBitmapSource */
#include "c-dwrite.h"   /* For painting c_IDWriteTextLayout */


static const GUID c_IID_ID2D1Factory =
        {0x06152247,0x6f50,0x465a,{0x92,0x45,0x11,0x8b,0xfd,0x3b,0x60,0x07}};

static const GUID c_IID_ID2D1GdiInteropRenderTarget =
        {0xe0db51c3,0x6f77,0x4bae,{0xb3,0xd5,0xe4,0x75,0x09,0xb3,0x58,0x38}};


/******************************
 ***  Forward declarations  ***
 ******************************/

typedef struct c_IDWriteTextLayout_tag              c_IDWriteTextLayout;

typedef struct c_ID2D1Bitmap_tag                    c_ID2D1Bitmap;
typedef struct c_ID2D1BitmapRenderTarget_tag        c_ID2D1BitmapRenderTarget;
typedef struct c_ID2D1Brush_tag                     c_ID2D1Brush;
typedef struct c_ID2D1StrokeStyle_tag               c_ID2D1StrokeStyle;
typedef struct c_ID2D1DCRenderTarget_tag            c_ID2D1DCRenderTarget;
typedef struct c_ID2D1Factory_tag                   c_ID2D1Factory;
typedef struct c_ID2D1GdiInteropRenderTarget_tag    c_ID2D1GdiInteropRenderTarget;
typedef struct c_ID2D1Geometry_tag                  c_ID2D1Geometry;
typedef struct c_ID2D1GeometrySink_tag              c_ID2D1GeometrySink;
typedef struct c_ID2D1HwndRenderTarget_tag          c_ID2D1HwndRenderTarget;
typedef struct c_ID2D1Layer_tag                     c_ID2D1Layer;
typedef struct c_ID2D1PathGeometry_tag              c_ID2D1PathGeometry;
typedef struct c_ID2D1RenderTarget_tag              c_ID2D1RenderTarget;
typedef struct c_ID2D1SolidColorBrush_tag           c_ID2D1SolidColorBrush;
typedef struct c_ID2D1LinearGradientBrush_tag       c_ID2D1LinearGradientBrush;
typedef struct c_ID2D1RadialGradientBrush_tag       c_ID2D1RadialGradientBrush;
typedef struct c_ID2D1GradientStopCollection_tag    c_ID2D1GradientStopCollection;


/*****************************
 ***  Helper Enumerations  ***
 *****************************/

#define c_D2D1_DRAW_TEXT_OPTIONS_CLIP               0x00000002
#define c_D2D1_PRESENT_OPTIONS_NONE                 0x00000000
#define c_D2D1_LAYER_OPTIONS_NONE                   0x00000000
#define c_D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE   0x00000002

typedef enum c_D2D1_TEXT_ANTIALIAS_MODE_tag c_D2D1_TEXT_ANTIALIAS_MODE;
enum  c_D2D1_TEXT_ANTIALIAS_MODE_tag {
    c_D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE = 1
};

typedef enum c_DXGI_FORMAT_tag c_DXGI_FORMAT;
enum c_DXGI_FORMAT_tag {
    c_DXGI_FORMAT_B8G8R8A8_UNORM = 87
};

typedef enum c_D2D1_ANTIALIAS_MODE_tag c_D2D1_ANTIALIAS_MODE;
enum c_D2D1_ANTIALIAS_MODE_tag {
    c_D2D1_ANTIALIAS_MODE_PER_PRIMITIVE = 0
};

typedef enum c_D2D1_ALPHA_MODE_tag c_D2D1_ALPHA_MODE;
enum c_D2D1_ALPHA_MODE_tag {
    c_D2D1_ALPHA_MODE_UNKNOWN = 0,
    c_D2D1_ALPHA_MODE_PREMULTIPLIED = 1
};

typedef enum c_D2D1_ARC_SIZE_tag c_D2D1_ARC_SIZE;
enum c_D2D1_ARC_SIZE_tag {
    c_D2D1_ARC_SIZE_SMALL = 0,
    c_D2D1_ARC_SIZE_LARGE = 1
};

typedef enum c_D2D1_DC_INITIALIZE_MODE_tag c_D2D1_DC_INITIALIZE_MODE;
enum c_D2D1_DC_INITIALIZE_MODE_tag {
    c_D2D1_DC_INITIALIZE_MODE_COPY = 0,
    c_D2D1_DC_INITIALIZE_MODE_CLEAR = 1
};

typedef enum c_D2D1_DEBUG_LEVEL_tag c_D2D1_DEBUG_LEVEL;
enum c_D2D1_DEBUG_LEVEL_tag {
    c_D2D1_DEBUG_LEVEL_NONE = 0
};

typedef enum c_D2D1_FACTORY_TYPE_tag c_D2D1_FACTORY_TYPE;
enum c_D2D1_FACTORY_TYPE_tag {
    c_D2D1_FACTORY_TYPE_SINGLE_THREADED = 0,
    c_D2D1_FACTORY_TYPE_MULTI_THREADED = 1
};

typedef enum c_D2D1_FEATURE_LEVEL_tag c_D2D1_FEATURE_LEVEL;
enum c_D2D1_FEATURE_LEVEL_tag {
    c_D2D1_FEATURE_LEVEL_DEFAULT = 0
};

typedef enum c_D2D1_FIGURE_BEGIN_tag c_D2D1_FIGURE_BEGIN;
enum c_D2D1_FIGURE_BEGIN_tag {
    c_D2D1_FIGURE_BEGIN_FILLED = 0,
    c_D2D1_FIGURE_BEGIN_HOLLOW = 1
};

typedef enum c_D2D1_FIGURE_END_tag c_D2D1_FIGURE_END;
enum c_D2D1_FIGURE_END_tag {
    c_D2D1_FIGURE_END_OPEN = 0,
    c_D2D1_FIGURE_END_CLOSED = 1
};

typedef enum c_D2D1_BITMAP_INTERPOLATION_MODE_tag c_D2D1_BITMAP_INTERPOLATION_MODE;
enum c_D2D1_BITMAP_INTERPOLATION_MODE_tag {
    c_D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR = 0,
    c_D2D1_BITMAP_INTERPOLATION_MODE_LINEAR = 1
};

typedef enum c_D2D1_RENDER_TARGET_TYPE_tag c_D2D1_RENDER_TARGET_TYPE;
enum c_D2D1_RENDER_TARGET_TYPE_tag {
    c_D2D1_RENDER_TARGET_TYPE_DEFAULT = 0
};

typedef enum c_D2D1_SWEEP_DIRECTION_tag c_D2D1_SWEEP_DIRECTION;
enum c_D2D1_SWEEP_DIRECTION_tag {
    c_D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE = 0,
    c_D2D1_SWEEP_DIRECTION_CLOCKWISE = 1
};

typedef enum c_D2D1_CAP_STYLE_tag c_D2D1_CAP_STYLE;
enum c_D2D1_CAP_STYLE_tag {
    c_D2D1_CAP_STYLE_FLAT = 0,
    c_D2D1_CAP_STYLE_SQUARE = 1,
    c_D2D1_CAP_STYLE_ROUND = 2,
    c_D2D1_CAP_STYLE_TRIANGLE = 3,
};

typedef enum c_D2D1_DASH_STYLE_tag c_D2D1_DASH_STYLE;
enum c_D2D1_DASH_STYLE_tag {
    c_D2D1_DASH_STYLE_SOLID = 0,
    c_D2D1_DASH_STYLE_DASH = 1,
    c_D2D1_DASH_STYLE_DOT = 2,
    c_D2D1_DASH_STYLE_DASH_DOT = 3,
    c_D2D1_DASH_STYLE_DASH_DOT_DOT = 4,
    c_D2D1_DASH_STYLE_CUSTOM = 5
};

typedef enum c_D2D1_LINE_JOIN_tag c_D2D1_LINE_JOIN;
enum c_D2D1_LINE_JOIN_tag {
    c_D2D1_LINE_JOIN_MITER = 0,
    c_D2D1_LINE_JOIN_BEVEL = 1,
    c_D2D1_LINE_JOIN_ROUND = 2
};

typedef enum c_D2D1_GAMMA_tag c_D2D1_GAMMA;
enum c_D2D1_GAMMA_tag {
    c_D2D1_GAMMA_2_2 = 0,
    c_D2D1_GAMMA_1_0 = 1,
    c_D2D1_GAMMA_FORCE_DWORD = 2
};

typedef enum c_D2D1_EXTEND_MODE_tag c_D2D1_EXTEND_MODE;
enum c_D2D1_EXTEND_MODE_tag {
    c_D2D1_EXTEND_MODE_CLAMP = 0,
    c_D2D1_EXTEND_MODE_WRAP = 1,
    c_D2D1_EXTEND_MODE_MIRROR = 2,
    c_D2D1_EXTEND_MODE_FORCE_DWORD = 3
};


/*************************
 ***  Helper Typedefs  ***
 *************************/

typedef struct c_D2D1_BITMAP_PROPERTIES_tag     c_D2D1_BITMAP_PROPERTIES;
typedef struct c_D2D1_BRUSH_PROPERTIES_tag      c_D2D1_BRUSH_PROPERTIES;
typedef D2D_COLOR_F                             c_D2D1_COLOR_F;
typedef struct D2D_MATRIX_3X2_F                 c_D2D1_MATRIX_3X2_F;
typedef struct D2D_POINT_2F                     c_D2D1_POINT_2F;
typedef struct D2D_RECT_F                       c_D2D1_RECT_F;
typedef struct D2D_SIZE_F                       c_D2D1_SIZE_F;
typedef struct D2D_SIZE_U                       c_D2D1_SIZE_U;


/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES_tag c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES;
struct c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES_tag {
    c_D2D1_POINT_2F startPoint;
    c_D2D1_POINT_2F endPoint;
};

typedef struct c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES_tag c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES;
struct c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES_tag {
    c_D2D1_POINT_2F center;
    c_D2D1_POINT_2F gradientOriginOffset;
    FLOAT radiusX;
    FLOAT radiusY;
};

typedef struct c_D2D1_GRADIENT_STOP_tag c_D2D1_GRADIENT_STOP;
struct c_D2D1_GRADIENT_STOP_tag {
    FLOAT position;
    c_D2D1_COLOR_F color;
};

typedef struct c_D2D1_ARC_SEGMENT_tag c_D2D1_ARC_SEGMENT;
struct c_D2D1_ARC_SEGMENT_tag {
    c_D2D1_POINT_2F point;
    c_D2D1_SIZE_F size;
    FLOAT rotationAngle;
    c_D2D1_SWEEP_DIRECTION sweepDirection;
    c_D2D1_ARC_SIZE arcSize;
};

typedef struct c_D2D1_BEZIER_SEGMENT_tag c_D2D1_BEZIER_SEGMENT;
struct c_D2D1_BEZIER_SEGMENT_tag {
    c_D2D1_POINT_2F point1;
    c_D2D1_POINT_2F point2;
    c_D2D1_POINT_2F point3;
};

typedef struct c_D2D1_ELLIPSE_tag c_D2D1_ELLIPSE;
struct c_D2D1_ELLIPSE_tag {
    c_D2D1_POINT_2F point;
    FLOAT radiusX;
    FLOAT radiusY;
};

typedef struct c_D2D1_FACTORY_OPTIONS_tag c_D2D1_FACTORY_OPTIONS;
struct c_D2D1_FACTORY_OPTIONS_tag {
    c_D2D1_DEBUG_LEVEL debugLevel;
};

typedef struct c_D2D1_HWND_RENDER_TARGET_PROPERTIES_tag c_D2D1_HWND_RENDER_TARGET_PROPERTIES;
struct c_D2D1_HWND_RENDER_TARGET_PROPERTIES_tag {
    HWND hwnd;
    c_D2D1_SIZE_U pixelSize;
    unsigned presentOptions;
};

typedef struct c_D2D1_PIXEL_FORMAT_tag c_D2D1_PIXEL_FORMAT;
struct c_D2D1_PIXEL_FORMAT_tag {
    c_DXGI_FORMAT format;
    c_D2D1_ALPHA_MODE alphaMode;
};

typedef struct c_D2D1_RENDER_TARGET_PROPERTIES_tag c_D2D1_RENDER_TARGET_PROPERTIES;
struct c_D2D1_RENDER_TARGET_PROPERTIES_tag {
    c_D2D1_RENDER_TARGET_TYPE type;
    c_D2D1_PIXEL_FORMAT pixelFormat;
    FLOAT dpiX;
    FLOAT dpiY;
    unsigned usage;
    c_D2D1_FEATURE_LEVEL minLevel;
};

typedef struct c_D2D1_STROKE_STYLE_PROPERTIES_tag c_D2D1_STROKE_STYLE_PROPERTIES;
struct c_D2D1_STROKE_STYLE_PROPERTIES_tag {
    c_D2D1_CAP_STYLE startCap;
    c_D2D1_CAP_STYLE endCap;
    c_D2D1_CAP_STYLE dashCap;
    c_D2D1_LINE_JOIN lineJoin;
    FLOAT miterLimit;
    c_D2D1_DASH_STYLE dashStyle;
    FLOAT dashOffset;
};

typedef struct c_D2D1_LAYER_PARAMETERS_tag c_D2D1_LAYER_PARAMETERS;
struct c_D2D1_LAYER_PARAMETERS_tag {
    c_D2D1_RECT_F contentBounds;
    c_ID2D1Geometry* geometricMask;
    c_D2D1_ANTIALIAS_MODE maskAntialiasMode;
    c_D2D1_MATRIX_3X2_F maskTransform;
    FLOAT opacity;
    c_ID2D1Brush* opacityBrush;
    unsigned layerOptions;
};


/*******************************
 ***  Interface ID2D1Bitmap  ***
 *******************************/

typedef struct c_ID2D1BitmapVtbl_tag c_ID2D1BitmapVtbl;
struct c_ID2D1BitmapVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Bitmap*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Bitmap*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Bitmap*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Image methods */
    /* none */

    /* ID2D1Bitmap methods */
    STDMETHOD(dummy_GetSize)(void);
#if 0
    /* Original vanilla method prototype. But this seems to be problematic
     * as compiler use different ABI when returning aggregate types.
     *
     * When built with incompatible compiler, it usually results in crash.
     *
     * For gcc, it seems to be completely incompatible:
     *  -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64384
     *  -- https://sourceforge.net/p/mingw-w64/mailman/message/36238073/
     *  -- https://source.winehq.org/git/wine.git/commitdiff/b42a15513eaa973b40ab967014b311af64acbb98
     *  -- https://www.winehq.org/pipermail/wine-devel/2017-July/118470.html
     *  -- https://bugzilla.mozilla.org/show_bug.cgi?id=1411401
     *
     * For MSVC, it is compatible only when building as C++. In C, it crashes
     * as well.
     */
    STDMETHOD_(c_D2D1_SIZE_U, GetPixelSize)(c_ID2D1Bitmap*);
#else
    /* This prototype corresponds more literally to what the COM calling
     * convention is expecting from us.
     *
     * Tested with MSVC 2017 (64-bit build), gcc (32-bit build).
     */
    STDMETHOD_(void, GetPixelSize)(c_ID2D1Bitmap*, c_D2D1_SIZE_U*);
#endif
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_CopyFromBitmap)(void);
    STDMETHOD(dummy_CopyFromRenderTarget)(void);
    STDMETHOD(dummy_CopyFromMemory)(void);
};

struct c_ID2D1Bitmap_tag {
    c_ID2D1BitmapVtbl* vtbl;
};

#define c_ID2D1Bitmap_QueryInterface(self,a,b)  (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1Bitmap_AddRef(self)              (self)->vtbl->AddRef(self)
#define c_ID2D1Bitmap_Release(self)             (self)->vtbl->Release(self)
#define c_ID2D1Bitmap_GetPixelSize(self,a)      (self)->vtbl->GetPixelSize(self,a)


/*******************************************
 ***  Interface ID2D1BitmapRenderTarget  ***
 *******************************************/

typedef struct c_ID2D1BitmapRenderTargetVtbl_tag c_ID2D1BitmapRenderTargetVtbl;
struct c_ID2D1BitmapRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1BitmapRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1BitmapRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1BitmapRenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(dummy_CreateBitmapFromWicBitmap)(void);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(dummy_CreateSolidColorBrush)(void);
    STDMETHOD(dummy_CreateGradientStopCollection)(void);
    STDMETHOD(dummy_CreateLinearGradientBrush)(void);
    STDMETHOD(dummy_CreateRadialGradientBrush)(void);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(dummy_CreateLayer)(void);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD(dummy_DrawLine)(void);
    STDMETHOD(dummy_DrawRectangle)(void);
    STDMETHOD(dummy_FillRectangle)(void);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD(dummy_DrawEllipse)(void);
    STDMETHOD(dummy_FillEllipse)(void);
    STDMETHOD(dummy_DrawGeometry)(void);
    STDMETHOD(dummy_FillGeometry)(void);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD(dummy_DrawBitmap)(void);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD(dummy_DrawTextLayout)(void);
    STDMETHOD(dummy_DrawGlyphRun)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetTransform)(void);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(dummy_SetTextAntialiasMode)(void);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD(dummy_PushLayer)(void);
    STDMETHOD(dummy_PopLayer)(void);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD(dummy_PushAxisAlignedClip)(void);
    STDMETHOD(dummy_PopAxisAlignedClip)(void);
    STDMETHOD(dummy_Clear)(void);
    STDMETHOD(dummy_BeginDraw)(void);
    STDMETHOD(dummy_EndDraw)(void);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_SetDpi)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);

    /* ID2D1BitmapRenderTarget methods */
    STDMETHOD(dummy_GetBitmap)(void);
};

struct c_ID2D1BitmapRenderTarget_tag {
    c_ID2D1BitmapRenderTargetVtbl* vtbl;
};

#define c_ID2D1BitmapRenderTarget_QueryInterface(self,a,b)  (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1BitmapRenderTarget_AddRef(self)              (self)->vtbl->AddRef(self)
#define c_ID2D1BitmapRenderTarget_Release(self)             (self)->vtbl->Release(self)


/******************************
 ***  Interface ID2D1Brush  ***
 ******************************/

typedef struct c_ID2D1BrushVtbl_tag c_ID2D1BrushVtbl;
struct c_ID2D1BrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Brush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Brush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Brush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);
};

struct c_ID2D1Brush_tag {
    c_ID2D1BrushVtbl* vtbl;
};

#define c_ID2D1Brush_QueryInterface(self,a,b)               (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1Brush_AddRef(self)                           (self)->vtbl->AddRef(self)
#define c_ID2D1Brush_Release(self)                          (self)->vtbl->Release(self)


/***********************************
***  Interface ID2D1StrokeStyle  ***
***********************************/

typedef struct c_ID2D1StrokeStyleVtbl_tag c_ID2D1StrokeStyleVtbl;
struct c_ID2D1StrokeStyleVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1StrokeStyle*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1StrokeStyle*);
    STDMETHOD_(ULONG, Release)(c_ID2D1StrokeStyle*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1StrokeStyle methods */
    STDMETHOD(dummy_GetStartCap)(void);
    STDMETHOD(dummy_GetEndCap)(void);
    STDMETHOD(dummy_GetDashCap)(void);
    STDMETHOD(dummy_GetMiterLimit)(void);
    STDMETHOD(dummy_GetLineJoin)(void);
    STDMETHOD(dummy_GetDashOffset)(void);
    STDMETHOD(dummy_GetDashStyle)(void);
    STDMETHOD(dummy_GetDashesCount)(void);
    STDMETHOD(dummy_GetDashes)(void);
};

struct c_ID2D1StrokeStyle_tag {
    c_ID2D1StrokeStyleVtbl* vtbl;
};

#define c_ID2D1StrokeStyle_QueryInterface(self,a,b)               (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1StrokeStyle_AddRef(self)                           (self)->vtbl->AddRef(self)
#define c_ID2D1StrokeStyle_Release(self)                          (self)->vtbl->Release(self)


/*****************************************
 ***  Interface ID2D1DCRenderTarget  ***
 *****************************************/

typedef struct c_ID2D1DCRenderTargetVtbl_tag c_ID2D1DCRenderTargetVtbl;
struct c_ID2D1DCRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1DCRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1DCRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1DCRenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(dummy_CreateBitmapFromWicBitmap)(void);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(dummy_CreateSolidColorBrush)(void);
    STDMETHOD(dummy_CreateGradientStopCollection)(void);
    STDMETHOD(dummy_CreateLinearGradientBrush)(void);
    STDMETHOD(dummy_CreateRadialGradientBrush)(void);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(dummy_CreateLayer)(void);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD(dummy_DrawLine)(void);
    STDMETHOD(dummy_DrawRectangle)(void);
    STDMETHOD(dummy_FillRectangle)(void);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD(dummy_DrawEllipse)(void);
    STDMETHOD(dummy_FillEllipse)(void);
    STDMETHOD(dummy_DrawGeometry)(void);
    STDMETHOD(dummy_FillGeometry)(void);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD(dummy_DrawBitmap)(void);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD(dummy_DrawTextLayout)(void);
    STDMETHOD(dummy_DrawGlyphRun)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetTransform)(void);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(dummy_SetTextAntialiasMode)(void);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD(dummy_PushLayer)(void);
    STDMETHOD(dummy_PopLayer)(void);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD(dummy_PushAxisAlignedClip)(void);
    STDMETHOD(dummy_PopAxisAlignedClip)(void);
    STDMETHOD(dummy_Clear)(void);
    STDMETHOD(dummy_BeginDraw)(void);
    STDMETHOD(dummy_EndDraw)(void);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_SetDpi)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);

    /* ID2D1DCRenderTarget methods */
    STDMETHOD(BindDC)(c_ID2D1DCRenderTarget*, const HDC, const RECT*);
};

struct c_ID2D1DCRenderTarget_tag {
    c_ID2D1DCRenderTargetVtbl* vtbl;
};

#define c_ID2D1DCRenderTarget_QueryInterface(self,a,b)      (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1DCRenderTarget_AddRef(self)                  (self)->vtbl->AddRef(self)
#define c_ID2D1DCRenderTarget_Release(self)                 (self)->vtbl->Release(self)
#define c_ID2D1DCRenderTarget_BindDC(self,a,b)              (self)->vtbl->BindDC(self,a,b)


/********************************
 ***  Interface ID2D1Factory  ***
 ********************************/

typedef struct c_ID2D1FactoryVtbl_tag c_ID2D1FactoryVtbl;
struct c_ID2D1FactoryVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Factory*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Factory*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Factory*);

    /* ID2D1Factory methods */
    STDMETHOD(dummy_ReloadSystemMetrics)(void);
    STDMETHOD(dummy_GetDesktopDpi)(void);
    STDMETHOD(dummy_CreateRectangleGeometry)(void);
    STDMETHOD(dummy_CreateRoundedRectangleGeometry)(void);
    STDMETHOD(dummy_CreateEllipseGeometry)(void);
    STDMETHOD(dummy_CreateGeometryGroup)(void);
    STDMETHOD(dummy_CreateTransformedGeometry)(void);
    STDMETHOD(CreatePathGeometry)(c_ID2D1Factory*, c_ID2D1PathGeometry**);
    STDMETHOD(CreateStrokeStyle)(c_ID2D1Factory*, const c_D2D1_STROKE_STYLE_PROPERTIES*, const FLOAT*, UINT32, c_ID2D1StrokeStyle**);
    STDMETHOD(dummy_CreateDrawingStateBlock)(void);
    STDMETHOD(dummy_CreateWicBitmapRenderTarget)(void);
    STDMETHOD(CreateHwndRenderTarget)(c_ID2D1Factory*, const c_D2D1_RENDER_TARGET_PROPERTIES*,
              const c_D2D1_HWND_RENDER_TARGET_PROPERTIES*, c_ID2D1HwndRenderTarget**);
    STDMETHOD(dummy_CreateDxgiSurfaceRenderTarget)(void);
    STDMETHOD(CreateDCRenderTarget)(c_ID2D1Factory*, const c_D2D1_RENDER_TARGET_PROPERTIES*, c_ID2D1DCRenderTarget**);
};

struct c_ID2D1Factory_tag {
    c_ID2D1FactoryVtbl* vtbl;
};

#define c_ID2D1Factory_QueryInterface(self,a,b)             (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1Factory_AddRef(self)                         (self)->vtbl->AddRef(self)
#define c_ID2D1Factory_Release(self)                        (self)->vtbl->Release(self)
#define c_ID2D1Factory_CreatePathGeometry(self,a)           (self)->vtbl->CreatePathGeometry(self,a)
#define c_ID2D1Factory_CreateHwndRenderTarget(self,a,b,c)   (self)->vtbl->CreateHwndRenderTarget(self,a,b,c)
#define c_ID2D1Factory_CreateDCRenderTarget(self,a,b)       (self)->vtbl->CreateDCRenderTarget(self,a,b)
#define c_ID2D1Factory_CreateStrokeStyle(self,a,b,c,d)      (self)->vtbl->CreateStrokeStyle(self,a,b,c,d)


/*****************************************************
 ***  Interface c_ID2D1GdiInteropRenderTarget  ***
 *****************************************************/

typedef struct c_ID2D1GdiInteropRenderTargetVtbl_tag c_ID2D1GdiInteropRenderTargetVtbl;
struct c_ID2D1GdiInteropRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1GdiInteropRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1GdiInteropRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1GdiInteropRenderTarget*);

    /* ID2D1GdiInteropRenderTarget methods */
    STDMETHOD(GetDC)(c_ID2D1GdiInteropRenderTarget*, c_D2D1_DC_INITIALIZE_MODE, HDC*);
    STDMETHOD(ReleaseDC)(c_ID2D1GdiInteropRenderTarget*, const RECT*);
};

struct c_ID2D1GdiInteropRenderTarget_tag {
    c_ID2D1GdiInteropRenderTargetVtbl* vtbl;
};

#define c_ID2D1GdiInteropRenderTarget_QueryInterface(self,a,b)      (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1GdiInteropRenderTarget_AddRef(self)                  (self)->vtbl->AddRef(self)
#define c_ID2D1GdiInteropRenderTarget_Release(self)                 (self)->vtbl->Release(self)
#define c_ID2D1GdiInteropRenderTarget_GetDC(self,a,b)               (self)->vtbl->GetDC(self,a,b)
#define c_ID2D1GdiInteropRenderTarget_ReleaseDC(self,a)             (self)->vtbl->ReleaseDC(self,a)


/*********************************
 ***  Interface ID2D1Geometry  ***
 *********************************/

typedef struct c_ID2D1GeometryVtbl_tag c_ID2D1GeometryVtbl;
struct c_ID2D1GeometryVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Geometry*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Geometry*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Geometry*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Geometry methods */
    STDMETHOD(dummy_GetBounds)(void);
    STDMETHOD(dummy_GetWidenedBounds)(void);
    STDMETHOD(dummy_StrokeContainsPoint)(void);
    STDMETHOD(dummy_FillContainsPoint)(void);
    STDMETHOD(dummy_CompareWithGeometry)(void);
    STDMETHOD(dummy_Simplify)(void);
    STDMETHOD(dummy_Tessellate)(void);
    STDMETHOD(dummy_CombineWithGeometry)(void);
    STDMETHOD(dummy_Outline)(void);
    STDMETHOD(dummy_ComputeArea)(void);
    STDMETHOD(dummy_ComputeLength)(void);
    STDMETHOD(dummy_ComputePointAtLength)(void);
    STDMETHOD(dummy_Widen)(void);
};

struct c_ID2D1Geometry_tag {
    c_ID2D1GeometryVtbl* vtbl;
};

#define c_ID2D1Geometry_QueryInterface(self,a,b)    (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1Geometry_AddRef(self)                (self)->vtbl->AddRef(self)
#define c_ID2D1Geometry_Release(self)               (self)->vtbl->Release(self)


/*************************************
 ***  Interface ID2D1GeometrySink  ***
 *************************************/

typedef struct c_ID2D1GeometrySinkVtbl_tag c_ID2D1GeometrySinkVtbl;
struct c_ID2D1GeometrySinkVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1GeometrySink*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1GeometrySink*);
    STDMETHOD_(ULONG, Release)(c_ID2D1GeometrySink*);

    /* ID2D1SimplifiedGeometrySink methods */
    STDMETHOD(dummy_SetFillMode)(void);
    STDMETHOD(dummy_SetSegmentFlags)(void);
    STDMETHOD_(void, BeginFigure)(c_ID2D1GeometrySink*, c_D2D1_POINT_2F, c_D2D1_FIGURE_BEGIN);
    STDMETHOD(dummy_AddLines)(void);
    STDMETHOD(dummy_AddBeziers)(void);
    STDMETHOD_(void, EndFigure)(c_ID2D1GeometrySink*, c_D2D1_FIGURE_END);
    STDMETHOD(Close)(c_ID2D1GeometrySink*) PURE;

    /* ID2D1GeometrySink methods */
    STDMETHOD_(void, AddLine)(c_ID2D1GeometrySink*, c_D2D1_POINT_2F point);
    STDMETHOD_(void, AddBezier)(c_ID2D1GeometrySink*, const c_D2D1_BEZIER_SEGMENT*);
    STDMETHOD(dummy_AddQuadraticBezier)(void);
    STDMETHOD(dummy_AddQuadraticBeziers)(void);
    STDMETHOD_(void, AddArc)(c_ID2D1GeometrySink*, const c_D2D1_ARC_SEGMENT*);
};

struct c_ID2D1GeometrySink_tag {
    c_ID2D1GeometrySinkVtbl* vtbl;
};

#define c_ID2D1GeometrySink_QueryInterface(self,a,b)    (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1GeometrySink_AddRef(self)                (self)->vtbl->AddRef(self)
#define c_ID2D1GeometrySink_Release(self)               (self)->vtbl->Release(self)
#define c_ID2D1GeometrySink_BeginFigure(self,a,b)       (self)->vtbl->BeginFigure(self,a,b)
#define c_ID2D1GeometrySink_EndFigure(self,a)           (self)->vtbl->EndFigure(self,a)
#define c_ID2D1GeometrySink_Close(self)                 (self)->vtbl->Close(self)
#define c_ID2D1GeometrySink_AddLine(self,a)             (self)->vtbl->AddLine(self,a)
#define c_ID2D1GeometrySink_AddArc(self,a)              (self)->vtbl->AddArc(self,a)
#define c_ID2D1GeometrySink_AddBezier(self,a)           (self)->vtbl->AddBezier(self,a)


/*****************************************
 ***  Interface ID2D1HwndRenderTarget  ***
 *****************************************/

typedef struct c_ID2D1HwndRenderTargetVtbl_tag c_ID2D1HwndRenderTargetVtbl;
struct c_ID2D1HwndRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1HwndRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1HwndRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1HwndRenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(dummy_CreateBitmapFromWicBitmap)(void);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(dummy_CreateSolidColorBrush)(void);
    STDMETHOD(dummy_CreateGradientStopCollection)(void);
    STDMETHOD(dummy_CreateLinearGradientBrush)(void);
    STDMETHOD(dummy_CreateRadialGradientBrush)(void);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(dummy_CreateLayer)(void);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD(dummy_DrawLine)(void);
    STDMETHOD(dummy_DrawRectangle)(void);
    STDMETHOD(dummy_FillRectangle)(void);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD(dummy_DrawEllipse)(void);
    STDMETHOD(dummy_FillEllipse)(void);
    STDMETHOD(dummy_DrawGeometry)(void);
    STDMETHOD(dummy_FillGeometry)(void);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD(dummy_DrawBitmap)(void);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD(dummy_DrawTextLayout)(void);
    STDMETHOD(dummy_DrawGlyphRun)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetTransform)(void);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(dummy_SetTextAntialiasMode)(void);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD(dummy_PushLayer)(void);
    STDMETHOD(dummy_PopLayer)(void);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD(dummy_PushAxisAlignedClip)(void);
    STDMETHOD(dummy_PopAxisAlignedClip)(void);
    STDMETHOD(dummy_Clear)(void);
    STDMETHOD(dummy_BeginDraw)(void);
    STDMETHOD(dummy_EndDraw)(void);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_SetDpi)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);

    /* ID2D1HwndRenderTarget methods */
    STDMETHOD(dummy_CheckWindowState)(void);
    STDMETHOD(Resize)(c_ID2D1HwndRenderTarget*, const c_D2D1_SIZE_U*);
    STDMETHOD(dummy_GetHwnd)(void);
};

struct c_ID2D1HwndRenderTarget_tag {
    c_ID2D1HwndRenderTargetVtbl* vtbl;
};

#define c_ID2D1HwndRenderTarget_QueryInterface(self,a,b)                (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1HwndRenderTarget_AddRef(self)                            (self)->vtbl->AddRef(self)
#define c_ID2D1HwndRenderTarget_Release(self)                           (self)->vtbl->Release(self)
#define c_ID2D1HwndRenderTarget_Resize(self,a)                          (self)->vtbl->Resize(self,a)


/******************************
 ***  Interface ID2D1Layer  ***
 ******************************/

typedef struct c_ID2D1LayerVtbl_tag c_ID2D1LayerVtbl;
struct c_ID2D1LayerVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Layer*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Layer*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Layer*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Layer methods */
    STDMETHOD(dummy_GetSize)(void);
};

struct c_ID2D1Layer_tag {
    c_ID2D1LayerVtbl* vtbl;
};

#define c_ID2D1Layer_QueryInterface(self,a,b)   (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1Layer_AddRef(self)               (self)->vtbl->AddRef(self)
#define c_ID2D1Layer_Release(self)              (self)->vtbl->Release(self)


/*************************************
 ***  Interface ID2D1PathGeometry  ***
 *************************************/

typedef struct c_ID2D1PathGeometryVtbl_tag c_ID2D1PathGeometryVtbl;
struct c_ID2D1PathGeometryVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1PathGeometry*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1PathGeometry*);
    STDMETHOD_(ULONG, Release)(c_ID2D1PathGeometry*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Geometry methods */
    STDMETHOD(dummy_GetBounds)(void);
    STDMETHOD(dummy_GetWidenedBounds)(void);
    STDMETHOD(dummy_StrokeContainsPoint)(void);
    STDMETHOD(dummy_FillContainsPoint)(void);
    STDMETHOD(dummy_CompareWithGeometry)(void);
    STDMETHOD(dummy_Simplify)(void);
    STDMETHOD(dummy_Tessellate)(void);
    STDMETHOD(dummy_CombineWithGeometry)(void);
    STDMETHOD(dummy_Outline)(void);
    STDMETHOD(dummy_ComputeArea)(void);
    STDMETHOD(dummy_ComputeLength)(void);
    STDMETHOD(dummy_ComputePointAtLength)(void);
    STDMETHOD(dummy_Widen)(void);

    /* ID2D1PathGeometry methods */
    STDMETHOD(Open)(c_ID2D1PathGeometry*, c_ID2D1GeometrySink**);
    STDMETHOD(dummy_Stream)(void);
    STDMETHOD(dummy_GetSegmentCount)(void);
    STDMETHOD(dummy_GetFigureCount)(void);
};

struct c_ID2D1PathGeometry_tag {
    c_ID2D1PathGeometryVtbl* vtbl;
};

#define c_ID2D1PathGeometry_QueryInterface(self,a,b)    (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1PathGeometry_AddRef(self)                (self)->vtbl->AddRef(self)
#define c_ID2D1PathGeometry_Release(self)               (self)->vtbl->Release(self)
#define c_ID2D1PathGeometry_Open(self,a)                (self)->vtbl->Open(self,a)


/*************************************
 ***  Interface ID2D1RenderTarget  ***
 *************************************/

typedef struct c_ID2D1RenderTargetVtbl_tag c_ID2D1RenderTargetVtbl;
struct c_ID2D1RenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1RenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1RenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1RenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(CreateBitmapFromWicBitmap)(c_ID2D1RenderTarget*, IWICBitmapSource*, const c_D2D1_BITMAP_PROPERTIES*, c_ID2D1Bitmap**);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(CreateSolidColorBrush)(c_ID2D1RenderTarget*, const c_D2D1_COLOR_F*, const void*, c_ID2D1SolidColorBrush**);
    STDMETHOD(CreateGradientStopCollection)(c_ID2D1RenderTarget*, const c_D2D1_GRADIENT_STOP*, UINT32, c_D2D1_GAMMA, c_D2D1_EXTEND_MODE, c_ID2D1GradientStopCollection**);
    STDMETHOD(CreateLinearGradientBrush)(c_ID2D1RenderTarget*, const c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES*, const c_D2D1_BRUSH_PROPERTIES*, c_ID2D1GradientStopCollection*, c_ID2D1LinearGradientBrush**);
    STDMETHOD(CreateRadialGradientBrush)(c_ID2D1RenderTarget*, const c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES*, const c_D2D1_BRUSH_PROPERTIES*, c_ID2D1GradientStopCollection*, c_ID2D1RadialGradientBrush**);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(CreateLayer)(c_ID2D1RenderTarget*, const c_D2D1_SIZE_F*, c_ID2D1Layer**);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD_(void, DrawLine)(c_ID2D1RenderTarget*, c_D2D1_POINT_2F, c_D2D1_POINT_2F, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, DrawRectangle)(c_ID2D1RenderTarget*, const c_D2D1_RECT_F*, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, FillRectangle)(c_ID2D1RenderTarget*, const c_D2D1_RECT_F*, c_ID2D1Brush*);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD_(void, DrawEllipse)(c_ID2D1RenderTarget*, const c_D2D1_ELLIPSE*, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, FillEllipse)(c_ID2D1RenderTarget*, const c_D2D1_ELLIPSE*, c_ID2D1Brush*);
    STDMETHOD_(void, DrawGeometry)(c_ID2D1RenderTarget*, c_ID2D1Geometry*, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, FillGeometry)(c_ID2D1RenderTarget*, c_ID2D1Geometry*, c_ID2D1Brush*, c_ID2D1Brush*);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD_(void, DrawBitmap)(c_ID2D1RenderTarget*, c_ID2D1Bitmap*, const c_D2D1_RECT_F*, FLOAT,
                                 c_D2D1_BITMAP_INTERPOLATION_MODE, const c_D2D1_RECT_F*);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD_(void, DrawTextLayout)(c_ID2D1RenderTarget*, c_D2D1_POINT_2F, c_IDWriteTextLayout*, c_ID2D1Brush*, unsigned);
    STDMETHOD_(void, DrawGlyphRun)(c_ID2D1RenderTarget*, c_D2D1_POINT_2F, const c_DWRITE_GLYPH_RUN*, c_ID2D1Brush*, c_DWRITE_MEASURING_MODE);
    STDMETHOD_(void, SetTransform)(c_ID2D1RenderTarget*, const c_D2D1_MATRIX_3X2_F*);
    STDMETHOD_(void, GetTransform)(c_ID2D1RenderTarget*, c_D2D1_MATRIX_3X2_F*);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(SetTextAntialiasMode)(c_ID2D1RenderTarget*, c_D2D1_TEXT_ANTIALIAS_MODE);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD_(void, PushLayer)(c_ID2D1RenderTarget*, const c_D2D1_LAYER_PARAMETERS*, c_ID2D1Layer*);
    STDMETHOD_(void, PopLayer)(c_ID2D1RenderTarget*);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD_(void, PushAxisAlignedClip)(c_ID2D1RenderTarget*, const c_D2D1_RECT_F*, c_D2D1_ANTIALIAS_MODE);
    STDMETHOD_(void, PopAxisAlignedClip)(c_ID2D1RenderTarget*);
    STDMETHOD_(void, Clear)(c_ID2D1RenderTarget*, const c_D2D1_COLOR_F*);
    STDMETHOD_(void, BeginDraw)(c_ID2D1RenderTarget*);
    STDMETHOD(EndDraw)(c_ID2D1RenderTarget*, void*, void*);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD_(void, SetDpi)(c_ID2D1RenderTarget*, FLOAT, FLOAT);
    STDMETHOD_(void, GetDpi)(c_ID2D1RenderTarget*, FLOAT*, FLOAT*);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);
};

struct c_ID2D1RenderTarget_tag {
    c_ID2D1RenderTargetVtbl* vtbl;
};

#define c_ID2D1RenderTarget_QueryInterface(self,a,b)                (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1RenderTarget_AddRef(self)                            (self)->vtbl->AddRef(self)
#define c_ID2D1RenderTarget_Release(self)                           (self)->vtbl->Release(self)
#define c_ID2D1RenderTarget_CreateBitmapFromWicBitmap(self,a,b,c)   (self)->vtbl->CreateBitmapFromWicBitmap(self,a,b,c)
#define c_ID2D1RenderTarget_CreateSolidColorBrush(self,a,b,c)       (self)->vtbl->CreateSolidColorBrush(self,a,b,c)
#define c_ID2D1RenderTarget_CreateLinearGradientBrush(self,a,b,c,d) (self)->vtbl->CreateLinearGradientBrush(self,a,b,c,d)
#define c_ID2D1RenderTarget_CreateRadialGradientBrush(self,a,b,c,d) (self)->vtbl->CreateRadialGradientBrush(self,a,b,c,d)
#define c_ID2D1RenderTarget_CreateGradientStopCollection(self,a,b,c,d,e) (self)->vtbl->CreateGradientStopCollection(self,a,b,c,d,e)
#define c_ID2D1RenderTarget_CreateLayer(self,a,b)                   (self)->vtbl->CreateLayer(self,a,b)
#define c_ID2D1RenderTarget_DrawLine(self,a,b,c,d,e)                (self)->vtbl->DrawLine(self,a,b,c,d,e)
#define c_ID2D1RenderTarget_DrawRectangle(self,a,b,c,d)             (self)->vtbl->DrawRectangle(self,a,b,c,d)
#define c_ID2D1RenderTarget_FillRectangle(self,a,b)                 (self)->vtbl->FillRectangle(self,a,b)
#define c_ID2D1RenderTarget_DrawEllipse(self,a,b,c,d)               (self)->vtbl->DrawEllipse(self,a,b,c,d)
#define c_ID2D1RenderTarget_FillEllipse(self,a,b)                   (self)->vtbl->FillEllipse(self,a,b)
#define c_ID2D1RenderTarget_DrawGeometry(self,a,b,c,d)              (self)->vtbl->DrawGeometry(self,a,b,c,d)
#define c_ID2D1RenderTarget_FillGeometry(self,a,b,c)                (self)->vtbl->FillGeometry(self,a,b,c)
#define c_ID2D1RenderTarget_DrawBitmap(self,a,b,c,d,e)              (self)->vtbl->DrawBitmap(self,a,b,c,d,e)
#define c_ID2D1RenderTarget_DrawTextLayout(self,a,b,c,d)            (self)->vtbl->DrawTextLayout(self,a,b,c,d)
#define c_ID2D1RenderTarget_DrawGlyphRun(self,a,b,c,d)              (self)->vtbl->DrawGlyphRun(self,a,b,c,d)
#define c_ID2D1RenderTarget_SetTransform(self,a)                    (self)->vtbl->SetTransform(self,a)
#define c_ID2D1RenderTarget_GetTransform(self,a)                    (self)->vtbl->GetTransform(self,a)
#define c_ID2D1RenderTarget_SetTextAntialiasMode(self,a)            (self)->vtbl->SetTextAntialiasMode(self,a)
#define c_ID2D1RenderTarget_PushLayer(self,a,b)                     (self)->vtbl->PushLayer(self,a,b)
#define c_ID2D1RenderTarget_PopLayer(self)                          (self)->vtbl->PopLayer(self)
#define c_ID2D1RenderTarget_PushAxisAlignedClip(self,a,b)           (self)->vtbl->PushAxisAlignedClip(self,a,b)
#define c_ID2D1RenderTarget_PopAxisAlignedClip(self)                (self)->vtbl->PopAxisAlignedClip(self)
#define c_ID2D1RenderTarget_Clear(self,a)                           (self)->vtbl->Clear(self,a)
#define c_ID2D1RenderTarget_BeginDraw(self)                         (self)->vtbl->BeginDraw(self)
#define c_ID2D1RenderTarget_EndDraw(self,a,b)                       (self)->vtbl->EndDraw(self,a,b)
#define c_ID2D1RenderTarget_SetDpi(self,a,b)                        (self)->vtbl->SetDpi(self,a,b)
#define c_ID2D1RenderTarget_GetDpi(self,a,b)                        (self)->vtbl->GetDpi(self,a,b)


/*********************************************
 ***  Interface ID2D1SolidColorBrushBrush  ***
 *********************************************/

typedef struct c_ID2D1SolidColorBrushVtbl_tag c_ID2D1SolidColorBrushVtbl;
struct c_ID2D1SolidColorBrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1SolidColorBrush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1SolidColorBrush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1SolidColorBrush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);

    /* ID2D1SolidColorBrushBrush methods */
    STDMETHOD_(void, SetColor)(c_ID2D1SolidColorBrush*, const c_D2D1_COLOR_F*);
    STDMETHOD(dummy_GetColor)(void);
};

struct c_ID2D1SolidColorBrush_tag {
    c_ID2D1SolidColorBrushVtbl* vtbl;
};

#define c_ID2D1SolidColorBrush_QueryInterface(self,a,b)     (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1SolidColorBrush_AddRef(self)                 (self)->vtbl->AddRef(self)
#define c_ID2D1SolidColorBrush_Release(self)                (self)->vtbl->Release(self)
#define c_ID2D1SolidColorBrush_SetColor(self,a)             (self)->vtbl->SetColor(self,a)


/*********************************************
 ***  Interface ID2D1LinearGradientBrush   ***
 *********************************************/

typedef struct c_ID2D1LinearGradientBrushVtbl_tag c_ID2D1LinearGradientBrushVtbl;
struct c_ID2D1LinearGradientBrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1LinearGradientBrush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1LinearGradientBrush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1LinearGradientBrush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);

    /* ID2D1LinearGradientBrush methods */
    STDMETHOD(dummy_SetStartPoint)(void);
    STDMETHOD(dummy_SetEndPoint)(void);
    STDMETHOD(dummy_GetStartPoint)(void);
    STDMETHOD(dummy_GetEndPoint)(void);
    STDMETHOD(dummy_GetGradientStopCollection)(void);
};

struct c_ID2D1LinearGradientBrush_tag {
    c_ID2D1LinearGradientBrushVtbl* vtbl;
};

#define c_ID2D1LinearGradientBrush_QueryInterface(self,a,b)     (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1LinearGradientBrush_AddRef(self)                 (self)->vtbl->AddRef(self)
#define c_ID2D1LinearGradientBrush_Release(self)                (self)->vtbl->Release(self)


/*********************************************
 ***  Interface ID2D1RadialGradientBrush   ***
 *********************************************/

typedef struct c_ID2D1RadialGradientBrushVtbl_tag c_ID2D1RadialGradientBrushVtbl;
struct c_ID2D1RadialGradientBrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1RadialGradientBrush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1RadialGradientBrush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1RadialGradientBrush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);

    /* ID2D1RadialGradientBrush methods */
    STDMETHOD(dummy_SetCenter)(void);
    STDMETHOD(dummy_SetGradientOriginOffset)(void);
    STDMETHOD(dummy_SetRadiusX)(void);
    STDMETHOD(dummy_SetRadiusY)(void);
    STDMETHOD(dummy_GetCenter)(void);
    STDMETHOD(dummy_GetGradientOriginOffset)(void);
    STDMETHOD(dummy_GetRadiusX)(void);
    STDMETHOD(dummy_GetRadiusY)(void);
    STDMETHOD(dummy_GetGradientStopCollection)(void);
};

struct c_ID2D1RadialGradientBrush_tag {
    c_ID2D1RadialGradientBrushVtbl* vtbl;
};

#define c_ID2D1RadialGradientBrush_QueryInterface(self,a,b)     (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1RadialGradientBrush_AddRef(self)                 (self)->vtbl->AddRef(self)
#define c_ID2D1RadialGradientBrush_Release(self)                (self)->vtbl->Release(self)


/************************************************
 ***  Interface ID2D1GradientStopCollection   ***
 ************************************************/

typedef struct c_ID2D1GradientStopCollectionVtbl_tag c_ID2D1GradientStopCollectionVtbl;
struct c_ID2D1GradientStopCollectionVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1GradientStopCollection*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1GradientStopCollection*);
    STDMETHOD_(ULONG, Release)(c_ID2D1GradientStopCollection*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1GradientStopCollection methods */
    STDMETHOD(dummy_GetGradientStopCount)(void);
    STDMETHOD(dummy_GetGradientStops)(void);
    STDMETHOD(dummy_GetColorInterpolationGamma)(void);
    STDMETHOD(dummy_GetExtendMode)(void);
};

struct c_ID2D1GradientStopCollection_tag {
    c_ID2D1GradientStopCollectionVtbl* vtbl;
};

#define c_ID2D1GradientStopCollection_QueryInterface(self,a,b)     (self)->vtbl->QueryInterface(self,a,b)
#define c_ID2D1GradientStopCollection_AddRef(self)                 (self)->vtbl->AddRef(self)
#define c_ID2D1GradientStopCollection_Release(self)                (self)->vtbl->Release(self)


#endif  /* C_D2D1_H */
