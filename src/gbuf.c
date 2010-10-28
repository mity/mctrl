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

#include "gbuf.h"


/* Uncomment this to have more verbous traces from this module. */
/*#define GBUF_DEBUG     1*/

#ifdef GBUF_DEBUG
    #define GBUF_TRACE        MC_TRACE
#else
    #define GBUF_TRACE(...)   do { } while(0)
#endif


void 
gbuf_init(gbuf_t* gb, WORD size_unit, WORD page_size)
{
    GBUF_TRACE("gbuf_init(%p, %hu, %hu)", gb, size_unit, page_size);
    
    gb->size_unit = size_unit;
    gb->page_size = page_size;
    gb->buffer = NULL;
    gb->capacity = 0;
    gb->gap_offset = 0;
    gb->gap_size = 0;
}

void 
gbuf_fini(gbuf_t* gb)
{
    GBUF_TRACE("gbuf_fini(%p)", gb);
    
    if(gb->buffer != NULL)
        free(gb->buffer);
}

void
gbuf_move_gap(gbuf_t* gb, DWORD offset)
{
    DWORD src_offset;
    DWORD dest_offset;
    DWORD n;
    
    GBUF_TRACE("gbuf_move_gap(%p, %lu->%lu)", gb, gb->gap_offset, offset);
    
    if(offset < gb->gap_offset) {
        src_offset = offset;
        dest_offset = offset + gb->gap_size;
        n = gb->gap_offset - offset;
    } else {
        src_offset = gb->gap_offset + gb->gap_size;
        dest_offset = gb->gap_offset;
        n = offset - gb->gap_offset;
    }
    
    memmove(gb->buffer + dest_offset * gb->size_unit, 
            gb->buffer + src_offset * gb->size_unit, 
            n * gb->size_unit);
}


static int
gbuf_resize(gbuf_t* gb, DWORD min_gap_size)
{
    BYTE* buffer;
    DWORD capacity;
    DWORD part2_size;

    GBUF_TRACE("gbuf_resize(%p, %lu)", gb, min_gap_size);
    
    /* Calc new capacity (what we really need now) */
    capacity = gb->capacity - gb->gap_size + min_gap_size;  
    /* Round it up to page boundary */
    capacity += gb->page_size - 1;
    capacity -= capacity % gb->page_size;
    /* Add some completely free pages to reduce reallocations in future
     * (in range 1...32, depending on current size) */
    capacity += gb->page_size * MC_MID(1, (capacity/gb->page_size) / 8, 32);
    
    GBUF_TRACE("gbuf_resize(%p, %lu): capacity %lu->%lu", 
               gb, min_gap_size, gb->capacity, capacity);
    
    /* Allocate new buffer */
    buffer = (BYTE*) malloc(capacity * gb->size_unit);
    if(MC_ERR(buffer == NULL)) {
        MC_TRACE("gbuf_resize: malloc(%lu) failed.", capacity * gb->size_unit);
        return -1;
    }
    
    /* Copy contents from old buffer */
    if(gb->gap_offset > 0)
        memcpy(buffer, gb->buffer, gb->gap_offset * gb->size_unit);
    part2_size = gb->capacity - gb->gap_offset - gb->gap_size;
    if(part2_size > 0)
        memcpy(buffer + capacity - part2_size, gb->buffer + gb->gap_offset + 
                                   gb->gap_size, part2_size * gb->size_unit);
        
    /* Install the new buffer */
    if(gb->buffer != NULL)
        free(gb->buffer);
    gb->buffer = buffer;
    gb->capacity = capacity;
    gb->gap_size = capacity - gb->gap_offset - part2_size;
    return 0;
}

int
gbuf_insert(gbuf_t* gb, DWORD pos, void* data, DWORD n)
{
    GBUF_TRACE("gbuf_insert(%p, %lu, %p, %lu)", gb, pos, data, n);
    
    if(n > gb->capacity - gb->gap_size) {
        /* Need more space */
        if(gbuf_resize(gb, n) != 0) {
            MC_TRACE("gbuf_insert: gbuf_resize(%ld) failed.", n);
            return -1;
        }
    }
    
    if(pos != gb->gap_offset)
        gbuf_move_gap(gb, pos);
    
    memcpy(gb->buffer + gb->gap_offset * gb->size_unit, data, n * gb->size_unit);
    gb->gap_offset += n;
    gb->gap_size -= n;
    return 0;
}

void 
gbuf_remove(gbuf_t* gb, DWORD pos, DWORD n)
{
    GBUF_TRACE("gbuf_remove(%p, %lu, %lu)", gb, pos, n);
    
    if(pos < gb->gap_size  ||  (2 * pos + n) / 2 <= (2 * gb->gap_offset + gb->gap_size) / 2) {
        /* Move gap so that the bytes to be removed are just before the gap. */
        if(pos + n != gb->gap_offset)
            gbuf_move_gap(gb, pos + n);
            
        /* The remove operation is now just making the gap larger */
        gb->gap_offset -= n;
        gb->gap_size += n;
    } else {
        /* Move gap so that the bytes to be removed are just after the gap. */
        gbuf_move_gap(gb, pos - gb->gap_size);
        
        /* The remove operation is now just making the gap larger */
        gb->gap_size += n;
    }
    
    if(gb->gap_size > gb->page_size * 
                        MC_MID(16, (gb->capacity/gb->page_size) / 2, 256)) {
        /* There is too much free space, shrink the buffer. */
        if(gbuf_resize(gb, 0) != 0)
            MC_TRACE("gbuf_remove: Failed to shring the buffer.");
    }
}

