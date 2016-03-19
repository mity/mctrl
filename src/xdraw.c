/*
 * Copyright (c) 2015 Martin Mitas
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


#include "xdraw.h"
#include "xcom.h"
#include "doublebuffer.h"

#include <float.h>
#include <math.h>


/* Uncomment this to have more verbose traces from this module. */
/*#define XDRAW_DEBUG     1*/

#ifdef XDRAW_DEBUG
    #define XDRAW_TRACE           MC_TRACE
#else
    #define XDRAW_TRACE(...)      do {} while(0)
#endif


#define XDRAW_DEFAULT_FONT_FAMILY                                           \
        (mc_win_version >= MC_WIN_VISTA ? _T("Segoe UI") : _T("Tahoma"))


static CRITICAL_SECTION xdraw_lock;


/*************************
 ***  Direct2D Driver  ***
 *************************/

#include <d2d1.h>
#include <wincodec.h>
#include "dummy/dwrite.h"


/* Mingw-w64 compatibility hacks. */
#ifdef MC_TOOLCHAIN_MINGW64
    /* In some mingw-w64 versions, these are missing from LIBUUID.A */
    static const GUID xdraw_IID_ID2D1Factory =
            {0x06152247,0x6f50,0x465a,{0x92,0x45,0x11,0x8b,0xfd,0x3b,0x60,0x07}};
    #define IID_ID2D1Factory xdraw_IID_ID2D1Factory
    static const GUID xdraw_IID_ID2D1GdiInteropRenderTarget =
            {0xe0db51c3,0x6f77,0x4bae,{0xb3,0xd5,0xe4,0x75,0x09,0xb3,0x58,0x38}};
    #define IID_ID2D1GdiInteropRenderTarget xdraw_IID_ID2D1GdiInteropRenderTarget

    /* In some mingw-w64 versions, macro wrapping ID2D1RenderTarget::CreateLayer()
     * is erroneously taking too few parameters. */
    #undef ID2D1RenderTarget_CreateLayer
    #define ID2D1RenderTarget_CreateLayer(self,A,B)   (self)->lpVtbl->CreateLayer(self,A,B)
#endif


static HMODULE d2d_dll = NULL;  /* D2D1.DLL */
static ID2D1Factory* d2d_factory;

static HMODULE dw_dll = NULL;   /* DWRITE.DLL */
static dummy_IDWriteFactory* dw_factory;

static int (WINAPI* fn_GetUserDefaultLocaleName)(WCHAR*, int);


static int
d2d_init(void)
{
    static const D2D1_FACTORY_OPTIONS factory_options = { D2D1_DEBUG_LEVEL_NONE };
    HRESULT (WINAPI* fn_D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
    HRESULT (WINAPI* fn_DWriteCreateFactory)(int, REFIID, void**);
    HRESULT hr;

    /* Load D2D1.DLL and create Direct2D factory */
    d2d_dll = mc_load_sys_dll(_T("D2D1.DLL"));
    if(MC_ERR(d2d_dll == NULL)) {
        MC_TRACE_ERR("d2d_init: LoadLibrary('D2D1.DLL') failed.");
        goto err_d2d_LoadLibrary;
    }

    fn_D2D1CreateFactory = (HRESULT (WINAPI*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**))
                GetProcAddress(d2d_dll, "D2D1CreateFactory");
    if(MC_ERR(fn_D2D1CreateFactory == NULL)) {
        MC_TRACE_ERR("d2d_init: GetProcAddress('D2D1CreateFactory') failed.");
        goto err_d2d_GetProcAddress;
    }

    /* We use D2D1_FACTORY_TYPE_SINGLE_THREADED because any D2D resource is
     * used exclusively by a single control which created them (i.e. single
     * thread). D2D1_FACTORY_TYPE_MULTI_THREADED would unnecessarily hurt
     * performance by implicit locking for every paint operation.
     *
     * Only the factory itself is shared between multiple controls (and, hence,
     * potentially threads). So we shall synchronize access to the factory
     * manually with xdraw_lock.
     */
    hr = fn_D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                &IID_ID2D1Factory, &factory_options, (void**) &d2d_factory);
    if(MC_ERR(hr != S_OK)) {
        MC_TRACE_HR("d2d_init: D2D1CreateFactory() failed.");
        goto err_d2d_CreateFactory;
    }

    /* Load DWRITE.DLL and create DirectWrite factory */
    dw_dll = mc_load_sys_dll(_T("DWRITE.DLL"));
    if(MC_ERR(d2d_dll == NULL)) {
        MC_TRACE_ERR("d2d_init: LoadLibrary('DWRITE.DLL') failed.");
        goto err_dw_LoadLibrary;
    }

    fn_DWriteCreateFactory = (HRESULT (WINAPI*)(int, REFIID, void**))
                GetProcAddress(dw_dll, "DWriteCreateFactory");
    if(MC_ERR(fn_DWriteCreateFactory == NULL)) {
        MC_TRACE_ERR("d2d_init: GetProcAddress('DWriteCreateFactory') failed.");
        goto err_dw_GetProcAddress;
    }

    hr = fn_DWriteCreateFactory(dummy_DWRITE_FACTORY_TYPE_SHARED,
                &dummy_IID_IDWriteFactory, (void**) &dw_factory);
    if(MC_ERR(hr != S_OK)) {
        MC_TRACE_HR("d2d_init: DWriteCreateFactory() failed.");
        goto err_dw_CreateFactory;
    }

    /* We need locale name for creation of dummy_IDWriteTextFormat. This functions is
     * available since Vista (which covers all systems with Direct2D and
     * DirectWrite). */
    fn_GetUserDefaultLocaleName = (int (WINAPI*)(WCHAR*, int))
            GetProcAddress(mc_instance_kernel32, "GetUserDefaultLocaleName");
    if(MC_ERR(fn_GetUserDefaultLocaleName == NULL)) {
        MC_TRACE_ERR("d2d_init: GetProcAddress('GetUserDefaultLocaleName') failed.");
        goto err_kernel_GetProcAddress;
    }

    /* Success */
    return 0;

    /* Error path unwinding */
err_kernel_GetProcAddress:
    dummy_IDWriteFactory_Release(dw_factory);
err_dw_CreateFactory:
err_dw_GetProcAddress:
    FreeLibrary(dw_dll);
    dw_dll = NULL;
err_dw_LoadLibrary:
    ID2D1Factory_Release(d2d_factory);
err_d2d_CreateFactory:
err_d2d_GetProcAddress:
    FreeLibrary(d2d_dll);
    d2d_dll = NULL;
err_d2d_LoadLibrary:
    return -1;
}

static void
d2d_fini(void)
{
    ID2D1Factory_Release(d2d_factory);
    FreeLibrary(d2d_dll);
    dw_dll = NULL;

    dummy_IDWriteFactory_Release(dw_factory);
    FreeLibrary(dw_dll);
    d2d_dll = NULL;
}

#define D2D_CANVASTYPE_BITMAP       0
#define D2D_CANVASTYPE_DC           1
#define D2D_CANVASTYPE_HWND         2

#define D2D_CANVASFLAG_RECTCLIP   0x1

typedef struct d2d_canvas_tag d2d_canvas_t;
struct d2d_canvas_tag {
    WORD type;
    WORD flags;
    union {
        ID2D1RenderTarget* target;
        ID2D1BitmapRenderTarget* bmp_target;
        ID2D1HwndRenderTarget* hwnd_target;
    };
    ID2D1GdiInteropRenderTarget* gdi_interop;
    ID2D1Layer* clip_layer;
};

static void
d2d_reset_transform(ID2D1RenderTarget* target)
{
    /* We want horizontal and vertical lines with non-fractional coordinates
     * and with stroke width 1.0 to really affect single line of pixels. To
     * achieve that, we need to setup our coordinate system to match the pixel
     * grid accordingly. */
    static const D2D1_MATRIX_3X2_F transform = {
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.5f, 0.5f
    };

    ID2D1RenderTarget_SetTransform(target, &transform);
}

static d2d_canvas_t*
d2d_canvas_alloc(ID2D1RenderTarget* target, WORD type)
{
    d2d_canvas_t* c;

    c = (d2d_canvas_t*) malloc(sizeof(d2d_canvas_t));
    if(MC_ERR(c == NULL)) {
        MC_TRACE("d2d_canvas_alloc: malloc() failed.");
        return NULL;
    }

    c->type = type;
    c->flags = 0;
    c->target = target;
    c->gdi_interop = NULL;
    c->clip_layer = NULL;

    /* mCtrl works with pixel measures as most of it uses GDI. D2D1 by default
     * works with DIPs (device independent pixels), which map 1:1 to physical
     * when DPI is 96. To be consistent with the other mCtrl parts, we enforce
     * the render target to think we have this DPI. */
    ID2D1RenderTarget_SetDpi(c->target, 96.0f, 96.0f);

    d2d_reset_transform(target);

    return c;
}

static IWICBitmapSource*
d2d_create_wic_source(const WCHAR* path, IStream* stream)
{
    IWICImagingFactory* wic_factory;
    IWICBitmapDecoder* wic_decoder;
    IWICFormatConverter* wic_converter;
    IWICBitmapFrameDecode* wic_source;
    HRESULT hr;

    wic_factory = (IWICImagingFactory*) xcom_init_create(
            &CLSID_WICImagingFactory, CLSCTX_INPROC, &IID_IWICImagingFactory);
    if(MC_ERR(wic_factory == NULL)) {
        MC_TRACE("d2d_create_wic_source: xcom_init_create() failed.");
        goto err_xcom_init_create;
    }

    if(path != NULL) {
        hr = IWICImagingFactory_CreateDecoderFromFilename(wic_factory, path,
                NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &wic_decoder);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("d2d_create_wic_source: "
                        "IWICImagingFactory::CreateDecoderFromFilename() failed.");
            goto err_CreateDecoder;
        }
    } else {
        hr = IWICImagingFactory_CreateDecoderFromStream(wic_factory, stream,
                NULL, WICDecodeMetadataCacheOnLoad, &wic_decoder);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("d2d_create_wic_source: "
                        "IWICImagingFactory::CreateDecoderFromStream() failed.");
            goto err_CreateDecoder;
        }
    }

    hr = IWICBitmapDecoder_GetFrame(wic_decoder, 0, &wic_source);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("d2d_create_wic_source: IWICBitmapDecoder::GetFrame() failed.");
        goto err_GetFrame;
    }

    hr = IWICImagingFactory_CreateFormatConverter(wic_factory, &wic_converter);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("d2d_create_wic_source: "
                    "IWICImagingFactory::CreateFormatConverter() failed.");
        goto err_CreateFormatConverter;
    }

    hr = IWICFormatConverter_Initialize(wic_converter,
            (IWICBitmapSource*) wic_source, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("d2d_create_wic_source: "
                    "IWICFormatConverter::Initialize() failed.");
        goto err_Initialize;
    }

    IWICBitmapFrameDecode_Release(wic_source);
    IWICBitmapDecoder_Release(wic_decoder);
    IWICImagingFactory_Release(wic_factory);
    return (IWICBitmapSource*) wic_converter;

err_Initialize:
    IWICFormatConverter_Release(wic_converter);
err_CreateFormatConverter:
    IWICBitmapFrameDecode_Release(wic_source);
err_GetFrame:
    IWICBitmapDecoder_Release(wic_decoder);
err_CreateDecoder:
    IWICImagingFactory_Release(wic_factory);
    xcom_uninit();
err_xcom_init_create:
    return NULL;
}

static void
d2d_setup_arc_segment(D2D1_ARC_SEGMENT* arc_seg, const xdraw_circle_t* circle,
                      float base_angle, float sweep_angle)
{
    float cx = circle->x;
    float cy = circle->y;
    float r = circle->r;
    float sweep_rads = (base_angle + sweep_angle) * (MC_PIf / 180.0f);

    arc_seg->point.x = cx + r * cosf(sweep_rads);
    arc_seg->point.y = cy + r * sinf(sweep_rads);
    arc_seg->size.width = r;
    arc_seg->size.height = r;
    arc_seg->rotationAngle = 0.0f;

    if(sweep_angle >= 0.0f)
        arc_seg->sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    else
        arc_seg->sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;

    if(sweep_angle >= 180.0f)
        arc_seg->arcSize = D2D1_ARC_SIZE_LARGE;
    else
        arc_seg->arcSize = D2D1_ARC_SIZE_SMALL;
}

static ID2D1Geometry*
d2d_create_arc_geometry(const xdraw_circle_t* circle, float base_angle,
                        float sweep_angle, BOOL pie)
{
    ID2D1PathGeometry* g = NULL;
    ID2D1GeometrySink* s;
    HRESULT hr;
    float base_rads = base_angle * (MC_PIf / 180.0f);
    D2D1_POINT_2F pt;
    D2D1_ARC_SEGMENT arc_seg;

    EnterCriticalSection(&xdraw_lock);
    hr = ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
    LeaveCriticalSection(&xdraw_lock);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("d2d_create_arc_geometry: "
                    "ID2D1Factory::CreatePathGeometry() failed.");
        return NULL;
    }
    hr = ID2D1PathGeometry_Open(g, &s);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("d2d_create_arc_geometry: ID2D1PathGeometry::Open() failed.");
        ID2D1PathGeometry_Release(g);
        return NULL;
    }

    pt.x = circle->x + circle->r * cosf(base_rads);
    pt.y = circle->y + circle->r * sinf(base_rads);
    ID2D1GeometrySink_BeginFigure(s, pt, D2D1_FIGURE_BEGIN_FILLED);

    d2d_setup_arc_segment(&arc_seg, circle, base_angle, sweep_angle);
    ID2D1GeometrySink_AddArc(s, &arc_seg);

    if(pie) {
        pt.x = circle->x;
        pt.y = circle->y;
        ID2D1GeometrySink_AddLine(s, pt);
        ID2D1GeometrySink_EndFigure(s, D2D1_FIGURE_END_CLOSED);
    } else {
        ID2D1GeometrySink_EndFigure(s, D2D1_FIGURE_END_OPEN);
    }

    ID2D1GeometrySink_Close(s);
    ID2D1GeometrySink_Release(s);

    return (ID2D1Geometry*) g;
}

static dummy_IDWriteTextLayout*
d2d_create_text_layout(dummy_IDWriteTextFormat* tf, const xdraw_rect_t* rect,
                       const TCHAR* str, int len, DWORD flags)
{
    dummy_IDWriteTextLayout* layout;
    HRESULT hr;
    int tla;

    if(len < 0)
        len = _tcslen(str);

    hr = dummy_IDWriteFactory_CreateTextLayout(dw_factory, str, len, tf,
                rect->x1 - rect->x0, rect->y1 - rect->y0, &layout);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("d2d_create_text_layout: "
                    "IDWriteFactory::CreateTextLayout() failed.");
        return NULL;
    }

    if(flags & XDRAW_STRING_RIGHT)
        tla = dummy_DWRITE_TEXT_ALIGNMENT_TRAILING;
    else if(flags & XDRAW_STRING_CENTER)
        tla = dummy_DWRITE_TEXT_ALIGNMENT_CENTER;
    else
        tla = dummy_DWRITE_TEXT_ALIGNMENT_LEADING;
    dummy_IDWriteTextLayout_SetTextAlignment(layout, tla);

    if(flags & XDRAW_STRING_NOWRAP)
        dummy_IDWriteTextLayout_SetWordWrapping(layout, dummy_DWRITE_WORD_WRAPPING_NO_WRAP);

    if((flags & XDRAW_STRING_ELLIPSIS_MASK) != 0) {
        static const dummy_DWRITE_TRIMMING trim_end = { dummy_DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        static const dummy_DWRITE_TRIMMING trim_word = { dummy_DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
        static const dummy_DWRITE_TRIMMING trim_path = { dummy_DWRITE_TRIMMING_GRANULARITY_WORD, L'\\', 1 };
        const dummy_DWRITE_TRIMMING* trim_options;
        void* trim_sign;

        hr = dummy_IDWriteFactory_CreateEllipsisTrimmingSign(dw_factory, tf, &trim_sign);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("d2d_create_text_layout: "
                        "IDWriteFactory::CreateEllipsisTrimmingSign() failed.");
            goto err_CreateEllipsisTrimmingSign;
        }

        switch(flags & XDRAW_STRING_ELLIPSIS_MASK) {
            case XDRAW_STRING_END_ELLIPSIS:     trim_options = &trim_end; break;
            case XDRAW_STRING_WORD_ELLIPSIS:    trim_options = &trim_word; break;
            case XDRAW_STRING_PATH_ELLIPSIS:    trim_options = &trim_path; break;
            default:                            MC_UNREACHABLE; break;
        }

        dummy_IDWriteTextLayout_SetTrimming(layout, trim_options, trim_sign);
    }

err_CreateEllipsisTrimmingSign:
    return layout;
}


/***************************************
 ***  Dummy <gdiplus/gdiplusflat.h>  ***
 ***************************************/

#include "dummy/gdiplus.h"

/* MSDN documentation for <gdiplus/gdiplusflat.h> sucks. This one is better:
 * http://www.jose.it-berater.org/gdiplus/iframe/index.htm
 */

/* Graphics functions */
static int (WINAPI* gdix_CreateFromHDC)(HDC, dummy_GpGraphics**);
static int (WINAPI* gdix_DeleteGraphics)(dummy_GpGraphics*);
static int (WINAPI* gdix_GraphicsClear)(dummy_GpGraphics*, dummy_ARGB);
static int (WINAPI* gdix_GetDC)(dummy_GpGraphics*, HDC*);
static int (WINAPI* gdix_ReleaseDC)(dummy_GpGraphics*, HDC);
static int (WINAPI* gdix_ResetWorldTransform)(dummy_GpGraphics*);
static int (WINAPI* gdix_RotateWorldTransform)(dummy_GpGraphics*, float, dummy_GpMatrixOrder);
static int (WINAPI* gdix_SetPixelOffsetMode)(dummy_GpGraphics*, dummy_GpPixelOffsetMode);
static int (WINAPI* gdix_SetSmoothingMode)(dummy_GpGraphics*, dummy_GpSmoothingMode);
static int (WINAPI* gdix_TranslateWorldTransform)(dummy_GpGraphics*, float, float, dummy_GpMatrixOrder);
static int (WINAPI* gdix_SetClipRect)(dummy_GpGraphics*, float, float, float, float, dummy_GpCombineMode);
static int (WINAPI* gdix_SetClipPath)(dummy_GpGraphics*, dummy_GpPath*, dummy_GpCombineMode);
static int (WINAPI* gdix_ResetClip)(dummy_GpGraphics*);

/* Brush functions */
static int (WINAPI* gdix_CreateSolidFill)(dummy_ARGB, dummy_GpSolidFill**);
static int (WINAPI* gdix_DeleteBrush)(dummy_GpBrush*);
static int (WINAPI* gdix_SetSolidFillColor)(dummy_GpSolidFill*, dummy_ARGB);

/* Pen functions */
static int (WINAPI* gdix_CreatePen1)(dummy_ARGB, float, dummy_GpUnit, dummy_GpPen**);
static int (WINAPI* gdix_DeletePen)(dummy_GpPen*);
static int (WINAPI* gdix_SetPenBrushFill)(dummy_GpPen*, dummy_GpBrush*);
static int (WINAPI* gdix_SetPenWidth)(dummy_GpPen*, float);

/* Path functions */
static int (WINAPI* gdix_CreatePath)(dummy_GpFillMode, dummy_GpPath**);
static int (WINAPI* gdix_DeletePath)(dummy_GpPath*);
static int (WINAPI* gdix_ClosePathFigure)(dummy_GpPath*);
static int (WINAPI* gdix_StartPathFigure)(dummy_GpPath*);
static int (WINAPI* gdix_GetPathLastPoint)(dummy_GpPath*, dummy_GpPointF*);
static int (WINAPI* gdix_AddPathArc)(dummy_GpPath*, float, float, float, float, float, float);
static int (WINAPI* gdix_AddPathLine)(dummy_GpPath*, float, float, float, float);

/* Font functions */
static int (WINAPI* gdix_CreateFontFromLogfontW)(HDC, const LOGFONTW*, dummy_GpFont**);
static int (WINAPI* gdix_DeleteFont)(dummy_GpFont*);
static int (WINAPI* gdix_DeleteFontFamily)(dummy_GpFont*);
static int (WINAPI* gdix_GetCellAscent)(const dummy_GpFont*, int, UINT16*);
static int (WINAPI* gdix_GetCellDescent)(const dummy_GpFont*, int, UINT16*);
static int (WINAPI* gdix_GetEmHeight)(const dummy_GpFont*, int, UINT16*);
static int (WINAPI* gdix_GetFamily)(dummy_GpFont*, void**);
static int (WINAPI* gdix_GetFontSize)(dummy_GpFont*, float*);
static int (WINAPI* gdix_GetFontStyle)(dummy_GpFont*, int*);
static int (WINAPI* gdix_GetLineSpacing)(const dummy_GpFont*, int, UINT16*);

/* Image & bitmap functions */
static int (WINAPI* gdix_LoadImageFromFile)(const WCHAR*, dummy_GpImage**);
static int (WINAPI* gdix_LoadImageFromStream)(IStream*, dummy_GpImage**);
static int (WINAPI* gdix_CreateBitmapFromHBITMAP)(HBITMAP, HPALETTE, dummy_GpBitmap**);
static int (WINAPI* gdix_CreateBitmapFromHICON)(HICON, dummy_GpBitmap**);
static int (WINAPI* gdix_DisposeImage)(dummy_GpImage*);
static int (WINAPI* gdix_GetImageBounds)(dummy_GpImage*, dummy_GpRectF*, dummy_GpUnit*);

/* String format functions */
static int (WINAPI* gdix_CreateStringFormat)(int, LANGID, dummy_GpStringFormat**);
static int (WINAPI* gdix_DeleteStringFormat)(dummy_GpStringFormat*);
static int (WINAPI* gdix_SetStringFormatAlign)(dummy_GpStringFormat*, dummy_GpStringAlignment);
static int (WINAPI* gdix_SetStringFormatFlags)(dummy_GpStringFormat*, int);
static int (WINAPI* gdix_SetStringFormatTrimming)(dummy_GpStringFormat*, dummy_GpStringTrimming);

/* Draw/fill functions */
static int (WINAPI* gdix_DrawArc)(dummy_GpGraphics*, dummy_GpPen*, float, float, float, float, float, float);
static int (WINAPI* gdix_DrawImageRectRect)(dummy_GpGraphics*, dummy_GpImage*, float, float, float, float, float, float, float, float, dummy_GpUnit, const void*, void*, void*);
static int (WINAPI* gdix_DrawLine)(dummy_GpGraphics*, dummy_GpPen*, float, float, float, float);
static int (WINAPI* gdix_DrawPath)(dummy_GpGraphics*, dummy_GpPen*, dummy_GpPath*);
static int (WINAPI* gdix_DrawPie)(dummy_GpGraphics*, dummy_GpPen*, float, float, float, float, float, float);
static int (WINAPI* gdix_DrawRectangle)(dummy_GpGraphics*, void*, float, float, float, float);
static int (WINAPI* gdix_DrawString)(dummy_GpGraphics*, const WCHAR*, int, const dummy_GpFont*, const dummy_GpRectF*, const dummy_GpStringFormat*, const dummy_GpBrush*);
static int (WINAPI* gdix_FillEllipse)(dummy_GpGraphics*, dummy_GpBrush*, float, float, float, float);
static int (WINAPI* gdix_FillPath)(dummy_GpGraphics*, dummy_GpBrush*, dummy_GpPath*);
static int (WINAPI* gdix_FillPie)(dummy_GpGraphics*, dummy_GpBrush*, float, float, float, float, float, float);
static int (WINAPI* gdix_FillRectangle)(dummy_GpGraphics*, void*, float, float, float, float);
static int (WINAPI* gdix_MeasureString)(dummy_GpGraphics*, const WCHAR*, int, const dummy_GpFont*, const dummy_GpRectF*, const dummy_GpStringFormat*, dummy_GpRectF*, int*, int*);


/*********************
 ***  GDI+ Driver  ***
 *********************/

static HMODULE gdix_dll = NULL;
static ULONG_PTR gdix_token;
static void (WINAPI* gdix_Shutdown)(ULONG_PTR);

static int
gdix_init(void)
{
    int (WINAPI* gdix_Startup)(ULONG_PTR*, const dummy_GpStartupInput*, void*);
    dummy_GpStartupInput input = { 0 };
    int status;

    if(MC_LIKELY(mc_win_version >= MC_WIN_XP)) {
        gdix_dll = mc_load_sys_dll(_T("GDIPLUS.DLL"));
    } else {
        /* Windows 2000 do not have GDIPLUS.DLL by default. Try whether the
         * application is equipped with the redistributable version of it. */
        gdix_dll = mc_load_redist_dll(_T("GDIPLUS.DLL"));
    }
    if(MC_ERR(gdix_dll == NULL)) {
        MC_TRACE_ERR("gdix_init: LoadLibrary(GDIPLUS.DLL) failed");
        goto err_LoadLibrary;
    }

    gdix_Startup = (int (WINAPI*)(ULONG_PTR*, const dummy_GpStartupInput*, void*))
                        GetProcAddress(gdix_dll, "GdiplusStartup");
    if(MC_ERR(gdix_Startup == NULL)) {
        MC_TRACE_ERR("gdix_init: GetProcAddress(GdiplusStartup) failed");
        goto err_GetProcAddress;
    }

    gdix_Shutdown = (void (WINAPI*)(ULONG_PTR))
                        GetProcAddress(gdix_dll, "GdiplusShutdown");
    if(MC_ERR(gdix_Shutdown == NULL)) {
        MC_TRACE_ERR("gdix_init: GetProcAddress(GdiplusShutdown) failed");
        goto err_GetProcAddress;
    }

#define GPA(name, params)                                                      \
        do {                                                                   \
            gdix_##name = (int (WINAPI*)params)                                \
                        GetProcAddress(gdix_dll, "Gdip"#name);                 \
            if(MC_ERR(gdix_##name == NULL)) {                                  \
                MC_TRACE_ERR("gdix_init: GetProcAddress(Gdip"#name") failed"); \
                goto err_GetProcAddress;                                       \
            }                                                                  \
        } while(0)

    /* Graphics functions */
    GPA(CreateFromHDC, (HDC, dummy_GpGraphics**));
    GPA(DeleteGraphics, (dummy_GpGraphics*));
    GPA(GraphicsClear, (dummy_GpGraphics*, dummy_ARGB));
    GPA(GetDC, (dummy_GpGraphics*, HDC*));
    GPA(ReleaseDC, (dummy_GpGraphics*, HDC));
    GPA(ResetWorldTransform, (dummy_GpGraphics*));
    GPA(RotateWorldTransform, (dummy_GpGraphics*, float, dummy_GpMatrixOrder));
    GPA(SetPixelOffsetMode, (dummy_GpGraphics*, dummy_GpPixelOffsetMode));
    GPA(SetSmoothingMode, (dummy_GpGraphics*, dummy_GpSmoothingMode));
    GPA(TranslateWorldTransform, (dummy_GpGraphics*, float, float, dummy_GpMatrixOrder));
    GPA(SetClipRect, (dummy_GpGraphics*, float, float, float, float, dummy_GpCombineMode));
    GPA(SetClipPath, (dummy_GpGraphics*, dummy_GpPath*, dummy_GpCombineMode));
    GPA(ResetClip, (dummy_GpGraphics*));

    /* Brush functions */
    GPA(CreateSolidFill, (dummy_ARGB, dummy_GpSolidFill**));
    GPA(DeleteBrush, (dummy_GpBrush*));
    GPA(SetSolidFillColor, (dummy_GpSolidFill*, dummy_ARGB));

    /* Pen functions */
    GPA(CreatePen1, (DWORD, float, dummy_GpUnit, dummy_GpPen**));
    GPA(DeletePen, (dummy_GpPen*));
    GPA(SetPenBrushFill, (dummy_GpPen*, dummy_GpBrush*));
    GPA(SetPenWidth, (dummy_GpPen*, float));

    /* Path functions */
    GPA(CreatePath, (dummy_GpFillMode, dummy_GpPath**));
    GPA(DeletePath, (dummy_GpPath*));
    GPA(ClosePathFigure, (dummy_GpPath*));
    GPA(StartPathFigure, (dummy_GpPath*));
    GPA(GetPathLastPoint, (dummy_GpPath*, dummy_GpPointF*));
    GPA(AddPathArc, (dummy_GpPath*, float, float, float, float, float, float));
    GPA(AddPathLine, (dummy_GpPath*, float, float, float, float));

    /* Font functions */
    GPA(CreateFontFromLogfontW, (HDC, const LOGFONTW*, dummy_GpFont**));
    GPA(DeleteFont, (dummy_GpFont*));
    GPA(DeleteFontFamily, (dummy_GpFont*));
    GPA(GetCellAscent, (const dummy_GpFont*, int, UINT16*));
    GPA(GetCellDescent, (const dummy_GpFont*, int, UINT16*));
    GPA(GetEmHeight, (const dummy_GpFont*, int, UINT16*));
    GPA(GetFamily, (dummy_GpFont*, void**));
    GPA(GetFontSize, (dummy_GpFont*, float*));
    GPA(GetFontStyle, (dummy_GpFont*, int*));
    GPA(GetLineSpacing, (const dummy_GpFont*, int, UINT16*));

    /* Image & bitmap functions */
    GPA(LoadImageFromFile, (const WCHAR*, dummy_GpImage**));
    GPA(LoadImageFromStream, (IStream*, dummy_GpImage**));
    GPA(CreateBitmapFromHBITMAP, (HBITMAP, HPALETTE, dummy_GpBitmap**));
    GPA(CreateBitmapFromHICON, (HICON, dummy_GpBitmap**));
    GPA(DisposeImage, (dummy_GpImage*));
    GPA(GetImageBounds, (dummy_GpImage*, dummy_GpRectF*, dummy_GpUnit*));

    /* String format functions */
    GPA(CreateStringFormat, (int, LANGID, dummy_GpStringFormat**));
    GPA(DeleteStringFormat, (dummy_GpStringFormat*));
    GPA(SetStringFormatAlign, (dummy_GpStringFormat*, dummy_GpStringAlignment));
    GPA(SetStringFormatFlags, (dummy_GpStringFormat*, int));
    GPA(SetStringFormatTrimming, (dummy_GpStringFormat*, dummy_GpStringTrimming));

    /* Draw/fill functions */
    GPA(DrawArc, (dummy_GpGraphics*, dummy_GpPen*, float, float, float, float, float, float));
    GPA(DrawImageRectRect, (dummy_GpGraphics*, dummy_GpImage*, float, float, float, float, float, float, float, float, dummy_GpUnit, const void*, void*, void*));
    GPA(DrawLine, (dummy_GpGraphics*, dummy_GpPen*, float, float, float, float));
    GPA(DrawPath, (dummy_GpGraphics*, dummy_GpPen*, dummy_GpPath*));
    GPA(DrawPie, (dummy_GpGraphics*, dummy_GpPen*, float, float, float, float, float, float));
    GPA(DrawRectangle, (dummy_GpGraphics*, void*, float, float, float, float));
    GPA(DrawString, (dummy_GpGraphics*, const WCHAR*, int, const dummy_GpFont*, const dummy_GpRectF*, const dummy_GpStringFormat*, const dummy_GpBrush*));
    GPA(FillEllipse, (dummy_GpGraphics*, dummy_GpBrush*, float, float, float, float));
    GPA(FillPath, (dummy_GpGraphics*, dummy_GpBrush*, dummy_GpPath*));
    GPA(FillPie, (dummy_GpGraphics*, dummy_GpBrush*, float, float, float, float, float, float));
    GPA(FillRectangle, (dummy_GpGraphics*, void*, float, float, float, float));
    GPA(MeasureString, (dummy_GpGraphics*, const WCHAR*, int, const dummy_GpFont*, const dummy_GpRectF*, const dummy_GpStringFormat*, dummy_GpRectF*, int*, int*));

#undef GPA

    input.GdiplusVersion = 1;
    input.SuppressExternalCodecs = TRUE;
    status = gdix_Startup(&gdix_token, &input, NULL);
    if(MC_ERR(status != 0)) {
        MC_TRACE("GdiplusStartup() failed. [%d]", status);
        goto err_Startup;
    }

    /* Success */
    return 0;

    /* Error path */
err_Startup:
err_GetProcAddress:
    FreeLibrary(gdix_dll);
    gdix_dll = NULL;
err_LoadLibrary:
    return -1;
}

static void
gdix_fini(void)
{
    gdix_Shutdown(gdix_token);

    FreeLibrary(gdix_dll);
    gdix_dll = NULL;
}


typedef struct gdix_canvas_tag gdix_canvas_t;
struct gdix_canvas_tag {
    HDC dc;
    dummy_GpGraphics* graphics;
    dummy_GpPen* pen;
    dummy_GpStringFormat* string_format;
    BOOL use_dblbuf;
    doublebuffer_t dblbuf;
};

static gdix_canvas_t*
gdix_canvas_alloc(HDC dc, RECT* doublebuffer_rect)
{
    gdix_canvas_t* c;
    int status;

    c = (gdix_canvas_t*) malloc(sizeof(gdix_canvas_t));
    if(MC_ERR(c == NULL)) {
        MC_TRACE("gdix_canvas_alloc: malloc() failed");
        goto err_malloc;
    }

    if(doublebuffer_rect != NULL) {
        c->dc = doublebuffer_open(&c->dblbuf, dc, doublebuffer_rect);
        c->use_dblbuf = TRUE;
    } else {
        c->dc = dc;
        c->use_dblbuf = FALSE;
    }

    status = gdix_CreateFromHDC(c->dc, &c->graphics);
    if(MC_ERR(status != 0)) {
        MC_TRACE("gdix_canvas_alloc: gdix_CreateFromHDC() failed. [%d]", status);
        goto err_creategraphics;
    }

    status = gdix_SetSmoothingMode(c->graphics, 5/*SmoothingModeAntiAlias8x8*/);  /* GDI+ 1.1 */
    if(MC_ERR(status != 0))
        gdix_SetSmoothingMode(c->graphics, 2/*SmoothingModeHighQuality*/);        /* GDI+ 1.0 */

    /* GDI+ has, unlike D2D, a concept of pen, which is used for "draw"
     * operations (while brush is used for "fill" operations.).
     * Our interface works only with brushes as D2D does. Hence we create
     * a pen as part of GDI+ canvas and we update it ad-hoc with
     * gdix_SetPenBrushFill() and gdix_SetPenWidth() in xdraw_draw_XXXX()
     * functions.
     */
    status = gdix_CreatePen1(0, 1.0f, 2/*UnitPixel*/, &c->pen);
    if(MC_ERR(status != 0)) {
        MC_TRACE("gdix_canvas_alloc: "
                 "GdipCreatePen1() failed. [%d]", status);
        goto err_createpen;
    }

    /* Needed for xdraw_draw_string() and xdraw_measure_string() */
    status = gdix_CreateStringFormat(0, LANG_NEUTRAL, &c->string_format);
    if(MC_ERR(status != 0)) {
        MC_TRACE("gdix_canvas_alloc: "
                 "GdipCreateStringFormat() failed. [%d]", status);
        goto err_createstringformat;
    }

    return c;

    /* Error path */
err_createstringformat:
    gdix_DeletePen(c->pen);
err_createpen:
    gdix_DeleteGraphics(c->graphics);
err_creategraphics:
    if(c->use_dblbuf)
        doublebuffer_close(&c->dblbuf, FALSE);
    free(c);
err_malloc:
    return NULL;
}

static void
gdix_canvas_apply_string_flags(gdix_canvas_t* c, DWORD flags)
{
    int sfa;
    int sff;
    int trim;

    if(flags & XDRAW_STRING_RIGHT)
        sfa = 2;  /* StringAlignmentFar */
    else if(flags & XDRAW_STRING_CENTER)
        sfa = 1;  /* StringAlignmentCenter */
    else
        sfa = 0;  /* StringAlignmentNear */
    gdix_SetStringFormatAlign(c->string_format, sfa);

    sff = 0;
    if(flags & XDRAW_STRING_NOWRAP)
        sff |= 0x00001000;  /* StringFormatFlagsNoWrap */
    if(!(flags & XDRAW_STRING_CLIP))
        sff |= 0x00004000;  /* StringFormatFlagsNoClip */
    gdix_SetStringFormatFlags(c->string_format, sff);

    if((flags & XDRAW_STRING_ELLIPSIS_MASK) != 0) {
        switch(flags & XDRAW_STRING_ELLIPSIS_MASK) {
            case XDRAW_STRING_END_ELLIPSIS:     trim = 3; break; /*StringTrimmingEllipsisCharacter */
            case XDRAW_STRING_WORD_ELLIPSIS:    trim = 4; break; /* StringTrimmingEllipsisWord */
            case XDRAW_STRING_PATH_ELLIPSIS:    trim = 5; break; /* StringTrimmingEllipsisPath */
            default:                            MC_UNREACHABLE; break;
        }

        gdix_SetStringFormatTrimming(c->string_format, trim);
    }
}


/**************************
 ***  Public Interface  ***
 **************************/

xdraw_canvas_t*
xdraw_canvas_create_with_paintstruct(HWND win, PAINTSTRUCT* ps, DWORD flags)
{
    XDRAW_TRACE("xdraw_canvas_create_with_paintstruct()");

    if(d2d_dll != NULL) {
        D2D1_RENDER_TARGET_PROPERTIES props = {
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            0.0f, 0.0f,
            ((flags & XDRAW_CANVAS_GDICOMPAT) ?
                        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE : 0),
            D2D1_FEATURE_LEVEL_DEFAULT
        };
        D2D1_HWND_RENDER_TARGET_PROPERTIES props2;
        d2d_canvas_t* c;
        RECT rect;
        ID2D1HwndRenderTarget* target;
        HRESULT hr;

        GetClientRect(win, &rect);

        props2.hwnd = win;
        props2.pixelSize.width = mc_width(&rect);
        props2.pixelSize.height = mc_height(&rect);
        props2.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

        EnterCriticalSection(&xdraw_lock);
        /* Note ID2D1HwndRenderTarget is implicitly double-buffered. */
        hr = ID2D1Factory_CreateHwndRenderTarget(d2d_factory, &props, &props2, &target);
        LeaveCriticalSection(&xdraw_lock);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_canvas_create_with_paintstruct: "
                        "ID2D1Factory::CreateHwndRenderTarget() failed.");
            return NULL;
        }

        c = d2d_canvas_alloc((ID2D1RenderTarget*)target, D2D_CANVASTYPE_HWND);
        if(MC_ERR(c == NULL)) {
            MC_TRACE("xdraw_canvas_create_with_paintstruct: d2d_canvas_alloc() failed.");
            ID2D1RenderTarget_Release((ID2D1RenderTarget*)target);
            return NULL;
        }

        return (xdraw_canvas_t*) c;
    } else {
        BOOL use_doublebuffer = (flags & XDRAW_CANVAS_DOUBLEBUFER);
        return (xdraw_canvas_t*) gdix_canvas_alloc(ps->hdc,
                                     (use_doublebuffer ? &ps->rcPaint : NULL));
    }
}

xdraw_canvas_t*
xdraw_canvas_create_with_dc(HDC dc, const RECT* rect, DWORD flags)
{
    XDRAW_TRACE("xdraw_canvas_create_with_dc()");

    if(d2d_dll != NULL) {
        D2D1_RENDER_TARGET_PROPERTIES props = {
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            0.0f, 0.0f,
            ((flags & XDRAW_CANVAS_GDICOMPAT) ?
                        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE : 0),
            D2D1_FEATURE_LEVEL_DEFAULT
        };
        d2d_canvas_t* c;
        ID2D1DCRenderTarget* target;
        HRESULT hr;

        EnterCriticalSection(&xdraw_lock);
        hr = ID2D1Factory_CreateDCRenderTarget(d2d_factory, &props, &target);
        LeaveCriticalSection(&xdraw_lock);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_canvas_create_with_dc: "
                        "ID2D1Factory::CreateDCRenderTarget() failed.");
            goto err_CreateDCRenderTarget;
        }

        hr = ID2D1DCRenderTarget_BindDC(target, dc, rect);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_canvas_create_with_dc: "
                        "ID2D1Factory::BindDC() failed.");
            goto err_BindDC;
        }

        c = d2d_canvas_alloc((ID2D1RenderTarget*)target, D2D_CANVASTYPE_DC);
        if(MC_ERR(c == NULL)) {
            MC_TRACE("xdraw_canvas_create_with_dc: d2d_canvas_alloc() failed.");
            goto err_d2d_canvas_alloc;
        }

        return (xdraw_canvas_t*) c;

err_d2d_canvas_alloc:
err_BindDC:
        ID2D1RenderTarget_Release((ID2D1RenderTarget*)target);
err_CreateDCRenderTarget:
        return NULL;
    } else {
        return (xdraw_canvas_t*) gdix_canvas_alloc(dc, NULL);
    }
}

void
xdraw_canvas_destroy(xdraw_canvas_t* canvas)
{
    XDRAW_TRACE("xdraw_canvas_destroy(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;

        MC_ASSERT(c->flags == 0);
        MC_ASSERT(c->gdi_interop == NULL);
        MC_ASSERT(c->clip_layer == NULL);

        ID2D1RenderTarget_Release(c->target);
        free(c);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;

        gdix_DeleteStringFormat(c->string_format);
        gdix_DeletePen(c->pen);
        gdix_DeleteGraphics(c->graphics);
        free(c);
    }
}

int
xdraw_canvas_resize(xdraw_canvas_t* canvas, UINT width, UINT height)
{
    XDRAW_TRACE("xdraw_canvas_destroy(%p, %u, %u)", canvas, width, height);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        if(c->type == D2D_CANVASTYPE_HWND) {
            D2D1_SIZE_U size = { width, height };
            HRESULT hr;

            hr = ID2D1HwndRenderTarget_Resize(c->hwnd_target, &size);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("xdraw_canvas_resize: "
                            "ID2D1HwndRenderTarget_Resize() failed.");
                return -1;
            }
            return 0;
        } else {
            /* Operation not supported. */
            MC_TRACE("xdraw_canvas_resize: Not supported (not ID2D1HwndRenderTarget).");
            return -1;
        }
    } else {
        /* Operation not supported. */
        MC_TRACE("xdraw_canvas_resize: Not supported (GDI+ driver).");

        /* Actually we should never be here as GDI+ driver never allows caching
         * of the canvas so there is no need to ever resize it. */
        MC_UNREACHABLE;

        return -1;
    }
}

void
xdraw_canvas_begin_paint(xdraw_canvas_t* canvas)
{
    XDRAW_TRACE("xdraw_canvas_begin_paint(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1RenderTarget_BeginDraw(c->target);
    } else {
        /* noop */
    }
}

BOOL
xdraw_canvas_end_paint(xdraw_canvas_t* canvas)
{
    XDRAW_TRACE("xdraw_canvas_end_paint(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        HRESULT hr;

        hr = ID2D1RenderTarget_EndDraw(c->target, NULL, NULL);
        return (SUCCEEDED(hr)  &&  hr != D2DERR_RECREATE_TARGET);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        if(c->use_dblbuf)
            doublebuffer_close(&c->dblbuf, TRUE);

        /* Ask caller to destroy the canvas (i.e. disable caching),
         * as GDI+ is not suitable for that. */
        return FALSE;
    }
}

HDC
xdraw_canvas_acquire_dc(xdraw_canvas_t* canvas, BOOL retain_contents)
{
    XDRAW_TRACE("xdraw_canvas_acquire_dc(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1GdiInteropRenderTarget* gdi_interop;
        D2D1_DC_INITIALIZE_MODE init_mode;
        HRESULT hr;
        HDC dc;

        MC_ASSERT(c->gdi_interop == NULL);

        hr = ID2D1RenderTarget_QueryInterface(c->target,
                    &IID_ID2D1GdiInteropRenderTarget, (void**) &gdi_interop);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_canvas_acquire_dc: ID2D1RenderTarget::"
                        "QueryInterface(IID_ID2D1GdiInteropRenderTarget) failed.");
            return NULL;
        }

        init_mode = (retain_contents ? D2D1_DC_INITIALIZE_MODE_COPY
                                     : D2D1_DC_INITIALIZE_MODE_CLEAR);
        hr = ID2D1GdiInteropRenderTarget_GetDC(gdi_interop, init_mode, &dc);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_canvas_acquire_dc: "
                        "ID2D1GdiInteropRenderTarget::GetDC() failed.");
            ID2D1GdiInteropRenderTarget_Release(gdi_interop);
            return NULL;
        }

        c->gdi_interop = gdi_interop;
        return dc;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        int status;
        HDC dc;

        status = gdix_GetDC(c->graphics, &dc);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_canvas_acquire_dc: GdipGetDC() failed. [%d]", status);
            return NULL;
        }

        return dc;
    }
}

void
xdraw_canvas_release_dc(xdraw_canvas_t* canvas, HDC dc)
{
    XDRAW_TRACE("xdraw_canvas_release_dc(%p, %p)", canvas, dc);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;

        ID2D1GdiInteropRenderTarget_ReleaseDC(c->gdi_interop, NULL);
        ID2D1GdiInteropRenderTarget_Release(c->gdi_interop);
        c->gdi_interop = NULL;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_ReleaseDC(c->graphics, dc);
    }
}

void
xdraw_canvas_transform_with_rotation(xdraw_canvas_t* canvas, float angle)
{
    XDRAW_TRACE("xdraw_canvas_transform_with_rotation(%p, %f)",
                canvas, (double)angle);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        D2D1_MATRIX_3X2_F old_transform, new_transform;
        float angle_rads = angle * (MC_PIf / 180.0f);
        float angle_sin = sinf(angle_rads);
        float angle_cos = cosf(angle_rads);

        ID2D1RenderTarget_GetTransform(c->target, &old_transform);
        new_transform._11 = old_transform._11 * angle_cos - old_transform._12 * angle_sin;
        new_transform._12 = old_transform._11 * angle_sin + old_transform._12 * angle_cos;
        new_transform._21 = old_transform._21 * angle_cos - old_transform._22 * angle_sin;
        new_transform._22 = old_transform._21 * angle_sin + old_transform._22 * angle_cos;
        new_transform._31 = old_transform._31;
        new_transform._32 = old_transform._32;
        ID2D1RenderTarget_SetTransform(c->target, &new_transform);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_RotateWorldTransform(c->graphics, angle, dummy_MatrixOrderPrepend);
    }
}

void
xdraw_canvas_transform_with_translation(xdraw_canvas_t* canvas, float dx, float dy)
{
    XDRAW_TRACE("xdraw_canvas_transform_with_translation(%p, %f, %f)",
                canvas, (double)dx, (double)dy);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        D2D1_MATRIX_3X2_F transform;

        ID2D1RenderTarget_GetTransform(c->target, &transform);
        transform._31 += dx;
        transform._32 += dy;
        ID2D1RenderTarget_SetTransform(c->target, &transform);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_TranslateWorldTransform(c->graphics, dx, dy, dummy_MatrixOrderPrepend);
    }
}

void
xdraw_canvas_transform_reset(xdraw_canvas_t* canvas)
{
    XDRAW_TRACE("xdraw_canvas_transform_reset(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        d2d_reset_transform(c->target);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_ResetWorldTransform(c->graphics);
    }
}

void
xdraw_canvas_set_clip(xdraw_canvas_t* canvas, const xdraw_rect_t* rect,
                      const xdraw_path_t* path)
{
    XDRAW_TRACE("xdraw_canvas_set_clip(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;

        if(c->clip_layer != NULL) {
            ID2D1RenderTarget_PopLayer(c->target);
            ID2D1Layer_Release(c->clip_layer);
            c->clip_layer = NULL;
        }

        if(c->flags & D2D_CANVASFLAG_RECTCLIP) {
            ID2D1RenderTarget_PopAxisAlignedClip(c->target);
            c->flags &= ~D2D_CANVASFLAG_RECTCLIP;
        }

        if(rect != NULL) {
            ID2D1RenderTarget_PushAxisAlignedClip(c->target,
                    (const D2D1_RECT_F*) rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            c->flags |= D2D_CANVASFLAG_RECTCLIP;
        }

        if(path != NULL) {
            ID2D1PathGeometry* g = (ID2D1PathGeometry*) path;
            D2D1_LAYER_PARAMETERS layer_params;
            HRESULT hr;

            hr = ID2D1RenderTarget_CreateLayer(c->target, NULL, &c->clip_layer);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("xdraw_canvas_set_clip_path: "
                            "ID2D1RenderTarget::CreateLayer() failed.");
                return;
            }

            if(rect != NULL) {
                layer_params.contentBounds.left = rect->x0;
                layer_params.contentBounds.top = rect->y0;
                layer_params.contentBounds.right = rect->x1;
                layer_params.contentBounds.bottom = rect->y1;
            } else {
                layer_params.contentBounds.left = FLT_MIN;
                layer_params.contentBounds.top = FLT_MIN;
                layer_params.contentBounds.right = FLT_MAX;
                layer_params.contentBounds.bottom = FLT_MAX;
            }
            layer_params.geometricMask = (ID2D1Geometry*) g;
            layer_params.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
            layer_params.maskTransform._11 = 1.0f;
            layer_params.maskTransform._12 = 0.0f;
            layer_params.maskTransform._21 = 0.0f;
            layer_params.maskTransform._22 = 1.0f;
            layer_params.maskTransform._31 = 0.0f;
            layer_params.maskTransform._32 = 0.0f;
            layer_params.opacity = 1.0f;
            layer_params.opacityBrush = NULL;
            layer_params.layerOptions = D2D1_LAYER_OPTIONS_NONE;
            ID2D1RenderTarget_PushLayer(c->target, &layer_params, c->clip_layer);
        }
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        int combine_mode = dummy_CombineModeReplace;

        if(rect == NULL  &&  path == NULL)
            gdix_ResetClip(c->graphics);

        if(rect != NULL) {
            gdix_SetClipRect(c->graphics, rect->x0, rect->y0,
                             rect->x1, rect->y1, combine_mode);
            combine_mode = dummy_CombineModeIntersect;
        }

        if(path != NULL)
            gdix_SetClipPath(c->graphics, (void*) path, combine_mode);
    }
}

BOOL
xdraw_is_always_doublebuffered(void)
{
    XDRAW_TRACE("xdraw_is_always_doublebuffered()");

    if(d2d_dll != NULL) {
        return TRUE;   /* D2D is always using double-buffering. */
    } else {
        return FALSE;
    }
}

xdraw_brush_t*
xdraw_brush_solid_create(xdraw_canvas_t* canvas, xdraw_color_t color)
{
    XDRAW_TRACE("xdraw_brush_solid_create(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1SolidColorBrush* b;
        D2D_COLOR_F clr;
        HRESULT hr;

        clr.r = XDRAW_REDVALUE(color) / 255.0f;
        clr.g = XDRAW_GREENVALUE(color) / 255.0f;
        clr.b = XDRAW_BLUEVALUE(color) / 255.0f;
        clr.a = XDRAW_ALPHAVALUE(color) / 255.0f;

        hr = ID2D1RenderTarget_CreateSolidColorBrush(
                        c->target, &clr, NULL, &b);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_brush_create_solid: "
                        "ID2D1RenderTarget::CreateSolidColorBrush() failed.");
            return NULL;
        }
        return (xdraw_brush_t*) b;
    } else {
        dummy_GpSolidFill* b;
        int status;

        status = gdix_CreateSolidFill(color, &b);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_brush_create_solid: "
                     "GdipCreateSolidFill() failed. [%d]", status);
            return NULL;
        }
        return (xdraw_brush_t*) b;
    }
}

void
xdraw_brush_destroy(xdraw_brush_t* brush)
{
    XDRAW_TRACE("xdraw_brush_destroy(%p)", brush);

    if(d2d_dll != NULL) {
        ID2D1Brush_Release((ID2D1Brush*) brush);
    } else {
        gdix_DeleteBrush((void*) brush);
    }
}

void
xdraw_brush_solid_set_color(xdraw_brush_t* solidbrush, xdraw_color_t color)
{
    XDRAW_TRACE("xdraw_brush_solid_set_color(%p)", solidbrush);

    if(d2d_dll != NULL) {
        D2D_COLOR_F clr;

        clr.r = XDRAW_REDVALUE(color) / 255.0f;
        clr.g = XDRAW_GREENVALUE(color) / 255.0f;
        clr.b = XDRAW_BLUEVALUE(color) / 255.0f;
        clr.a = XDRAW_ALPHAVALUE(color) / 255.0f;

        ID2D1SolidColorBrush_SetColor((ID2D1SolidColorBrush*) solidbrush, &clr);
    } else {
        gdix_SetSolidFillColor((dummy_GpSolidFill*) solidbrush, (dummy_ARGB) color);
    }
}

xdraw_font_t*
xdraw_font_create_with_LOGFONT(xdraw_canvas_t* canvas, const LOGFONT* logfont)
{
    XDRAW_TRACE("xdraw_font_create_with_LOGFONT(%p)", canvas);

#ifndef UNICODE
    #error xdraw_font_create_with_LOGFONT() is not (yet?) implemented for ANSI build.
#endif

    if(d2d_dll != NULL) {
        WCHAR user_locale[LOCALE_NAME_MAX_LENGTH];
        static WCHAR no_locale[] = L"";
        static WCHAR enus_locale[] = L"en-us";
        WCHAR* locales[] = { user_locale, no_locale, enus_locale };
        int i;
        dummy_IDWriteTextFormat* tf;
        float size;
        dummy_DWRITE_FONT_STYLE style;
        dummy_DWRITE_FONT_WEIGHT weight;
        HRESULT hr;

        if(fn_GetUserDefaultLocaleName(user_locale, LOCALE_NAME_MAX_LENGTH) == 0) {
            MC_TRACE("xdraw_font_create_with_LOGFONT: "
                     "GetUserDefaultLocaleName() failed.");
            user_locale[0] = _T('\0');
        }

        /* DirectWrite does not support "default" font size. */
        size = (logfont->lfHeight != 0 ? MC_ABS(logfont->lfHeight) : 12);

        if(logfont->lfItalic)
            style = dummy_DWRITE_FONT_STYLE_ITALIC;
        else
            style = dummy_DWRITE_FONT_STYLE_NORMAL;

        /* FIXME: Right now, we ignore some LOGFONT members here.
         *        For example:
         *          -- LOGFONT::lfUnderline should propagate into
         *             dummy_IDWriteTextLayout::SetUnderline()
         *          -- LOGFONT::lfStrikeOut should propagate into
         *             dummy_IDWriteTextLayout::SetStrikethrough()
         */

        /* DirectWrite does not support FW_DONTCARE */
        weight = (logfont->lfWeight != FW_DONTCARE ? logfont->lfWeight : FW_REGULAR);

        for(i = 0; i < MC_SIZEOF_ARRAY(locales); i++) {
            hr = dummy_IDWriteFactory_CreateTextFormat(dw_factory, logfont->lfFaceName,
                    NULL, weight, style, dummy_DWRITE_FONT_STRETCH_NORMAL, size,
                    locales[i], &tf);
            if(SUCCEEDED(hr))
                return (xdraw_font_t*) tf;
        }

        /* In case of a failure, try to fall back to a reasonable the default
         * font. */
        for(i = 0; i < MC_SIZEOF_ARRAY(locales); i++) {
            hr = dummy_IDWriteFactory_CreateTextFormat(dw_factory,
                    XDRAW_DEFAULT_FONT_FAMILY, NULL, weight, style,
                    dummy_DWRITE_FONT_STRETCH_NORMAL, size, locales[i], &tf);
            if(SUCCEEDED(hr))
                return (xdraw_font_t*) tf;
        }

        MC_TRACE("xdraw_font_create_with_LOGFONT: "
                 "IDWriteFactory::CreateTextFormat(%S, %S) failed. [0x%lx]",
                 logfont->lfFaceName, user_locale, hr);
        return NULL;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        dummy_GpFont* f;
        int status;

        status = gdix_CreateFontFromLogfontW(c->dc, logfont, &f);
        if(MC_ERR(status != 0)) {
            /* GDI+ supports only fonts with true-type outlines. For this
             * reason, implement simple fall back. */
            LOGFONT fallback_logfont;

            memcpy(&fallback_logfont, logfont, sizeof(LOGFONT));
            _tcscpy(fallback_logfont.lfFaceName, XDRAW_DEFAULT_FONT_FAMILY);
            status = gdix_CreateFontFromLogfontW(c->dc, &fallback_logfont, &f);
        }

        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_font_create_with_LOGFONT: "
                     "GdipCreateFontFromLogfontW(%S) failed. [%d]",
                     logfont->lfFaceName, status);
            return NULL;
        }
        return (xdraw_font_t*) f;
    }
}

xdraw_font_t*
xdraw_font_create_with_HFONT(xdraw_canvas_t* canvas, HFONT font_handle)
{
    LOGFONT lf;
    xdraw_font_t* f;

    XDRAW_TRACE("xdraw_font_create_with_HFONT(%p)", canvas);

    if(font_handle == NULL)
        font_handle = GetStockObject(SYSTEM_FONT);

    GetObject(font_handle, sizeof(LOGFONT), &lf);
    f = xdraw_font_create_with_LOGFONT(canvas, &lf);
    if(MC_ERR(f == NULL))
        MC_TRACE("xdraw_font_create_with_HFONT: "
                 "xdraw_font_create_with_LOGFONT() failed.");
    return f;
}

void
xdraw_font_destroy(xdraw_font_t* font)
{
    XDRAW_TRACE("xdraw_font_destroy(%p)", font);

    if(d2d_dll != NULL) {
        dummy_IDWriteTextFormat* tf = (dummy_IDWriteTextFormat*) font;
        dummy_IDWriteTextFormat_Release(tf);
    } else {
        gdix_DeleteFont((dummy_GpFont*) font);
    }
}

void
xdraw_font_get_metrics(const xdraw_font_t* font, xdraw_font_metrics_t* metrics)
{
    XDRAW_TRACE("xdraw_font_get_metrics(%p)", font);

    if(MC_ERR(font == NULL)) {
        /* Treat NULL as "no font". This simplifies paint code when font
         * creation fails. */
        MC_TRACE("xdraw_font_get_metrics: font == NULL");
        metrics->em_height = 0.0f;
        metrics->cell_ascent = 0.0f;
        metrics->cell_descent = 0.0f;
        metrics->line_spacing = 0.0f;
        return;
    }

    if(d2d_dll != NULL) {
        dummy_IDWriteTextFormat* tf = (dummy_IDWriteTextFormat*) font;
        dummy_IDWriteFontCollection* fc;
        dummy_IDWriteFontFamily* ff;
        dummy_IDWriteFont* f;
        WCHAR* name;
        UINT32 name_len;
        dummy_DWRITE_FONT_WEIGHT weight;
        dummy_DWRITE_FONT_STRETCH stretch;
        dummy_DWRITE_FONT_STYLE style;
        UINT32 ix;
        BOOL exists;
        dummy_DWRITE_FONT_METRICS fm;
        float factor;
        HRESULT hr;
        BOOL ok = FALSE;

        /* Getting DWRITE_FONT_METRICS.
         * (Based on http://stackoverflow.com/a/5610139/917880) */
        name_len = dummy_IDWriteTextFormat_GetFontFamilyNameLength(tf) + 1;
        name = _malloca(name_len * sizeof(WCHAR));
        if(MC_ERR(name == NULL)) {
            MC_TRACE("xdraw_font_get_metrics: _malloca() failed.");
            goto err_malloca;
        }
        hr = dummy_IDWriteTextFormat_GetFontFamilyName(tf, name, name_len);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_font_get_metrics: "
                        "IDWriteTextFormat::GetFontFamilyName() failed.");
            goto err_GetFontFamilyName;
        }

        weight = dummy_IDWriteTextFormat_GetFontWeight(tf);
        stretch = dummy_IDWriteTextFormat_GetFontStretch(tf);
        style = dummy_IDWriteTextFormat_GetFontStyle(tf);

        hr = dummy_IDWriteTextFormat_GetFontCollection(tf, &fc);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_font_get_metrics: "
                        "IDWriteTextFormat::GetFontCollection() failed.");
            goto err_GetFontCollection;
        }

        hr = dummy_IDWriteFontCollection_FindFamilyName(fc, name, &ix, &exists);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_font_get_metrics: "
                        "IDWriteFontCollection::FindFamilyName() failed.");
            goto err_FindFamilyName;
        }

        if(MC_ERR(!exists)) {
            /* For some reason, this happens for "SYSTEM" font family on Win7. */
            MC_TRACE("xdraw_font_get_metrics: WTF? Font does not exist? (%S)", name);
            goto err_exists;
        }

        hr = dummy_IDWriteFontCollection_GetFontFamily(fc, ix, &ff);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_font_get_metrics: "
                        "IDWriteFontCollection::GetFontFamily() failed.");
            goto err_GetFontFamily;
        }

        hr = dummy_IDWriteFontFamily_GetFirstMatchingFont(ff, weight, stretch, style, &f);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_font_get_metrics: "
                        "IDWriteFontFamily::GetFirstMatchingFont() failed.");
            goto err_GetFirstMatchingFont;
        }

        dummy_IDWriteFont_GetMetrics(f, &fm);
        ok = TRUE;

        dummy_IDWriteFont_Release(f);
err_GetFirstMatchingFont:
        dummy_IDWriteFontFamily_Release(ff);
err_GetFontFamily:
err_exists:
err_FindFamilyName:
        dummy_IDWriteFontCollection_Release(fc);
err_GetFontCollection:
err_GetFontFamilyName:
        _freea(name);
err_malloca:

        metrics->em_height = dummy_IDWriteTextFormat_GetFontSize(tf);
        if(ok) {
            factor = (metrics->em_height / fm.designUnitsPerEm);
            metrics->cell_ascent = fm.ascent * factor;
            metrics->cell_descent = fm.descent * factor;
            metrics->line_spacing = (fm.ascent + fm.descent + fm.lineGap) * factor;
        } else {
            /* Something above failed. Lets invent some sane defaults. */
            metrics->cell_ascent = 1.1f * metrics->em_height;
            metrics->cell_descent = 0.1f * metrics->em_height;
            metrics->line_spacing = metrics->cell_ascent + metrics->cell_descent;
        }
        return;
    } else {
        int font_style;
        float font_size;
        void* font_family;
        UINT16 cell_ascent;
        UINT16 cell_descent;
        UINT16 em_height;
        UINT16 line_spacing;
        int status;

        gdix_GetFontSize((void*) font, &font_size);
        gdix_GetFontStyle((void*) font, &font_style);

        status = gdix_GetFamily((void*) font, &font_family);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_font_get_metrics: GdipGetFamily() failed. [%d]", status);
            return;
        }
        gdix_GetCellAscent(font_family, font_style, &cell_ascent);
        gdix_GetCellDescent(font_family, font_style, &cell_descent);
        gdix_GetEmHeight(font_family, font_style, &em_height);
        gdix_GetLineSpacing(font_family, font_style, &line_spacing);
        gdix_DeleteFontFamily(font_family);

        metrics->em_height = font_size;
        metrics->cell_ascent = font_size * cell_ascent / em_height;
        metrics->cell_descent = font_size * cell_descent / em_height;
        metrics->line_spacing = font_size * line_spacing / em_height;
    }
}

xdraw_image_t*
xdraw_image_create_from_HBITMAP(HBITMAP bmp)
{
    XDRAW_TRACE("xdraw_image_create_from_HBITMAP(%p)", bmp);

    if(d2d_dll != NULL) {
        IWICImagingFactory* wic_factory;
        IWICBitmap* b;
        HRESULT hr;

        wic_factory = (IWICImagingFactory*) xcom_init_create(
                &CLSID_WICImagingFactory, CLSCTX_INPROC, &IID_IWICImagingFactory);
        if(MC_ERR(wic_factory == NULL)) {
            MC_TRACE("xdraw_image_create_from_HBITMAP: xcom_init_create() failed.");
            goto err_xcom_init_create;
        }

        hr = IWICImagingFactory_CreateBitmapFromHBITMAP(wic_factory, bmp, NULL,
                WICBitmapIgnoreAlpha, &b);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_image_create_from_HBITMAP: "
                        "IWICImagingFactory::CreateBitmapFromHBITMAP() failed.");
            goto err_CreateBitmapFromHBITMAP;
        }

        IWICImagingFactory_Release(wic_factory);

        return (xdraw_image_t*) b;

err_CreateBitmapFromHBITMAP:
        IWICImagingFactory_Release(wic_factory);
        xcom_uninit();
err_xcom_init_create:
        return NULL;
    } else {
        dummy_GpBitmap* b;
        int status;

        status = gdix_CreateBitmapFromHBITMAP(bmp, NULL, &b);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_image_create_from_HBITMAP: "
                     "gdix_CreateBitmapFromHBITMAP() failed. [%d]", status);
            return NULL;
        }

        return (xdraw_image_t*) b;
    }
}

xdraw_image_t*
xdraw_image_load_from_file(const TCHAR* path)
{
    XDRAW_TRACE("xdraw_image_load_from_file()");

#ifndef UNICODE
    #error Non-unicode support not implemented.
#endif

    if(d2d_dll != NULL) {
        IWICBitmapSource* source;

        source = d2d_create_wic_source(path, NULL);
        if(MC_ERR(source == NULL)) {
            MC_TRACE("xdraw_image_load_from_file: "
                     "d2d_create_wic_source() failed.");
            return NULL;
        }

        return (xdraw_image_t*) source;
    } else {
        dummy_GpImage* img;
        int status;

        status = gdix_LoadImageFromFile(path, &img);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_image_load_from_file: "
                     "GdipLoadImageFromFile() failed. [%d]", status);
            return NULL;
        }

        return (xdraw_image_t*) img;
    }
}

xdraw_image_t*
xdraw_image_load_from_stream(IStream* stream)
{
    XDRAW_TRACE("xdraw_image_load_from_stream()");

    if(d2d_dll != NULL) {
        IWICBitmapSource* source;

        source = d2d_create_wic_source(NULL, stream);
        if(MC_ERR(source == NULL)) {
            MC_TRACE("xdraw_image_load_from_stream: "
                     "d2d_create_wic_source() failed.");
            return NULL;
        }

        return (xdraw_image_t*) source;
    } else {
        dummy_GpImage* img;
        int status;

        status = gdix_LoadImageFromStream(stream, &img);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_image_load_from_file: "
                     "GdipLoadImageFromFile() failed. [%d]", status);
            return NULL;
        }

        return (xdraw_image_t*) img;
    }
}

void
xdraw_image_destroy(xdraw_image_t* image)
{
    XDRAW_TRACE("xdraw_image_destroy()");

    if(d2d_dll != NULL) {
        IWICBitmapSource* source = (IWICBitmapSource*) image;
        IWICBitmapSource_Release(source);
        xcom_uninit();
    } else {
        gdix_DisposeImage((dummy_GpImage*) image);
    }
}

void
xdraw_image_get_size(xdraw_image_t* image, float* w, float* h)
{
    XDRAW_TRACE("xdraw_image_get_size(%p)", image);

    if(d2d_dll != NULL) {
        IWICBitmapSource* source = (IWICBitmapSource*) image;
        UINT iw, ih;

        IWICBitmapSource_GetSize(source, &iw, &ih);
        *w = iw;
        *h = ih;
    } else {
        dummy_GpRectF bounds;
        dummy_GpUnit unit;

        gdix_GetImageBounds((void*) image, &bounds, &unit);
        MC_ASSERT(unit == 2/*UnitPixel*/);
        *w = bounds.w;
        *h = bounds.h;
    }
}

xdraw_path_t*
xdraw_path_create(xdraw_canvas_t* canvas)
{
    XDRAW_TRACE("xdraw_path_create(%p)", canvas);

    if(d2d_dll != NULL) {
        ID2D1PathGeometry* g;
        HRESULT hr;

        EnterCriticalSection(&xdraw_lock);
        hr = ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
        LeaveCriticalSection(&xdraw_lock);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_path_create: "
                        "ID2D1Factory::CreatePathGeometry() failed.");
            return NULL;
        }

        return (xdraw_path_t*) g;
    } else {
        dummy_GpPath* p;
        int status;

        status = gdix_CreatePath(dummy_FillModeAlternate, &p);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_path_create: GdipCreatePath() failed. [%d]", status);
            return NULL;
        }

        return (xdraw_path_t*) p;
    }
}

xdraw_path_t*
xdraw_path_create_with_polygon(xdraw_canvas_t* canvas,
                               const xdraw_point_t* points, UINT n)
{
    xdraw_path_t* path;
    xdraw_path_sink_t sink;
    UINT i;

    XDRAW_TRACE("xdraw_path_create_with_polygon(%p, %u points)", canvas, n);

    MC_ASSERT(n > 0);

    path = xdraw_path_create(canvas);
    if(MC_ERR(path == NULL)) {
        MC_TRACE("xdraw_path_create_with_polygon: xdraw_path_create() failed.");
        return NULL;
    }

    if(MC_ERR(xdraw_path_open_sink(&sink, path) != 0)) {
        MC_TRACE("xdraw_path_create_with_polygon: xdraw_path_open_sink() failed.");
        xdraw_path_destroy(path);
        return NULL;
    }

    xdraw_path_begin_figure(&sink, &points[0]);
    for(i = 1; i < n; i++)
        xdraw_path_add_line(&sink, &points[i]);
    xdraw_path_end_figure(&sink, TRUE);
    xdraw_path_close_sink(&sink);

    return path;
}

void
xdraw_path_destroy(xdraw_path_t* path)
{
    XDRAW_TRACE("xdraw_path_destroy(%p)", path);

    if(d2d_dll != NULL) {
        ID2D1PathGeometry* g = (ID2D1PathGeometry*) path;
        ID2D1PathGeometry_Release(g);
    } else {
        gdix_DeletePath((dummy_GpPath*) path);
    }
}

int
xdraw_path_open_sink(xdraw_path_sink_t* sink, xdraw_path_t* path)
{
    XDRAW_TRACE("xdraw_path_open_sink(%p, %p)", sink, path);

    if(d2d_dll != NULL) {
        ID2D1PathGeometry* g = (ID2D1PathGeometry*) path;
        ID2D1GeometrySink* s;
        HRESULT hr;

        hr = ID2D1PathGeometry_Open(g, &s);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_path_open_sink: ID2D1PathGeometry::Open() failed.");
            return -1;
        }

        sink->data = (void*) s;
        return 0;
    } else {
        sink->data = (void*) path;
        return 0;
    }
}

void
xdraw_path_close_sink(xdraw_path_sink_t* sink)
{
    XDRAW_TRACE("xdraw_path_close_sink(%p)", sink);

    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink->data;
        ID2D1GeometrySink_Close(s);
        ID2D1GeometrySink_Release(s);
    } else {
        /* noop */
    }
}

void
xdraw_path_begin_figure(xdraw_path_sink_t* sink, const xdraw_point_t* start_point)
{
    XDRAW_TRACE("xdraw_path_begin_figure(%p)", sink);

    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink->data;
        ID2D1GeometrySink_BeginFigure(s, *((D2D1_POINT_2F*) start_point),
                                      D2D1_FIGURE_BEGIN_FILLED);
    } else {
        gdix_StartPathFigure(sink->data);
    }

    sink->end.x = start_point->x;
    sink->end.y = start_point->y;
}

void
xdraw_path_end_figure(xdraw_path_sink_t* sink, BOOL closed_end)
{
    XDRAW_TRACE("xdraw_path_end_figure(%p)", sink);

    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink->data;
        ID2D1GeometrySink_EndFigure(s, (closed_end ? D2D1_FIGURE_END_CLOSED
                                                   : D2D1_FIGURE_END_OPEN));
    } else {
        if(closed_end)
            gdix_ClosePathFigure(sink->data);
    }
}

void
xdraw_path_add_line(xdraw_path_sink_t* sink, const xdraw_point_t* end_point)
{
    XDRAW_TRACE("xdraw_path_add_line(%p)", sink);

    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink->data;
        ID2D1GeometrySink_AddLine(s, *((D2D1_POINT_2F*) end_point));
    } else {
        gdix_AddPathLine(sink->data, sink->end.x, sink->end.y,
                         end_point->x, end_point->y);
    }

    sink->end.x = end_point->x;
    sink->end.y = end_point->y;
}

void
xdraw_path_add_arc(xdraw_path_sink_t* sink, const xdraw_point_t* center,
                   float sweep_angle)
{
    float cx = center->x;
    float cy = center->y;
    float ax = sink->end.x;
    float ay = sink->end.y;
    float xdiff = ax - cx;
    float ydiff = ay - cy;
    float r;
    float base_angle;

    XDRAW_TRACE("xdraw_path_add_arc(%p)", sink);

    r = sqrtf(xdiff * xdiff + ydiff * ydiff);

    /* Avoid undefined case for atan2f().
     * Note we check for range (-Epsion, +Epsilon) instead of exact float zero.
     */
    if(r < 0.001f)
        return;

    base_angle = atan2f(ydiff, xdiff) * (180.0f / MC_PIf);

    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink->data;
        xdraw_circle_t circle = { cx, cy, r };
        D2D1_ARC_SEGMENT arc_seg;

        d2d_setup_arc_segment(&arc_seg, &circle, base_angle, sweep_angle);
        ID2D1GeometrySink_AddArc(s, &arc_seg);
        sink->end.x = arc_seg.point.x;
        sink->end.y = arc_seg.point.y;
    } else {
        float d = 2.0f * r;
        float sweep_rads = (base_angle + sweep_angle) * (MC_PIf / 180.0f);

        gdix_AddPathArc(sink->data, cx - r, cy - r, d, d, base_angle, sweep_angle);
        sink->end.x = cx + r * cosf(sweep_rads);
        sink->end.y = cy + r * sinf(sweep_rads);
    }
}

void
xdraw_clear(xdraw_canvas_t* canvas, xdraw_color_t color)
{
    XDRAW_TRACE("xdraw_clear(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        D2D_COLOR_F clr;

        clr.r = XDRAW_REDVALUE(color) / 255.0f;
        clr.g = XDRAW_GREENVALUE(color) / 255.0f;
        clr.b = XDRAW_BLUEVALUE(color) / 255.0f;
        clr.a = XDRAW_ALPHAVALUE(color) / 255.0f;

        ID2D1RenderTarget_Clear(c->target, &clr);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_GraphicsClear(c->graphics, color);
    }
}

void
xdraw_draw_arc(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
               const xdraw_circle_t* circle, float base_angle,
               float sweep_angle, float stroke_width)
{
    XDRAW_TRACE("xdraw_draw_arc(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(circle, base_angle, sweep_angle, FALSE);
        if(MC_ERR(g == NULL)) {
            MC_TRACE("xdraw_draw_arc: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_DrawGeometry(c->target, g, b, stroke_width, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        float d = 2.0f * circle->r;
        gdix_SetPenBrushFill(c->pen, (void*) brush);
        gdix_SetPenWidth(c->pen, stroke_width);
        gdix_DrawArc(c->graphics, c->pen, circle->x - circle->r,
                     circle->y - circle->r, d, d, base_angle, sweep_angle);
    }
}

void
xdraw_draw_line(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                const xdraw_line_t* line, float stroke_width)
{
    XDRAW_TRACE("xdraw_draw_line(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        D2D1_POINT_2F pt0 = { line->x0, line->y0 };
        D2D1_POINT_2F pt1 = { line->x1, line->y1 };
        ID2D1RenderTarget_DrawLine(c->target, pt0, pt1, b, stroke_width, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_SetPenBrushFill(c->pen, (void*) brush);
        gdix_SetPenWidth(c->pen, stroke_width);
        gdix_DrawLine(c->graphics, c->pen, line->x0, line->y0, line->x1, line->y1);
    }
}

void
xdraw_draw_path(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                const xdraw_path_t* path, float stroke_width)
{
    XDRAW_TRACE("xdraw_draw_path(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Geometry* g = (ID2D1Geometry*) path;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1RenderTarget_DrawGeometry(c->target, g, b, 1.0f, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_SetPenBrushFill(c->pen, (void*) brush);
        gdix_SetPenWidth(c->pen, stroke_width);
        gdix_DrawPath(c->graphics, (void*) c->pen, (void*) path);
    }
}

void
xdraw_draw_pie(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
               const xdraw_circle_t* circle, float base_angle,
               float sweep_angle, float stroke_width)
{
    XDRAW_TRACE("xdraw_draw_pie(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(circle, base_angle, sweep_angle, TRUE);
        if(MC_ERR(g == NULL)) {
            MC_TRACE("xdraw_draw_pie: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_DrawGeometry(c->target, g, b, stroke_width, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        float d = 2.0f * circle->r;
        gdix_SetPenBrushFill(c->pen, (void*) brush);
        gdix_SetPenWidth(c->pen, stroke_width);
        gdix_DrawPie(c->graphics, c->pen, circle->x - circle->r,
                     circle->y - circle->r, d, d, base_angle, sweep_angle);
    }
}

void
xdraw_draw_rect(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                const xdraw_rect_t* rect, float stroke_width)
{
    XDRAW_TRACE("xdraw_draw_rect(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1RenderTarget_DrawRectangle(c->target, (const D2D1_RECT_F*) rect,
                                        b, stroke_width, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_SetPenBrushFill(c->pen, (void*) brush);
        gdix_SetPenWidth(c->pen, stroke_width);
        gdix_DrawRectangle(c->graphics, c->pen, rect->x0, rect->y0,
                           rect->x1 - rect->x0, rect->y1 - rect->y0);
    }
}

void
xdraw_fill_circle(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                  const xdraw_circle_t* circle)
{
    XDRAW_TRACE("xdraw_fill_circle(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        D2D1_ELLIPSE e = { { circle->x, circle->y }, circle->r, circle->r };
        ID2D1RenderTarget_FillEllipse(c->target, &e, b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        float d = 2.0f * circle->r;
        gdix_FillEllipse(c->graphics, (void*) brush, circle->x - circle->r,
                circle->y - circle->r, d, d);
    }
}

void
xdraw_fill_path(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                const xdraw_path_t* path)
{
    XDRAW_TRACE("xdraw_fill_path(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Geometry* g = (ID2D1Geometry*) path;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1RenderTarget_FillGeometry(c->target, g, b, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_FillPath(c->graphics, (void*) brush, (void*) path);
    }
}

void
xdraw_fill_pie(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
               const xdraw_circle_t* circle, float base_angle, float sweep_angle)
{
    XDRAW_TRACE("xdraw_fill_pie(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1Geometry* g;

        g = d2d_create_arc_geometry(circle, base_angle, sweep_angle, TRUE);
        if(MC_ERR(g == NULL)) {
            MC_TRACE("xdraw_fill_pie: d2d_create_arc_geometry() failed.");
            return;
        }

        ID2D1RenderTarget_FillGeometry(c->target, g, b, NULL);
        ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        float d = 2.0f * circle->r;
        gdix_FillPie(c->graphics, (void*) brush, circle->x - circle->r,
                     circle->y - circle->r, d, d, base_angle, sweep_angle);
    }
}

void
xdraw_fill_rect(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                const xdraw_rect_t* rect)
{
    XDRAW_TRACE("xdraw_fill_rect(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        ID2D1RenderTarget_FillRectangle(c->target, (const D2D1_RECT_F*) rect, b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_FillRectangle(c->graphics, (void*) brush, rect->x0, rect->y0,
                           rect->x1 - rect->x0, rect->y1 - rect->y0);
    }
}

void
xdraw_blit_image(xdraw_canvas_t* canvas, const xdraw_image_t* image,
                 const xdraw_rect_t* dst, const xdraw_rect_t* src)
{
    XDRAW_TRACE("xdraw_blit_image(%p)", canvas);

    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        IWICBitmapSource* source = (IWICBitmapSource*) image;
        ID2D1Bitmap* b;
        HRESULT hr;
        /* Compensation for translation in xdraw_canvas_transform_reset() */
        D2D1_RECT_F dst_fix = { dst->x0 - 0.5f, dst->y0 - 0.5f,
                                dst->x1 - 0.5f, dst->y1 - 0.5f };

        hr = ID2D1RenderTarget_CreateBitmapFromWicBitmap(c->target, source, NULL, &b);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_blit_image: "
                        "ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed.");
            return;
        }
        ID2D1RenderTarget_DrawBitmap(c->target, b, &dst_fix, 1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, (D2D1_RECT_F*) src);
        ID2D1Bitmap_Release(b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_DrawImageRectRect(c->graphics, (void*) image,
                dst->x0, dst->y0, dst->x1 - dst->x0, dst->y1 - dst->y0,
                src->x0, src->y0, src->x1 - src->x0, src->y1 - src->y0,
                2/*UnitPixel*/, NULL, NULL, NULL);
    }
}

void
xdraw_blit_HICON(xdraw_canvas_t* canvas, HICON icon, const xdraw_rect_t* rect)
{
    xdraw_rect_t source_rect = { 0, 0, rect->x1 - rect->x0, rect->y1 - rect->y0 };

    XDRAW_TRACE("xdraw_blit_HICON(%p, %p)", canvas, icon);

    if(d2d_dll != NULL) {
        IWICImagingFactory* wic_factory;
        IWICBitmap* wic_bitmap;
        IWICFormatConverter* wic_converter;
        HRESULT hr;

        wic_factory = (IWICImagingFactory*) xcom_init_create(
                &CLSID_WICImagingFactory, CLSCTX_INPROC, &IID_IWICImagingFactory);
        if(MC_ERR(wic_factory == NULL)) {
            MC_TRACE("xdraw_blit_HICON: xcom_init_create() failed.");
            goto err_xcom_init_create;
        }

        hr = IWICImagingFactory_CreateBitmapFromHICON(wic_factory, icon, &wic_bitmap);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_blit_HICON: "
                        "IWICImagingFactory::CreateBitmapFromHICON() failed.");
            goto err_CreateBitmapFromHICON;
        }

        hr = IWICImagingFactory_CreateFormatConverter(wic_factory, &wic_converter);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_blit_HICON: "
                        "IWICImagingFactory::CreateFormatConverter() failed.");
            goto err_CreateFormatConverter;
        }

        hr = IWICFormatConverter_Initialize(wic_converter,
                (IWICBitmapSource*) wic_bitmap, &GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdraw_blit_HICON: "
                        "IWICFormatConverter::Initialize() failed.");
            goto err_Initialize;
        }

        xdraw_blit_image(canvas, (xdraw_image_t*) wic_converter, rect, &source_rect);

err_Initialize:
        IWICFormatConverter_Release(wic_converter);
err_CreateFormatConverter:
        IWICBitmap_Release(wic_bitmap);
err_CreateBitmapFromHICON:
        IWICImagingFactory_Release(wic_factory);
        xcom_uninit();
err_xcom_init_create:
        ;  /* noop */
    } else {
        dummy_GpBitmap* b;
        int status;

        status = gdix_CreateBitmapFromHICON(icon, &b);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_blit_HICON: gdix_CreateBitmapFromHICON() failed. "
                     "[%d]", status);
            return;
        }
        xdraw_blit_image(canvas, (xdraw_image_t*) b, rect, &source_rect);
        gdix_DisposeImage(b);
    }
}

void
xdraw_draw_string(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                  const xdraw_rect_t* rect, const TCHAR* str, int len,
                  const xdraw_brush_t* brush, DWORD flags)
{
    XDRAW_TRACE("xdraw_draw_string(%p, %.*s)", canvas, len, str);

    if(d2d_dll != NULL) {
        D2D1_POINT_2F origin = { rect->x0, rect->y0 };
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        dummy_IDWriteTextFormat* tf = (dummy_IDWriteTextFormat*) font;
        dummy_IDWriteTextLayout* layout;

        layout = d2d_create_text_layout(tf, rect, str, len, flags);
        if(MC_ERR(layout == NULL)) {
            MC_TRACE("xdraw_draw_string: d2d_create_text_layout() failed.");
            return;
        }

        ID2D1RenderTarget_DrawTextLayout(c->target, origin,
                (IDWriteTextLayout*) layout, b,
                (flags & XDRAW_STRING_CLIP) ? D2D1_DRAW_TEXT_OPTIONS_CLIP : 0);

        dummy_IDWriteTextLayout_Release(layout);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        dummy_GpRectF r = { rect->x0, rect->y0, rect->x1 - rect->x0, rect->y1 - rect->y0 };
        dummy_GpFont* f = (dummy_GpFont*) font;
        dummy_GpBrush* b = (dummy_GpBrush*) brush;

        gdix_canvas_apply_string_flags(c, flags);
        gdix_DrawString(c->graphics, str, len, f, &r, c->string_format, b);
    }
}

void
xdraw_measure_string(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                     const xdraw_rect_t* rect, const TCHAR* str, int len,
                     xdraw_string_measure_t* measure, DWORD flags)
{
    XDRAW_TRACE("xdraw_measure_string(%p, %.*s)", canvas, len, str);

    if(d2d_dll != NULL) {
        dummy_IDWriteTextFormat* tf = (dummy_IDWriteTextFormat*) font;
        dummy_IDWriteTextLayout* layout;
        dummy_DWRITE_TEXT_METRICS tm;

        layout = d2d_create_text_layout(tf, rect, str, len, flags);
        if(MC_ERR(layout == NULL)) {
            MC_TRACE("xdraw_measure_string: d2d_create_text_layout() failed.");
            return;
        }

        dummy_IDWriteTextLayout_GetMetrics(layout, &tm);

        measure->bound.x0 = rect->x0 + tm.left;
        measure->bound.y0 = rect->y0 + tm.top;
        measure->bound.x1 = measure->bound.x0 + tm.width;
        measure->bound.y1 = measure->bound.y0 + tm.height;

        dummy_IDWriteTextLayout_Release(layout);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        dummy_GpRectF r = { rect->x0, rect->y0, rect->x1 - rect->x0, rect->y1 - rect->y0 };
        dummy_GpFont* f = (dummy_GpFont*) font;
        dummy_GpRectF br;

        gdix_canvas_apply_string_flags(c, flags);
        gdix_MeasureString(c->graphics, str, len, f, &r, c->string_format,
                           &br, NULL, NULL);

        measure->bound.x0 = br.x;
        measure->bound.y0 = br.y;
        measure->bound.x1 = br.x + br.w;
        measure->bound.y1 = br.y + br.h;
    }
}

float
xdraw_measure_string_width(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                           const TCHAR* str)
{
    static const xdraw_rect_t infinite_rect = { 0, 0, FLT_MAX, FLT_MAX };
    xdraw_string_measure_t measure;

    XDRAW_TRACE("xdraw_measure_string_width(%p, %s)", canvas, str);

    xdraw_measure_string(canvas, font, &infinite_rect, str, _tcslen(str),
                         &measure, XDRAW_STRING_LEFT | XDRAW_STRING_NOWRAP);
    return (measure.bound.x1 - measure.bound.x0);
}


/*******************************
 ***  Module Initialization  ***
 *******************************/

int
xdraw_init_module(void)
{
    int ret = -1;

    /* Direct2D was added into Windows Vista/Server2008 with some SP and/or
     * updates. On older system versions, there is no need to even try it. */
    if(mc_win_version >= MC_WIN_VISTA) {
        ret = d2d_init();
        if(MC_ERR(ret != 0))
            MC_TRACE("xdraw_init_module: d2d_init() failed.");
    }

    /* Fall back to GDI+. */
    if(ret != 0) {
        ret = gdix_init();
        if(MC_ERR(ret != 0))
            MC_TRACE("xdraw_init_module: gdix_init() failed.");
    }

    if(MC_ERR(ret != 0)) {
        /* Ouch. Both drivers failed or are not available. This should not
         * normally happen unless on Win2K, if the application does not provide
         * distributable version of GDIPLUS.DLL. */
        MC_TRACE("xdraw_init_module: No suitable driver.");
        return -1;
    }

    InitializeCriticalSection(&xdraw_lock);

    XDRAW_TRACE("xdraw_init_module: Using %s.", (d2d_dll != NULL ? "Direct2D" : "GDI+"));
    return 0;
}

void
xdraw_fini_module(void)
{
    DeleteCriticalSection(&xdraw_lock);

    if(d2d_dll != NULL)
        d2d_fini();
    else
        gdix_fini();
}

