/*
 * Copyright (c) 2012 Martin Mitas
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

#ifndef MC_OPTIM_H
#define MC_OPTIM_H


/***************************
 *** Memory manipulation ***
 ***************************/

/* memcpy/memmove can be slow because of 'call' instruction for very small
 * blocks of memory. Hence if we know the size is small already in compile
 * time, we should inline it. */

static inline void
mc_inlined_memcpy(void* addr0, const void* addr1, size_t n)
{
    BYTE* iter0 = (BYTE*) addr0;
    const BYTE* iter1 = (const BYTE*) addr1;

    while(n-- > 0)
        *iter0++ = *iter1++;
}

static inline void
mc_inlined_memmove(void* addr0, const void* addr1, size_t n)
{
    BYTE* iter0 = (BYTE*) addr0;
    const BYTE* iter1 = (const BYTE*) addr1;

    if(iter0 < iter1) {
        while(n-- > 0)
            *iter0++ = *iter1++;
    } else {
        iter0 += n;
        iter1 += n;
        while(n-- > 0)
            *(--iter0) = *(--iter1);
    }
}

/* Swapping two memory blocks. They must not overlay. */
static inline void
mc_inlined_memswap(void* addr0, void* addr1, size_t n)
{
    BYTE* iter0 = (BYTE*) addr0;
    BYTE* iter1 = (BYTE*) addr1;
    BYTE tmp;

    while(n-- > 0) {
        tmp = *iter0;
        *iter0++ = *iter1;
        *iter1++ = tmp;
    }
}

#define mc_memswap    mc_inlined_memswap


/*************************
 *** RECT manipulation ***
 *************************/

/* These are so trivial that inlining them is probably better then calling
 * Win32API functions like InflateRect() etc. */

static inline LONG
mc_width(const RECT* r)
{
    return (r->right - r->left);
}

static inline LONG
mc_height(const RECT* r)
{
    return (r->bottom - r->top);
}

static inline BOOL
mc_contains(const RECT* r, const POINT* pt)
{
    return (r->left <= pt->x  &&  pt->x < r->right  &&
            r->top <= pt->y  &&  pt->y < r->bottom);
}

static inline BOOL
mc_rect_is_empty(const RECT* r)
{
    return (r->left >= r->right  ||  r->top >= r->bottom);
}

static inline void
mc_set_rect(RECT* r, LONG x0, LONG y0, LONG x1, LONG y1)
{
    r->left = x0;
    r->top = y0;
    r->right = x1;
    r->bottom = y1;
}

static inline void
mc_copy_rect(RECT* r0, const RECT* r1)
{
    r0->left = r1->left;
    r0->top = r1->top;
    r0->right = r1->right;
    r0->bottom = r1->bottom;
}

static inline void
mc_offset_rect(RECT* r, LONG dx, LONG dy)
{
    r->left += dx;
    r->top += dy;
    r->right += dx;
    r->bottom += dy;
}

static inline void
mc_inflate_rect(RECT* r, LONG dx, LONG dy)
{
    r->left -= dx;
    r->top -= dy;
    r->right += dx;
    r->bottom += dy;
}


#endif  /* MC_OPTIM_H */
