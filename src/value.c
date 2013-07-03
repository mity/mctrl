/*
 * Copyright (c) 2010-2013 Martin Mitas
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

#include "value.h"


static UINT
draw_text_format(DWORD flags, UINT defaults)
{
    UINT format = 0;
    switch(flags & VALUE_PF_ALIGNMASKHORZ) {
        case VALUE_PF_ALIGNLEFT: format |= DT_LEFT; break;
        case VALUE_PF_ALIGNCENTER: format |= DT_CENTER; break;
        case VALUE_PF_ALIGNRIGHT: format |= DT_RIGHT; break;
        default: format |= defaults & (DT_LEFT | DT_CENTER | DT_RIGHT); break;
    }
    switch(flags & VALUE_PF_ALIGNMASKVERT) {
        case VALUE_PF_ALIGNTOP: format |= DT_TOP; break;
        case VALUE_PF_ALIGNVCENTER: format |= DT_VCENTER; break;
        case VALUE_PF_ALIGNBOTTOM: format |= DT_BOTTOM; break;
        default: format |= defaults & (DT_TOP | DT_VCENTER | DT_BOTTOM); break;
    }
    return format;
}


/***************************************
 *** Reusable Method Implementations ***
 ***************************************/

static value_t* __stdcall
flat32_ctor_val(const value_t* v)
{
    value_t* copy;

    copy = (value_t*) malloc(sizeof(value_t) + sizeof(uint32_t));
    if(copy)
        memcpy(copy, v, sizeof(value_t) + sizeof(uint32_t));
    return copy;
}

static value_t* __stdcall
flat64_ctor_val(const value_t* v)
{
    value_t* copy;

    copy = (value_t*) malloc(sizeof(value_t) + sizeof(uint64_t));
    if(copy)
        memcpy(copy, v, sizeof(value_t) + sizeof(uint64_t));
    return copy;
}

#ifdef _WIN64
    MC_STATIC_ASSERT(sizeof(void*) == sizeof(uint64_t));
    #define flatptr_ctor_val   flat64_ctor_val
#else
    MC_STATIC_ASSERT(sizeof(void*) == sizeof(uint32_t));
    #define flatptr_ctor_val   flat32_ctor_val
#endif

static void __stdcall
flat_dtor(value_t* v)
{
    free(v);
}

static void __stdcall
ptr_dtor(value_t* v)
{
    void* ptr = VALUE_DATA(v, void*);

    if(ptr)
        free(ptr);
    free(v);
}


/****************************
 *** Int32 Implementation ***
 ****************************/

static value_t* __stdcall
int32_ctor_str(const TCHAR* str)
{
    int sign = +1;
    int32_t i;

    if(MC_ERR(*str == _T('\0')))  /* empty string? */
        goto err_invalid;

    if(*str == _T('-')) {
        sign = -1;
        str++;
    } else if(*str == _T('+')) {
        str++;
    }

    if(MC_ERR(*str == _T('\0')))  /* no digits? */
        goto err_invalid;

    i = 0;
    while(_T('0') <= *str  &&  *str <=  _T('9')) {
        i = i * 10 + (*str-_T('0'));
        str++;
    }

    if(MC_ERR(*str != _T('\0')))  /* unexpected chars? */
        goto err_invalid;

    return (value_t*) mcValue_CreateInt32(sign * i);

err_invalid:
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

static int __stdcall
int32_cmp(const value_t* v1, const value_t* v2)
{
    if(VALUE_DATA(v1, int32_t) < VALUE_DATA(v2, int32_t))
        return -1;
    if(VALUE_DATA(v1, int32_t) > VALUE_DATA(v2, int32_t))
        return +1;
    return 0;
}

static size_t __stdcall
int32_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%d"), VALUE_DATA(v, int32_t));
        buffer[bufsize-1] = _T('\0');
    }
    if(MC_ERR(n < 0)) {
        /* Buffer too small: Return required buffer size. */
        TCHAR tmp[16];
        n = _stprintf(tmp, _T("%d"), VALUE_DATA(v, int32_t));
    }
    return n+1;  /* +1 for '\0' */
}

static void __stdcall
int32_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[16];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%d"), VALUE_DATA(v, int32_t));
    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, (RECT*) rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}

static const type_t int32_type = {
    int32_ctor_str,
    flat32_ctor_val,
    flat_dtor,
    int32_cmp,
    int32_dump,
    int32_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateInt32(INT iValue)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(int32_t));
    if(v) {
        v->type = &int32_type;
        VALUE_DATA(v, int32_t) = iValue;
    }
    return (MC_HVALUE) v;
}

INT MCTRL_API
mcValue_GetInt32(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &int32_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    return VALUE_DATA(v, int32_t);
}


/*****************************
 *** UInt32 Implementation ***
 *****************************/

static value_t* __stdcall
uint32_ctor_str(const TCHAR* str)
{
    uint32_t u;

    if(MC_ERR(*str == _T('\0')))  /* empty string? */
        goto err_invalid;

    if(*str == _T('+'))
        str++;

    if(MC_ERR(*str == _T('\0')))  /* no digits? */
        goto err_invalid;

    u = 0;
    while(_T('0') <= *str  &&  *str <=  _T('9')) {
        u = u * 10 + (*str-_T('0'));
        str++;
    }

    if(MC_ERR(*str != _T('\0')))  /* unexpected chars? */
        goto err_invalid;

    return (value_t*) mcValue_CreateUInt32(u);

err_invalid:
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

static int __stdcall
uint32_cmp(const value_t* v1, const value_t* v2)
{
    if(VALUE_DATA(v1, uint32_t) < VALUE_DATA(v2, uint32_t))
        return -1;
    if(VALUE_DATA(v1, uint32_t) > VALUE_DATA(v2, uint32_t))
        return +1;
    return 0;
}

static size_t __stdcall
uint32_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%u"), VALUE_DATA(v, uint32_t));
        buffer[bufsize-1] = _T('\0');
    }
    if(MC_ERR(n < 0)) {
        /* Buffer too small: Return required buffer size. */
        TCHAR tmp[16];
        n = _stprintf(tmp, _T("%u"), VALUE_DATA(v, uint32_t));
    }
    return n+1;  /* +1 for '\0' */
}

static void __stdcall
uint32_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[16];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%u"), VALUE_DATA(v, uint32_t));
    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}

static const type_t uint32_type = {
    uint32_ctor_str,
    flat32_ctor_val,
    flat_dtor,
    uint32_cmp,
    uint32_dump,
    uint32_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateUInt32(UINT uValue)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(uint32_t));
    if(v) {
        v->type = &uint32_type;
        VALUE_DATA(v, uint32_t) = uValue;
    }
    return (MC_HVALUE) v;
}

UINT MCTRL_API
mcValue_GetUInt32(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &uint32_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    return VALUE_DATA(v, uint32_t);
}


/****************************
 *** Int64 Implementation ***
 ****************************/

static value_t* __stdcall
int64_ctor_str(const TCHAR* str)
{
    int sign = +1;
    int64_t i;

    if(MC_ERR(*str == _T('\0')))  /* empty string? */
        goto err_invalid;

    if(*str == _T('-')) {
        sign = -1;
        str++;
    } else if(*str == _T('+')) {
        str++;
    }

    if(MC_ERR(*str == _T('\0')))  /* no digits? */
        goto err_invalid;

    i = 0;
    while(_T('0') <= *str  &&  *str <=  _T('9')) {
        i = i * 10 + (*str-_T('0'));
        str++;
    }

    if(MC_ERR(*str != _T('\0')))  /* unexpected chars? */
        goto err_invalid;

    return (value_t*) mcValue_CreateInt64(sign * i);

err_invalid:
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

static int __stdcall
int64_cmp(const value_t* v1, const value_t* v2)
{
    if(VALUE_DATA(v1, int64_t) < VALUE_DATA(v2, int64_t))
        return -1;
    if(VALUE_DATA(v1, int64_t) > VALUE_DATA(v2, int64_t))
        return +1;
    return 0;
}

static size_t __stdcall
int64_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%d"), VALUE_DATA(v, int64_t));
        buffer[bufsize-1] = _T('\0');
    }
    if(MC_ERR(n < 0)) {
        /* Buffer too small: Return required buffer size. */
        TCHAR tmp[16];
        n = _stprintf(tmp, _T("%d"), VALUE_DATA(v, int64_t));
    }
    return n+1;  /* +1 for '\0' */
}

static void __stdcall
int64_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[16];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%d"), VALUE_DATA(v, int64_t));
    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}

static const type_t int64_type = {
    int64_ctor_str,
    flat64_ctor_val,
    flat_dtor,
    int64_cmp,
    int64_dump,
    int64_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateInt64(INT iValue)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(int64_t));
    if(v) {
        v->type = &int64_type;
        VALUE_DATA(v, int64_t) = iValue;
    }
    return (MC_HVALUE) v;
}

INT MCTRL_API
mcValue_GetInt64(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &int64_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    return VALUE_DATA(v, int64_t);
}


/*****************************
 *** UInt64 Implementation ***
 *****************************/

static value_t* __stdcall
uint64_ctor_str(const TCHAR* str)
{
    uint64_t u;

    if(MC_ERR(*str == _T('\0')))  /* empty string? */
        goto err_invalid;

    if(*str == _T('+'))
        str++;

    if(MC_ERR(*str == _T('\0')))  /* no digits? */
        goto err_invalid;

    u = 0;
    while(_T('0') <= *str  &&  *str <=  _T('9')) {
        u = u * 10 + (*str-_T('0'));
        str++;
    }

    if(MC_ERR(*str != _T('\0')))  /* unexpected chars? */
        goto err_invalid;

    return (value_t*) mcValue_CreateUInt64(u);

err_invalid:
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

static int __stdcall
uint64_cmp(const value_t* v1, const value_t* v2)
{
    if(VALUE_DATA(v1, uint64_t) < VALUE_DATA(v2, uint64_t))
        return -1;
    if(VALUE_DATA(v1, uint64_t) > VALUE_DATA(v2, uint64_t))
        return +1;
    return 0;
}

static size_t __stdcall
uint64_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%u"), VALUE_DATA(v, uint64_t));
        buffer[bufsize-1] = _T('\0');
    }
    if(MC_ERR(n < 0)) {
        /* Buffer too small: Return required buffer size. */
        TCHAR tmp[16];
        n = _stprintf(tmp, _T("%u"), VALUE_DATA(v, uint64_t));
    }
    return n+1;  /* +1 for '\0' */
}

static void __stdcall
uint64_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[16];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%u"), VALUE_DATA(v, uint64_t));
    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}

static const type_t uint64_type = {
    uint64_ctor_str,
    flat64_ctor_val,
    flat_dtor,
    uint64_cmp,
    uint64_dump,
    uint64_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateUInt64(UINT uValue)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(uint64_t));
    if(v) {
        v->type = &uint64_type;
        VALUE_DATA(v, uint64_t) = uValue;
    }
    return (MC_HVALUE) v;
}

UINT MCTRL_API
mcValue_GetUInt64(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &uint64_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    return VALUE_DATA(v, uint64_t);
}


/***********************************
 *** StringW type implementation ***
 ***********************************/

static const WCHAR strw_empty[] = L"";

static value_t* __stdcall
strw_ctor_str(const TCHAR* str)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(WCHAR*));
    if(MC_ERR(v == NULL))
        return NULL;

    VALUE_DATA(v, WCHAR*) = mc_str(str, MC_STRT, MC_STRW);
    if(MC_ERR(VALUE_DATA(v, WCHAR*) == NULL)) {
        free(v);
        return NULL;
    }

    return (MC_HVALUE) v;
}

static value_t* __stdcall
strw_ctor_val(const value_t* v)
{
    return mcValue_CreateStringW(VALUE_DATA(v, WCHAR*));
}

static int __stdcall
strw_cmp(const value_t* v1, const value_t* v2)
{
    const WCHAR* s1 = VALUE_DATA(v1, const WCHAR*);
    const WCHAR* s2 = VALUE_DATA(v2, const WCHAR*);

    return wcscmp((s1 != NULL ? s1 : strw_empty), (s2 != NULL ? s2 : strw_empty));
}

static size_t __stdcall
strw_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    const WCHAR* s = VALUE_DATA(v, WCHAR*);
    if(s == NULL)
        s = strw_empty;

    if(bufsize > 0)
        mc_str_inbuf(s, MC_STRW, buffer, MC_STRT, bufsize);

#ifdef UNICODE
    return wcslen(s) + 1;
#else
    return WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
#endif
}

static void __stdcall
strw_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    int old_bkmode;
    COLORREF old_color;
    const WCHAR* s = VALUE_DATA(v, const WCHAR*);

    if(s == NULL)
        return;

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextW(dc, s, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
              draw_text_format(flags, DT_LEFT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}

static const type_t strw_type = {
    strw_ctor_str,
    strw_ctor_val,
    ptr_dtor,
    strw_cmp,
    strw_dump,
    strw_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateStringW(const WCHAR* lpszStr)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(WCHAR*));
    if(!v)
        return NULL;

    v->type = &strw_type;
    if(lpszStr != NULL) {
        VALUE_DATA(v, WCHAR*) = mc_str_W2W(lpszStr);
        if(MC_ERR(VALUE_DATA(v, WCHAR*) == NULL)) {
            free(v);
            return NULL;
        }
    } else {
        VALUE_DATA(v, WCHAR*) = NULL;
    }

    return (MC_HVALUE) v;
}

const WCHAR* MCTRL_API
mcValue_GetStringW(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &strw_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return VALUE_DATA(v, WCHAR*);
}


/***********************************
 *** StringA type implementation ***
 ***********************************/

static const char stra_empty[] = "";

static value_t* __stdcall
stra_ctor_str(const TCHAR* str)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(char*));
    if(MC_ERR(v == NULL))
        return NULL;

    VALUE_DATA(v, char*) = mc_str(str, MC_STRT, MC_STRA);
    if(MC_ERR(VALUE_DATA(v, char*) == NULL)) {
        free(v);
        return NULL;
    }

    return (MC_HVALUE) v;
}

static value_t* __stdcall
stra_ctor_val(const value_t* v)
{
    return mcValue_CreateStringA(VALUE_DATA(v, const char*));
}

static int __stdcall
stra_cmp(const value_t* v1, const value_t* v2)
{
    const char* s1 = VALUE_DATA(v1, const char*);
    const char* s2 = VALUE_DATA(v2, const char*);

    return strcmp((s1 != NULL ? s1 : stra_empty), (s2 != NULL ? s2 : stra_empty));
}

static size_t __stdcall
stra_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    const char* s = VALUE_DATA(v, const char*);

    if(s == NULL)
        s = stra_empty;

    if(bufsize > 0)
        mc_str_inbuf(s, MC_STRA, buffer, MC_STRT, bufsize);

#ifdef UNICODE
    return MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
#else
    return strlen(s) + 1;
#endif
}

static void __stdcall
stra_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    int old_bkmode;
    COLORREF old_color;
    const char* s = VALUE_DATA(v, const char*);

    if(s == NULL)
        return;

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextA(dc, s, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
              draw_text_format(flags, DT_LEFT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}

static const type_t stra_type = {
    stra_ctor_str,
    stra_ctor_val,
    ptr_dtor,
    stra_cmp,
    stra_dump,
    stra_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateStringA(const char* lpszStr)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(char*));
    if(!v)
        return NULL;

    v->type = &stra_type;

    if(lpszStr != NULL) {
        VALUE_DATA(v, char*) = mc_str_A2A(lpszStr);
        if(MC_ERR(VALUE_DATA(v, char*) == NULL)) {
            free(v);
            return NULL;
        }
    } else {
        VALUE_DATA(v, char*) = NULL;
    }

    return (MC_HVALUE) v;
}

const char* MCTRL_API
mcValue_GetStringA(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &stra_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return VALUE_DATA(v, char*);
}


/**************************************
 *** ImmStringW type implementation ***
 **************************************/

static const type_t immstrw_type = {
    NULL,
    flatptr_ctor_val,
    flat_dtor,
    strw_cmp,
    strw_dump,
    strw_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateImmStringW(const WCHAR* lpszStr)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(const WCHAR*));
    if(v) {
        v->type = &immstrw_type;
        VALUE_DATA(v, const WCHAR*) = lpszStr;
    }
    return (MC_HVALUE) v;
}

const WCHAR* MCTRL_API
mcValue_GetImmStringW(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &immstrw_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return VALUE_DATA(v, const WCHAR*);
}


/**************************************
 *** ImmStringA type implementation ***
 **************************************/

static const type_t immstra_type = {
    NULL,
    flatptr_ctor_val,
    flat_dtor,
    stra_cmp,
    stra_dump,
    stra_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateImmStringA(const char* lpszStr)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(const char*));
    if(v) {
        v->type = &immstra_type;
        VALUE_DATA(v, const char*) = lpszStr;
    }
    return (MC_HVALUE) v;
}

const char* MCTRL_API
mcValue_GetImmStringA(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &immstra_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return VALUE_DATA(v, const char*);
}


/*********************************
 *** Color type implementation ***
 *********************************/

static value_t* __stdcall
color_ctor_str(const TCHAR* str)
{
    const TCHAR* ptr = str;
    TCHAR* end;
    ULONG rgb;
    UCHAR r, g, b;

    /* We expect "#rrggbb" */
    if(*ptr != _T('#'))
        goto err_parse;
    ptr++;
    rgb = _tcstoul(ptr, &end, 16);
    if(end - ptr != 6  ||  *end != _T('\0'))
        goto err_parse;

    r = (rgb & 0xff0000) >> 16;
    g = (rgb & 0xff00) >> 8;
    b = (rgb & 0xff);
    return mcValue_CreateColor(RGB(r, g, b));

err_parse:
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

static size_t __stdcall
color_dump(const value_t* v, TCHAR* buffer, size_t bufsize)
{
    COLORREF color = VALUE_DATA(v, COLORREF);
    if(bufsize > 0) {
        _sntprintf(buffer, bufsize, _T("#%02x%02x%02x"), (UINT)GetRValue(color),
                   (UINT)GetGValue(color), (UINT)GetBValue(color));
        buffer[bufsize-1] = _T('\0');
    }
    return sizeof("#rrggbb");
}

static void __stdcall
color_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    COLORREF color = VALUE_DATA(v, COLORREF);
    HBRUSH brush;
    HBRUSH old_brush;
    HPEN old_pen;

    brush = CreateSolidBrush(color);
    old_brush = SelectObject(dc, brush);
    old_pen = SelectObject(dc, GetStockObject(BLACK_PEN));

    Rectangle(dc, rect->left + 2, rect->top + 2, rect->right - 2, rect->bottom - 2);

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
}

MC_STATIC_ASSERT(sizeof(COLORREF) == sizeof(uint32_t));

static const type_t color_type = {
    color_ctor_str,
    flat32_ctor_val,
    flat_dtor,
    NULL,
    color_dump,
    color_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateColor(COLORREF color)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(COLORREF));
    if(v) {
        v->type = &color_type;
        VALUE_DATA(v, COLORREF) = color;
    }
    return (MC_HVALUE) v;
}

COLORREF MCTRL_API
mcValue_GetColor(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &color_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return MC_CLR_NONE;
    }

    return VALUE_DATA(v, COLORREF);
}


/********************************
 *** Icon type implementation ***
 ********************************/

static void __stdcall
icon_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    HICON icon = VALUE_DATA(v, HICON);
    SIZE icon_size;
    int x, y;

    if(icon == NULL)
        return;

    mc_icon_size(icon, &icon_size);

    switch(flags & VALUE_PF_ALIGNMASKHORZ) {
        case VALUE_PF_ALIGNLEFT:    x = rect->left; break;
        case VALUE_PF_ALIGNDEFAULT: /* fall through */
        case VALUE_PF_ALIGNCENTER:  x = (rect->left + rect->right - icon_size.cx) / 2; break;
        case VALUE_PF_ALIGNRIGHT:   x = rect->right - icon_size.cx; break;
    }

    switch(flags & VALUE_PF_ALIGNMASKVERT) {
        case VALUE_PF_ALIGNTOP:      y = rect->top; break;
        case VALUE_PF_ALIGNVDEFAULT: /* fall through */
        case VALUE_PF_ALIGNVCENTER:  y = (rect->top + rect->bottom - icon_size.cy) / 2; break;
        case VALUE_PF_ALIGNBOTTOM:   y = rect->bottom - icon_size.cy; break;
    }

    DrawIconEx(dc, x, y, icon, 0, 0, 0, NULL, DI_NORMAL);
}

MC_STATIC_ASSERT(sizeof(HICON) == sizeof(void*));

static const type_t icon_type = {
    NULL,
    flatptr_ctor_val,
    flat_dtor,
    NULL,
    NULL,
    icon_paint
};

MC_HVALUE MCTRL_API
mcValue_CreateIcon(HICON hIcon)
{
    value_t* v;

    v = (value_t*) malloc(sizeof(value_t) + sizeof(HICON));
    if(v) {
        v->type = &icon_type;
        VALUE_DATA(v, HICON) = hIcon;
    }
    return (MC_HVALUE) v;
}

HICON MCTRL_API
mcValue_GetIcon(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL  ||  v->type != &icon_type)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return VALUE_DATA(v, HICON);
}


/************************
 *** Common Functions ***
 ************************/

MC_HVALUETYPE MCTRL_API
mcValueType_GetBuiltin(int id)
{
    switch(id) {
        case MC_VALUETYPEID_INT32:      return (MC_HVALUETYPE) &int32_type;
        case MC_VALUETYPEID_UINT32:     return (MC_HVALUETYPE) &uint32_type;
        case MC_VALUETYPEID_INT64:      return (MC_HVALUETYPE) &int64_type;
        case MC_VALUETYPEID_UINT64:     return (MC_HVALUETYPE) &uint64_type;
        case MC_VALUETYPEID_STRINGW:    return (MC_HVALUETYPE) &strw_type;
        case MC_VALUETYPEID_STRINGA:    return (MC_HVALUETYPE) &stra_type;
        case MC_VALUETYPEID_IMMSTRINGW: return (MC_HVALUETYPE) &immstrw_type;
        case MC_VALUETYPEID_IMMSTRINGA: return (MC_HVALUETYPE) &immstra_type;
        case MC_VALUETYPEID_COLOR:      return (MC_HVALUETYPE) &color_type;
        case MC_VALUETYPEID_ICON:       return (MC_HVALUETYPE) &icon_type;
    }

    MC_TRACE("mcValueType_GetBuiltin: id %d unknown", id);
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

MC_HVALUETYPE MCTRL_API
mcValue_Type(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return (MC_HVALUETYPE) v->type;
}

MC_HVALUE MCTRL_API
mcValue_Duplicate(const MC_HVALUE hValue)
{
    const value_t* v = (const value_t*) hValue;

    if(MC_ERR(v == NULL)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    return (MC_HVALUE) v->type->ctor_val(v);
}

void MCTRL_API
mcValue_Destroy(MC_HVALUE hValue)
{
    value_t* v = (value_t*) hValue;

    if(MC_ERR(v == NULL)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return;
    }

    v->type->dtor(v);
}
