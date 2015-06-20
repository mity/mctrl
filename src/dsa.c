/*
 * Copyright (c) 2011-2014 Martin Mitas
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
    #define DSA_TRACE         MC_TRACE
#else
    #define DSA_TRACE(...)    do { } while(0)
#endif


#define DSA_IS_COMPACT(dsa)             (dsa->capacity == 0xffff)

#define DSA_MAX_ITEM_SIZE               32
#define DSA_DEFAULT_GROW_SIZE(size)     (MC_MAX(8, (size) / 4))
#define DSA_DEFAULT_SHRINK_SIZE(size)   (2*DSA_DEFAULT_GROW_SIZE(size))


void
dsa_init_ex(dsa_t* dsa, WORD item_size, BOOL compact)
{
    DSA_TRACE("dsa_init(%p, %d)", dsa, (int)item_size);

    /* Check we do not use DSA for too big structures. */
    MC_ASSERT(item_size <= DSA_MAX_ITEM_SIZE);

    dsa->buffer = NULL;
    dsa->item_size = item_size;
    dsa->size = 0;
    dsa->capacity = (compact ? 0xffff : 0);
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

    if(!DSA_IS_COMPACT(dsa)  &&  capacity <= dsa->capacity)
        return 0;

    buffer = (BYTE*) realloc(dsa->buffer,
                             (size_t)capacity * (size_t)dsa->item_size);
    if(MC_ERR(buffer == NULL)) {
        MC_TRACE("dsa_reserve: realloc() failed.");
        return -1;
    }

    dsa->buffer = buffer;
    if(!DSA_IS_COMPACT(dsa))
        dsa->capacity = capacity;
    return 0;
}

void*
dsa_insert_raw(dsa_t* dsa, WORD index)
{
    DSA_TRACE("dsa_insert_raw(%p, %d)", dsa, (int)index);
    MC_ASSERT(index <= dsa->size);

    if(DSA_IS_COMPACT(dsa)  ||  dsa->capacity - dsa->size == 0) {
        WORD reserve = (DSA_IS_COMPACT(dsa) ? DSA_DEFAULT_GROW_SIZE(dsa->size) : 1);
        if(MC_ERR(dsa_reserve(dsa, reserve) != 0)) {
            MC_TRACE("dsa_insert_raw: dsa_reserve() failed.");
            return NULL;
        }
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
    BYTE* buffer;

    DSA_TRACE("dsa_remove(%p, %d)", dsa, (int)index);
    MC_ASSERT(index < dsa->size);

    if(dtor_func != NULL)
        dtor_func(dsa, dsa_item(dsa, index));

    /* Fast (no-realloc) path. */
    if(!DSA_IS_COMPACT(dsa)  &&
       dsa->capacity < dsa->size + DSA_DEFAULT_SHRINK_SIZE(dsa->size))
    {
no_realloc:
        memmove(dsa_item(dsa, index), dsa_item(dsa, index+1),
                (size_t)(dsa->size - index - 1) * (size_t)dsa->item_size);
        dsa->size--;
        return;
    }

    if(index == dsa->size - 1) {
        /* Getting rid of the tail element. */
        if(dsa->size > 1) {
            buffer = (BYTE*) realloc(dsa->buffer, (size_t)(dsa->size - 1) * (size_t)dsa->item_size);
            if(MC_ERR(buffer == NULL))
                goto no_realloc;
        } else {
            free(dsa->buffer);
            buffer = NULL;
        }
    } else {
        /* General path. */
        buffer = (BYTE*) malloc((size_t)(dsa->size - 1) * (size_t)dsa->item_size);
        if(MC_ERR(buffer == NULL))
            goto no_realloc;
        memcpy(buffer, dsa->buffer, (size_t)index * (size_t)dsa->item_size);
        memcpy(buffer + index * dsa->item_size, dsa_item(dsa, index+1),
               (size_t)(dsa->size - index - 1) * (size_t)dsa->item_size);
        free(dsa->buffer);
    }
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

