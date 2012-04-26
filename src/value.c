/*
 * Copyright (c) 2010-2011 Martin Mitas
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
        case VALUE_PF_ALIGNLEFT:   format |= DT_LEFT; break;
        case VALUE_PF_ALIGNCENTER: format |= DT_CENTER; break;
        case VALUE_PF_ALIGNRIGHT:  format |= DT_RIGHT; break;
        default:                   format |= defaults & (DT_LEFT | DT_CENTER | DT_RIGHT); break;
    }
    
    switch(flags & VALUE_PF_ALIGNMASKVERT) {
        case VALUE_PF_ALIGNTOP:     format |= DT_TOP; break;
        case VALUE_PF_ALIGNVCENTER: format |= DT_VCENTER; break;
        case VALUE_PF_ALIGNBOTTOM:  format |= DT_BOTTOM; break;
        default:                    format |= defaults & (DT_TOP | DT_VCENTER | DT_BOTTOM); break;
    }
    
    return format;
}


/************************
 *** Reusable methods ***
 ************************/

static void
default_destroy(value_t v)
{
    free(v);
}

static void
scalar_destroy(value_t v)
{
    /* noop */
}

static int
scalar_copy(value_t* dest, const value_t src)
{
    *dest = src;
    return 0;
}


/*********************************
 *** Int32 type implementation ***
 *********************************/

void
value_set_int32(value_t* v, int32_t i)
{
    *v = (value_t)(intptr_t) i;
}

int32_t
value_get_int32(const value_t v)
{
    return (int32_t)(intptr_t) v;
}

static int
int32_cmp(const value_t v1, const value_t v2)
{
    int32_t i1 = (int32_t)(intptr_t) v1;
    int32_t i2 = (int32_t)(intptr_t) v2;

    if(i1 < i2)  return -1;
    if(i1 > i2)  return +1;
    return 0;
}

static int
int32_from_string(value_t* v, const TCHAR* str)
{
    int32_t i;
    TCHAR* end;

    MC_ASSERT(sizeof(long) == sizeof(int32_t));

    /* _tcstol() accepts leading whitespaces, we do not */
    if(MC_ERR(_istspace(str[0])))
        return -1;

    i = _tcstol(str, &end, 10);
    if(MC_ERR(end == str || *end != _T('\0')))
        return -1;

    *v = (value_t)(intptr_t) i;
    return 0;
}

static size_t
int32_to_string(const value_t v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;
    int32_t i = (int32_t)(intptr_t) v;
    int32_t limit;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%I32d"), i);
        if(n >= 0)
            return n + 1;  /* +1 for '\0' */
    }

    /* Buffer too small: return required buffer size. */
    if(i >= 0) {
        n = 1;  /* +1 for '\0' */
        for(limit = 10; limit < 1000000000; limit *= 10) {
            n++;
            if(i < limit)
                return n;
        }
        return n+1;
    } else {
        n = 2;  /* +2 for '-' and '\0' */
        for(limit = -10; limit > -1000000000; limit *= 10) {
            n++;
            if(i > limit)
                return n;
        }
        return n+1;
    }
}

static void
int32_paint(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[16];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%I32d"), (int32_t)(intptr_t) v);

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}


static const struct value_type_tag int32_type = {
    scalar_destroy,
    scalar_copy,
    int32_cmp,
    int32_from_string,
    int32_to_string,
    int32_paint
};

const value_type_t* VALUE_TYPE_INT32 = &int32_type;


/**********************************
 *** UInt32 type implementation ***
 **********************************/

void
value_set_uint32(value_t* v, uint32_t u)
{
    *v = (value_t)(uintptr_t) u;
}

uint32_t
value_get_uint32(const value_t v)
{
    return (uint32_t)(uintptr_t) v;
}

static int
uint32_cmp(const value_t v1, const value_t v2)
{
    uint32_t u1 = (uint32_t)(uintptr_t) v1;
    uint32_t u2 = (uint32_t)(uintptr_t) v2;

    if(u1 < u2)  return -1;
    if(u1 > u2)  return +1;
    return 0;
}

static int
uint32_from_string(value_t* v, const TCHAR* str)
{
    uint32_t u;
    WCHAR* end;

    MC_ASSERT(sizeof(unsigned long) == sizeof(uint32_t));

    /* _tcstoul() accepts leading whitespaces, we do not */
    if(MC_ERR(_istspace(str[0])))
        return -1;

    u = _tcstoul(str, &end, 10);
    if(MC_ERR(end == str || *end != _T('\0')))
        return -1;

    *v = (value_t)(uintptr_t) u;
    return 0;
}

static size_t
uint32_to_string(const value_t v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;
    uint32_t u = (uint32_t)(uintptr_t) v;
    uint32_t limit;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%I32u"), u);
        if(n >= 0)
            return n + 1;  /* +1 for '\0' */
    }

    /* Buffer too small: return required buffer size. */
    n = 1;  /* +1 for '\0' */
    for(limit = 10U; limit < 1000000000U; limit *= 10U) {
        n++;
        if(u < limit)
            return n;
    }
    return n+1;
}

static void
uint32_paint(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[16];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%I32u"), (uint32_t)(uintptr_t) v);

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}


static const struct value_type_tag uint32_type = {
    scalar_destroy,
    scalar_copy,
    uint32_cmp,
    uint32_from_string,
    uint32_to_string,
    uint32_paint
};

const value_type_t* VALUE_TYPE_UINT32 = &uint32_type;


/*********************************
 *** Int64 type implementation ***
 *********************************/

int
value_set_int64(value_t* v, int64_t i64)
{
#ifdef _WIN64
    *v = (value_t)(intptr_t) i64;
    return 0;
#else
    *v = malloc(sizeof(int64_t));
    if(MC_ERR(*v == NULL)) {
        MC_TRACE("value_set_int64: malloc() failed.");
        return -1;
    }
    *((int64_t*)*v) = i64;
    return 0;
#endif
}

int64_t
value_get_int64(const value_t v)
{
#ifdef _WIN64
    return (int64_t)(intptr_t) v;
#else
    return *((int64_t*) v);
#endif
}

#ifndef _WIN64
static int
int64_copy(value_t* dest, const value_t src)
{
    return value_set_int64(dest, value_get_int64(src));
}
#endif

static int
int64_cmp(const value_t v1, const value_t v2)
{
    int64_t i64_1, i64_2;

    i64_1 = value_get_int64(v1);
    i64_2 = value_get_int64(v2);

    if(i64_1 < i64_2)  return -1;
    if(i64_1 > i64_2)  return +1;
    return 0;
}

static int
int64_from_string(value_t* v, const TCHAR* str)
{
    int64_t i64;
    TCHAR* end;

    /* _tcstoi64() accepts leading whitespaces, we do not */
    if(MC_ERR(_istspace(str[0])))
        return -1;

    i64 = _tcstoi64(str, &end, 10);
    if(MC_ERR(end == str || *end != _T('\0')))
        return -1;

    return value_set_int64(v, i64);
}

static size_t
int64_to_string(const value_t v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;
    int64_t i64 = value_get_int64(v);
    int64_t limit;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%I64d"), i64);
        if(n >= 0)
            return n + 1;  /* +1 for '\0' */
    }

    /* Buffer too small: return required buffer size. */
    if(i64 >= 0) {
        n = 1;  /* +1 for '\0' */
        for(limit = 10; limit < 1000000000000000000; limit *= 10) {
            n++;
            if(i64 < limit)
                return n;
        }
        return n+1;
    } else {
        n = 2;  /* +2 for '-' and '\0' */
        for(limit = -10; limit > -1000000000000000000; limit *= 10) {
            n++;
            if(i64 > limit)
                return n;
        }
        return n+1;
    }
}

static void
int64_paint(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[24];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%I64d"), value_get_int64(v));

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}


static const struct value_type_tag int64_type = {
#ifdef _WIN64
    scalar_destroy,
#else
    default_destroy,
#endif
#ifdef _WIN64
    scalar_copy,
#else
    int64_copy,
#endif
    int64_cmp,
    int64_from_string,
    int64_to_string,
    int64_paint
};

const value_type_t* VALUE_TYPE_INT64 = &int64_type;


/**********************************
 *** UInt64 type implementation ***
 **********************************/

int
value_set_uint64(value_t* v, uint64_t u64)
{
#ifdef _WIN64
    *v = (value_t)(uintptr_t) u64;
    return 0;
#else
    *v = malloc(sizeof(uint64_t));
    if(MC_ERR(*v == NULL)) {
        MC_TRACE("value_set_uint64: malloc() failed.");
        return -1;
    }
    *((uint64_t*)*v) = u64;
    return 0;
#endif
}

uint64_t
value_get_uint64(const value_t v)
{
#ifdef _WIN64
    return (uint64_t)(uintptr_t) v;
#else
    return *((uint64_t*) v);
#endif
}

#ifndef _WIN64
static int
uint64_copy(value_t* dest, const value_t src)
{
    return value_set_uint64(dest, value_get_uint64(src));
}
#endif

static int
uint64_cmp(const value_t v1, const value_t v2)
{
    uint64_t u64_1, u64_2;

    u64_1 = value_get_uint64(v1);
    u64_2 = value_get_uint64(v2);

    if(u64_1 < u64_2)  return -1;
    if(u64_1 > u64_2)  return +1;
    return 0;
}

static int
uint64_from_string(value_t* v, const TCHAR* str)
{
    uint64_t u64;
    TCHAR* end;

    /* _tcstoi64() accepts leading whitespaces, we do not */
    if(MC_ERR(_istspace(str[0])))
        return -1;

    u64 = _tcstoui64(str, &end, 10);
    if(MC_ERR(end == str || *end != _T('\0')))
        return -1;

    return value_set_uint64(v, u64);
}

static size_t
uint64_to_string(const value_t v, TCHAR* buffer, size_t bufsize)
{
    int n = -1;
    uint64_t u64 = value_get_uint64(v);
    uint64_t limit;

    if(bufsize > 0) {
        n = _sntprintf(buffer, bufsize, _T("%I64u"), u64);
        if(n >= 0)
            return n + 1;  /* +1 for '\0' */
    }

    /* Buffer too small: return required buffer size. */
    n = 1;  /* +1 for '\0' */
    for(limit = 10; limit < 1000000000000000000; limit *= 10) {
        n++;
        if(u64 < limit)
            return n;
    }
    return n+1;
}

static void
uint64_paint(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    TCHAR buffer[24];
    int old_bkmode;
    COLORREF old_color;

    _stprintf(buffer, _T("%I64u"), value_get_uint64(v));

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawText(dc, buffer, -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
             draw_text_format(flags, DT_RIGHT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}


static const struct value_type_tag uint64_type = {
#ifdef _WIN64
    scalar_destroy,
#else
    default_destroy,
#endif
#ifdef _WIN64
    scalar_copy,
#else
    uint64_copy,
#endif
    uint64_cmp,
    uint64_from_string,
    uint64_to_string,
    uint64_paint
};

const value_type_t* VALUE_TYPE_UINT64 = &uint64_type;


/***********************************
 *** StringW type implementation ***
 ***********************************/

static const WCHAR str_empty_w[] = L"";

int
value_set_string_w(value_t* v, const WCHAR* str)
{
    WCHAR* s;
    size_t size;

    if(str == NULL  ||  str[0] == L'\0') {
        *v = NULL;
        return 0;
    }

    size = sizeof(WCHAR) * (wcslen(str)+1);
    s = (WCHAR*) malloc(size);
    if(MC_ERR(s == NULL)) {
        MC_TRACE("value_set_string_w: malloc() failed.");
        return -1;
    }

    memcpy(s, str, size);
    *v = (value_t) s;
    return 0;
}

const WCHAR*
value_get_string_w(const value_t v)
{
    return (v != NULL ? (const WCHAR*)v : str_empty_w);
}

static int
str_copy_w(value_t* dest, const value_t src)
{
    return value_set_string_w(dest, value_get_string_w(src));
}

static int
str_cmp_w(const value_t v1, const value_t v2)
{
    return _wcsicmp(value_get_string_w(v1), value_get_string_w(v2));
}

static int
str_from_string_w(value_t* v, const TCHAR* str)
{
#ifdef UNICODE
    return value_set_string_w(v, str);
#else
    if(str == NULL || str[0] == L'\0') {
        *v = NULL;
        return 0;
    }

    *v = mc_str(str, MC_STRA, MC_STRW);
    if(MC_ERR(*v == NULL))
        return -1;

    return 0;
#endif
}

static size_t
str_to_string_w(const value_t v, TCHAR* buffer, size_t bufsize)
{
    const WCHAR* s = value_get_string_w(v);
#ifdef UNICODE
    size_t len;

    len = wcslen(s);
    if(bufsize > 0)
        wcsncpy(buffer, s, MC_MIN(len + 1, bufsize));
    return len + 1;
#else
    WideCharToMultiByte(CP_ACP, 0, s, -1, buffer, bufsize, NULL, NULL);
    return WideCharToMultiByte(CP_ACP, 0, s, -1, buffer, 0, NULL, NULL);
#endif
}

static void
str_paint_w(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    int old_bkmode;
    COLORREF old_color;

    if(v == NULL)
        return;

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextW(dc, value_get_string_w(v), -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
              draw_text_format(flags, DT_LEFT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}


static const struct value_type_tag str_type_w = {
    default_destroy,
    str_copy_w,
    str_cmp_w,
    str_from_string_w,
    str_to_string_w,
    str_paint_w
};

const value_type_t* VALUE_TYPE_STRING_W = &str_type_w;


/***********************************
 *** StringA type implementation ***
 ***********************************/

static const char str_empty_a[] = "";

int
value_set_string_a(value_t* v, const char* str)
{
    char* s;
    size_t size;

    if(str == NULL  ||  str[0] == '\0') {
        *v = NULL;
        return 0;
    }

    size = strlen(str)+1;
    s = (char*) malloc(size);
    if(MC_ERR(s == NULL)) {
        MC_TRACE("value_set_string_a: malloc() failed.");
        return -1;
    }

    memcpy(s, str, size);
    *v = (value_t) s;
    return 0;
}

const char*
value_get_string_a(const value_t v)
{
    return (v != NULL ? (const char*)v : str_empty_a);
}

static int
str_copy_a(value_t* dest, const value_t src)
{
    return value_set_string_a(dest, value_get_string_a(src));
}

static int
str_cmp_a(const value_t v1, const value_t v2)
{
    return stricmp(value_get_string_a(v1), value_get_string_a(v2));
}

static int
str_from_string_a(value_t* v, const TCHAR* str)
{
#ifdef UNICODE
    if(str == NULL || str[0] == L'\0') {
        *v = NULL;
        return 0;
    }

    *v = mc_str(str, MC_STRA, MC_STRW);
    if(MC_ERR(*v == NULL))
        return -1;

    return 0;
#else
    return value_set_string_a(v, str);
#endif
}

static size_t
str_to_string_a(const value_t v, TCHAR* buffer, size_t bufsize)
{
    const char* s = value_get_string_a(v);
#ifdef UNICODE
    MultiByteToWideChar(CP_ACP, 0, s, -1, buffer, bufsize);
    return MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
#else
    size_t len;

    len = strlen(s);
    if(bufsize > 0)
        strncpy(buffer, s, MC_MIN(len + 1, bufsize));
    return len + 1;
#endif
}

static void
str_paint_a(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    int old_bkmode;
    COLORREF old_color;

    if(v == NULL)
        return;

    old_bkmode = SetBkMode(dc, TRANSPARENT);
    old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    DrawTextA(dc, value_get_string_a(v), -1, rect, DT_SINGLELINE | DT_END_ELLIPSIS |
              draw_text_format(flags, DT_LEFT | DT_VCENTER));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_bkmode);
}


static const struct value_type_tag str_type_a = {
    default_destroy,
    str_copy_a,
    str_cmp_a,
    str_from_string_a,
    str_to_string_a,
    str_paint_a
};

const value_type_t* VALUE_TYPE_STRING_A = &str_type_a;


/**************************************
 *** ImmStringW type implementation ***
 **************************************/

void
value_set_immstring_w(value_t* v, const WCHAR* str)
{
    *v = (value_t) str;
}

static const struct value_type_tag immstr_type_w = {
    scalar_destroy,
    scalar_copy,
    str_cmp_w,
    NULL,
    str_to_string_w,
    str_paint_w
};

const value_type_t* VALUE_TYPE_IMMSTRING_W = &immstr_type_w;


/**************************************
 *** ImmStringA type implementation ***
 **************************************/

void
value_set_immstring_a(value_t* v, const char* str)
{
    *v = (value_t) str;
}

static const struct value_type_tag immstr_type_a = {
    scalar_destroy,
    scalar_copy,
    str_cmp_a,
    NULL,
    str_to_string_a,
    str_paint_a
};

const value_type_t* VALUE_TYPE_IMMSTRING_A = &immstr_type_a;


/*********************************
 *** Color type implementation ***
 *********************************/

void
value_set_colorref(value_t* v, COLORREF cref)
{
    *v = (value_t)(uintptr_t) cref;
}

COLORREF
value_get_colorref(const value_t v)
{
    return (COLORREF)(uintptr_t) v;
}

static int
colorref_from_string(value_t* v, const WCHAR* str)
{
    const WCHAR* ptr = str;
    WCHAR* end;
    ULONG rgb;
    UCHAR r, g, b;

    /* We expect "#rrggbb" */
    if(*ptr != _T('#'))
        return -1;
    ptr++;
    rgb = _tcstoul(ptr, &end, 16);
    if(end - ptr != 6  ||  *end != _T('\0'))
        return -1;

    r = (rgb & 0xff0000) >> 16;
    g = (rgb & 0xff00) >> 8;
    b = (rgb & 0xff);
    v = (value_t)(uintptr_t) RGB(r, g, b);
    return 0;
}

static size_t
colorref_to_string(const value_t v, WCHAR* buffer, size_t bufsize)
{
    COLORREF clr = (COLORREF)(uintptr_t) v;

    if(bufsize > 0) {
        _sntprintf(buffer, bufsize, _T("#02x02x02x"),
                   (UINT)GetRValue(clr), (UINT)GetGValue(clr), (UINT)GetBValue(clr));
    }
    return sizeof("#rrggbb");
}

static void
colorref_paint(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    COLORREF clr = (COLORREF)(uintptr_t) v;
    HBRUSH brush;
    HBRUSH old_brush;
    HPEN old_pen;

    brush = CreateSolidBrush(clr);
    old_brush = SelectObject(dc, brush);
    old_pen = SelectObject(dc, GetStockObject(BLACK_PEN));

    Rectangle(dc, rect->left, rect->top, rect->right, rect->bottom);

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
}

static const struct value_type_tag colorref_type = {
    scalar_destroy,
    scalar_copy,
    NULL,
    colorref_from_string,
    colorref_to_string,
    colorref_paint
};

const value_type_t* VALUE_TYPE_COLORREF = &colorref_type;


/*********************************
 *** HICON type implementation ***
 *********************************/

void
value_set_hicon(value_t* v, HICON icon)
{
    *v = (value_t)icon;
}

HICON
value_get_hicon(const value_t v)
{
    return (HICON)v;
}

static void
hicon_paint(const value_t v, HDC dc, RECT* rect, DWORD flags)
{
    HICON icon = (HICON)v;
    SIZE icon_size;
    int x, y;
    
    if(icon == NULL)
        return;
    
    mc_icon_size(icon, &icon_size);
    switch(flags & VALUE_PF_ALIGNMASKHORZ) {
        case VALUE_PF_ALIGNLEFT:    x = rect->left; break;
        case VALUE_PF_ALIGNDEFAULT:
        case VALUE_PF_ALIGNCENTER:  x = (rect->left + rect->right - icon_size.cx) / 2; break;
        case VALUE_PF_ALIGNRIGHT:   x = rect->right - icon_size.cx; break;
    }
    switch(flags & VALUE_PF_ALIGNMASKVERT) {
        case VALUE_PF_ALIGNTOP:      y = rect->top; break;
        case VALUE_PF_ALIGNVDEFAULT:
        case VALUE_PF_ALIGNVCENTER:  y = (rect->top + rect->bottom - icon_size.cy) / 2; break;
        case VALUE_PF_ALIGNBOTTOM:   y = rect->bottom - icon_size.cy; break;
    }
    
    DrawIconEx(dc, x, y, icon, 0, 0, 0, NULL, DI_NORMAL);
}

static const struct value_type_tag hicon_type = {
    scalar_destroy,
    scalar_copy,
    NULL,
    NULL,
    NULL,
    hicon_paint
};

const value_type_t* VALUE_TYPE_HICON = &hicon_type;


/**************************
 *** Exported functions ***
 **************************/

MC_HVALUETYPE MCTRL_API
mcValueType_GetBuiltin(int id)
{
    switch(id) {
        case MC_VALUETYPEID_INT32:      return VALUE_TYPE_INT32;
        case MC_VALUETYPEID_UINT32:     return VALUE_TYPE_UINT32;
        case MC_VALUETYPEID_INT64:      return VALUE_TYPE_INT64;
        case MC_VALUETYPEID_UINT64:     return VALUE_TYPE_UINT64;
        case MC_VALUETYPEID_STRINGW:    return VALUE_TYPE_STRING_W;
        case MC_VALUETYPEID_STRINGA:    return VALUE_TYPE_STRING_A;
        case MC_VALUETYPEID_IMMSTRINGW: return VALUE_TYPE_IMMSTRING_W;
        case MC_VALUETYPEID_IMMSTRINGA: return VALUE_TYPE_IMMSTRING_A;
        case MC_VALUETYPEID_COLORREF:   return VALUE_TYPE_COLORREF;
        case MC_VALUETYPEID_HICON:      return VALUE_TYPE_HICON;
    }
    
    MC_TRACE("mcValueType_GetBuiltin: id %d unknown", id);
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}


BOOL MCTRL_API
mcValue_CreateFromInt32(MC_HVALUE* phValue, INT iValue)
{
    value_set_int32((value_t*) phValue, (int32_t) iValue);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromUInt32(MC_HVALUE* phValue, UINT uValue)
{
    value_set_uint32((value_t*) phValue, (uint32_t) uValue);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromInt64(MC_HVALUE* phValue, INT64 i64Value)
{
    value_set_int64((value_t*) phValue, (int64_t) i64Value);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromUInt64(MC_HVALUE* phValue, UINT64 u64Value)
{
    value_set_uint64((value_t*) phValue, (uint64_t) u64Value);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromStringW(MC_HVALUE* phValue, LPCWSTR lpStr)
{
    return (value_set_string_w((value_t*) phValue, lpStr) == 0 ? TRUE : FALSE);
}

BOOL MCTRL_API
mcValue_CreateFromStringA(MC_HVALUE* phValue, LPCSTR lpStr)
{
    return (value_set_string_a((value_t*) phValue, lpStr) == 0 ? TRUE : FALSE);
}

BOOL MCTRL_API
mcValue_CreateFromImmStringW(MC_HVALUE* phValue, LPCWSTR lpStr)
{
    value_set_immstring_w((value_t*) phValue, lpStr);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromImmStringA(MC_HVALUE* phValue, LPCSTR lpStr)
{
    value_set_immstring_a((value_t*) phValue, lpStr);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromColorref(MC_HVALUE* phValue, COLORREF crColor)
{
    value_set_colorref((value_t*) phValue, crColor);
    return TRUE;
}

BOOL MCTRL_API
mcValue_CreateFromHIcon(MC_HVALUE* phValue, HICON hIcon)
{
    value_set_hicon((value_t*) phValue, hIcon);
    return TRUE;
}


INT MCTRL_API
mcValue_GetInt32(const MC_HVALUE hValue)
{
    return (INT) value_get_int32((value_t)hValue);
}

UINT MCTRL_API
mcValue_GetUInt32(const MC_HVALUE hValue)
{
    return (UINT) value_get_uint32((value_t)hValue);
}

INT64 MCTRL_API
mcValue_GetInt64(const MC_HVALUE hValue)
{
    return (INT64) value_get_int64((value_t)hValue);
}

UINT64 MCTRL_API
mcValue_GetUInt64(const MC_HVALUE hValue)
{
    return (UINT64) value_get_uint64((value_t)hValue);
}

LPCWSTR MCTRL_API
mcValue_GetStringW(const MC_HVALUE hValue)
{
    return value_get_string_w((value_t)hValue);
}

LPCSTR MCTRL_API
mcValue_GetStringA(const MC_HVALUE hValue)
{
    return value_get_string_a((value_t)hValue);
}

LPCWSTR MCTRL_API
mcValue_GetImmStringW(const MC_HVALUE hValue)
{
    return value_get_immstring_w((value_t)hValue);
}

LPCSTR MCTRL_API
mcValue_GetImmStringA(const MC_HVALUE hValue)
{
    return value_get_immstring_a((value_t)hValue);
}

COLORREF MCTRL_API
mcValue_GetColorref(const MC_HVALUE hValue)
{
    return value_get_colorref((value_t)hValue);
}

HICON MCTRL_API
mcValue_GetHIcon(const MC_HVALUE hValue)
{
    return value_get_hicon((value_t)hValue);
}


BOOL MCTRL_API
mcValue_Duplicate(MC_HVALUETYPE hType, MC_HVALUE* phDest, const MC_HVALUE hSrc)
{
    if(MC_ERR(hType == NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    return (((value_type_t*)hType)->copy((value_t*)phDest, (value_t)hSrc)
            ? TRUE : FALSE);
}

void MCTRL_API
mcValue_Destroy(MC_HVALUETYPE hType, MC_HVALUE hValue)
{
    if(hType != NULL)
        ((value_type_t*)hType)->destroy((value_t)hValue);
}
