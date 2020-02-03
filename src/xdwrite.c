/*
 * Copyright (c) 2019-2020 Martin Mitas
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

#include "xdwrite.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define XDWRITE_DEBUG     1*/

#ifdef XDWRITE_DEBUG
    #define XDWRITE_TRACE       MC_TRACE
    #define XDWRITE_TRACE_GUID  MC_TRACE_GUID
#else
    #define XDWRITE_TRACE       MC_NOOP
    #define XDWRITE_TRACE_GUID  MC_NOOP
#endif


/************************
 *** DWrite Factories ***
 ************************/

static c_IDWriteFactory* xdwrite_factory;
static HMODULE xdwrite_dll;


c_IDWriteTextFormat*
xdwrite_create_text_format(HFONT gdi_font, c_DWRITE_FONT_METRICS* p_metrics)
{
    /* See https://github.com/Microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/multimedia/DirectWrite/RenderTest/TextHelpers.cpp */
    LOGFONT lf;
    c_IDWriteGdiInterop* gdi_interop;
    c_IDWriteFont* font;
    c_IDWriteFontFamily* family;
    c_IDWriteLocalizedStrings* family_names;
    c_IDWriteTextFormat* tf = NULL;
    WCHAR user_locale_name[LOCALE_NAME_MAX_LENGTH];
    UINT32 buf_size;
    WCHAR* buf;
    float font_size;
    HRESULT hr;

    if(gdi_font == NULL)
        gdi_font = GetStockObject(SYSTEM_FONT);
    GetObject(gdi_font, sizeof(LOGFONT), &lf);

    hr = c_IDWriteFactory_GetGdiInterop(xdwrite_factory, &gdi_interop);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_format: IDWriteFactory::GetGdiInterop() failed.");
        goto err_GetGdiInterop;
    }

retry_CreateFontFromLOGFONT:
    hr = c_IDWriteGdiInterop_CreateFontFromLOGFONT(gdi_interop, &lf, &font);
    if(MC_ERR(FAILED(hr))) {
        /* DWrite seems to not support non-TrueType fonts. On the failure,
         * lets retry with a true-type font face. */
        static const TCHAR fallback_font_face[] = _T("Segoe UI");
        if(_tcscmp(lf.lfFaceName, fallback_font_face) != 0) {
            _tcscpy(lf.lfFaceName, fallback_font_face);
            goto retry_CreateFontFromLOGFONT;
        }

        MC_TRACE_HR("xdwrite_create_text_format: IDWriteGdiInterop::CreateFontFromLOGFONT() failed.");
        goto err_CreateFontFromLOGFONT;
    }

    if(p_metrics != NULL)
        c_IDWriteFont_GetMetrics(font, p_metrics);

    hr = c_IDWriteFont_GetFontFamily(font, &family);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_format: IDWriteFont::GetFontFamily() failed.");
        goto err_GetFontFamily;
    }

    hr = c_IDWriteFontFamily_GetFamilyNames(family, &family_names);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_format: IDWriteFontFamily::GetFamilyNames() failed.");
        goto err_GetFamilyNames;
    }

    hr = c_IDWriteLocalizedStrings_GetStringLength(family_names, 0, &buf_size);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_format: IDWriteLocalizedStrings::GetStringLength() failed.");
        goto err_GetStringLength;
    }

    buf = (WCHAR*) _malloca(sizeof(WCHAR) * (buf_size + 1));
    if(buf == NULL) {
        MC_TRACE("xdwrite_create_text_format: _malloca() failed.");
        goto err_malloca;
    }

    hr = c_IDWriteLocalizedStrings_GetString(family_names, 0, buf, buf_size + 1);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_format: IDWriteLocalizedStrings::GetString() failed.");
        goto err_GetString;
    }

    if(lf.lfHeight < 0) {
        font_size = (float) -lf.lfHeight;
    } else if(lf.lfHeight > 0) {
        c_DWRITE_FONT_METRICS metrics;

        c_IDWriteFont_GetMetrics(font, &metrics);
        font_size = (float)lf.lfHeight * (float)metrics.designUnitsPerEm
                    / (float)(metrics.ascent + metrics.descent);
    } else {
        font_size = 12.0f;
    }

    GetUserDefaultLocaleName(user_locale_name, LOCALE_NAME_MAX_LENGTH);

    hr = c_IDWriteFactory_CreateTextFormat(xdwrite_factory, buf, NULL,
            c_IDWriteFont_GetWeight(font), c_IDWriteFont_GetStyle(font),
            c_IDWriteFont_GetStretch(font), font_size, user_locale_name, &tf);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_format: IDWriteFactory::CreateTextFormat() failed.");
        goto err_CreateTextFormat;
    }

err_CreateTextFormat:
err_GetString:
    _freea(buf);
err_malloca:
err_GetStringLength:
    c_IDWriteLocalizedStrings_Release(family_names);
err_GetFamilyNames:
    c_IDWriteFontFamily_Release(family);
err_GetFontFamily:
    c_IDWriteFont_Release(font);
err_CreateFontFromLOGFONT:
    c_IDWriteGdiInterop_Release(gdi_interop);
err_GetGdiInterop:
    return tf;
}

c_IDWriteTextLayout*
xdwrite_create_text_layout(const TCHAR* str, UINT len, c_IDWriteTextFormat* tf,
                           float max_width, float max_height, DWORD flags)
{
    c_IDWriteTextLayout* tl;
    int align;
    HRESULT hr;

    if(str == NULL  ||  tf == NULL)
        return NULL;

    hr = c_IDWriteFactory_CreateTextLayout(xdwrite_factory, str, len, tf, max_width, max_height, &tl);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_layout: IDWriteFactory::CreateTextLayout() failed.");
        return NULL;
    }

    if(flags & XDWRITE_ALIGN_MASK) {
        switch(flags & XDWRITE_ALIGN_MASK) {
            case XDWRITE_ALIGN_CENTER:  align = c_DWRITE_TEXT_ALIGNMENT_CENTER; break;
            case XDWRITE_ALIGN_RIGHT:   align = c_DWRITE_TEXT_ALIGNMENT_TRAILING; break;
            case XDWRITE_ALIGN_JUSTIFY: align = c_DWRITE_TEXT_ALIGNMENT_JUSTIFIED; break;
            default:                    MC_UNREACHABLE;
        }
        c_IDWriteTextLayout_SetTextAlignment(tl, align);
    }

    if(flags & XDWRITE_VALIGN_MASK) {
        switch(flags & XDWRITE_VALIGN_MASK) {
            case XDWRITE_VALIGN_CENTER: align = c_DWRITE_PARAGRAPH_ALIGNMENT_CENTER; break;
            case XDWRITE_VALIGN_BOTTOM: align = c_DWRITE_PARAGRAPH_ALIGNMENT_FAR; break;
            default:                    MC_UNREACHABLE;
        };
        c_IDWriteTextLayout_SetParagraphAlignment(tl, align);
    }

    if(flags & XDWRITE_ELLIPSIS_MASK) {
        static const c_DWRITE_TRIMMING trim_end = { c_DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        static const c_DWRITE_TRIMMING trim_word = { c_DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
        static const c_DWRITE_TRIMMING trim_path = { c_DWRITE_TRIMMING_GRANULARITY_WORD, L'\\', 1 };
        const c_DWRITE_TRIMMING* trim_options = NULL;
        c_IDWriteInlineObject* trim_sign;

        hr = c_IDWriteFactory_CreateEllipsisTrimmingSign(xdwrite_factory, tf, &trim_sign);
        if(MC_ERR(FAILED(hr))) {
            MC_TRACE_HR("xdwrite_create_text_layout: IDWriteFactory::CreateEllipsisTrimmingSign() failed");
            goto err_CreateEllipsisTrimmingSign;
        }

        switch(flags & XDWRITE_ELLIPSIS_MASK) {
            case XDWRITE_ELLIPSIS_END:  trim_options = &trim_end; break;
            case XDWRITE_ELLIPSIS_WORD: trim_options = &trim_word; break;
            case XDWRITE_ELLIPSIS_PATH: trim_options = &trim_path; break;
            default:                    MC_UNREACHABLE;
        }
        c_IDWriteTextLayout_SetTrimming(tl, trim_options, trim_sign);
        c_IDWriteInlineObject_Release(trim_sign);

err_CreateEllipsisTrimmingSign:
        ;
    }

    if(flags & XDWRITE_NOWRAP)
        c_IDWriteTextLayout_SetWordWrapping(tl, c_DWRITE_WORD_WRAPPING_NO_WRAP);

    return tl;
}


/******************************************
 *** Custom IDWriteTextRenderer Effects ***
 ******************************************/

static const GUID IID_xeff =
            {0x23d224e8,0x9e4c,0x4b73,{0xac,0xc3,0x98,0xfc,0x3f,0x2b,0x32,0x65}};

static HRESULT STDMETHODCALLTYPE
xeff_QueryInterface(IUnknown* self, REFIID riid, void** obj)
{
    if(IsEqualGUID(riid, &IID_IUnknown)  ||  IsEqualGUID(riid, &IID_xeff)) {
        XDWRITE_TRACE_GUID("xce_QueryInterface", riid);
        *obj = self;
        return S_OK;
    } else {
        XDWRITE_TRACE_GUID("xce_QueryInterface: unsupported GUID", riid);
        *obj = NULL;
        return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
xeff_AddRef_Release(IUnknown* self)
{
    /* Dummy AddRef/Release() - we never really release this object, so
     * we may just return any non-zero. */
    return 42;
}

IUnknownVtbl xdwrite_effect_vtbl_ = {
    xeff_QueryInterface,
    xeff_AddRef_Release,
    xeff_AddRef_Release
};


/*************************************************
 *** Custom IDWriteTextRenderer Implementation ***
 *************************************************/

/* We need this in order to apply our custom text effects.
 * See https://docs.microsoft.com/en-us/windows/win32/directwrite/how-to-implement-a-custom-text-renderer
 */

static void
xtr_apply_effect(xdwrite_ctx_t* ctx, float x, float y,
                 c_DWRITE_GLYPH_RUN const* run, IUnknown* effect_obj)
{
    void* obj;
    xdwrite_effect_t* effect;
    HRESULT hr;

    if(effect_obj == NULL)
        return;
    hr = IUnknown_QueryInterface(effect_obj, &IID_xeff, &obj);
    if(!SUCCEEDED(hr))
        return;

    effect = MC_CONTAINEROF(obj, xdwrite_effect_t, vtbl);

    /* If provided, use the given background color. */
    if((effect->mask & XDWRITE_EFFECT_MASK_BK_COLOR)  &&  run != NULL) {
        c_DWRITE_FONT_METRICS font_metrics;
        float run_width, run_ascent, run_descent, size_factor;
        c_D2D1_RECT_F rect;
        UINT32 i;

        c_IDWriteFontFace_GetMetrics(run->fontFace, &font_metrics);
        size_factor = run->fontEmSize / font_metrics.designUnitsPerEm;
        run_ascent = size_factor * font_metrics.ascent;
        run_descent = size_factor * font_metrics.descent;

        /* FIXME: Isn't there a way how to get the width directly, without
         * manually summing all the glyph advances? */
        run_width = 0;
        for(i = 0; i < run->glyphCount; i++)
            run_width += run->glyphAdvances[i];

        rect.left = x;
        rect.top = y - run_ascent;
        rect.right = x + run_width;
        rect.bottom = y + run_descent;
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &effect->bk_color);
        c_ID2D1RenderTarget_FillRectangle(ctx->rt, &rect, (c_ID2D1Brush*) ctx->solid_brush);
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &ctx->default_color);
    }

    /* If provided, use the given foreground color. */
    if(effect->mask & XDWRITE_EFFECT_MASK_COLOR) {
        c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &effect->color);
    }
}

static HRESULT STDMETHODCALLTYPE
xtr_QueryInterface(c_IDWriteTextRenderer* self, REFIID riid, void** obj)
{
    if(IsEqualGUID(riid, &IID_IUnknown)  ||
       IsEqualGUID(riid, &c_IID_IDWritePixelSnapping)  ||
       IsEqualGUID(riid, &c_IID_IDWriteTextRenderer))
    {
        XDWRITE_TRACE_GUID("xtr_QueryInterface", riid);
        *obj = self;
        return S_OK;
    } else {
        XDWRITE_TRACE_GUID("xtr_QueryInterface: unsupported GUID", riid);
        *obj = NULL;
        return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
xtr_AddRef_Release(c_IDWriteTextRenderer* self)
{
    /* Dummy AddRef/Release() - we never really release this object, so
     * we may just return any non-zero. */
    return 42;
}

static HRESULT STDMETHODCALLTYPE
xtr_IsPixelSnappingDisabled(c_IDWriteTextRenderer* self, void* context, BOOL* p_disabled)
{
    XDWRITE_TRACE("xtr_IsPixelSnappingDisabled()");
    *p_disabled = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
xtr_GetCurrentTransform(c_IDWriteTextRenderer* self, void* context, c_DWRITE_MATRIX* p_matrix)
{
    xdwrite_ctx_t* ctx = (xdwrite_ctx_t*) context;

    XDWRITE_TRACE("xtr_GetCurrentTransform()");

    /* Note c_D2D1_MATRIX_3X2_F and c_DWRITE_MATRIX are binary-compatible. */
    c_ID2D1RenderTarget_GetTransform(ctx->rt, (c_D2D1_MATRIX_3X2_F*) p_matrix);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
xtr_GetPixelsPerDip(c_IDWriteTextRenderer* self, void* context, float* p_ppd)
{
    xdwrite_ctx_t* ctx = (xdwrite_ctx_t*) context;
    float dpi_x, dpi_y;

    XDWRITE_TRACE("xtr_GetPixelsPerDip()");

    c_ID2D1RenderTarget_GetDpi(ctx->rt, &dpi_x, &dpi_y);
    *p_ppd = dpi_x / 96.0f;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
xtr_DrawGlyphRun(c_IDWriteTextRenderer* self, void* context, float x, float y,
                 c_DWRITE_MEASURING_MODE measuring_mode,
                 c_DWRITE_GLYPH_RUN const* run,
                 c_DWRITE_GLYPH_RUN_DESCRIPTION const* desc,
                 IUnknown* effect)
{
    xdwrite_ctx_t* ctx = (xdwrite_ctx_t*) context;
    c_D2D1_POINT_2F pt = { x, y };

    XDWRITE_TRACE("xtr_DrawGlyphRun()");

    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &ctx->default_color);
    xtr_apply_effect(ctx, x, y, run, effect);
    c_ID2D1RenderTarget_DrawGlyphRun(ctx->rt, pt, run,
                    (c_ID2D1Brush*) ctx->solid_brush, measuring_mode);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
xtr_DrawUnderline(c_IDWriteTextRenderer* self, void* context, float x, float y,
                  c_DWRITE_UNDERLINE const* u, IUnknown* effect)
{
    xdwrite_ctx_t* ctx = (xdwrite_ctx_t*) context;
    c_D2D1_POINT_2F pt0 = { x, y + u->offset + 0.5f };
    c_D2D1_POINT_2F pt1 = { x + u->width, y + u->offset + 0.5f };

    XDWRITE_TRACE("xtr_DrawUnderline(y: %f, offset: %f, thickness: %f)",
                    (double)y, (double)u->offset, (double)u->thickness);

    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &ctx->default_color);

    xtr_apply_effect(ctx, x, y, NULL, effect);
    c_ID2D1RenderTarget_DrawLine(ctx->rt, pt0, pt1,
                (c_ID2D1Brush*) ctx->solid_brush, u->thickness, NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
xtr_DrawStrikethrough(c_IDWriteTextRenderer* self, void* context, float x, float y,
                      c_DWRITE_STRIKETHROUGH const* s, IUnknown* effect)
{
    xdwrite_ctx_t* ctx = (xdwrite_ctx_t*) context;
    c_D2D1_POINT_2F pt0 = { x, y + s->offset + 0.5f };
    c_D2D1_POINT_2F pt1 = { x + s->width, y + s->offset + 0.5f };

    XDWRITE_TRACE("xtr_DrawStrikethrough(y: %f, offset: %f, thickness: %f)",
                    (double)y, (double)s->offset, (double)s->thickness);

    c_ID2D1SolidColorBrush_SetColor(ctx->solid_brush, &ctx->default_color);

    xtr_apply_effect(ctx, x, y, NULL, effect);
    c_ID2D1RenderTarget_DrawLine(ctx->rt, pt0, pt1,
                (c_ID2D1Brush*) ctx->solid_brush, s->thickness, NULL);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
xtr_DrawInlineObject(c_IDWriteTextRenderer* self, void* context, float x, float y,
                     c_IDWriteInlineObject* inline_object, BOOL is_sideways,
                     BOOL is_rtl, IUnknown* effect)
{
    XDWRITE_TRACE("xtr_DrawInlineObject()");
    return E_NOTIMPL;
}

static c_IDWriteTextRendererVtbl xdwrite_text_renderer_vtbl = {
    xtr_QueryInterface,
    xtr_AddRef_Release,
    xtr_AddRef_Release,
    xtr_IsPixelSnappingDisabled,
    xtr_GetCurrentTransform,
    xtr_GetPixelsPerDip,
    xtr_DrawGlyphRun,
    xtr_DrawUnderline,
    xtr_DrawStrikethrough,
    xtr_DrawInlineObject
};

c_IDWriteTextRenderer xdwrite_text_renderer_ = { &xdwrite_text_renderer_vtbl };


/*****************************
 *** Module Initialization ***
 *****************************/

int
xdwrite_init_module(void)
{
    HRESULT (WINAPI* fn_DWriteCreateFactory)(int, REFIID, IUnknown**);
    HRESULT hr;

    xdwrite_dll = mc_load_sys_dll(_T("DWRITE.DLL"));
    if(MC_ERR(xdwrite_dll == NULL)) {
        MC_TRACE_ERR("xdwrite_init_module: mc_load_sys_dll(DWRITE.DLL) failed.");
        goto err_LoadLibrary;
    }

    fn_DWriteCreateFactory = (HRESULT (WINAPI*)(int, REFIID, IUnknown**))
                GetProcAddress(xdwrite_dll, "DWriteCreateFactory");
    if(MC_ERR(fn_DWriteCreateFactory == NULL)) {
        MC_TRACE_ERR("xdwrite_init_module: GetProcAddress(DWriteCreateFactory) failed.");
        goto err_GetProcAddress;
    }

    hr = fn_DWriteCreateFactory(c_DWRITE_FACTORY_TYPE_SHARED,
                &c_IID_IDWriteFactory, (IUnknown**) &xdwrite_factory);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_init_module: DWriteCreateFactory() failed.");
        goto err_DWriteCreateFactory;
    }

    /* Success. */
    return 0;

    /* Error unrolling. */
err_DWriteCreateFactory:
err_GetProcAddress:
    FreeLibrary(xdwrite_dll);
err_LoadLibrary:
    return -1;
}

void
xdwrite_fini_module(void)
{
    c_IDWriteFactory_Release(xdwrite_factory);
    FreeLibrary(xdwrite_dll);
}
