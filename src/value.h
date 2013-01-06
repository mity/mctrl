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

#ifndef MC_VALUE_H
#define MC_VALUE_H

#include "mCtrl/value.h"
#include "misc.h"


typedef struct value_tag value_t;
typedef struct type_tag type_t;

struct value_tag {
    const type_t* type;
};

struct type_tag {
    value_t* (__stdcall *ctor_str)(const TCHAR*);                      /* optional */
    value_t* (__stdcall *ctor_val)(const value_t*);                    /* mandatory */
    void     (__stdcall *dtor)    (value_t*);                          /* mandatory */
    int      (__stdcall *cmp)     (const value_t*, const value_t*);    /* optional */
    size_t   (__stdcall *dump)    (const value_t*, TCHAR*, size_t);    /* optional */
    void     (__stdcall *paint)   (const value_t*, HDC, RECT*, DWORD); /* mandatory */
};

#define VALUE_PTR(v)         ((void*) ((v)+1))
#define VALUE_DATA(v, type)  (*(type*) VALUE_PTR(v))


/* Flags for type_t::paint() */
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


static inline void
value_destroy(value_t* v)
{
    v->type->dtor(v);
}

static inline void
value_paint(const value_t* v, HDC dc, RECT* rect, DWORD flags)
{
    v->type->paint(v, dc, rect, flags);
}


#endif  /* MC_VALUE_H */
