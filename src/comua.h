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

#ifndef MC_COMUA_H
#define MC_COMUA_H

#include "misc.h"
#include "c-reusables/data/buffer.h"


/* COMUA == Compressed UINTs Array
 *
 * Sometimes we need to store many UINTs (e.g. some offsets) which generally
 * tend to be rather small, but we need to be ready even for the case when they
 * are big.
 *
 * This is a specialized storage for some data.
 *
 * When building the storage, we use BUFFER. When done, caller may then use
 * e.g. buffer_acquire() or copy the data elsewhere, so the reading functions
 * take a pointer to the data instead.
 */

/* Use this to mark "main" or "first" mumbers in multi-number records.
 * comua_bsearch() considers only these. */
#define COMUA_FLAG_RECORD_LEADER        0x1

int comua_append(BUFFER* buffer, uint64_t num, DWORD flags);

/* Read a number at the given offsets. Caller is responsible to make sure
 * the data points into the COMUA-compatible buffer, to the first byte of
 * an encoded integer. */
uint64_t comua_read(const void* data, size_t size, size_t offset, size_t* p_end_offset);

/* Binary search in COMUA of the given size. Note the function considers
 * only numbers which where marked with COMUA_FLAG_IS_LEADER as potential
 * candidates of the search.
 *
 * Note the function can iterate back byte by byte to find the nearest leader,
 * therefore:
 * (1) All the "records" should be reasonably short if you want to use this.
 * (2) The very 1st number in the COMUA must be leader.
 *
 * On success, the return value is an offset of the matching record.
 * On failure, ((size_t) -1) is returned.
 */
size_t comua_bsearch(const void* data, size_t size, const void* key,
            int (*cmp_func)(const void* /*key*/, const void* /*data*/,
                            size_t /*data_len*/, size_t /*offset*/));


#endif  /* MC_COMUA_H */
