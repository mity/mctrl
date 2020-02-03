/*
 * Copyright (c) 2020 Martin Mitas
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

#include "comua.h"


int
comua_append(BUFFER* buffer, uint64_t num, DWORD flags)
{
    BYTE tmp[8];
    size_t n = 0;

    num = num << 1;
    if(flags & COMUA_FLAG_RECORD_LEADER)
        num |= 0x1;

    /* Write in the little-endian order (so the leading byte encodes the record
     * leader flag) 7-bit blocks, with the MSB set to indicate a leader byte
     * of the number. */
    tmp[n] = (num & 0x7f) | 0x80;
    num = num >> 7;
    n++;
    while(num > 0) {
        tmp[n] = (num & 0x7f);
        num = num >> 7;
        n++;
    }

    return buffer_append(buffer, tmp, n);
}

uint64_t
comua_read(const void* data, size_t size, size_t offset, size_t* p_end_offset)
{
    const BYTE* bytes = (const BYTE*)data;
    uint64_t num;
    int shift = 0;

    MC_ASSERT(offset < size);
    MC_ASSERT(bytes[offset] & 0x80);
    num = bytes[offset] & 0x7f;
    offset++;
    while(offset < size  &&  !(bytes[offset] & 0x80)) {
        shift += 7;
        num |= ((uint64_t)bytes[offset]) << shift;
        offset++;
    }

    if(p_end_offset != NULL)
        *p_end_offset = offset;
    return (num >> 1);
}

size_t
comua_bsearch(const void* data, size_t size, const void* key,
              int (*cmp_func)(const void*, const void*, size_t, size_t))
{
    BYTE* total_beg = (BYTE*) data;
    BYTE* total_end = total_beg + size;
    BYTE* beg = total_beg;
    BYTE* end = total_end;

    while(beg < end) {
        BYTE* record_beg = beg + ((end-beg) / 2);
        BYTE* record_end = record_beg;
        int cmp;

        /* Find the first byte of the record. */
        while((*record_beg & 0x81) != 0x81) {
            MC_ASSERT(record_beg > total_beg);
            record_beg--;
        }

        /* Find the first byte of the next record (or end of the COMUA). */
        do {
            record_end++;
        } while(record_end < total_end  &&  (*record_end & 0x81) != 0x81);

        cmp = cmp_func(key, data, size, record_beg - total_beg);
        if(cmp < 0)
            end = record_beg;
        else if(cmp > 0)
            beg = record_end;
        else
            return (size_t) (record_beg - total_beg);
    }

    return (size_t) -1;
}
