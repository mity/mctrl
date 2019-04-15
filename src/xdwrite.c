/*
 * Copyright (c) 2019 Martin Mitas
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

    hr = c_IDWriteFactory_CreateTextLayout(xdwrite_factory, str, len, tf, max_width, max_height, &tl);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwrite_create_text_layout: IDWriteFactory::CreateTextLayout() failed.");
        return NULL;
    }

    if(flags & XDWRITE_ALIGN_MASK) {
        switch(flags & XDWRITE_ALIGN_MASK) {
            case XDWRITE_ALIGN_CENTER:  align = c_DWRITE_TEXT_ALIGNMENT_CENTER; break;
            case XDWRITE_ALIGN_RIGHT:   align = c_DWRITE_TEXT_ALIGNMENT_TRAILING; break;
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
