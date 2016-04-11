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

#include "misc.h"
#include "backend-d2d.h"
#include "backend-dwrite.h"
#include "backend-gdix.h"
#include "lock.h"


static void
wd_get_default_gui_fontface(WCHAR buffer[LF_FACESIZE])
{
    NONCLIENTMETRICS metrics;

    metrics.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, (void*) &metrics, 0);
    wcsncpy(buffer, metrics.lfMessageFont.lfFaceName, LF_FACESIZE);
}


WD_HFONT
wdCreateFont(const LOGFONTW* pLogFont)
{
    if(d2d_enabled()) {
        static WCHAR no_locale[] = L"";
        static WCHAR enus_locale[] = L"en-us";

        WCHAR user_locale[LOCALE_NAME_MAX_LENGTH];
        WCHAR* locales[] = { user_locale, no_locale, enus_locale };
        WCHAR default_fontface[LF_FACESIZE];
        int i;
        dummy_IDWriteTextFormat* tf;
        float size;
        dummy_DWRITE_FONT_STYLE style;
        dummy_DWRITE_FONT_WEIGHT weight;
        HRESULT hr;

        dwrite_default_user_locale(user_locale);

        /* DirectWrite does not support "default" font size. */
        size = (pLogFont->lfHeight != 0 ? WD_ABS(pLogFont->lfHeight) : 12.0f);

        if(pLogFont->lfItalic)
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
        weight = (pLogFont->lfWeight != FW_DONTCARE ? pLogFont->lfWeight : FW_REGULAR);

        for(i = 0; i < WD_SIZEOF_ARRAY(locales); i++) {
            hr = dummy_IDWriteFactory_CreateTextFormat(dwrite_factory,
                    pLogFont->lfFaceName, NULL, weight, style,
                    dummy_DWRITE_FONT_STRETCH_NORMAL, size, locales[i], &tf);
            if(SUCCEEDED(hr))
                return (WD_HFONT) tf;
        }

        /* In case of a failure, try to fall back to a reasonable the default
         * font. */
        wd_get_default_gui_fontface(default_fontface);
        for(i = 0; i < WD_SIZEOF_ARRAY(locales); i++) {
            hr = dummy_IDWriteFactory_CreateTextFormat(dwrite_factory,
                    default_fontface, NULL, weight, style,
                    dummy_DWRITE_FONT_STRETCH_NORMAL, size, locales[i], &tf);
            if(SUCCEEDED(hr))
                return (WD_HFONT) tf;
        }

        WD_TRACE("wdCreateFont: "
                 "IDWriteFactory::CreateTextFormat(%S, %S) failed. [0x%lx]",
                 pLogFont->lfFaceName, user_locale, hr);
        return NULL;
    } else {
        HDC dc;
        dummy_GpFont* f;
        int status;

        dc = GetDC(NULL);
        status = gdix_CreateFontFromLogfontW(dc, pLogFont, &f);
        if(status != 0) {
            LOGFONTW fallback_logfont;

            /* Failure: This may happen because GDI+ does not support
             * non-TrueType fonts. Fallback to default GUI font, typically
             * Tahoma or Segoe UI on newer Windows. */
            memcpy(&fallback_logfont, pLogFont, sizeof(LOGFONTW));
            wd_get_default_gui_fontface(fallback_logfont.lfFaceName);
            status = gdix_CreateFontFromLogfontW(dc, &fallback_logfont, &f);
        }
        ReleaseDC(NULL, dc);

        if(status != 0) {
            WD_TRACE("wdCreateFont: GdipCreateFontFromLogfontW(%S) failed. [%d]",
                     pLogFont->lfFaceName, status);
            return NULL;
        }

        return (WD_HFONT) f;
    }
}

WD_HFONT
wdCreateFontWithGdiHandle(HFONT hGdiFont)
{
    LOGFONTW lf;

    if(hGdiFont == NULL)
        hGdiFont = GetStockObject(SYSTEM_FONT);

    GetObjectW(hGdiFont, sizeof(LOGFONTW), &lf);
    return wdCreateFont(&lf);
}

void
wdDestroyFont(WD_HFONT hFont)
{
    if(d2d_enabled()) {
        dummy_IDWriteTextFormat* tf = (dummy_IDWriteTextFormat*) hFont;
        dummy_IDWriteTextFormat_Release(tf);
    } else {
        gdix_DeleteFont((dummy_GpFont*) hFont);
    }
}

void
wdFontMetrics(WD_HFONT hFont, WD_FONT_METRICS* pMetrics)
{
    if(hFont == NULL) {
        /* Treat NULL as "no font". This simplifies paint code when font
         * creation fails. */
        WD_TRACE("wdFontMetrics: font == NULL");
        goto err;
    }

    if(d2d_enabled()) {
        dummy_IDWriteTextFormat* tf = (dummy_IDWriteTextFormat*) hFont;
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
        if(name == NULL) {
            WD_TRACE("wdFontMetrics: _malloca() failed.");
            goto err_malloca;
        }
        hr = dummy_IDWriteTextFormat_GetFontFamilyName(tf, name, name_len);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdFontMetrics: "
                        "IDWriteTextFormat::GetFontFamilyName() failed.");
            goto err_GetFontFamilyName;
        }

        weight = dummy_IDWriteTextFormat_GetFontWeight(tf);
        stretch = dummy_IDWriteTextFormat_GetFontStretch(tf);
        style = dummy_IDWriteTextFormat_GetFontStyle(tf);

        hr = dummy_IDWriteTextFormat_GetFontCollection(tf, &fc);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdFontMetrics: "
                        "IDWriteTextFormat::GetFontCollection() failed.");
            goto err_GetFontCollection;
        }

        hr = dummy_IDWriteFontCollection_FindFamilyName(fc, name, &ix, &exists);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdFontMetrics: "
                        "IDWriteFontCollection::FindFamilyName() failed.");
            goto err_FindFamilyName;
        }

        if(!exists) {
            /* For some reason, this happens for "SYSTEM" font family on Win7. */
            WD_TRACE("wdFontMetrics: WTF? Font does not exist? (%S)", name);
            goto err_exists;
        }

        hr = dummy_IDWriteFontCollection_GetFontFamily(fc, ix, &ff);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdFontMetrics: "
                        "IDWriteFontCollection::GetFontFamily() failed.");
            goto err_GetFontFamily;
        }

        hr = dummy_IDWriteFontFamily_GetFirstMatchingFont(ff, weight, stretch, style, &f);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdFontMetrics: "
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

        pMetrics->fEmHeight = dummy_IDWriteTextFormat_GetFontSize(tf);
        if(ok) {
            factor = (pMetrics->fEmHeight / fm.designUnitsPerEm);
            pMetrics->fAscent = fm.ascent * factor;
            pMetrics->fDescent = WD_ABS(fm.descent * factor);
            pMetrics->fLeading = (fm.ascent + fm.descent + fm.lineGap) * factor;
        } else {
            /* Something above failed. Lets invent some sane defaults. */
            pMetrics->fAscent = 0.9f * pMetrics->fEmHeight;
            pMetrics->fDescent = 0.1f * pMetrics->fEmHeight;
            pMetrics->fLeading = 1.1f * pMetrics->fEmHeight;
        }
    } else {
        int font_style;
        float font_size;
        void* font_family;
        UINT16 cell_ascent;
        UINT16 cell_descent;
        UINT16 em_height;
        UINT16 line_spacing;
        int status;

        gdix_GetFontSize((void*) hFont, &font_size);
        gdix_GetFontStyle((void*) hFont, &font_style);

        status = gdix_GetFamily((void*) hFont, &font_family);
        if(status != 0) {
            WD_TRACE("wdFontMetrics: GdipGetFamily() failed. [%d]", status);
            goto err;
        }
        gdix_GetCellAscent(font_family, font_style, &cell_ascent);
        gdix_GetCellDescent(font_family, font_style, &cell_descent);
        gdix_GetEmHeight(font_family, font_style, &em_height);
        gdix_GetLineSpacing(font_family, font_style, &line_spacing);
        gdix_DeleteFontFamily(font_family);

        pMetrics->fEmHeight = font_size;
        pMetrics->fAscent = font_size * (float)cell_ascent / (float)em_height;
        pMetrics->fDescent = WD_ABS(font_size * (float)cell_descent / (float)em_height);
        pMetrics->fLeading = font_size * (float)line_spacing / (float)em_height;
    }

    return;

err:
    pMetrics->fEmHeight = 0.0f;
    pMetrics->fAscent = 0.0f;
    pMetrics->fDescent = 0.0f;
    pMetrics->fLeading = 0.0f;
}
