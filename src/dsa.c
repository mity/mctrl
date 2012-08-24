/*
 * Copyright (c) 2011 Martin Mitas
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

#include "dsa.h"


/* Uncomment this to have more verbous traces from this module. */
/*#define DSA_DEBUG     1*/

#ifdef DSA_DEBUG
    #define DSA_TRACE         MC_TRACE
#else
    #define DSA_TRACE(...)    do { } while(0)
#endif



#define DSA_DEFAULT_GROW_SIZE    8


void
dsa_init(dsa_t* dsa, WORD item_size)
{
    DSA_TRACE("dsa_init(%p, %d)", dsa, (int)item_size);

    dsa->buffer = NULL;
    dsa->item_size = item_size;
    dsa->size = 0;
    dsa->capacity = 0;
}

void
dsa_fini(dsa_t* dsa, dsa_dtor_t dtor_func)
{
    DSA_TRACE("dsa_fini(%p)", dsa);

    if(dsa->buffer != NULL)
        free(dsa->buffer);
}

int
dsa_reserve(dsa_t* dsa, WORD size)
{
    BYTE* buffer;

    DSA_TRACE("dsa_reserve(%p, %d)", dsa, (int)size);

    if(dsa->size + size <= dsa->capacity)
        return 0;

    buffer = (BYTE*) malloc((dsa->size + size) * dsa->item_size);
    if(MC_ERR(buffer == NULL)) {
        MC_TRACE("dsa_reserve: malloc() failed.");
        return -1;
    }

    if(dsa->buffer != NULL) {
        memcpy(buffer, dsa->buffer, dsa->size * dsa->item_size);
        free(dsa->buffer);
    }

    dsa->buffer = buffer;
    dsa->capacity = dsa->size + size;
    return 0;
}

void*
dsa_insert_raw(dsa_t* dsa, WORD index)
{
    DSA_TRACE("dsa_insert_raw(%p, %d)", dsa, (int)index);
    MC_ASSERT(index <= dsa->size);

    if(dsa->capacity - dsa->size == 0) {
        if(MC_ERR(dsa_reserve(dsa, DSA_DEFAULT_GROW_SIZE) != 0)) {
            MC_TRACE("dsa_insert_raw: dsa_reserve() failed.");
            return NULL;
        }
    }

    if(index < dsa->size) {
        memmove(dsa_item(dsa, index+1), dsa_item(dsa, index),
                (dsa->size - index) * dsa->item_size);
    }

    dsa->size++;
    return dsa_item(dsa, index);
}

int
dsa_insert(dsa_t* dsa, WORD index, void* item)
{
    void* ptr;

    DSA_TRACE("dsa_insert(%p, %d, %p)", dsa, (int)index, item);
    MC_ASSERT(index <= dsa->size);

    ptr = dsa_insert_raw(dsa, index);
    if(MC_ERR(ptr == NULL)) {
        MC_TRACE("dsa_insert: dsa_insert_raw() failed.");
        return -1;
    }

    mc_inlined_memcpy(ptr, item, dsa->item_size);
    return index;
}

void
dsa_remove(dsa_t* dsa, WORD index, dsa_dtor_t dtor_func)
{
    BYTE* buffer;

    DSA_TRACE("dsa_remove(%p, %d)", dsa, (int)index);
    MC_ASSERT(index < dsa->size);

    if(dtor_func != NULL)
        dtor_func(dsa, dsa_item(dsa, index));

    if(dsa->size == 1) {
        if(dsa->buffer != NULL) {
            free(dsa->buffer);
            dsa->buffer = NULL;
        }
        dsa->size = 0;
        dsa->capacity = 0;
        return;
    }

    if(dsa->size + 7 > dsa->capacity) {
no_realloc:
        memmove(dsa_item(dsa, index), dsa_item(dsa, index+1),
                (dsa->size - index - 1) * dsa->item_size);
        dsa->size--;
        return;
    }

    buffer = (BYTE*) malloc((dsa->size - 1) * dsa->item_size);
    if(MC_ERR(buffer == NULL))
        goto no_realloc;

    memcpy(buffer, dsa->buffer, index * dsa->item_size);
    memcpy(buffer + index * dsa->item_size,
           dsa->buffer + (index+1) * dsa->item_size,
           (dsa->size - index - 1) * dsa->item_size);

    free(dsa->buffer);
    dsa->buffer = buffer;
    dsa->size--;
    dsa->capacity = dsa->size;
}

void
dsa_clear(dsa_t* dsa, dsa_dtor_t dtor_func)
{
    WORD index;

    DSA_TRACE("dsa_clear(%p)", dsa);

    if(dtor_func != NULL) {
        for(index = 0; index < dsa_size(dsa); index++)
            dtor_func(dsa, dsa_item(dsa, index));
    }

    if(dsa->buffer != NULL) {
        free(dsa->buffer);
        dsa->buffer = NULL;
    }
    dsa->size = 0;
    dsa->capacity = 0;
}

/* dsa_sort() below is adapted qsort() from glibc-2.13.
 * Copyright (C) 1991,1992,1996,1997,1999,2004 Free Software Foundation, Inc.
 * Written by Douglas C. Schmidt (schmidt@ics.uci.edu). */

#define MAX_THRESH 4

typedef struct {
    BYTE* lo;
    BYTE* hi;
} stack_node;

#define STACK_SIZE      (CHAR_BIT * sizeof(WORD))
#define PUSH(low, high) ((void)((top->lo = (low)), (top->hi = (high)), ++top))
#define POP(low, high)  ((void)(--top, (low = top->lo), (high = top->hi)))
#define STACK_NOT_EMPTY (stack < top)

void
dsa_sort(dsa_t* dsa, dsa_cmp_t cmp_func)
{
    register BYTE* base_ptr = (BYTE*) dsa->buffer;
    const WORD max_thresh = MAX_THRESH * dsa->item_size;

    DSA_TRACE("dsa_sort(%p, %p)", dsa, cmp_func);

    if (dsa->size == 0)
        return;

    if (dsa->size > MAX_THRESH) {
        BYTE* lo = base_ptr;
        BYTE* hi = &lo[dsa->item_size * (dsa->size - 1)];
        stack_node stack[STACK_SIZE];
        stack_node *top = stack;

        PUSH (NULL, NULL);

        while (STACK_NOT_EMPTY) {
            BYTE* left_ptr;
            BYTE* right_ptr;

            /* Select median value from among LO, MID, and HI. Rearrange
               LO and HI so the three values are sorted. This lowers the
               probability of picking a pathological pivot value and
               skips a comparison for both the LEFT_PTR and RIGHT_PTR in
               the while loops. */

            BYTE* mid = lo + dsa->item_size * ((hi - lo) / dsa->item_size >> 1);

            if(cmp_func(dsa, (void*)mid, (void*)lo) < 0)
                mc_inlined_memswap(mid, lo, dsa->item_size);
            if(cmp_func(dsa, (void*)hi, (void*)mid) < 0)
                mc_inlined_memswap(mid, hi, dsa->item_size);
            else
                goto jump_over;
            if (cmp_func(dsa, (void*)mid, (void*)lo) < 0)
                mc_inlined_memswap(mid, lo, dsa->item_size);
jump_over:
            left_ptr  = lo + dsa->item_size;
            right_ptr = hi - dsa->item_size;

            /* Here's the famous ``collapse the walls'' section of quicksort.
               Gotta like those tight inner loops!  They are the main reason
               that this algorithm runs much faster than others. */
            do {
                while(cmp_func(dsa, (void*)left_ptr, (void*)mid) < 0)
                    left_ptr += dsa->item_size;
                while(cmp_func(dsa, (void*)mid, (void*)right_ptr) < 0)
                    right_ptr -= dsa->item_size;

                if(left_ptr < right_ptr) {
                    mc_inlined_memswap(left_ptr, right_ptr, dsa->item_size);
                    if(mid == left_ptr)
                        mid = right_ptr;
                    else if(mid == right_ptr)
                        mid = left_ptr;
                    left_ptr += dsa->item_size;
                    right_ptr -= dsa->item_size;
                } else if(left_ptr == right_ptr) {
                    left_ptr += dsa->item_size;
                    right_ptr -= dsa->item_size;
                    break;
                }
            } while(left_ptr <= right_ptr);

            /* Set up pointers for next iteration.  First determine whether
               left and right partitions are below the threshold size.  If so,
               ignore one or both.  Otherwise, push the larger partition's
               bounds on the stack and continue sorting the smaller one. */

            if ((WORD)(right_ptr - lo) <= max_thresh) {
                if((WORD)(hi - left_ptr) <= max_thresh)
                    POP(lo, hi);   /* Ignore both small partitions. */
                else
                    lo = left_ptr; /* Ignore small left partition. */
            } else if((WORD)(hi - left_ptr) <= max_thresh) {
                hi = right_ptr;    /* Ignore small right partition. */
            } else if((right_ptr - lo) > (hi - left_ptr)) {
                /* Push larger left partition indices. */
                PUSH(lo, right_ptr);
                lo = left_ptr;
            } else {
                /* Push larger right partition indices. */
                PUSH(left_ptr, hi);
                hi = right_ptr;
            }
        }
    }

    /* Once the BASE_PTR array is partially sorted by quicksort the rest
       is completely sorted using insertion sort, since this is efficient
       for partitions below MAX_THRESH size. BASE_PTR points to the beginning
       of the array to sort, and END_PTR points at the very last element in
       the array (*not* one beyond it!). */

    {
        BYTE* const end_ptr = &base_ptr[dsa->item_size * (dsa->size - 1)];
        BYTE* tmp_ptr = base_ptr;
        BYTE* thresh = MC_MIN(end_ptr, base_ptr + max_thresh);
        register BYTE* run_ptr;

        /* Find smallest element in first threshold and place it at the
           array's beginning.  This is the smallest array element,
           and the operation speeds up insertion sort's inner loop. */

        for(run_ptr = tmp_ptr + dsa->item_size; run_ptr <= thresh; run_ptr += dsa->item_size) {
            if (cmp_func(dsa, (void*)run_ptr, (void*)tmp_ptr) < 0)
                tmp_ptr = run_ptr;
        }

        if(tmp_ptr != base_ptr)
            mc_inlined_memswap(tmp_ptr, base_ptr, dsa->item_size);

        /* Insertion sort, running from left-hand-side up to right-hand-side.  */

        run_ptr = base_ptr + dsa->item_size;
        while((run_ptr += dsa->item_size) <= end_ptr) {
            tmp_ptr = run_ptr - dsa->item_size;
            while(cmp_func(dsa, (void*)run_ptr, (void*)tmp_ptr) < 0)
                tmp_ptr -= dsa->item_size;

            tmp_ptr += dsa->item_size;
            if(tmp_ptr != run_ptr) {
                BYTE* trav;

                trav = run_ptr + dsa->item_size;
                while(--trav >= run_ptr) {
                    BYTE c = *trav;
                    BYTE* hi;
                    BYTE* lo;

                    for(hi = lo = trav; (lo -= dsa->item_size) >= tmp_ptr; hi = lo)
                        *hi = *lo;
                    *hi = c;
                }
            }
        }
    }
}

int
dsa_insert_sorted(dsa_t* dsa, void* item, dsa_cmp_t cmp_func)
{
    WORD index0, index1;
    WORD index;
    int cmp;

    DSA_TRACE("dsa_insert_sorted(%p, %p, %p)", dsa, item, cmp_func);

    /* App. developers may try to optimize and insert multiple items in the
     * correct order whenever possible. If they do, we can skip the bsearch. */
    if(dsa->size == 0 || cmp_func(dsa, item, dsa_item(dsa, dsa->size-1)) >= 0) {
        index = dsa->size;
        goto found_index;
    }

    /* Binary search */
    index0 = 0;
    index1 = dsa->size - 1;
    while(index0 < index1) {
        index = (index0 + index1) / 2;
        cmp = cmp_func(dsa, item, dsa_item(dsa, index));
        if(cmp < 0)
            index1 = index;
        else if(cmp > 0)
            index0 = index + 1;
        else
            goto found_index;
    }

    MC_ASSERT(index0 == index1);
    index = index1;

found_index:
    return dsa_insert(dsa, index, item);
}

int
dsa_move_sorted(dsa_t* dsa, WORD index, dsa_cmp_t cmp_func)
{
    WORD index0, index1;
    WORD old_index = index;
#ifdef __GNUC__
    BYTE tmp[dsa->item_size];
#else
    BYTE tmp[256];
    MC_ASSERT(dsa->item_size <= sizeof(tmp));
#endif

    DSA_TRACE("dsa_move_sorted(%p, %d, %p)", dsa, (int)index, cmp_func);
    MC_ASSERT(index < dsa->size);

    if(index < dsa->size-1  &&  cmp_func(dsa, dsa_item(dsa, index+1), dsa_item(dsa, index)) < 0) {
        /* We have to move the item to right */
        index0 = index + 1;
        index1 = dsa->size;
    } else if(index > 0  &&  cmp_func(dsa, dsa_item(dsa, index), dsa_item(dsa, index-1)) < 0) {
        /* We have to move the item to left */
        index0 = 0;
        index1 = index;
    } else {
        return index;
    }

    /* Binary search */
    while(index0 < index1) {
        int cmp;

        index = (index0 + index1) / 2;
        cmp = cmp_func(dsa, dsa_item(dsa, index), dsa_item(dsa, old_index));
        if(cmp < 0)
            index1 = index;
        else if(cmp > 0)
            index0 = index + 1;
        else
            goto found_index;
    }

    MC_ASSERT(index0 == index1);
    index = index1;

found_index:
    if(index == old_index)
        return index;

    /* Do the move */
    mc_inlined_memcpy(tmp, dsa_item(dsa, old_index), dsa->item_size);
    if(index < old_index) {
        memmove(dsa_item(dsa, index+1), dsa_item(dsa, index),
                (old_index - index) * dsa->item_size);
    } else {
        memmove(dsa_item(dsa, old_index), dsa_item(dsa, old_index+1),
                (index - old_index) * dsa->item_size);
    }
    mc_inlined_memcpy(dsa_item(dsa, index), tmp, dsa->item_size);
    return index;
}

int
dsa_insert_smart(dsa_t* dsa, WORD index, void* item, dsa_cmp_t cmp_func)
{
    BOOL need_sorted_insert = FALSE;

    DSA_TRACE("dsa_insert_smart(%p, %d, %p, %p)", dsa, (int)index, item, cmp_func);
    MC_ASSERT(index <= dsa->size);

    /* Check if [index] is OK with the respect to the order. */
    if(cmp_func != NULL) {
        if(index > 0  &&  cmp_func(dsa, item, dsa_item(dsa, index-1)) < 0)
            need_sorted_insert = TRUE;
        else if(index < dsa->size  &&  cmp_func(dsa, dsa_item(dsa, index), item) < 0)
            need_sorted_insert = TRUE;
    }

    /* Do the insert */
    if(need_sorted_insert) {
        index = dsa_insert_sorted(dsa, item, cmp_func);
        if(MC_ERR(index < 0))
            MC_TRACE("dsa_insert_smart: dsa_insert_sorted() failed.");
    } else {
        index = dsa_insert(dsa, index, item);
        if(MC_ERR(index < 0))
            MC_TRACE("dsa_insert_smart: dsa_insert() failed.");
    }

    return index;
}

