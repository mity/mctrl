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

#include "rgn16.h"


/***************************************************************
 ***  Combining of rgn16_rect_t vectors into complex region  ***
 ***************************************************************/

/* Whole this section is about construction of a new complex region.
 *
 * The complex regions are regions which cannot be easily described by
 * a single rect. To work with them in some effective way, we place the
 * following limitations to their representations:
 *
 * (1) We manage an "extents" rect for the region, stored within
 *     rgn16_complex_t::vec[0].
 * (2) The region consists of a set of rects (all other members of the array
 *     rgn16_complex_t::vec), which are organized as follows:
 *   (2.1) The rects of the region do not overlap each other.
 *   (2.2) The rects are stored in an  array, ordered primarily by their y0.
 *   (2.3) Rects with the same y0 share also the same y1. (When adding a
 *         rectangle, it may be split to smaller ones which do follow this
 *         rule, or rects already within the region may need to be split.)
 *   (2.4) Within a subset ("band") of rects sharing y0 (and y1), the rects
 *         are ordered by their x0.
 *   (2.5) Rects in a band do not overlap each other. (They even do not touch
 *         each other, or they would be coalesced as per rule 2.6.)
 *   (2.6) Minimal set of such rects is used, i.e. when possible, the touching
 *         rects in a band or even whole bands are coalesced together to form
 *         larger rects and bands respectively.
 */


/* Try to coalesce two bands into one. Note that this is called only after
 * the cur_band is the last band in the complex region being constructed.
 */
static WORD
rgn16_coalesce_bands(rgn16_complex_t* c, WORD prev_band, WORD cur_band)
{
    rgn16_rect_t* prev_vec;
    rgn16_rect_t* cur_vec;
    WORD i, n;
    WORD y1;

    MC_ASSERT(cur_band > prev_band);
    n = cur_band - prev_band;

    /* Only bands of the same size can be coalesced. */
    if(n != c->n - cur_band)
        return cur_band;

    prev_vec = c->vec + prev_band;
    cur_vec = c->vec + cur_band;

    /* Only bands touching each other vertically can be coalesced. */
    if(prev_vec[0].y1 != cur_vec[0].y0)
        return cur_band;

    /* Only bands with the same horizontal layout can be coalesced. */
    for(i = 0; i < n; i++) {
        if(prev_vec[i].x0 != cur_vec[i].x0  ||  prev_vec[i].x1 != cur_vec[i].x1)
            return cur_band;
    }

    /* Do the coalesce. */
    y1 = cur_vec[0].y1;
    for(i = 0; i < n; i++)
        prev_vec[i].y1 = y1;

    /* Strip the coalesced band away. */
    c->n -= n;
    return prev_band;
}


/* Callback prototypes for rgn16_combine() below.
 */
typedef int (*rgn16_ovelap_func)(rgn16_complex_t* /*c*/,
                const rgn16_rect_t* /*vec1*/, const rgn16_rect_t* /*vec1_end*/,
                const rgn16_rect_t* /*vec2*/, const rgn16_rect_t* /*vec2_end*/,
                WORD /*y0*/, WORD /*y1*/);

typedef int (*rgn16_nonovelap_func)(rgn16_complex_t* /*c*/,
                const rgn16_rect_t* /*vec*/, const rgn16_rect_t* /*vec_end*/,
                WORD /*y0*/, WORD /*y1*/);

/* This function is the heart of the combining of two sets of rects together
 * when constructing a new complex region from them.
 *
 * Notes:
 *  -- On input, 'c' is assumed to be completely uninitialized.
 *  -- The function computes and fills its contents (on success), but with
 *     exception of extents (c->vec[0]) as caller usually can do so
 *     in a more effective way.
 *  -- Both input rect vectors, 'vec1' and 'vec2', have to follow constrains
 *     for complex regions about their organization and ordering.
 */
static int
rgn16_combine(rgn16_complex_t* c,
              const rgn16_rect_t* vec1, WORD n1,
              const rgn16_rect_t* vec2, WORD n2,
              rgn16_ovelap_func func_overlap,
              rgn16_nonovelap_func func_nonoverlap1,
              rgn16_nonovelap_func func_nonoverlap2)
{
    const rgn16_rect_t* rc1 = vec1;
    const rgn16_rect_t* rc1_end = vec1 + n1;
    const rgn16_rect_t* rc1_band_end;
    const rgn16_rect_t* rc2 = vec2;
    const rgn16_rect_t* rc2_end = vec2 + n2;
    const rgn16_rect_t* rc2_band_end;
    WORD y0, y1;     /* top and bottom of intersection */
    WORD prev_band;  /* last complete band */
    WORD cur_band;   /* current band (for coalescing into prev_band) */
    int err;

    MC_ASSERT(n1 > 0  &&  n2 > 0);

    c->n = 1; /* Reserve space for extents */
    c->alloc = MC_MAX3(8, 2 * n1, 2 * n2);
    c->vec = (rgn16_rect_t*) malloc(c->alloc * sizeof(rgn16_rect_t));
    if(MC_ERR(c->vec == NULL)) {
        MC_TRACE("rgn16_combine: malloc() failed.");
        return -1;
    }

    y1 = 0;
    prev_band = c->n;

    rc1_band_end = rc1;
    rc2_band_end = rc2;

    do {
        cur_band = c->n;
        y0 = MC_MAX(y1, MC_MIN(rc1->y0, rc2->y0));

        /* We need to know ends of current bands (i.e. of the sequences of
         * rects with same y0 and y1). */
        if(rc1_band_end == rc1) {
            while(rc1_band_end != rc1_end  &&  rc1_band_end->y0 == rc1->y0)
                rc1_band_end++;
        }
        if(rc2_band_end == rc2) {
            while(rc2_band_end != rc2_end  &&  rc2_band_end->y0 == rc2->y0)
                rc2_band_end++;
        }

        /* Handle a band which does not intersect with the other vector. */
        if(rc2->y0 > y0) {
            y1 = MC_MIN(rc1->y1, rc2->y0);
            if(func_nonoverlap1 != NULL) {
                err = func_nonoverlap1(c, rc1, rc1_band_end, y0, y1);
                if(MC_ERR(err != 0))
                    goto err_callback;
            }
            if(y1 == rc1->y1)
                rc1 = rc1_band_end;
            if(c->n != cur_band  &&  cur_band > prev_band)
                prev_band = rgn16_coalesce_bands(c, prev_band, cur_band);
            continue;
        }
        if(rc1->y0 > y0) {
            y1 = MC_MIN(rc2->y1, rc1->y0);
            if(func_nonoverlap2 != NULL) {
                err = func_nonoverlap2(c, rc2, rc2_band_end, y0, y1);
                if(MC_ERR(err != 0))
                    goto err_callback;
            }
            if(y1 == rc2->y1)
                rc2 = rc2_band_end;
            if(c->n != cur_band  &&  cur_band > prev_band)
                prev_band = rgn16_coalesce_bands(c, prev_band, cur_band);
            continue;
        }

        /* Handle bands from both vectors which intersect each other. */
        y1 = MC_MIN(rc1->y1, rc2->y1);
        cur_band = c->n;
        if(func_overlap != NULL) {
            err = func_overlap(c, rc1, rc1_band_end, rc2, rc2_band_end, y0, y1);
            if(MC_ERR(err != 0))
                goto err_callback;
        }

        /* If we added a new band, we may need to coalesce it with the previous
         * band. */
        if(c->n != cur_band  &&  cur_band > prev_band)
            prev_band = rgn16_coalesce_bands(c, prev_band, cur_band);

        /* Do a new band(s) if we are done with it. */
        if(y1 == rc1->y1)
            rc1 = rc1_band_end;
        if(y1 == rc2->y1)
            rc2 = rc2_band_end;
        y0 = y1;
    } while(rc1 != rc1_end  &&  rc2 != rc2_end);

    /* All what can be left are non-overlapping bands in one of the two
     * rect vectors. Note only 1st band of this remainder may need the
     * coalescing. */
    cur_band = c->n;
    if(rc1 != rc1_end  &&  func_nonoverlap1 != NULL) {
        BOOL coalesce = TRUE;
        do {
            y0 = MC_MAX(y1, rc1->y0);
            y1 = rc1->y1;

            while(rc1_band_end != rc1_end  &&  rc1_band_end->y0 == rc1->y0)
                rc1_band_end++;

            err = func_nonoverlap1(c, rc1, rc1_band_end, y0, rc1->y1);
            if(MC_ERR(err != 0))
                goto err_callback;

            if(coalesce) {
                if(c->n != cur_band  &&  cur_band > prev_band)
                    prev_band = rgn16_coalesce_bands(c, prev_band, cur_band);
                coalesce = FALSE;
            }

            rc1 = rc1_band_end;
        } while(rc1 != rc1_end);
    } else if(rc2 != rc2_end  &&  func_nonoverlap2 != NULL) {
        BOOL coalesce = TRUE;
        do {
            y0 = MC_MAX(y1, rc2->y0);
            y1 = rc2->y1;

            while(rc2_band_end != rc2_end  &&  rc2_band_end->y0 == rc2->y0)
                rc2_band_end++;

            err = func_nonoverlap2(c, rc2, rc2_band_end, y0, rc2->y1);
            if(MC_ERR(err != 0))
                goto err_callback;

            if(coalesce) {
                if(c->n != cur_band  &&  cur_band > prev_band)
                    prev_band = rgn16_coalesce_bands(c, prev_band, cur_band);
                coalesce = FALSE;
            }

            rc2 = rc2_band_end;
        } while(rc2 != rc2_end);
    }

    /* Realloc if our allocation strategy was too generous. */
    if(c->alloc > 8  &&  c->n < c->alloc / 2) {
        rgn16_rect_t* vec;
        WORD alloc = (c->n + 7) & ~0x7;

        vec = (rgn16_rect_t*) realloc(c->vec, alloc * sizeof(rgn16_rect_t));
        if(vec != NULL) {
            c->alloc = alloc;
            c->vec = vec;
        }
    }

    return 0;

err_callback:
    if(c->vec != NULL)
        free(c->vec);
    return -1;
}


/* Helper for the callbacks below.
 */
static int
rgn16_append_rect(rgn16_complex_t* c, WORD x0, WORD y0, WORD x1, WORD y1)
{
    if(c->n >= c->alloc) {
        rgn16_rect_t* vec;
        WORD alloc = c->alloc * 2;

        vec = (rgn16_rect_t*) realloc(c->vec, alloc * sizeof(rgn16_rect_t));
        if(MC_ERR(vec == NULL)) {
            MC_TRACE("rgn16_append_rect: realloc() failed.");
            return -1;
        }

        c->alloc = alloc;
        c->vec = vec;
    }

    rgn16_rect_set(c->vec + c->n, x0, y0, x1, y1);
    c->n++;
    return 0;
}

/* Helper macros for callbacks below.
 */
#define APPEND(x0_, x1_)                                                      \
    do {                                                                      \
        if(MC_ERR(rgn16_append_rect(c, (x0_), y0, (x1_), y1) != 0))           \
            goto err_append_rect;                                             \
    } while(0)

#define MERGE(x0_, x1_)                                                       \
    do {                                                                      \
        if(c->n > 1  &&                                                       \
           c->vec[c->n - 1].y0 == y0  &&                                      \
           (x0_) <= c->vec[c->n - 1].x1  &&                                   \
           (x1_) > c->vec[c->n - 1].x1)                                       \
        {                                                                     \
            c->vec[c->n - 1].x1 = (x1_);                                      \
        } else {                                                              \
            APPEND((x0_), (x1_));                                             \
        }                                                                     \
    } while(0)


/* Combine callback for uniting two bands.
 */
static int
rgn16_combine_union_overlapped_bands(rgn16_complex_t* c,
                const rgn16_rect_t* vec1, const rgn16_rect_t* vec1_end,
                const rgn16_rect_t* vec2, const rgn16_rect_t* vec2_end,
                WORD y0, WORD y1)
{
    const rgn16_rect_t* rc1 = vec1;
    const rgn16_rect_t* rc2 = vec2;
    WORD x0, x1;

    x0 = 0;

    /* All rects are subject for coalescing with the previous one,
     * if possible. */
    while(rc1 != vec1_end  &&  rc2 != vec2_end) {
        x0 = MC_MAX(x0, MC_MIN(rc1->x0, rc2->x0));
        if(rc2->x0 > x0) {
            x1 = MC_MIN(rc1->x1, rc2->x0);
            MERGE(x0, x1);
            rc1++;
        } else {
            x1 = MC_MIN(rc2->x1, rc1->x0);
            MERGE(x0, x1);
            rc2++;
        }
        x0 = x1;
    }

    /* Finish the tail of the remaining vector. */
    if(rc1 != vec1_end) {
        MERGE(x0, rc1->x1);
        rc1++;
        while(rc1 != vec1_end) {
            APPEND(rc1->x0, rc1->x1);
            rc1++;
        }
    } else {
        MERGE(rc2->x0, rc2->x1);
        rc2++;
        while(rc2 != vec2_end) {
            APPEND(rc2->x0, rc2->x1);
            rc2++;
        }
    }

    return 0;

err_append_rect:
    MC_TRACE("rgn16_combine_union_overlapped_bands: rgn16_append_rect() failed");
    return -1;
}

/* Combine callback for subtracting two bands.
 */
static int
rgn16_combine_subtract_overlapped_bands(rgn16_complex_t* c,
                const rgn16_rect_t* vec1, const rgn16_rect_t* vec1_end,
                const rgn16_rect_t* vec2, const rgn16_rect_t* vec2_end,
                WORD y0, WORD y1)
{
    const rgn16_rect_t* rc1 = vec1;
    const rgn16_rect_t* rc2 = vec2;
    WORD x0 = rc1->x0;

    while(rc1 != vec1_end  &&  rc2 != vec2_end) {
        if(rc1->x1 <= rc2->x0) {
            /* Minuend completely precedes the subtrahend. */
            APPEND(x0, rc1->x1);
            rc1++;
            if(rc1 != vec1_end)
                x0 = rc1->x0;
        } else if(x0 < rc2->x0) {
            /* Minuend partially precedes the subtrahend. */
            APPEND(x0, rc2->x0);
            x0 = rc2->x0;
        } else if(rc1->x1 <= rc2->x1) {
            /* Minuend completely covered by the subtrahend. */
            rc1++;
            if(rc1 != vec1_end)
                x0 = rc1->x0;
            if(x0 >= rc2->x1)
                rc2++;
        } else {
            /* Minuend partially covered by the subtrahend. */
            x0 = rc2->x1;
            rc2++;
        }
    }

    /* Finish the tail of the minuend vector. */
    while(rc1 != vec1_end) {
        APPEND(x0, rc1->x1);
        rc1++;
        if(rc1 != vec1_end)
            x0 = rc1->x0;
    }

    return 0;

err_append_rect:
    MC_TRACE("rgn16_combine_subtract_overlapped_bands: rgn16_append_rect() failed");
    return -1;
}

/* Combine callback for xor'ing two bands.
 */
static int
rgn16_combine_xor_overlapped_bands(rgn16_complex_t* c,
                const rgn16_rect_t* vec1, const rgn16_rect_t* vec1_end,
                const rgn16_rect_t* vec2, const rgn16_rect_t* vec2_end,
                WORD y0, WORD y1)
{
    const rgn16_rect_t* rc1 = vec1;
    const rgn16_rect_t* rc2 = vec2;
    WORD x0 = 0;

    while(rc1 != vec1_end  &&  rc2 != vec2_end) {
        x0 = MC_MAX(x0, MC_MIN(rc1->x0, rc2->x0));

        if(rc2->x0 > x0) {
            if(rc2->x0 >= rc1->x1) {
                MERGE(x0, rc1->x1);
                x0 = rc1->x1;
                rc1++;
            } else {
                MERGE(x0, rc2->x0);
                x0 = rc2->x0;
            }
        } else if(rc1->x0 > x0) {
            if(rc1->x0 >= rc2->x1) {
                MERGE(x0, rc2->x1);
                x0 = rc2->x1;
                rc2++;
            } else {
                MERGE(x0, rc1->x0);
                x0 = rc1->x0;
            }
        } else {
            if(rc1->x1 < rc2->x1) {
                x0 = rc1->x1;
                rc1++;
            } else if(rc2->x1 < rc1->x1) {
                x0 = rc2->x1;
                rc2++;
            } else {
                rc1++;
                rc2++;
                x0 = rc1->x1;
            }
        }
    }

    /* Finish the tail of the remaining vector. */
    if(rc1 != vec1_end) {
        x0 = MC_MAX(x0, rc1->x0);
        MERGE(x0, rc1->x1);
        rc1++;
        while(rc1 != vec1_end) {
            APPEND(rc1->x0, rc1->x1);
            rc1++;
        }
    } else if(rc2 != vec2_end) {
        x0 = MC_MAX(x0, rc2->x0);
        MERGE(x0, rc2->x1);
        rc2++;
        while(rc2 != vec2_end) {
            APPEND(rc2->x0, rc2->x1);
            rc2++;
        }
    }

    return 0;

err_append_rect:
    MC_TRACE("rgn16_combine_xor_overlapped_bands: rgn16_append_rect() failed");
    return -1;
}

/* Combine callback for adding a whole (non-overlapped) band.
 */
static int
rgn16_combine_add_band(rgn16_complex_t* c,
                const rgn16_rect_t* vec, const rgn16_rect_t* vec_end,
                WORD y0, WORD y1)
{
    const rgn16_rect_t* rc;

    for(rc = vec; rc != vec_end; rc++)
        APPEND(rc->x0, rc->x1);
    return 0;

err_append_rect:
    MC_TRACE("rgn16_combine_add_band: rgn16_append_rect() failed");
    return -1;
}

/* Wrapper of rgn16_combine() to create a union from two rect sets.
 */
static int
rgn16_do_union(rgn16_complex_t* c,
               const rgn16_rect_t* vec1, WORD n1, const rgn16_rect_t* extents1,
               const rgn16_rect_t* vec2, WORD n2, const rgn16_rect_t* extents2)
{
    int err;

    err = rgn16_combine(c, vec1, n1, vec2, n2,
                        rgn16_combine_union_overlapped_bands,
                        rgn16_combine_add_band,
                        rgn16_combine_add_band);
    if(MC_ERR(err != 0)) {
        MC_TRACE("rgn16_union: rgn16_combine() failed.");
        return -1;
    }

    /* Set new extents. */
    c->vec[0].y0 = c->vec[1].y0;
    c->vec[0].y1 = c->vec[c->n - 1].y1;
    c->vec[0].x0 = MC_MIN(extents1->x0, extents2->x0);
    c->vec[0].x1 = MC_MAX(extents1->x1, extents2->x1);

    return 0;
}

/* Wrapper of rgn16_combine() to create a subtraction from two rect sets.
 */
static int
rgn16_do_subtract(rgn16_complex_t* c,
                  const rgn16_rect_t* vec1, WORD n1, const rgn16_rect_t* extents1,
                  const rgn16_rect_t* vec2, WORD n2, const rgn16_rect_t* extents2)
{
    int err;
    WORD i;

    err = rgn16_combine(c, vec1, n1, vec2, n2,
                        rgn16_combine_subtract_overlapped_bands,
                        rgn16_combine_add_band,
                        NULL);
    if(MC_ERR(err != 0)) {
        MC_TRACE("rgn16_subtract: rgn16_combine() failed.");
        return -1;
    }

    if(c->n < 2)
        return 0;

    /* Set new extents. Vertical extents are trivial. */
    c->vec[0].y0 = c->vec[1].y0;
    c->vec[0].y1 = c->vec[c->n - 1].y1;

    if(extents1->x0 < extents2->x0  &&  extents2->x1 < extents1->x1) {
        /* If the subtrahend does not touch the leftmost and rightmost parts
         * of the minuend as the minuend, then result horizontal extents
         * are not changed. */
        c->vec[0].x0 = extents1->x0;
        c->vec[0].x1 = extents1->x1;
    } else {
        /* Otherwise we have to inspect all rects within the resulted region. */
        c->vec[0].x0 = c->vec[1].x0;
        c->vec[0].x1 = c->vec[1].x1;
        for(i = 2; i < c->n; i++) {
            if(c->vec[i].x0 < c->vec[0].x0)
                c->vec[0].x0 = c->vec[i].x0;

            if(c->vec[i].x1 > c->vec[0].x1)
                c->vec[0].x1 = c->vec[i].x1;
        }
    }

    return 0;
}

/* Wrapper of rgn16_combine() to create a xor from two rect sets.
 */
static int
rgn16_do_xor(rgn16_complex_t* c,
             const rgn16_rect_t* vec1, WORD n1, const rgn16_rect_t* extents1,
             const rgn16_rect_t* vec2, WORD n2, const rgn16_rect_t* extents2)
{
    int err;

    err = rgn16_combine(c, vec1, n1, vec2, n2,
                        rgn16_combine_xor_overlapped_bands,
                        rgn16_combine_add_band,
                        rgn16_combine_add_band);
    if(MC_ERR(err != 0)) {
        MC_TRACE("rgn16_xor: rgn16_combine() failed.");
        return -1;
    }

    /* Set new extents. */
    c->vec[0].y0 = c->vec[1].y0;
    c->vec[0].y1 = c->vec[c->n - 1].y1;
    c->vec[0].x0 = MC_MIN(extents1->x0, extents2->x0);
    c->vec[0].x1 = MC_MAX(extents1->x1, extents2->x1);

    return 0;
}


/****************************************
 ***  Simple unit tests of the above  ***
 ****************************************/

#if DEBUG >= 2

/* Note the functions are not exposed from the MCTRL.DLL hence we cannot use
 * normal unit tests here. To enable these tests, build with -DDEBUG=2.
 */

static void
rgn16_test_compute_extents(rgn16_rect_t* extents, const rgn16_rect_t* vec, WORD n)
{
    WORD i;
    MC_ASSERT(n > 0);

    rgn16_rect_copy(extents, &vec[0]);
    for(i = 1; i < n; i++) {
        rgn16_rect_set(extents,
            MC_MIN(extents->x0, vec[i].x0), MC_MIN(extents->y0, vec[i].y0),
            MC_MAX(extents->x1, vec[i].x1), MC_MAX(extents->y1, vec[i].y1));
    }
}

static void
rgn16_test_check(int (*func)(rgn16_complex_t*,
                             const rgn16_rect_t*, WORD, const rgn16_rect_t*,
                             const rgn16_rect_t*, WORD, const rgn16_rect_t*),
                 const rgn16_rect_t* vec1, WORD n1,  /* input 1 */
                 const rgn16_rect_t* vec2, WORD n2,  /* input 2 */
                 const rgn16_rect_t* vecR, WORD nR)  /* expected result */
{
    rgn16_rect_t extents1;
    rgn16_rect_t extents2;
    rgn16_rect_t extentsR;
    rgn16_complex_t c;
    int err;
    WORD i;

    rgn16_test_compute_extents(&extents1, vec1, n1);
    rgn16_test_compute_extents(&extents2, vec2, n2);
    if(nR > 0)
        rgn16_test_compute_extents(&extentsR, vecR, nR);

    err = func(&c, vec1, n1, &extents1, vec2, n2, &extents2);

    MC_ASSERT(err == 0);
    MC_ASSERT(c.n == nR + 1);
    if(nR > 0)
        MC_ASSERT(rgn16_rect_equals_rect(&c.vec[0], &extentsR));
    for(i = 0; i < nR; i++)
        MC_ASSERT(rgn16_rect_equals_rect(&c.vec[i+1], &vecR[i]));

    free(c.vec);
}

void
rgn16_test(void)
{
#define TEST_UNION() rgn16_test_check(rgn16_do_union,                        \
                                      vec1, MC_SIZEOF_ARRAY(vec1),           \
                                      vec2, MC_SIZEOF_ARRAY(vec2),           \
                                      vecR, MC_SIZEOF_ARRAY(vecR))

    {   /* Union with no overlap. */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {10,30,20,40} };
        const rgn16_rect_t vecR[] = { {10,10,20,20}, {10,30,20,40} };
        TEST_UNION();
    }

    {   /* Union with vertical overlap (split into multiple bands). */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {30,15,40,25} };
        const rgn16_rect_t vecR[] = { {10,10,20,15}, {10,15,20,20}, {30,15,40,20}, {30,20,40,25} };
        TEST_UNION();
    }

    {   /* Union with band coalescing. */
        const rgn16_rect_t vec1[] = { {10,10,20,15}, {10,20,20,30} };
        const rgn16_rect_t vec2[] = { {10,15,20,25} };
        const rgn16_rect_t vecR[] = { {10,10,20,30} };
        TEST_UNION();
    }

    {   /* Union with overlap. */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {15,15,25,25} };
        const rgn16_rect_t vecR[] = { {10,10,20,15}, {10,15,25,20}, {15,20,25,25} };
        TEST_UNION();
    }

#define TEST_SUBTRACT() rgn16_test_check(rgn16_do_subtract,                  \
                                      vec1, MC_SIZEOF_ARRAY(vec1),           \
                                      vec2, MC_SIZEOF_ARRAY(vec2),           \
                                      vecR, MC_SIZEOF_ARRAY(vecR))

    {   /* Subtract with no overlap. */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {10,30,20,40} };
        const rgn16_rect_t vecR[] = { {10,10,20,20} };
        TEST_SUBTRACT();
    }

    {   /* Subtract with complete overlap. */
        const rgn16_rect_t vec1[] = { {10,10,20,15}, {10,20,20,30} };
        const rgn16_rect_t vec2[] = { {10,10,50,50} };
        const rgn16_rect_t vecR[] = { };
        TEST_SUBTRACT();
    }

    {   /* Subtract with partial overlap. */
        const rgn16_rect_t vec1[] = { {10,10,25,20}, {10,25,20,30} };
        const rgn16_rect_t vec2[] = { {15,15,50,50} };
        const rgn16_rect_t vecR[] = { {10,10,25,15}, {10,15,15,20}, {10,25,15,30} };
        TEST_SUBTRACT();
    }

#define TEST_XOR() rgn16_test_check(rgn16_do_xor,                            \
                                    vec1, MC_SIZEOF_ARRAY(vec1),             \
                                    vec2, MC_SIZEOF_ARRAY(vec2),             \
                                    vecR, MC_SIZEOF_ARRAY(vecR))

    {   /* Xor with no overlap */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {30,10,40,20} };
        const rgn16_rect_t vecR[] = { {10,10,20,20}, {30,10,40,20} };
        TEST_XOR();
    }

    {   /* Xor with full overlap. */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {10,10,20,20} };
        const rgn16_rect_t vecR[] = { };
        TEST_XOR();
    }

    {   /* Xor with overlap. */
        const rgn16_rect_t vec1[] = { {10,10,20,20} };
        const rgn16_rect_t vec2[] = { {15,15,25,25} };
        const rgn16_rect_t vecR[] = { {10,10,20,15}, {10,15,15,20}, {20,15,25,20}, {15,20,25,25} };
        TEST_XOR();
    }
}

#endif  /* #if DEBUG >= 2 */


/**************************
 ***  Global functions  ***
 **************************/

const rgn16_rect_t*
rgn16_extents(const rgn16_t* rgn)
{
    if(rgn->n == 0)
        return NULL;
    else if(rgn->n == 1)
        return &rgn->s.rc;
    else
        return &rgn->c.vec[0];
}

BOOL
rgn16_equals_rgn(const rgn16_t* rgn1, const rgn16_t* rgn2)
{
    const rgn16_rect_t* extents1;
    const rgn16_rect_t* vec1;
    WORD n1;
    const rgn16_rect_t* extents2;
    const rgn16_rect_t* vec2;
    WORD n2;

    switch(rgn1->n) {
        case 0:     extents1 = NULL;            vec1 = NULL;            n1 = 0;           break;
        case 1:     extents1 = &rgn1->s.rc;     vec1 = &rgn1->s.rc;     n1 = 1;           break;
        default:    extents1 = &rgn1->c.vec[0]; vec1 = &rgn1->c.vec[1]; n1 = rgn1->n - 1; break;
    }

    switch(rgn2->n) {
        case 0:     extents2 = NULL;            vec2 = NULL;            n2 = 0;           break;
        case 1:     extents2 = &rgn2->s.rc;     vec2 = &rgn2->s.rc;     n2 = 1;           break;
        default:    extents2 = &rgn2->c.vec[0]; vec2 = &rgn2->c.vec[1]; n2 = rgn2->n - 1; break;
    }

    if(n1 != n2)
        return FALSE;

    if(n1 == 0)
        return TRUE;

    if(n1 >= 2  &&  !rgn16_rect_equals_rect(extents1, extents2))
        return FALSE;

    return (memcmp(vec1, vec2, n1 * sizeof(rgn16_rect_t)) == 0);
}

BOOL
rgn16_contains_rect(const rgn16_t* rgn, const rgn16_rect_t* rect)
{
    WORD i;
    WORD y;

    /* Empty region */
    if(rgn->n == 0)
        return FALSE;

    /* Simple region */
    if(rgn->n == 1)
        return rgn16_rect_contains_rect(&rgn->s.rc, rect);

    /* complex region */
    if(!rgn16_rect_contains_rect(&rgn->c.vec[0], rect))
        return FALSE;

    y = rect->y0;
    for(i = 1; i < rgn->n; i++) {
        if(rgn->c.vec[i].y1 <= y)
            continue;
        if(rgn->c.vec[i].y0 > y)
            break;
        if(rgn->c.vec[i].x1 <= rect->x0)
            continue;

        if(rgn->c.vec[i].x0 <= rect->x0  &&  rect->x1 <= rgn->c.vec[i].x1) {
            y = rgn->c.vec[i].y1;
            if(y >= rect->y1)
                return TRUE;
        } else {
            break;
        }
    }

    return FALSE;
}

int
rgn16_copy(rgn16_t* rgnR, const rgn16_t* rgn1)
{
    memcpy(rgnR, rgn1, sizeof(rgn16_t));

    if(rgn1->n >= 2) {
        rgnR->c.vec = (rgn16_rect_t*) malloc(rgn1->c.n * sizeof(rgn16_rect_t));
        if(MC_ERR(rgnR->c.vec == NULL)) {
            MC_TRACE("rgn16_copy: malloc() failed.");
            return -1;
        }

        memcpy(rgnR->c.vec, rgn1->c.vec, rgn1->c.n * sizeof(rgn16_rect_t));
    }

    return 0;
}

int
rgn16_union(rgn16_t* rgnR, const rgn16_t* rgn1, const rgn16_t* rgn2)
{
    const rgn16_rect_t* extents1;
    const rgn16_rect_t* vec1;
    WORD n1;
    const rgn16_rect_t* extents2;
    const rgn16_rect_t* vec2;
    WORD n2;
    int err;

    switch(rgn1->n) {
        case 0:     extents1 = NULL;            vec1 = NULL;            n1 = 0;           break;
        case 1:     extents1 = &rgn1->s.rc;     vec1 = &rgn1->s.rc;     n1 = 1;           break;
        default:    extents1 = &rgn1->c.vec[0]; vec1 = &rgn1->c.vec[1]; n1 = rgn1->n - 1; break;
    }
    switch(rgn2->n) {
        case 0:     extents2 = NULL;            vec2 = NULL;            n2 = 0;           break;
        case 1:     extents2 = &rgn2->s.rc;     vec2 = &rgn2->s.rc;     n2 = 1;           break;
        default:    extents2 = &rgn2->c.vec[0]; vec2 = &rgn2->c.vec[1]; n2 = rgn2->n - 1; break;
    }

    /* Simple cases */
    if(n1 == 0)
        return rgn16_copy(rgnR, rgn2);

    if(n2 == 0)
        return rgn16_copy(rgnR, rgn1);

    if(n1 == 1  &&  rgn16_rect_contains_rect(extents1, extents2))
        return rgn16_copy(rgnR, rgn1);

    if(n2 == 1  &&  rgn16_rect_contains_rect(extents2, extents1))
        return rgn16_copy(rgnR, rgn2);

    if(n1 == 1  &&  n2 == 1) {
        if(extents1->x0 == extents2->x0  &&  extents1->x1 == extents2->x1  &&
           extents1->y1 >= extents2->y0  &&  extents1->y0 <= extents2->y1)
        {
            rgnR->s.n = 1;
            rgnR->s.rc.x0 = extents1->x0;
            rgnR->s.rc.y0 = MC_MIN(extents1->y0, extents2->y0);
            rgnR->s.rc.x1 = extents1->x1;
            rgnR->s.rc.y1 = MC_MAX(extents1->y1, extents2->y1);
            return 0;
        }

        if(extents1->y0 == extents2->y0  &&  extents1->y1 == extents2->y1  &&
           extents1->x1 >= extents2->x0  &&  extents1->x0 <= extents2->x1)
        {
            rgnR->s.n = 1;
            rgnR->s.rc.x0 = MC_MIN(extents1->x0, extents2->x0);
            rgnR->s.rc.y0 = extents1->y0;
            rgnR->s.rc.x1 = MC_MAX(extents1->x1, extents2->x1);
            rgnR->s.rc.y1 = extents1->y1;
            return 0;
        }
    }

    /* General case */
    err = rgn16_do_union(&rgnR->c, vec1, n1, extents1, vec2, n2, extents2);
    if(MC_ERR(err != 0)) {
        MC_TRACE("rgn16_union: rgn16_do_union() failed.");
        return -1;
    }

    return 0;
}

int
rgn16_subtract(rgn16_t* rgnR, const rgn16_t* rgn1, const rgn16_t* rgn2)
{
    const rgn16_rect_t* extents1;
    const rgn16_rect_t* vec1;
    WORD n1;
    const rgn16_rect_t* extents2;
    const rgn16_rect_t* vec2;
    WORD n2;
    int err;

    switch(rgn1->n) {
        case 0:     extents1 = NULL;            vec1 = NULL;            n1 = 0;           break;
        case 1:     extents1 = &rgn1->s.rc;     vec1 = &rgn1->s.rc;     n1 = 1;           break;
        default:    extents1 = &rgn1->c.vec[0]; vec1 = &rgn1->c.vec[1]; n1 = rgn1->n - 1; break;
    }
    switch(rgn2->n) {
        case 0:     extents2 = NULL;            vec2 = NULL;            n2 = 0;           break;
        case 1:     extents2 = &rgn2->s.rc;     vec2 = &rgn2->s.rc;     n2 = 1;           break;
        default:    extents2 = &rgn2->c.vec[0]; vec2 = &rgn2->c.vec[1]; n2 = rgn2->n - 1; break;
    }

    /* Simple cases */
    if(n1 == 0  ||  n2 == 0)
        return rgn16_copy(rgnR, rgn1);

    if(!rgn16_rect_overlaps_rect(extents1, extents2))
        return rgn16_copy(rgnR, rgn1);

    if(n1 == 1  &&  n2 == 1  &&  rgn16_rect_contains_rect(extents2, extents1)) {
        rgnR->n = 0;
        return 0;
    }

    /* General case */
    err = rgn16_do_subtract(&rgnR->c, vec1, n1, extents1, vec2, n2, extents2);
    if(MC_ERR(err != 0)) {
        MC_TRACE("rgn16_subtract: rgn16_do_subtract() failed.");
        return -1;
    }

    if(rgnR->c.n == 1) {
        /* Nothing but extents? The subtraction ate out whole minuend. */
        free(rgnR->c.vec);
        rgnR->n = 0;
    }

    return 0;
}

int
rgn16_xor(rgn16_t* rgnR, const rgn16_t* rgn1, const rgn16_t* rgn2)
{
    const rgn16_rect_t* extents1;
    const rgn16_rect_t* vec1;
    WORD n1;
    const rgn16_rect_t* extents2;
    const rgn16_rect_t* vec2;
    WORD n2;
    int err;

    switch(rgn1->n) {
        case 0:     extents1 = NULL;            vec1 = NULL;            n1 = 0;           break;
        case 1:     extents1 = &rgn1->s.rc;     vec1 = &rgn1->s.rc;     n1 = 1;           break;
        default:    extents1 = &rgn1->c.vec[0]; vec1 = &rgn1->c.vec[1]; n1 = rgn1->n - 1; break;
    }
    switch(rgn2->n) {
        case 0:     extents2 = NULL;            vec2 = NULL;            n2 = 0;           break;
        case 1:     extents2 = &rgn2->s.rc;     vec2 = &rgn2->s.rc;     n2 = 1;           break;
        default:    extents2 = &rgn2->c.vec[0]; vec2 = &rgn2->c.vec[1]; n2 = rgn2->n - 1; break;
    }

    /* Simple cases */
    if(n1 == 0)
        return rgn16_copy(rgnR, rgn2);

    if(n2 == 0)
        return rgn16_copy(rgnR, rgn1);

    if(rgn16_equals_rgn(rgn1, rgn2)) {
        rgnR->n = 0;
        return 0;
    }

    /* General case */
    err = rgn16_do_xor(&rgnR->c, vec1, n1, extents1, vec2, n2, extents2);
    if(MC_ERR(err != 0)) {
        MC_TRACE("rgn16_xor: rgn16_do_xor() failed.");
        return -1;
    }

    return 0;
}
