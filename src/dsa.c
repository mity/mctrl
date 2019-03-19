/*
 * Copyright (c) 2011-2015 Martin Mitas
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


/* Uncomment this to have more verbose traces from this module. */
/*#define DSA_DEBUG     1*/

#ifdef DSA_DEBUG
    #define DSA_TRACE   MC_TRACE
#else
    #define DSA_TRACE   MC_NOOP
#endif


#define DSA_MAX_ITEM_SIZE               32


/* To avoid waste of memory as heap allocator needs to store somewhere some
 * internal bookkeeping. */
#ifdef _WIN64
    #define DSA_BIGBUFFER_SIZE                  2048
    #define DSA_BIGBUFFER_BOOKKEEPING_PADDING     32
#else
    #define DSA_BIGBUFFER_SIZE                  1024
    #define DSA_BIGBUFFER_BOOKKEEPING_PADDING     16
#endif


void
dsa_init(dsa_t* dsa, WORD item_size)
{
    DSA_TRACE("dsa_init(%p, %d)", dsa, (int)item_size);

    /* Check we do not use DSA for too big structures. */
    MC_ASSERT(item_size <= DSA_MAX_ITEM_SIZE);

    dsa->buffer = NULL;
    dsa->item_size = item_size;
    dsa->size = 0;
    dsa->capacity = 0;
}

void
dsa_fini(dsa_t* dsa, dsa_dtor_t dtor_func)
{
    WORD index;

    DSA_TRACE("dsa_fini(%p)", dsa);

    if(dtor_func != NULL) {
        for(index = 0; index < dsa_size(dsa); index++)
            dtor_func(dsa, dsa_item(dsa, index));
    }

    if(dsa->buffer != NULL)
        free(dsa->buffer);
}

int
dsa_reserve(dsa_t* dsa, WORD size)
{
    WORD capacity = dsa->size + size;
    BYTE* buffer;

    DSA_TRACE("dsa_reserve(%p, %d)", dsa, (int)size);

    if(capacity <= dsa->capacity)
        return 0;

    DSA_TRACE("dsa_reserve: Capacity growing: %d -> %d",
                (int) dsa->capacity, (int) capacity);

    buffer = (BYTE*) realloc(dsa->buffer,
                             (size_t)capacity * (size_t)dsa->item_size);
    if(MC_ERR(buffer == NULL)) {
        MC_TRACE("dsa_reserve: realloc() failed.");
        return -1;
    }

    dsa->buffer = buffer;
    dsa->capacity = capacity;
    return 0;
}

void*
dsa_insert_raw(dsa_t* dsa, WORD index)
{
    DSA_TRACE("dsa_insert_raw(%p, %d)", dsa, (int)index);
    MC_ASSERT(index <= dsa->size);

    if(dsa->size >= dsa->capacity) {
        BYTE* buffer;
        size_t sz = (size_t)dsa->size * (size_t)dsa->item_size;

        if(sz > DSA_BIGBUFFER_SIZE) {
            sz += DSA_BIGBUFFER_BOOKKEEPING_PADDING + dsa->item_size - 1;
            sz %= dsa->item_size;
        }

        /* Make the buffer about twice as large, but round up to power of two. */
        sz = 2 * sz;
#ifdef _WIN64
        MC_ASSERT(sizeof(size_t) == 8);
        sz = mc_round_up_to_power_of_two_64(sz);
#else
        MC_ASSERT(sizeof(size_t) == 4);
        sz = mc_round_up_to_power_of_two_32(sz);
#endif
        /* Make sure at least 4 items fit inside. */
        sz = MC_MAX(sz, 4 * dsa->item_size);

        if(sz > DSA_BIGBUFFER_SIZE) {
            sz -= DSA_BIGBUFFER_BOOKKEEPING_PADDING;
            sz %= dsa->item_size;
        }

        DSA_TRACE("dsa_insert_raw: Capacity growing: %d -> %d",
                    (int) dsa->capacity, (int) (sz / dsa->item_size));

        buffer = (BYTE*) realloc(dsa->buffer, sz);
        if(MC_ERR(buffer == NULL)) {
            MC_TRACE("dsa_insert_raw: realloc() failed.");
            return NULL;
        }

        dsa->buffer = buffer;
        dsa->capacity = sz / dsa->item_size;
    }

    if(index < dsa->size) {
        memmove(dsa_item(dsa, index+1), dsa_item(dsa, index),
                (size_t)(dsa->size - index) * (size_t)dsa->item_size);
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

    memcpy(ptr, item, dsa->item_size);
    return index;
}

void
dsa_remove(dsa_t* dsa, WORD index, dsa_dtor_t dtor_func)
{
    DSA_TRACE("dsa_remove(%p, %d)", dsa, (int)index);
    MC_ASSERT(index < dsa->size);

    /* Call the destructor. */
    if(dtor_func != NULL)
        dtor_func(dsa, dsa_item(dsa, index));

    /* Removing last element? Free the buffer altogether. */
    if(MC_UNLIKELY(dsa->size == 1)) {
        dsa_clear(dsa, NULL);
        return;
    }

    /* Remove the given element. */
    if(index < dsa->size - 1) {
        memmove(dsa_item(dsa, index), dsa_item(dsa, index+1),
                (size_t)(dsa->size - index - 1) * (size_t)dsa->item_size);
    }
    dsa->size--;

    /* Shrink if less then 25% of the buffer is used. */
    if(4 * dsa->size < dsa->capacity) {
        size_t sz;
        BYTE* buffer;

        sz = (size_t)dsa->capacity * (size_t)dsa->item_size;
        if(sz > DSA_BIGBUFFER_SIZE) {
            sz += DSA_BIGBUFFER_BOOKKEEPING_PADDING + dsa->item_size - 1;
            sz %= dsa->item_size;
        }

        sz = sz / 2;

        /* Make sure at least 4 items fit inside. */
        sz = MC_MAX(sz, 4 * dsa->item_size);

        if(sz > DSA_BIGBUFFER_SIZE) {
            sz -= DSA_BIGBUFFER_BOOKKEEPING_PADDING;
            sz %= dsa->item_size;
        }

        MC_ASSERT((size_t)dsa->size * (size_t)dsa->item_size < sz);

        DSA_TRACE("dsa_remove: Capacity shrinking: %d -> %d",
                    (int) dsa->capacity, (int) (sz / dsa->item_size));

        buffer = (BYTE*) realloc(dsa->buffer, sz);
        if(MC_ERR(buffer == NULL)) {
            MC_TRACE("dsa_remove: realloc() failed. Cannot shrink.");
            return;
        }

        dsa->buffer = buffer;
        dsa->capacity = sz / dsa->item_size;
    }
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

void
dsa_move(dsa_t* dsa, WORD old_index, WORD new_index)
{
    BYTE tmp[DSA_MAX_ITEM_SIZE];
    int i0, i1, n;

    DSA_TRACE("dsa_move(%p, %d -> %d)", dsa, (int) old_index, (int) new_index);
    MC_ASSERT(dsa->item_size <= DSA_MAX_ITEM_SIZE);

    if(old_index == new_index)
        return;

    if(old_index < new_index) {
        i0 = old_index + 1;
        i1 = old_index;
        n = new_index - old_index;
    } else {
        i0 = new_index;
        i1 = new_index + 1;
        n = old_index - new_index;
    }

    memcpy(&tmp, dsa_item(dsa, old_index), dsa->item_size);
    memmove(dsa_item(dsa, i1), dsa_item(dsa, i0), n * dsa->item_size);
    memcpy(dsa_item(dsa, new_index), &tmp, dsa->item_size);
}
