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

#include "backend-dwrite.h"


static const GUID dwrite_IID_ID2D1Factory =
        {0x06152247,0x6f50,0x465a,{0x92,0x45,0x11,0x8b,0xfd,0x3b,0x60,0x07}};
static const GUID dwrite_IID_ID2D1GdiInteropRenderTarget =
        {0xe0db51c3,0x6f77,0x4bae,{0xb3,0xd5,0xe4,0x75,0x09,0xb3,0x58,0x38}};

static HMODULE dwrite_dll;

dummy_IDWriteFactory* dwrite_factory = NULL;


static int (WINAPI* fn_GetUserDefaultLocaleName)(WCHAR*, int) = NULL;

int
dwrite_init(void)
{
    HMODULE dll_kernel32;
    HRESULT (WINAPI* fn_DWriteCreateFactory)(int, REFIID, void**);
    HRESULT hr;

    dwrite_dll = wd_load_system_dll(_T("DWRITE.DLL"));
    if(dwrite_dll == NULL) {
        WD_TRACE_ERR("dwrite_init: LoadLibrary('DWRITE.DLL') failed.");
        goto err_LoadLibrary;
    }

    fn_DWriteCreateFactory = (HRESULT (WINAPI*)(int, REFIID, void**))
                GetProcAddress(dwrite_dll, "DWriteCreateFactory");
    if(fn_DWriteCreateFactory == NULL) {
        WD_TRACE_ERR("dwrite_init: "
                     "GetProcAddress('DWriteCreateFactory') failed.");
        goto err_GetProcAddress;
    }

    hr = fn_DWriteCreateFactory(dummy_DWRITE_FACTORY_TYPE_SHARED,
                &dummy_IID_IDWriteFactory, (void**) &dwrite_factory);
    if(FAILED(hr)) {
        WD_TRACE_HR("dwrite_init: DWriteCreateFactory() failed.");
        goto err_DWriteCreateFactory;
    }

    /* We need locale name for creation of dummy_IDWriteTextFormat. This
     * functions is available since Vista (which covers all systems with
     * Direct2D and DirectWrite). */
    dll_kernel32 = GetModuleHandle(_T("KERNEL32.DLL"));
    if(dll_kernel32 != NULL) {
        fn_GetUserDefaultLocaleName = (int (WINAPI*)(WCHAR*, int))
                GetProcAddress(dll_kernel32, "GetUserDefaultLocaleName");
    }

    /* Success. */
    return 0;

    /* Error path unwinding. */
err_DWriteCreateFactory:
err_GetProcAddress:
    FreeLibrary(dwrite_dll);
err_LoadLibrary:
    return -1;
}

void
dwrite_fini(void)
{
    dummy_IDWriteFactory_Release(dwrite_factory);
    FreeLibrary(dwrite_dll);
    dwrite_factory = NULL;
}

void
dwrite_default_user_locale(WCHAR buffer[LOCALE_NAME_MAX_LENGTH])
{
    if(fn_GetUserDefaultLocaleName != NULL) {
        if(fn_GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH) > 0)
            return;
        WD_TRACE_ERR("dwrite_default_user_locale: "
                     "GetUserDefaultLocaleName() failed.");
    } else {
        WD_TRACE_ERR("dwrite_default_user_locale: "
                     "function GetUserDefaultLocaleName() not available.");
    }

    buffer[0] = L'\0';
}

dummy_IDWriteTextLayout*
dwrite_create_text_layout(dummy_IDWriteTextFormat* tf, const WD_RECT* rect,
                          const WCHAR* str, int len, DWORD flags)
{
    dummy_IDWriteTextLayout* layout;
    HRESULT hr;
    int tla;

    if(len < 0)
        len = wcslen(str);

    hr = dummy_IDWriteFactory_CreateTextLayout(dwrite_factory, str, len, tf,
                rect->x1 - rect->x0, rect->y1 - rect->y0, &layout);
    if(FAILED(hr)) {
        WD_TRACE_HR("dwrite_create_text_layout: "
                    "IDWriteFactory::CreateTextLayout() failed.");
        return NULL;
    }

    if(flags & WD_STR_RIGHTALIGN)
        tla = dummy_DWRITE_TEXT_ALIGNMENT_TRAILING;
    else if(flags & WD_STR_CENTERALIGN)
        tla = dummy_DWRITE_TEXT_ALIGNMENT_CENTER;
    else
        tla = dummy_DWRITE_TEXT_ALIGNMENT_LEADING;
    dummy_IDWriteTextLayout_SetTextAlignment(layout, tla);

    if(flags & WD_STR_NOWRAP)
        dummy_IDWriteTextLayout_SetWordWrapping(layout, dummy_DWRITE_WORD_WRAPPING_NO_WRAP);

    if((flags & WD_STR_ELLIPSISMASK) != 0) {
        static const dummy_DWRITE_TRIMMING trim_end = { dummy_DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        static const dummy_DWRITE_TRIMMING trim_word = { dummy_DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
        static const dummy_DWRITE_TRIMMING trim_path = { dummy_DWRITE_TRIMMING_GRANULARITY_WORD, L'\\', 1 };

        const dummy_DWRITE_TRIMMING* trim_options = NULL;
        dummy_IDWriteInlineObject* trim_sign;

        hr = dummy_IDWriteFactory_CreateEllipsisTrimmingSign(dwrite_factory, tf, &trim_sign);
        if(FAILED(hr)) {
            WD_TRACE_HR("dwrite_create_text_layout: "
                        "IDWriteFactory::CreateEllipsisTrimmingSign() failed.");
            goto err_CreateEllipsisTrimmingSign;
        }

        switch(flags & WD_STR_ELLIPSISMASK) {
            case WD_STR_ENDELLIPSIS:    trim_options = &trim_end; break;
            case WD_STR_WORDELLIPSIS:   trim_options = &trim_word; break;
            case WD_STR_PATHELLIPSIS:   trim_options = &trim_path; break;
        }

        if(trim_options != NULL)
            dummy_IDWriteTextLayout_SetTrimming(layout, trim_options, trim_sign);

        dummy_IDWriteInlineObject_Release(trim_sign);
    }

err_CreateEllipsisTrimmingSign:
    return layout;
}
