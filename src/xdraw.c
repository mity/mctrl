/*
 * Copyright (c) 2015 Martin Mitas
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


#include "xdraw.h"
#include "xcom.h"
#include "doublebuffer.h"

#include <math.h>


/* Uncomment this to have more verbose traces from this module. */
/*#define XDRAW_DEBUG     1*/

#ifdef XDRAW_DEBUG
    #define XDRAW_TRACE           MC_TRACE
#else
    #define XDRAW_TRACE(...)      do {} while(0)
#endif


static CRITICAL_SECTION xdraw_lock;



/**************************
 ***  Dummy <dwrite.h>  ***
 **************************/

/* <dwrite.h> as in MS SDK is unfortunately usable only from C++. Therefore we
 * declare needed interfaces manually here. Why? Just tell me why? */


/* LOGFONT::lfWeight and DWRITE_FONT_WEIGHT have the same values for each
 * font weight. */
typedef LONG dw_DWRITE_FONT_WEIGHT;

typedef enum dw_DWRITE_FONT_STYLE_tag dw_DWRITE_FONT_STYLE;
enum dw_DWRITE_FONT_STYLE_tag {
    dw_DWRITE_FONT_STYLE_NORMAL = 0,
    dw_DWRITE_FONT_STYLE_OBLIQUE,
    dw_DWRITE_FONT_STYLE_ITALIC
};

typedef enum dw_DWRITE_FONT_STRETCH_tag dw_DWRITE_FONT_STRETCH;
enum dw_DWRITE_FONT_STRETCH_tag {
    dw_DWRITE_FONT_STRETCH_UNDEFINED = 0,
    dw_DWRITE_FONT_STRETCH_ULTRA_CONDENSED,
    dw_DWRITE_FONT_STRETCH_EXTRA_CONDENSED,
    dw_DWRITE_FONT_STRETCH_CONDENSED,
    dw_DWRITE_FONT_STRETCH_SEMI_CONDENSED,
    dw_DWRITE_FONT_STRETCH_NORMAL,
    dw_DWRITE_FONT_STRETCH_MEDIUM = dw_DWRITE_FONT_STRETCH_NORMAL,
    dw_DWRITE_FONT_STRETCH_SEMI_EXPANDED,
    dw_DWRITE_FONT_STRETCH_EXPANDED,
    dw_DWRITE_FONT_STRETCH_EXTRA_EXPANDED,
    dw_DWRITE_FONT_STRETCH_ULTRA_EXPANDED
};

typedef enum dw_DWRITE_WORD_WRAPPING_tag dw_DWRITE_WORD_WRAPPING;
enum dw_DWRITE_WORD_WRAPPING_tag {
    dw_DWRITE_WORD_WRAPPING_WRAP = 0,
    dw_DWRITE_WORD_WRAPPING_NO_WRAP
};

typedef enum dw_DWRITE_TEXT_ALIGNMENT_tag dw_DWRITE_TEXT_ALIGNMENT;
enum dw_DWRITE_TEXT_ALIGNMENT_tag {
    dw_DWRITE_TEXT_ALIGNMENT_LEADING = 0,
    dw_DWRITE_TEXT_ALIGNMENT_TRAILING,
    dw_DWRITE_TEXT_ALIGNMENT_CENTER
};

typedef struct dw_DWRITE_FONT_METRICS_tag dw_DWRITE_FONT_METRICS;
struct dw_DWRITE_FONT_METRICS_tag {
    UINT16 designUnitsPerEm;
    UINT16 ascent;
    UINT16 descent;
    INT16 lineGap;
    UINT16 capHeight;
    UINT16 xHeight;
    INT16 underlinePosition;
    UINT16 underlineThickness;
    INT16 strikethroughPosition;
    UINT16 strikethroughThickness;
};

typedef struct dw_DWRITE_TEXT_METRICS_tag dw_DWRITE_TEXT_METRICS;
struct dw_DWRITE_TEXT_METRICS_tag {
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT widthIncludingTrailingWhitespace;
    FLOAT height;
    FLOAT layoutWidth;
    FLOAT layoutHeight;
    UINT32 maxBidiReorderingDepth;
    UINT32 lineCount;
};


static const GUID dw_IID_IDWriteFactory = {0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};


typedef struct dw_IDWriteFactory_tag        dw_IDWriteFactory;
typedef struct dw_IDWriteFont_tag           dw_IDWriteFont;
typedef struct dw_IDWriteFontCollection_tag dw_IDWriteFontCollection;
typedef struct dw_IDWriteFontFamily_tag     dw_IDWriteFontFamily;
typedef struct dw_IDWriteTextFormat_tag     dw_IDWriteTextFormat;
typedef struct dw_IDWriteTextLayout_tag     dw_IDWriteTextLayout;


typedef struct dw_IDWriteFactoryVtbl_tag dw_IDWriteFactoryVtbl;
struct dw_IDWriteFactoryVtbl_tag {
    /* IUknown methods */
    STDMETHOD(QueryInterface)(dw_IDWriteFactory*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dw_IDWriteFactory*);
    STDMETHOD_(ULONG, Release)(dw_IDWriteFactory*);

    /* IDWriteFactory methods */
    STDMETHOD(dummy_GetSystemFontCollection)(void);
    STDMETHOD(dummy_CreateCustomFontCollection)(void);
    STDMETHOD(dummy_RegisterFontCollectionLoader)(void);
    STDMETHOD(dummy_UnregisterFontCollectionLoader)(void);
    STDMETHOD(dummy_CreateFontFileReference)(void);
    STDMETHOD(dummy_CreateCustomFontFileReference)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
    STDMETHOD(dummy_CreateRenderingParams)(void);
    STDMETHOD(dummy_CreateMonitorRenderingParams)(void);
    STDMETHOD(dummy_CreateCustomRenderingParams)(void);
    STDMETHOD(dummy_RegisterFontFileLoader)(void);
    STDMETHOD(dummy_UnregisterFontFileLoader)(void);
    STDMETHOD(CreateTextFormat)(dw_IDWriteFactory*, WCHAR const*, void*,
            dw_DWRITE_FONT_WEIGHT, dw_DWRITE_FONT_STYLE, dw_DWRITE_FONT_STRETCH,
            FLOAT, WCHAR const*, dw_IDWriteTextFormat**);
    STDMETHOD(dummy_CreateTypography)(void);
    STDMETHOD(dummy_GetGdiInterop)(void);
    STDMETHOD(CreateTextLayout)(dw_IDWriteFactory*, WCHAR const*, UINT32,
            dw_IDWriteTextFormat*, FLOAT, FLOAT, dw_IDWriteTextLayout**);
    STDMETHOD(dummy_CreateGdiCompatibleTextLayout)(void);
    STDMETHOD(dummy_CreateEllipsisTrimmingSign)(void);
    STDMETHOD(dummy_CreateTextAnalyzer)(void);
    STDMETHOD(dummy_CreateNumberSubstitution)(void);
    STDMETHOD(dummy_CreateGlyphRunAnalysis)(void);
};

struct dw_IDWriteFactory_tag {
    dw_IDWriteFactoryVtbl* vtbl;
};

#define IDWriteFactory_QueryInterface(self,a,b)                 (self)->vtbl->QueryInterface(self,a,b)
#define IDWriteFactory_AddRef(self)                             (self)->vtbl->AddRef(self)
#define IDWriteFactory_Release(self)                            (self)->vtbl->Release(self)
#define IDWriteFactory_CreateTextFormat(self,a,b,c,d,e,f,g,h)   (self)->vtbl->CreateTextFormat(self,a,b,c,d,e,f,g,h)
#define IDWriteFactory_CreateTextLayout(self,a,b,c,d,e,f)       (self)->vtbl->CreateTextLayout(self,a,b,c,d,e,f)


typedef struct dw_IDWriteFontVtbl_tag dw_IDWriteFontVtbl;
struct dw_IDWriteFontVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dw_IDWriteFont*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dw_IDWriteFont*);
    STDMETHOD_(ULONG, Release)(dw_IDWriteFont*);

    /* IDWriteFont methods */
    STDMETHOD(dummy_GetFontFamily)(void);
    STDMETHOD(dummy_GetWeight)(void);
    STDMETHOD(dummy_GetStretch)(void);
    STDMETHOD(dummy_GetStyle)(void);
    STDMETHOD(dummy_IsSymbolFont)(void);
    STDMETHOD(dummy_GetFaceNames)(void);
    STDMETHOD(dummy_GetInformationalStrings)(void);
    STDMETHOD(dummy_GetSimulations)(void);
    STDMETHOD_(void, GetMetrics)(dw_IDWriteFont*, dw_DWRITE_FONT_METRICS*);
    STDMETHOD(dummy_HasCharacter)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
};

struct dw_IDWriteFont_tag {
    dw_IDWriteFontVtbl* vtbl;
};

#define IDWriteFont_QueryInterface(self,a,b)                    (self)->vtbl->QueryInterface(self,a,b)
#define IDWriteFont_AddRef(self)                                (self)->vtbl->AddRef(self)
#define IDWriteFont_Release(self)                               (self)->vtbl->Release(self)
#define IDWriteFont_GetMetrics(self,a)                          (self)->vtbl->GetMetrics(self,a)


typedef struct dw_IDWriteFontCollectionVtbl_tag dw_IDWriteFontCollectionVtbl;
struct dw_IDWriteFontCollectionVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dw_IDWriteFontCollection*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dw_IDWriteFontCollection*);
    STDMETHOD_(ULONG, Release)(dw_IDWriteFontCollection*);

    /* IDWriteFontCollection methods */
    STDMETHOD(dummy_GetFontFamilyCount)(void);
    STDMETHOD(GetFontFamily)(dw_IDWriteFontCollection*, UINT32, dw_IDWriteFontFamily**);
    STDMETHOD(FindFamilyName)(dw_IDWriteFontCollection*, WCHAR*, UINT32*, BOOL*);
    STDMETHOD(dummy_GetFontFromFontFace)(void);
};

struct dw_IDWriteFontCollection_tag {
    dw_IDWriteFontCollectionVtbl* vtbl;
};

#define IDWriteFontCollection_QueryInterface(self,a,b)          (self)->vtbl->QueryInterface(self,a,b)
#define IDWriteFontCollection_AddRef(self)                      (self)->vtbl->AddRef(self)
#define IDWriteFontCollection_Release(self)                     (self)->vtbl->Release(self)
#define IDWriteFontCollection_GetFontFamily(self,a,b)           (self)->vtbl->GetFontFamily(self,a,b)
#define IDWriteFontCollection_FindFamilyName(self,a,b,c)        (self)->vtbl->FindFamilyName(self,a,b,c)


typedef struct dw_IDWriteFontFamilyVtbl_tag dw_IDWriteFontFamilyVtbl;
struct dw_IDWriteFontFamilyVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dw_IDWriteFontFamily*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dw_IDWriteFontFamily*);
    STDMETHOD_(ULONG, Release)(dw_IDWriteFontFamily*);

    /* IDWriteFontList methods */
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontCount)(void);
    STDMETHOD(dummy_GetFont)(void);

    /* IDWriteFontFamily methods */
    STDMETHOD(dummy_GetFamilyNames)(void);
    STDMETHOD(GetFirstMatchingFont)(dw_IDWriteFontFamily*, dw_DWRITE_FONT_WEIGHT,
            dw_DWRITE_FONT_STRETCH, dw_DWRITE_FONT_STYLE, dw_IDWriteFont**);
    STDMETHOD(dummy_GetMatchingFonts)(void);
};

struct dw_IDWriteFontFamily_tag {
    dw_IDWriteFontFamilyVtbl* vtbl;
};

#define IDWriteFontFamily_QueryInterface(self,a,b)              (self)->vtbl->QueryInterface(self,a,b)
#define IDWriteFontFamily_AddRef(self)                          (self)->vtbl->AddRef(self)
#define IDWriteFontFamily_Release(self)                         (self)->vtbl->Release(self)
#define IDWriteFontFamily_GetFirstMatchingFont(self,a,b,c,d)    (self)->vtbl->GetFirstMatchingFont(self,a,b,c,d)


typedef struct dw_IDWriteTextFormatVtbl_tag dw_IDWriteTextFormatVtbl;
struct dw_IDWriteTextFormatVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dw_IDWriteTextFormat*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dw_IDWriteTextFormat*);
    STDMETHOD_(ULONG, Release)(dw_IDWriteTextFormat*);

    /* IDWriteTextFormat methods */
    STDMETHOD(dummy_SetTextAlignment)(void);
    STDMETHOD(dummy_SetParagraphAlignment)(void);
    STDMETHOD(dummy_SetWordWrapping)(void);
    STDMETHOD(dummy_SetReadingDirection)(void);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(dummy_SetTrimming)(void);
    STDMETHOD(dummy_SetLineSpacing)(void);
    STDMETHOD(dummy_GetTextAlignment)(void);
    STDMETHOD(dummy_GetParagraphAlignment)(void);
    STDMETHOD(dummy_GetWordWrapping)(void);
    STDMETHOD(dummy_GetReadingDirection)(void);
    STDMETHOD(dummy_GetFlowDirection)(void);
    STDMETHOD(dummy_GetIncrementalTabStop)(void);
    STDMETHOD(dummy_GetTrimming)(void);
    STDMETHOD(dummy_GetLineSpacing)(void);
    STDMETHOD(GetFontCollection)(dw_IDWriteTextFormat*, dw_IDWriteFontCollection**);
    STDMETHOD_(UINT32, GetFontFamilyNameLength)(dw_IDWriteTextFormat*);
    STDMETHOD(GetFontFamilyName)(dw_IDWriteTextFormat*, WCHAR*, UINT32);
    STDMETHOD_(dw_DWRITE_FONT_WEIGHT, GetFontWeight)(dw_IDWriteTextFormat*);
    STDMETHOD_(dw_DWRITE_FONT_STYLE, GetFontStyle)(dw_IDWriteTextFormat*);
    STDMETHOD_(dw_DWRITE_FONT_STRETCH, GetFontStretch)(dw_IDWriteTextFormat*);
    STDMETHOD_(FLOAT, GetFontSize)(dw_IDWriteTextFormat*);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);
};

struct dw_IDWriteTextFormat_tag {
    dw_IDWriteTextFormatVtbl* vtbl;
};

#define IDWriteTextFormat_QueryInterface(self,a,b)              (self)->vtbl->QueryInterface(self,a,b)
#define IDWriteTextFormat_AddRef(self)                          (self)->vtbl->AddRef(self)
#define IDWriteTextFormat_Release(self)                         (self)->vtbl->Release(self)
#define IDWriteTextFormat_GetFontCollection(self,a)             (self)->vtbl->GetFontCollection(self,a)
#define IDWriteTextFormat_GetFontFamilyNameLength(self)         (self)->vtbl->GetFontFamilyNameLength(self)
#define IDWriteTextFormat_GetFontFamilyName(self,a,b)           (self)->vtbl->GetFontFamilyName(self,a,b)
#define IDWriteTextFormat_GetFontWeight(self)                   (self)->vtbl->GetFontWeight(self)
#define IDWriteTextFormat_GetFontStretch(self)                  (self)->vtbl->GetFontStretch(self)
#define IDWriteTextFormat_GetFontStyle(self)                    (self)->vtbl->GetFontStyle(self)
#define IDWriteTextFormat_GetFontSize(self)                     (self)->vtbl->GetFontSize(self)


typedef struct dw_IDWriteTextLayoutVtbl_tag dw_IDWriteTextLayoutVtbl;
struct dw_IDWriteTextLayoutVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dw_IDWriteTextLayout*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dw_IDWriteTextLayout*);
    STDMETHOD_(ULONG, Release)(dw_IDWriteTextLayout*);

    /* IDWriteTextFormat methods */
    STDMETHOD(SetTextAlignment)(dw_IDWriteTextLayout*, dw_DWRITE_TEXT_ALIGNMENT);
    STDMETHOD(dummy_SetParagraphAlignment)(void);
    STDMETHOD(SetWordWrapping)(dw_IDWriteTextLayout*, dw_DWRITE_WORD_WRAPPING);
    STDMETHOD(dummy_SetReadingDirection)(void);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(dummy_SetTrimming)(void);
    STDMETHOD(dummy_SetLineSpacing)(void);
    STDMETHOD(dummy_GetTextAlignment)(void);
    STDMETHOD(dummy_GetParagraphAlignment)(void);
    STDMETHOD(dummy_GetWordWrapping)(void);
    STDMETHOD(dummy_GetReadingDirection)(void);
    STDMETHOD(dummy_GetFlowDirection)(void);
    STDMETHOD(dummy_GetIncrementalTabStop)(void);
    STDMETHOD(dummy_GetTrimming)(void);
    STDMETHOD(dummy_GetLineSpacing)(void);
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength)(void);
    STDMETHOD(dummy_GetFontFamilyName)(void);
    STDMETHOD(dummy_GetFontWeight)(void);
    STDMETHOD(dummy_GetFontStyle)(void);
    STDMETHOD(dummy_GetFontStretch)(void);
    STDMETHOD(dummy_GetFontSize)(void);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);

    /* IDWriteTextLayout methods */
    STDMETHOD(dummy_SetMaxWidth)(void);
    STDMETHOD(dummy_SetMaxHeight)(void);
    STDMETHOD(dummy_SetFontCollection)(void);
    STDMETHOD(dummy_SetFontFamilyName)(void);
    STDMETHOD(dummy_SetFontWeight)(void);
    STDMETHOD(dummy_SetFontStyle)(void);
    STDMETHOD(dummy_SetFontStretch)(void);
    STDMETHOD(dummy_SetFontSize)(void);
    STDMETHOD(dummy_SetUnderline)(void);
    STDMETHOD(dummy_SetStrikethrough)(void);
    STDMETHOD(dummy_SetDrawingEffect)(void);
    STDMETHOD(dummy_SetInlineObject)(void);
    STDMETHOD(dummy_SetTypography)(void);
    STDMETHOD(dummy_SetLocaleName)(void);
    STDMETHOD(dummy_GetMaxWidth)(void);
    STDMETHOD(dummy_GetMaxHeight)(void);
    STDMETHOD(dummy_GetFontCollection2)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength2)(void);
    STDMETHOD(dummy_GetFontFamilyName2)(void);
    STDMETHOD(dummy_GetFontWeight2)(void);
    STDMETHOD(dummy_GetFontStyle2)(void);
    STDMETHOD(dummy_GetFontStretch2)(void);
    STDMETHOD(dummy_GetFontSize2)(void);
    STDMETHOD(dummy_GetUnderline)(void);
    STDMETHOD(dummy_GetStrikethrough)(void);
    STDMETHOD(dummy_GetDrawingEffect)(void);
    STDMETHOD(dummy_GetInlineObject)(void);
    STDMETHOD(dummy_GetTypography)(void);
    STDMETHOD(dummy_GetLocaleNameLength2)(void);
    STDMETHOD(dummy_GetLocaleName2)(void);
    STDMETHOD(dummy_Draw)(void);
    STDMETHOD(dummy_GetLineMetrics)(void);
    STDMETHOD(GetMetrics)(dw_IDWriteTextLayout*, dw_DWRITE_TEXT_METRICS*);
    STDMETHOD(dummy_GetOverhangMetrics)(void);
    STDMETHOD(dummy_GetClusterMetrics)(void);
    STDMETHOD(dummy_DetermineMinWidth)(void);
    STDMETHOD(dummy_HitTestPoint)(void);
    STDMETHOD(dummy_HitTestTextPosition)(void);
    STDMETHOD(dummy_HitTestTextRange)(void);
};

struct dw_IDWriteTextLayout_tag {
    dw_IDWriteTextLayoutVtbl* vtbl;
};

#define IDWriteTextLayout_QueryInterface(self,a,b)              (self)->vtbl->QueryInterface(self,a,b)
#define IDWriteTextLayout_AddRef(self)                          (self)->vtbl->AddRef(self)
#define IDWriteTextLayout_Release(self)                         (self)->vtbl->Release(self)
#define IDWriteTextLayout_SetTextAlignment(self,a)              (self)->vtbl->SetTextAlignment(self,a)
#define IDWriteTextLayout_SetWordWrapping(self,a)               (self)->vtbl->SetWordWrapping(self,a)
#define IDWriteTextLayout_GetMetrics(self,a)                    (self)->vtbl->GetMetrics(self,a)


/*************************
 ***  Direct2D Driver  ***
 *************************/

#include <d2d1.h>
#include <wincodec.h>

#ifdef MC_TOOLCHAIN_MINGW64
    /* In mingw-w64, these IIDs are missing in libuuid.a. */
    static const GUID xdraw_IID_ID2D1Factory = {0x06152247,0x6f50,0x465a,{0x92,0x45,0x11,0x8b,0xfd,0x3b,0x60,0x07}};
    #define IID_ID2D1Factory xdraw_IID_ID2D1Factory

    static const GUID xdraw_IID_ID2D1GdiInteropRenderTarget = {0xe0db51c3,0x6f77,0x4bae,{0xb3,0xd5,0xe4,0x75,0x09,0xb3,0x58,0x38}};
    #define IID_ID2D1GdiInteropRenderTarget xdraw_IID_ID2D1GdiInteropRenderTarget
#endif


static HMODULE d2d_dll = NULL;  /* D2D1.DLL */
static ID2D1Factory* d2d_factory;

static HMODULE dw_dll = NULL;   /* DWRITE.DLL */
static dw_IDWriteFactory* dw_factory;

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
        MC_TRACE("d2d_init: D2D1CreateFactory() failed. [0x%lx]", hr);
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

    hr = fn_DWriteCreateFactory(0/*DWRITE_FACTORY_TYPE_SHARED*/,
                &dw_IID_IDWriteFactory, (void**) &dw_factory);
    if(MC_ERR(hr != S_OK)) {
        MC_TRACE("d2d_init: DWriteCreateFactory() failed. [0x%lx]", hr);
        goto err_dw_CreateFactory;
    }

    /* We need locale name for creation of IDWriteTextFormat. This functions is
     * available since Vista (which covers all systems with Direct2D and
     * DirectWrite). */
    fn_GetUserDefaultLocaleName = (int (WINAPI*)(WCHAR*, int))
            GetProcAddress(GetModuleHandle(_T("KERNEL32.DLL")), "GetUserDefaultLocaleName");
    if(MC_ERR(fn_GetUserDefaultLocaleName == NULL)) {
        MC_TRACE_ERR("d2d_init: GetProcAddress('GetUserDefaultLocaleName') failed.");
        goto err_kernel_GetProcAddress;
    }

    /* Success */
    return 0;

    /* Error path unwinding */
err_kernel_GetProcAddress:
    IDWriteFactory_Release(dw_factory);
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

    IDWriteFactory_Release(dw_factory);
    FreeLibrary(dw_dll);
    d2d_dll = NULL;
}

typedef struct d2d_canvas_tag d2d_canvas_t;
struct d2d_canvas_tag {
    ID2D1RenderTarget* target;
    ID2D1GdiInteropRenderTarget* gdi_interop;
    BOOL is_hwnd_target;
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
d2d_canvas_alloc(ID2D1RenderTarget* target, BOOL is_hwnd_target)
{
    d2d_canvas_t* c;

    c = (d2d_canvas_t*) malloc(sizeof(d2d_canvas_t));
    if(MC_ERR(c == NULL)) {
        MC_TRACE("d2d_canvas_alloc: malloc() failed.");
        return NULL;
    }

    c->target = target;
    c->gdi_interop = NULL;
    c->is_hwnd_target = is_hwnd_target;

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
        MC_TRACE("d2d_create_wic_converter: xcom_init_create() failed.");
        goto err_xcom_init_create;
    }

    if(path != NULL) {
        hr = IWICImagingFactory_CreateDecoderFromFilename(wic_factory, path,
                NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &wic_decoder);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("d2d_create_wic_converter: "
                     "IWICImagingFactory::CreateDecoderFromFilename() failed. "
                     "[0x%lx]", hr);
            goto err_CreateDecoder;
        }
    } else {
        hr = IWICImagingFactory_CreateDecoderFromStream(wic_factory, stream,
                NULL, WICDecodeMetadataCacheOnLoad, &wic_decoder);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("d2d_create_wic_converter: "
                     "IWICImagingFactory::CreateDecoderFromStream() failed. "
                     "[0x%lx]", hr);
            goto err_CreateDecoder;
        }
    }

    hr = IWICBitmapDecoder_GetFrame(wic_decoder, 0, &wic_source);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("d2d_create_wic_converter: "
                 "IWICBitmapDecoder::GetFrame() failed. [0x%lx]", hr);
        goto err_GetFrame;
    }

    hr = IWICImagingFactory_CreateFormatConverter(wic_factory, &wic_converter);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("d2d_create_wic_converter: "
                 "IWICImagingFactory::CreateFormatConverter() failed. "
                 "[0x%lx]", hr);
        goto err_CreateFormatConverter;
    }

    hr = IWICFormatConverter_Initialize(wic_converter,
            (IWICBitmapSource*) wic_source, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeMedianCut);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("d2d_create_wic_converter: "
                 "IWICFormatConverter::Initialize() failed. [0x%lx]", hr);
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
    xcom_fini();
err_xcom_init_create:
    return NULL;
}

static ID2D1Geometry*
d2d_create_arc_geometry(const xdraw_circle_t* circle, float base_angle,
                        float sweep_angle, BOOL pie)
{
    ID2D1PathGeometry* g = NULL;
    ID2D1GeometrySink* s;
    HRESULT hr;
    float base_rads = base_angle * (MC_PIf / 180.0f);
    float sweep_rads = (base_angle + sweep_angle) * (MC_PIf / 180.0f);
    D2D1_POINT_2F pt;
    D2D1_ARC_SEGMENT arc;

    EnterCriticalSection(&xdraw_lock);
    hr = ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
    LeaveCriticalSection(&xdraw_lock);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("d2d_create_arc_geometry: "
                 "ID2D1Factory::CreatePathGeometry() failed. [0x%lx]", hr);
        return NULL;
    }
    hr = ID2D1PathGeometry_Open(g, &s);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("d2d_create_arc_geometry: "
                 "ID2D1PathGeometry::Open() failed. [0x%lx]", hr);
        ID2D1PathGeometry_Release(g);
        return NULL;
    }

    pt.x = circle->x + circle->r * cosf(base_rads);
    pt.y = circle->y + circle->r * sinf(base_rads);
    ID2D1GeometrySink_BeginFigure(s, pt, D2D1_FIGURE_BEGIN_FILLED);

    arc.point.x = circle->x + circle->r * cosf(sweep_rads);
    arc.point.y = circle->y + circle->r * sinf(sweep_rads);
    arc.size.width = circle->r;
    arc.size.height = circle->r;
    arc.rotationAngle = 0.0f;
    arc.sweepDirection = (sweep_rads >= 0.0f ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                            : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE);
    arc.arcSize = (sweep_angle >= 180.0f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL);
    ID2D1GeometrySink_AddArc(s, &arc);

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

static dw_IDWriteTextLayout*
d2d_create_text_layout(dw_IDWriteTextFormat* tf, const xdraw_rect_t* rect,
                       const TCHAR* str, int len, DWORD flags)
{
    dw_IDWriteTextLayout* layout;
    HRESULT hr;
    int tla;

    if(len < 0)
        len = _tcslen(str);

    hr = IDWriteFactory_CreateTextLayout(dw_factory, str, len, tf,
                rect->x1 - rect->x0, rect->y1 - rect->y0, &layout);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("d2d_create_text_layout: "
                 "IDWriteFactory::CreateTextLayout() failed. [0x%lx]", hr);
        return NULL;
    }

    if(flags & XDRAW_STRING_RIGHT)
        tla = dw_DWRITE_TEXT_ALIGNMENT_TRAILING;
    else if(flags & XDRAW_STRING_CENTER)
        tla = dw_DWRITE_TEXT_ALIGNMENT_CENTER;
    else
        tla = dw_DWRITE_TEXT_ALIGNMENT_LEADING;
    IDWriteTextLayout_SetTextAlignment(layout, tla);

    if(flags & XDRAW_STRING_NOWRAP)
        IDWriteTextLayout_SetWordWrapping(layout, dw_DWRITE_WORD_WRAPPING_NO_WRAP);

    return layout;
}


/***************************************
 ***  Dummy <gdiplus/gdiplusflat.h>  ***
 ***************************************/

/* Most GDI+ headers provide only C++ wrapper classes over the functions
 * from <gdiplus/gdiplusflat.h> so they are unusable for us (not just for the
 * language itself but also because we load GDIPLUS.DLL via LoadLibrary().
 * However for no apparent reasons, even the header <gdiplus/gdiplusflat.h>
 * requires C++ and does not compile with plain C, at least as of MS SDK for
 * Windows 8.1.
 */

/* MSDN documentation for <gdiplus/gdiplusflat.h> sucks. This one is better:
 * http://www.jose.it-berater.org/gdiplus/iframe/index.htm
 */

typedef struct gdix_RectF_tag gdix_RectF;
struct gdix_RectF_tag {
    float x;
    float y;
    float w;
    float h;
};

/* Graphics functions */
static int (WINAPI* gdix_CreateFromHDC)(HDC, void**);
static int (WINAPI* gdix_DeleteGraphics)(void*);
static int (WINAPI* gdix_GraphicsClear)(void*, DWORD);
static int (WINAPI* gdix_GetDC)(void*, HDC*);
static int (WINAPI* gdix_ReleaseDC)(void*, HDC);
static int (WINAPI* gdix_ResetWorldTransform)(void*);
static int (WINAPI* gdix_RotateWorldTransform)(void*,float,int);
static int (WINAPI* gdix_SetPixelOffsetMode)(void*, int);
static int (WINAPI* gdix_SetSmoothingMode)(void*, int);
static int (WINAPI* gdix_TranslateWorldTransform)(void*, float, float, int);

/* Brush functions */
static int (WINAPI* gdix_CreateSolidFill)(DWORD, void**);
static int (WINAPI* gdix_DeleteBrush)(void*);
static int (WINAPI* gdix_SetSolidFillColor)(void*, DWORD);

/* Pen functions */
static int (WINAPI* gdix_CreatePen1)(DWORD, float, int, void**);
static int (WINAPI* gdix_DeletePen)(void*);
static int (WINAPI* gdix_SetPenBrushFill)(void*, void*);
static int (WINAPI* gdix_SetPenWidth)(void*, float);

/* Path functions */
static int (WINAPI* gdix_CreatePath)(int, void**);
static int (WINAPI* gdix_DeletePath)(void*);
static int (WINAPI* gdix_ClosePathFigure)(void*);
static int (WINAPI* gdix_StartPathFigure)(void*);
static int (WINAPI* gdix_GetPathLastPoint)(void*, void*);
static int (WINAPI* gdix_AddPathArc)(void*, float, float, float, float, float, float);
static int (WINAPI* gdix_AddPathLine)(void*, float, float, float, float);

/* Font functions */
static int (WINAPI* gdix_CreateFontFromLogfontW)(HDC, const LOGFONTW*, void**);
static int (WINAPI* gdix_DeleteFont)(void*);
static int (WINAPI* gdix_DeleteFontFamily)(void*);
static int (WINAPI* gdix_GetCellAscent)(const void*, int, UINT16*);
static int (WINAPI* gdix_GetCellDescent)(const void*, int, UINT16*);
static int (WINAPI* gdix_GetEmHeight)(const void*, int, UINT16*);
static int (WINAPI* gdix_GetFamily)(void*, void**);
static int (WINAPI* gdix_GetFontSize)(void*, float*);
static int (WINAPI* gdix_GetFontStyle)(void*, int*);
static int (WINAPI* gdix_GetLineSpacing)(const void*, int, UINT16*);

/* Image functions */
static int (WINAPI* gdix_LoadImageFromFile)(const WCHAR*, void**);
static int (WINAPI* gdix_LoadImageFromStream)(IStream*, void**);
static int (WINAPI* gdix_DisposeImage)(void*);
static int (WINAPI* gdix_GetImageBounds)(void*, gdix_RectF*, int*);

/* String format functions */
static int (WINAPI* gdix_CreateStringFormat)(int, LANGID, void**);
static int (WINAPI* gdix_DeleteStringFormat)(void*);
static int (WINAPI* gdix_SetStringFormatAlign)(void*, int);
static int (WINAPI* gdix_SetStringFormatFlags)(void*, int);

/* Draw/fill functions */
static int (WINAPI* gdix_DrawArc)(void*, void*, float, float, float, float, float, float);
static int (WINAPI* gdix_DrawImageRectRect)(void*, void*, float, float, float, float, float, float, float, float, int, const void*, void*, void*);
static int (WINAPI* gdix_DrawLine)(void*, void*, float, float, float, float);
static int (WINAPI* gdix_DrawPie)(void*, void*, float, float, float, float, float, float);
static int (WINAPI* gdix_DrawRectangle)(void*, void*, float, float, float, float);
static int (WINAPI* gdix_DrawString)(void*, const WCHAR*, int, const void*, const gdix_RectF*, const void*, const void*);
static int (WINAPI* gdix_FillEllipse)(void*, void*, float, float, float, float);
static int (WINAPI* gdix_FillPath)(void*, void*, void*);
static int (WINAPI* gdix_FillPie)(void*, void*, float, float, float, float, float, float);
static int (WINAPI* gdix_FillRectangle)(void*, void*, float, float, float, float);
static int (WINAPI* gdix_MeasureString)(void*, const WCHAR*, int, const void*, const gdix_RectF*, const void*, gdix_RectF*, int*, int*);


typedef struct gdix_StartupInput_tag gdix_StartupInput;
struct gdix_StartupInput_tag {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;  /* DebugEventProc */
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
};


/*********************
 ***  GDI+ Driver  ***
 *********************/

static HMODULE gdix_dll = NULL;
static ULONG_PTR gdix_token;
static void (WINAPI* gdix_Shutdown)(ULONG_PTR);

static int
gdix_init(void)
{
    int(WINAPI* gdix_Startup)(ULONG_PTR*,const gdix_StartupInput*,void*);
    gdix_StartupInput input = { 0 };
    int status;

    gdix_dll = mc_load_sys_dll(_T("GDIPLUS.DLL"));
    if(MC_ERR(gdix_dll == NULL)) {
        MC_TRACE_ERR("gdix_init: LoadLibrary(GDIPLUS.DLL) failed");
        goto err_LoadLibrary;
    }

    gdix_Startup = (int(WINAPI*)(ULONG_PTR*,const gdix_StartupInput*,void*))
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
    GPA(CreateFromHDC, (HDC, void**));
    GPA(DeleteGraphics, (void*));
    GPA(GraphicsClear, (void*, DWORD));
    GPA(GetDC, (void*, HDC*));
    GPA(ReleaseDC, (void*, HDC));
    GPA(ResetWorldTransform, (void*));
    GPA(RotateWorldTransform, (void*, float, int));
    GPA(SetPixelOffsetMode, (void*, int));
    GPA(SetSmoothingMode, (void*, int));
    GPA(TranslateWorldTransform, (void*, float, float, int));

    /* Brush functions */
    GPA(CreateSolidFill, (DWORD, void**));
    GPA(DeleteBrush, (void*));
    GPA(SetSolidFillColor, (void*, DWORD));

    /* Pen functions */
    GPA(CreatePen1, (DWORD, float, int, void**));
    GPA(DeletePen, (void*));
    GPA(SetPenBrushFill, (void*, void*));
    GPA(SetPenWidth, (void*, float));

    /* Path functions */
    GPA(CreatePath, (int, void**));
    GPA(DeletePath, (void*));
    GPA(ClosePathFigure, (void*));
    GPA(StartPathFigure, (void*));
    GPA(GetPathLastPoint, (void*, void*));
    GPA(AddPathArc, (void*, float, float, float, float, float, float));
    GPA(AddPathLine, (void*, float, float, float, float));

    /* Font functions */
    GPA(CreateFontFromLogfontW, (HDC,const LOGFONTW*,void**));
    GPA(DeleteFont, (void*));
    GPA(DeleteFontFamily, (void*));
    GPA(GetCellAscent, (const void*, int, UINT16*));
    GPA(GetCellDescent, (const void*, int, UINT16*));
    GPA(GetEmHeight, (const void*, int, UINT16*));
    GPA(GetFamily, (void*, void**));
    GPA(GetFontSize, (void*, float*));
    GPA(GetFontStyle, (void*, int*));
    GPA(GetLineSpacing, (const void*, int, UINT16*));

    /* Image functions */
    GPA(LoadImageFromFile, (const WCHAR*, void**));
    GPA(LoadImageFromStream, (IStream*, void**));
    GPA(DisposeImage, (void*));
    GPA(GetImageBounds, (void*, gdix_RectF*, int*));

    /* String format functions */
    GPA(CreateStringFormat, (int, LANGID, void**));
    GPA(DeleteStringFormat, (void*));
    GPA(SetStringFormatAlign, (void*, int));
    GPA(SetStringFormatFlags, (void*, int));

    /* Draw/fill functions */
    GPA(DrawArc, (void*, void*, float, float, float, float, float, float));
    GPA(DrawImageRectRect, (void*, void*, float, float, float, float, float, float, float, float, int, const void*, void*, void*));
    GPA(DrawLine, (void*, void*, float, float, float, float));
    GPA(DrawPie, (void*, void*, float, float, float, float, float, float));
    GPA(DrawRectangle, (void*, void*, float, float, float, float));
    GPA(DrawString, (void*, const WCHAR*, int, const void*, const gdix_RectF*, const void*, const void*));
    GPA(FillEllipse, (void*, void*, float, float, float, float));
    GPA(FillPath, (void*, void*, void*));
    GPA(FillPie, (void*, void*, float, float, float, float, float, float));
    GPA(FillRectangle, (void*, void*, float, float, float, float));
    GPA(MeasureString, (void*, const WCHAR*, int, const void*, const gdix_RectF*, const void*, gdix_RectF*, int*, int*));

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
    void* graphics;
    void* pen;
    void* string_format;
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
        MC_TRACE("xdraw_canvas_create_with_dc: "
                 "GdipCreatePen1() failed. [%d]", status);
        goto err_createpen;
    }

    /* Needed for xdraw_draw_string() and xdraw_measure_string() */
    status = gdix_CreateStringFormat(0, LANG_NEUTRAL, &c->string_format);
    if(MC_ERR(status != 0)) {
        MC_TRACE("xdraw_canvas_create_with_dc: "
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
}

typedef struct gdix_path_sink_tag gdix_path_sink_t;
struct gdix_path_sink_tag {
    void* path;
    xdraw_point_t last_point;
};


/**************************
 ***  Public Interface  ***
 **************************/

xdraw_canvas_t*
xdraw_canvas_create_with_paintstruct(HWND win, PAINTSTRUCT* ps, DWORD flags)
{
    if(d2d_dll != NULL) {
        D2D1_RENDER_TARGET_PROPERTIES props = {
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN },
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
            MC_TRACE("xdraw_canvas_create_with_paintstruct: "
                     "ID2D1Factory::CreateHwndRenderTarget() failed. [0x%lx]", hr);
            return NULL;
        }

        c = d2d_canvas_alloc((ID2D1RenderTarget*)target, TRUE);
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
            MC_TRACE("xdraw_canvas_create_with_dc: "
                     "ID2D1Factory::CreateDCRenderTarget() failed. [0x%lx]", hr);
            goto err_CreateDCRenderTarget;
        }

        hr = ID2D1DCRenderTarget_BindDC(target, dc, rect);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_canvas_create_with_dc: "
                     "ID2D1Factory::BindDC() failed. [0x%lx]", hr);
            goto err_BindDC;
        }

        c = d2d_canvas_alloc((ID2D1RenderTarget*)target, FALSE);
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
    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;

        ID2D1RenderTarget_Release(c->target);
        MC_ASSERT(c->gdi_interop == NULL);
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
    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        if(c->is_hwnd_target) {
            D2D1_SIZE_U size = { width, height };
            HRESULT hr;

            hr = ID2D1HwndRenderTarget_Resize((ID2D1HwndRenderTarget*) c->target, &size);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE("xdraw_canvas_resize: ID2D1HwndRenderTarget_Resize() failed. [0x%lx]", hr);
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
    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        HRESULT hr;
        HDC dc;

        MC_ASSERT(c->gdi_interop == NULL);

        hr = ID2D1RenderTarget_QueryInterface(c->target,
                    &IID_ID2D1GdiInteropRenderTarget, (void**) &c->gdi_interop);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_canvas_acquire_dc: "
                     "ID2D1RenderTarget::QueryInterface(IID_ID2D1GdiInteropRenderTarget) "
                     "failed. [0x%lx]", hr);
            return NULL;
        }

        hr = ID2D1GdiInteropRenderTarget_GetDC(c->gdi_interop,
                (retain_contents ? D2D1_DC_INITIALIZE_MODE_COPY : D2D1_DC_INITIALIZE_MODE_CLEAR),
                &dc);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_canvas_acquire_dc: "
                     "ID2D1GdiInteropRenderTarget::GetDC() failed. [0x%lx]", hr);
            ID2D1GdiInteropRenderTarget_Release(c->gdi_interop);
            c->gdi_interop = NULL;
            return NULL;
        }

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
        gdix_RotateWorldTransform(c->graphics, angle, 0/*MatrixOrderPrepend*/);
    }
}

void
xdraw_canvas_transform_with_translation(xdraw_canvas_t* canvas, float dx, float dy)
{
    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        D2D1_MATRIX_3X2_F transform;

        ID2D1RenderTarget_GetTransform(c->target, &transform);
        transform._31 += dx;
        transform._32 += dy;
        ID2D1RenderTarget_SetTransform(c->target, &transform);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_TranslateWorldTransform(c->graphics, dx, dy, 0/*MatrixOrderPrepend*/);
    }
}

void
xdraw_canvas_transform_reset(xdraw_canvas_t* canvas)
{
    if(d2d_dll != NULL) {
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        d2d_reset_transform(c->target);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_ResetWorldTransform(c->graphics);
    }
}

xdraw_brush_t*
xdraw_brush_solid_create(xdraw_canvas_t* canvas, xdraw_color_t color)
{
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
            MC_TRACE("xdraw_brush_create_solid: "
                     "ID2D1RenderTarget::CreateSolidColorBrush() failed. [0x%lx]", hr);
            return NULL;
        }
        return (xdraw_brush_t*) b;
    } else {
        void* b;
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
    if(d2d_dll != NULL) {
        ID2D1Brush_Release((ID2D1Brush*) brush);
    } else {
        gdix_DeleteBrush((void*) brush);
    }
}

void
xdraw_brush_solid_set_color(xdraw_brush_t* solidbrush, xdraw_color_t color)
{
    if(d2d_dll != NULL) {
        D2D_COLOR_F clr;

        clr.r = XDRAW_REDVALUE(color) / 255.0f;
        clr.g = XDRAW_GREENVALUE(color) / 255.0f;
        clr.b = XDRAW_BLUEVALUE(color) / 255.0f;
        clr.a = XDRAW_ALPHAVALUE(color) / 255.0f;

        ID2D1SolidColorBrush_SetColor((ID2D1SolidColorBrush*) solidbrush, &clr);
    } else {
        gdix_SetSolidFillColor(solidbrush, color);
    }
}

xdraw_font_t*
xdraw_font_create_with_LOGFONT(xdraw_canvas_t* canvas, const LOGFONT* logfont)
{
#ifndef UNICODE
    #error xdraw_font_create_with_LOGFONT() is not (yet?) implemented for ANSI build.
#endif

    if(d2d_dll != NULL) {
        WCHAR locale_name[LOCALE_NAME_MAX_LENGTH];
        dw_IDWriteTextFormat* tf;
        dw_DWRITE_FONT_STYLE style;
        HRESULT hr;

        if(fn_GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) == 0) {
            MC_TRACE("xdraw_font_create_with_LOGFONT: "
                     "GetUserDefaultLocaleName() failed.");
            locale_name[0] = _T('\0');
        }

        if(logfont->lfItalic)
            style = dw_DWRITE_FONT_STYLE_ITALIC;
        else
            style = dw_DWRITE_FONT_STYLE_NORMAL;

        /* FIXME: Right now, we ignore some LOGFONT members here.
         *        For example:
         *          -- LOGFONT::lfUnderline should propagate into
         *             dw_IDWriteTextLayout::SetUnderline()
         *          -- LOGFONT::lfStrikeOut should propagate into
         *             dw_IDWriteTextLayout::SetStrikethrough()
         */

        hr = IDWriteFactory_CreateTextFormat(dw_factory, logfont->lfFaceName,
                NULL, logfont->lfWeight, style, dw_DWRITE_FONT_STRETCH_NORMAL,
                MC_ABS(logfont->lfHeight), locale_name, &tf);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_font_create_with_LOGFONT: "
                     "IDWriteFactory::CreateTextFormat(%S) failed. [0x%lx]",
                     logfont->lfFaceName, hr);
            return NULL;
        }
        return (xdraw_font_t*) tf;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        void* f;
        int status;

        status = gdix_CreateFontFromLogfontW(c->dc, logfont, &f);
        if(MC_ERR(status != 0)) {
            /* GDI+ supports only fonts with true-type outlines. For this
             * reason, implement simple fall back. */
            LOGFONT fallback_logfont;

            memcpy(&fallback_logfont, logfont, sizeof(LOGFONT));
            _tcscpy(fallback_logfont.lfFaceName,
                    (mc_win_version >= MC_WIN_VISTA ? _T("Segoe UI") : _T("Tahoma")));
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
    if(d2d_dll != NULL) {
        dw_IDWriteTextFormat* tf = (dw_IDWriteTextFormat*) font;
        IDWriteTextFormat_Release(tf);
    } else {
        gdix_DeleteFont(font);
    }
}

void
xdraw_font_get_metrics(const xdraw_font_t* font, xdraw_font_metrics_t* metrics)
{
    if(d2d_dll != NULL) {
        dw_IDWriteTextFormat* tf = (dw_IDWriteTextFormat*) font;
        dw_IDWriteFontCollection* fc;
        dw_IDWriteFontFamily* ff;
        dw_IDWriteFont* f;
        WCHAR* name;
        UINT32 name_len;
        dw_DWRITE_FONT_WEIGHT weight;
        dw_DWRITE_FONT_STRETCH stretch;
        dw_DWRITE_FONT_STYLE style;
        UINT32 ix;
        BOOL exists;
        dw_DWRITE_FONT_METRICS fm;
        float factor;
        HRESULT hr;

        /* Based on http://stackoverflow.com/a/5610139/917880 */

        name_len = IDWriteTextFormat_GetFontFamilyNameLength(tf) + 1;
        name = _malloca(name_len * sizeof(WCHAR));
        if(MC_ERR(name == NULL)) {
            MC_TRACE("xdraw_font_get_metrics: _malloca() failed.");
            goto err_malloca;
        }
        hr = IDWriteTextFormat_GetFontFamilyName(tf, name, name_len);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_font_get_metrics: "
                     "IDWriteTextFormat::GetFontFamilyName() failed. [0x%lx]", hr);
            goto err_GetFontFamilyName;
        }

        weight = IDWriteTextFormat_GetFontWeight(tf);
        stretch = IDWriteTextFormat_GetFontStretch(tf);
        style = IDWriteTextFormat_GetFontStyle(tf);

        hr = IDWriteTextFormat_GetFontCollection(tf, &fc);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_font_get_metrics: "
                     "IDWriteTextFormat::GetFontCollection() failed. [0x%lx]", hr);
            goto err_GetFontCollection;
        }

        hr = IDWriteFontCollection_FindFamilyName(fc, name, &ix, &exists);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_font_get_metrics: "
                     "IDWriteFontCollection::FindFamilyName() failed. [0x%lx]", hr);
            goto err_FindFamilyName;
        }

        if(MC_ERR(!exists)) {
            MC_TRACE("xdraw_font_get_metrics: WTF? Font does not exist?");
            goto err_exists;
        }

        hr = IDWriteFontCollection_GetFontFamily(fc, ix, &ff);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_font_get_metrics: "
                     "IDWriteFontCollection::GetFontFamily() failed. [0x%lx]", hr);
            goto err_GetFontFamily;
        }

        hr = IDWriteFontFamily_GetFirstMatchingFont(ff, weight, stretch, style, &f);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_font_get_metrics: "
                     "IDWriteFontFamily::GetFirstMatchingFont() failed. [0x%lx]", hr);
            goto err_GetFirstMatchingFont;
        }

        IDWriteFont_GetMetrics(f, &fm);

        metrics->em_height = IDWriteTextFormat_GetFontSize(tf);
        factor = (metrics->em_height / fm.designUnitsPerEm);
        metrics->cell_ascent = fm.ascent * factor;
        metrics->cell_descent = fm.descent * factor;
        metrics->line_spacing = (fm.ascent + fm.descent + fm.lineGap) * factor;

        IDWriteFont_Release(f);
err_GetFirstMatchingFont:
        IDWriteFontFamily_Release(ff);
err_GetFontFamily:
err_exists:
err_FindFamilyName:
        IDWriteFontCollection_Release(fc);
err_GetFontCollection:
err_GetFontFamilyName:
        _freea(name);
err_malloca:
        ;
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
xdraw_image_load_from_file(const TCHAR* path)
{
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
        void* img;
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
        void* img;
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
    if(d2d_dll != NULL) {
        IWICBitmapSource* source = (IWICBitmapSource*) image;
        IWICBitmapSource_Release(source);
        xcom_fini();
    } else {
        gdix_DisposeImage((void*) image);
    }
}

void
xdraw_image_get_size(xdraw_image_t* image, float* w, float* h)
{
    if(d2d_dll != NULL) {
        IWICBitmapSource* source = (IWICBitmapSource*) image;
        UINT iw, ih;

        IWICBitmapSource_GetSize(source, &iw, &ih);
        *w = iw;
        *h = ih;
    } else {
        gdix_RectF bounds;
        int unit;

        gdix_GetImageBounds((void*) image, &bounds, &unit);
        MC_ASSERT(unit == 2/*UnitPixel*/);
        *w = bounds.w;
        *h = bounds.h;
    }
}

xdraw_path_t*
xdraw_path_create(xdraw_canvas_t* canvas)
{
    if(d2d_dll != NULL) {
        ID2D1PathGeometry* g;
        HRESULT hr;

        EnterCriticalSection(&xdraw_lock);
        hr = ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
        LeaveCriticalSection(&xdraw_lock);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_path_create: "
                     "ID2D1Factory::CreatePathGeometry() failed. [0x%lx]", hr);
            return NULL;
        }

        return (xdraw_path_t*) g;
    } else {
        void* p;
        int status;

        status = gdix_CreatePath(0/*FillModeAlternate*/, &p);
        if(MC_ERR(status != 0)) {
            MC_TRACE("xdraw_path_create: GdipCreatePath() failed. [%d]", status);
            return NULL;
        }

        return (xdraw_path_t*) p;
    }
}

void
xdraw_path_destroy(xdraw_path_t* path)
{
    if(d2d_dll != NULL) {
        ID2D1PathGeometry* g = (ID2D1PathGeometry*) path;
        ID2D1PathGeometry_Release(g);
    } else {
        gdix_DeletePath(path);
    }
}

xdraw_path_sink_t*
xdraw_path_open_sink(xdraw_path_t* path)
{
    if(d2d_dll != NULL) {
        ID2D1PathGeometry* g = (ID2D1PathGeometry*) path;
        ID2D1GeometrySink* s;
        HRESULT hr;

        hr = ID2D1PathGeometry_Open(g, &s);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_path_open_sink: "
                     "ID2D1PathGeometry::Open() failed. [0x%lx]", hr);
            return NULL;
        }
        return (xdraw_path_sink_t*) s;
    } else {
        gdix_path_sink_t* s;

        s = (gdix_path_sink_t*) malloc(sizeof(gdix_path_sink_t));
        if(MC_ERR(s == NULL)) {
            MC_TRACE("xdraw_path_open_sink: malloc() failed.");
            return NULL;
        }
        s->path = path;
        return (xdraw_path_sink_t*) s;
    }
}

void
xdraw_path_close_sink(xdraw_path_sink_t* sink)
{
    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink;
        ID2D1GeometrySink_Close(s);
        ID2D1GeometrySink_Release(s);
    } else {
        free(sink);
    }
}

void
xdraw_path_begin_figure(xdraw_path_sink_t* sink, const xdraw_point_t* start_point)
{
    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink;
        ID2D1GeometrySink_BeginFigure(s, *((D2D1_POINT_2F*) start_point),
                                      D2D1_FIGURE_BEGIN_FILLED);
    } else {
        gdix_path_sink_t* s = (gdix_path_sink_t*) sink;
        gdix_StartPathFigure(s->path);
        s->last_point.x = start_point->x;
        s->last_point.y = start_point->y;
    }
}

void
xdraw_path_end_figure(xdraw_path_sink_t* sink, BOOL closed_end)
{
    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink;
        ID2D1GeometrySink_EndFigure(s, (closed_end ? D2D1_FIGURE_END_CLOSED
                                                   : D2D1_FIGURE_END_OPEN));
    } else {
        if(closed_end) {
            gdix_path_sink_t* s = (gdix_path_sink_t*) sink;
            gdix_ClosePathFigure(s->path);
        }
    }
}

void
xdraw_path_add_line(xdraw_path_sink_t* sink, const xdraw_point_t* end_point)
{
    if(d2d_dll != NULL) {
        ID2D1GeometrySink* s = (ID2D1GeometrySink*) sink;
        ID2D1GeometrySink_AddLine(s, *((D2D1_POINT_2F*) end_point));
    } else {
        gdix_path_sink_t* s = (gdix_path_sink_t*) sink;
        gdix_AddPathLine(s->path, s->last_point.x, s->last_point.y,
                         end_point->x, end_point->y);
        s->last_point.x = end_point->x;
        s->last_point.y = end_point->y;
    }
}

void
xdraw_clear(xdraw_canvas_t* canvas, xdraw_color_t color)
{
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
xdraw_draw_image(xdraw_canvas_t* canvas, const xdraw_image_t* image,
                 const xdraw_rect_t* dst, const xdraw_rect_t* src)
{
    if(d2d_dll != NULL) {
        IWICBitmapSource* source = (IWICBitmapSource*) image;
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Bitmap* b;
        HRESULT hr;

        hr = ID2D1RenderTarget_CreateBitmapFromWicBitmap(c->target, source, NULL, &b);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE("xdraw_draw_image: "
                     "ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed. "
                     "[0x%lx]", hr);
            return;
        }
        ID2D1RenderTarget_DrawBitmap(c->target, b, (D2D1_RECT_F*) dst, 1.0f,
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
xdraw_draw_line(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
                const xdraw_line_t* line, float stroke_width)
{
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
xdraw_draw_pie(xdraw_canvas_t* canvas, const xdraw_brush_t* brush,
               const xdraw_circle_t* circle, float base_angle,
               float sweep_angle, float stroke_width)
{
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
xdraw_draw_string(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                  const xdraw_rect_t* rect, const TCHAR* str, int len,
                  const xdraw_brush_t* brush, DWORD flags)
{
    if(d2d_dll != NULL) {
        D2D1_POINT_2F origin = { rect->x0, rect->y0 };
        d2d_canvas_t* c = (d2d_canvas_t*) canvas;
        ID2D1Brush* b = (ID2D1Brush*) brush;
        dw_IDWriteTextFormat* tf = (dw_IDWriteTextFormat*) font;
        dw_IDWriteTextLayout* layout;

        layout = d2d_create_text_layout(tf, rect, str, len, flags);
        if(MC_ERR(layout == NULL)) {
            MC_TRACE("xdraw_draw_string: d2d_create_text_layout() failed.");
            return;
        }

        ID2D1RenderTarget_DrawTextLayout(c->target, origin,
                (IDWriteTextLayout*) layout, b,
                (flags & XDRAW_STRING_CLIP) ? D2D1_DRAW_TEXT_OPTIONS_CLIP : 0);

        IDWriteTextLayout_Release(layout);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_RectF r = { rect->x0, rect->y0, rect->x1 - rect->x0, rect->y1 - rect->y0 };

        gdix_canvas_apply_string_flags(c, flags);
        gdix_DrawString(c->graphics, str, len, font, &r, c->string_format, brush);
    }
}

void
xdraw_measure_string(xdraw_canvas_t* canvas, const xdraw_font_t* font,
                     const xdraw_rect_t* rect, const TCHAR* str, int len,
                     xdraw_string_measure_t* measure, DWORD flags)
{
    if(d2d_dll != NULL) {
        dw_IDWriteTextFormat* tf = (dw_IDWriteTextFormat*) font;
        dw_IDWriteTextLayout* layout;
        dw_DWRITE_TEXT_METRICS tm;

        layout = d2d_create_text_layout(tf, rect, str, len, flags);
        if(MC_ERR(layout == NULL)) {
            MC_TRACE("xdraw_measure_string: d2d_create_text_layout() failed.");
            return;
        }

        IDWriteTextLayout_GetMetrics(layout, &tm);

        measure->bound.x0 = rect->x0 + tm.left;
        measure->bound.y0 = rect->y0 + tm.top;
        measure->bound.x1 = measure->bound.x0 + tm.width;
        measure->bound.y1 = measure->bound.y0 + tm.height;

        IDWriteTextLayout_Release(layout);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) canvas;
        gdix_RectF r = { rect->x0, rect->y0, rect->x1 - rect->x0, rect->y1 - rect->y0 };
        gdix_RectF br;

        gdix_canvas_apply_string_flags(c, flags);
        gdix_MeasureString(c->graphics, str, len, font, &r, c->string_format,
                           &br, NULL, NULL);

        measure->bound.x0 = br.x;
        measure->bound.y0 = br.y;
        measure->bound.x1 = br.x + br.w;
        measure->bound.y1 = br.y + br.h;
    }
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

