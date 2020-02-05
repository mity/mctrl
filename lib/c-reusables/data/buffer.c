/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2018-2019 Martin Mitas
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

#include "buffer.h"


int
buffer_realloc(BUFFER* buf, size_t alloc)
{
    void* tmp;

    tmp = realloc(buf->data, alloc);
    if(tmp == NULL  &&  alloc > 0)
        return -1;

    buf->data = tmp;
    buf->alloc = alloc;
    if(buf->size > alloc)
        buf->size = alloc;
    return 0;
}

int
buffer_reserve(BUFFER* buf, size_t extra_alloc)
{
    if(buf->size + extra_alloc > buf->alloc)
        return buffer_realloc(buf, buf->size + extra_alloc);
    else
        return 0;
}

void
buffer_shrink(BUFFER* buf)
{
    /* Avoid realloc() if the potential memory gain is negligible. */
    if(buf->alloc / 11 > buf->size / 10)
        buffer_realloc(buf, buf->size);
}

void*
buffer_insert_raw(BUFFER* buf, size_t pos, size_t n)
{
    if(buf->size + n > buf->alloc) {
        if(buffer_realloc(buf, buf->size + n + buf->size / 2) != 0)
            return NULL;
    }

    if(buf->size > pos)
        memmove((uint8_t*)buf->data + pos + n, (uint8_t*)buf->data + pos, buf->size - pos);

    buf->size += n;
    return buffer_data_at(buf, pos);
}

int
buffer_insert(BUFFER* buf, size_t pos, const void* data, size_t n)
{
    void* ptr;

    ptr = buffer_insert_raw(buf, pos, n);
    if(ptr == NULL)
        return -1;

    memcpy(ptr, data, n);
    return 0;
}

void
buffer_remove(BUFFER* buf, size_t pos, size_t n)
{
    if(pos + n < buf->size) {
        memmove((uint8_t*)buf->data + pos, (uint8_t*)buf->data + pos + n, n);
        buf->size -= n;
    } else {
        buf->size = pos;
    }
}

