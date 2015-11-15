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

#ifndef MC_DSA_H
#define MC_DSA_H

#include "misc.h"


typedef struct dsa_tag dsa_t;
struct dsa_tag {
    void* buffer;
    WORD item_size;
    WORD size;
    WORD capacity;
};

/* destructor */
typedef void (*dsa_dtor_t)(dsa_t*, void*);


static inline WORD
dsa_size(dsa_t* dsa)
{
    return dsa->size;
}

static inline WORD
dsa_index(dsa_t* dsa, const void* item)
{
    return (int) (((BYTE*)item - (BYTE*)dsa->buffer) / dsa->item_size);
}

static inline void*
dsa_item(dsa_t* dsa, WORD index)
{
    return (void*)&((BYTE*)dsa->buffer)[index * dsa->item_size];
}

/* This is harder to use, but it should lead to better optimization.
 * (Assuming item_size is a constant known at compile time.) */
static inline void*
dsa_item_(dsa_t* dsa, WORD index, WORD item_size)
{
    return (void*)&((BYTE*)dsa->buffer)[index * item_size];
}

#define DSA_ITEM(dsa, index, type)                                            \
        ((type*) dsa_item_((dsa), (index), sizeof(type)))


void dsa_init(dsa_t* dsa, WORD item_size);
void dsa_fini(dsa_t* dsa, dsa_dtor_t dtor_func);

int dsa_reserve(dsa_t* dsa, WORD size);

void* dsa_insert_raw(dsa_t* dsa, WORD index);
int dsa_insert(dsa_t* dsa, WORD index, void* item);
void dsa_remove(dsa_t* dsa, WORD index, dsa_dtor_t dtor_func);
void dsa_clear(dsa_t* dsa, dsa_dtor_t dtor_func);

/* Move an item to the new position. */
void dsa_move(dsa_t* dsa, WORD old_index, WORD new_index);


#endif  /* MC_DSA_H */
