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

#ifndef MC_VALUE_H
#define MC_VALUE_H

#include "mCtrl/value.h"
#include "misc.h"


typedef void* value_t;


/* Flags for value_type_t::paint().
 * (We define them to match public table cell flags) */
#define VALUE_PF_ALIGNDEFAULT              0x00000000
#define VALUE_PF_ALIGNLEFT                 0x00000001
#define VALUE_PF_ALIGNCENTER               0x00000003
#define VALUE_PF_ALIGNRIGHT                0x00000002
#define VALUE_PF_ALIGNVDEFAULT             0x00000000
#define VALUE_PF_ALIGNTOP                  0x00000004
#define VALUE_PF_ALIGNVCENTER              0x0000000c
#define VALUE_PF_ALIGNBOTTOM               0x00000008

#define VALUE_PF_ALIGNMASKHORZ             0x00000003
#define VALUE_PF_ALIGNMASKVERT             0x0000000c
#define VALUE_PF_ALIGNMASK                 0x0000000f


typedef struct value_type_tag value_type_t;
struct value_type_tag {
    void   (*free)(value_t);
    int    (*copy)(value_t*, const value_t);
    int    (*cmp)(const value_t, const value_t);
    int    (*from_string)(value_t*, const TCHAR*);
    size_t (*to_string)(const value_t, TCHAR*, size_t);
    void   (*paint)(const value_t, HDC, RECT*, DWORD);
};


extern const value_type_t* VALUE_TYPE_INT32;
extern const value_type_t* VALUE_TYPE_UINT32;
extern const value_type_t* VALUE_TYPE_INT64;
extern const value_type_t* VALUE_TYPE_UINT64;
extern const value_type_t* VALUE_TYPE_STRING_W;
extern const value_type_t* VALUE_TYPE_STRING_A;
extern const value_type_t* VALUE_TYPE_IMMSTRING_W;
extern const value_type_t* VALUE_TYPE_IMMSTRING_A;
extern const value_type_t* VALUE_TYPE_COLORREF;
extern const value_type_t* VALUE_TYPE_HICON;

#ifdef UNICODE
    #define VALUE_TYPE_STRING             VALUE_TYPE_STRING_W
    #define VALUE_TYPE_IMMUTABLE_STRING   VALUE_TYPE_IMMUTABLE_STRING_W
#else
    #define VALUE_TYPE_STRING             VALUE_TYPE_STRING_A
    #define VALUE_TYPE_IMMUTABLE_STRING   VALUE_TYPE_IMMUTABLE_STRING_A
#endif


void value_set_int32(value_t* v, int32_t i);
int32_t value_get_int32(const value_t v);

void value_set_uint32(value_t* v, uint32_t u);
uint32_t value_get_uint32(const value_t v);

int value_set_int64(value_t* v, int64_t i64);
int64_t value_get_int64(const value_t v);

int  value_set_uint64(value_t* v, uint64_t u64);
uint64_t value_get_uint64(const value_t v);

int value_set_string_w(value_t* v, const WCHAR* str);
const WCHAR* value_get_string_w(const value_t v);

int value_set_string_a(value_t* v, const char* str);
const char* value_get_string_a(const value_t v);

void value_set_immstring_w(value_t* v, const WCHAR* str);
#define value_get_immstring_w value_get_string_w

void value_set_immstring_a(value_t* v, const char* str);
#define value_get_immstring_a value_get_string_a
 
void value_set_colorref(value_t* v, COLORREF cref);
COLORREF value_get_colorref(const value_t v);

void value_set_hicon(value_t* v, HICON icon);
HICON value_get_hicon(const value_t v);


#ifdef UNICODE
    #define value_set_string             value_set_string_w
    #define value_get_string             value_get_string_w
    #define value_set_immutable_string   value_set_immutable_string_w
#else
    #define value_set_string             value_set_string_a
    #define value_get_string             value_get_string_a
    #define value_set_immutable_string   value_set_immutable_string_a
#endif


#endif  /* MC_VALUE_H */
