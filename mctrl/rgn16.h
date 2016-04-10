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

#ifndef MC_RGN16_H
#define MC_RGN16_H

#include "misc.h"


typedef struct rgn16_rect_tag rgn16_rect_t;
struct rgn16_rect_tag {
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;
};

typedef struct rgn16_simple_tag rgn16_simple_t;
struct rgn16_simple_tag {
    WORD n;
    rgn16_rect_t rc;
};

typedef struct rgn16_complex_tag rgn16_complex_t;
struct rgn16_complex_tag {
    WORD n;
    WORD alloc;
    rgn16_rect_t* vec;      /* vec[0] is special: It holds extents. */
};

typedef union rgn16_tag rgn16_t;
union rgn16_tag {
    WORD n;                 /* Region is empty when n == 0 */
    rgn16_simple_t s;       /* Used when n == 1 */
    rgn16_complex_t c;      /* Used when n >= 2 */
};

/* Note that one-rect region can be expressed in two ways:
 * (1) simple region (n == 1).
 * (2) complex region with extents and one rect, i.e. two same rects (n == 2).
 */


static inline BOOL
rgn16_rect_equals_rect(const rgn16_rect_t* a, const rgn16_rect_t* b)
{
    return (a->x0 == b->x0  &&  a->y0 == b->y0  &&
            a->x1 == b->x1  &&  a->y1 == b->y1);
}

static inline BOOL
rgn16_rect_contains_rect(const rgn16_rect_t* a, const rgn16_rect_t* b)
{
    return (a->x0 <= b->x0  &&  b->x1 <= a->x1  &&
            a->y0 <= b->y0  &&  b->y1 <= a->y1);
}

static inline BOOL
rgn16_rect_overlaps_rect(const rgn16_rect_t* a, const rgn16_rect_t* b)
{
    return (a->x1 > b->x0  &&  a->x0 < b->x1  &&
            a->y1 > b->y0  &&  a->y0 < b->y1);
}

static inline void
rgn16_rect_set(rgn16_rect_t* a, WORD x0, WORD y0, WORD x1, WORD y1)
{
    a->x0 = x0;  a->y0 = y0;
    a->x1 = x1;  a->y1 = y1;
}

static inline void
rgn16_rect_copy(rgn16_rect_t* a, const rgn16_rect_t* b)
{
    a->x0 = b->x0;  a->y0 = b->y0;
    a->x1 = b->x1;  a->y1 = b->y1;
}


static inline void rgn16_init(rgn16_t* rgn)
    { rgn->n = 0; }
static inline void rgn16_init_with_rect(rgn16_t* rgn, const rgn16_rect_t* rect)
    { rgn->s.n = 1; rgn16_rect_copy(&rgn->s.rc, rect); }
static inline void rgn16_init_with_xy(rgn16_t* rgn, WORD x, WORD y)
    { rgn->s.n = 1; rgn16_rect_set(&rgn->s.rc, x, y, x+1, y+1); }

static inline void rgn16_fini(rgn16_t* rgn)
    { if(rgn->n > 1) free(rgn->c.vec); }

static inline void rgn16_clear(rgn16_t* rgn)
    { if(rgn->n > 1) free(rgn->c.vec); rgn->n = 0; }

static inline BOOL rgn16_is_empty(const rgn16_t* rgn)
    { return (rgn->n == 0); }

const rgn16_rect_t* rgn16_extents(const rgn16_t* rgn);

BOOL rgn16_equals_rgn(const rgn16_t* rgn1, const rgn16_t* rgn2);

BOOL rgn16_contains_rect(const rgn16_t* rgn, const rgn16_rect_t* rect);

static inline BOOL rgn16_contains_xy(const rgn16_t* rgn, WORD x, WORD y)
    { rgn16_rect_t r = { x, y, x+1, y+1 }; return rgn16_contains_rect(rgn, &r); }

int rgn16_copy(rgn16_t* rgnR, const rgn16_t* rgn1);

int rgn16_union(rgn16_t* rgnR, const rgn16_t* rgn1, const rgn16_t* rgn2);
int rgn16_subtract(rgn16_t* rgnR, const rgn16_t* rgn1, const rgn16_t* rgn2);
int rgn16_xor(rgn16_t* rgnR, const rgn16_t* rgn1, const rgn16_t* rgn2);


#endif  /* MC_RGN16_H */
