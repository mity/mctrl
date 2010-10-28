/*
 * Copyright (c) 2008-2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MC_GBUF_H
#define MC_GBUF_H

#include "misc.h"


/* Gap buffer implementation. 
 * See http://en.wikipedia.org/wiki/Gap_buffer */



/* Note that size_unit is element size in bytes. All other sizes/offsets
 * are in this measure unit. */
typedef struct gbuf_tag gbuf_t;
struct gbuf_tag {
    WORD size_unit;
    WORD page_size;
    BYTE* buffer;
    DWORD capacity;
    DWORD gap_offset;
    DWORD gap_size;
};


void gbuf_init(gbuf_t* gb, WORD size_unit, WORD page_size);
void gbuf_fini(gbuf_t* gb);

void gbuf_move_gap(gbuf_t* gb, DWORD offset);

int gbuf_insert(gbuf_t* gb, DWORD pos, void* elem_array, DWORD n);
void gbuf_remove(gbuf_t* gb, DWORD pos, DWORD n);


#define gbuf_size(gb)      ((gb)->capacity - (gb)->gap_size)

#define gbuf_element_on_offset(gb, offset)                                 \
    ((void*) &(gb)->buffer[(offset) * (gb)->size_unit])

#define gbuf_element(gb, pos)                                              \
    gbuf_element_on_offset(gb, (pos) < (gb)->gap_offset ?                  \
                              (pos) : (pos) + (gb)->gap_size)

#endif  /* MC_GBUF_H */
