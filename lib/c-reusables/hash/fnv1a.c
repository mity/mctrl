/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2016 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "fnv1a.h"


#define FNV1A_PRIME_32      16777619
#define FNV1A_PRIME_64      1099511628211


uint32_t
fnv1a_32(uint32_t fnv1a, const void* data, size_t n)
{
    const uint8_t* bytes = (const uint8_t*) data;
    size_t i;

    for(i = 0; i < n; i++) {
        fnv1a ^= bytes[i];
        fnv1a *= FNV1A_PRIME_32;
    }

    return fnv1a;
}

uint64_t
fnv1a_64(uint64_t fnv1a, const void* data, size_t n)
{
    const uint8_t* bytes = (const uint8_t*) data;
    size_t i;

    for(i = 0; i < n; i++) {
        fnv1a ^= bytes[i];
        fnv1a *= FNV1A_PRIME_64;
    }

    return fnv1a;
}
