/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2018 Martin Mitas
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

#ifndef CRE_BUFFER_H
#define CRE_BUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


#if defined __cplusplus
    #define BUFFER_INLINE__     inline
#elif defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
    #define BUFFER_INLINE__     static inline
#elif defined __GNUC__
    #define BUFFER_INLINE__     static __inline__
#elif defined _MSC_VER
    #define BUFFER_INLINE__     static __inline
#else
    #define BUFFER_INLINE__     static
#endif


typedef struct BUFFER {
    void* data;
    size_t size;
    size_t alloc;
} BUFFER;


/* Static initializer. */
#define BUFFER_INITIALIZER          { NULL, 0, 0 }

/* Initialize/deinitialize buffer structure. */
BUFFER_INLINE__ void buffer_init(BUFFER* buf)
        { buf->data = NULL; buf->size = 0; buf->alloc = 0; }
BUFFER_INLINE__ void buffer_fini(BUFFER* buf)
        { free(buf->data); }

/* Change capacity of the buffer. If lower then current size, the data contents
 * beyond the new buffer capacity is lost. */
int buffer_realloc(BUFFER* buf, size_t alloc);
/* Reserve new space for extra_alloc bytes at the end of the buffer. */
int buffer_reserve(BUFFER* buf, size_t extra_alloc);
/* Remove the empty space from the buffer. */
void buffer_shrink(BUFFER* buf);

/* Contents getters. */
BUFFER_INLINE__ void* buffer_data_at(BUFFER* buf, size_t pos)
        { return (void*) (((uint8_t*)buf->data) + pos); }
BUFFER_INLINE__ void* buffer_data(BUFFER* buf)
        { return buf->data; }
BUFFER_INLINE__ size_t buffer_size(const BUFFER* buf)
        { return buf->size; }
BUFFER_INLINE__ int buffer_is_empty(const BUFFER* buf)
        { return (buf->size == 0); }

/* Contents modifiers. */
void* buffer_insert_(BUFFER* buf, size_t pos, size_t n);
BUFFER_INLINE__ void* buffer_append_(BUFFER* buf, size_t n)
        { return buffer_insert_(buf, buf->size, n); }
int buffer_insert(BUFFER* buf, size_t pos, const void* data, size_t n);
BUFFER_INLINE__ int buffer_append(BUFFER* buf, const void* data, size_t n)
        { return buffer_insert(buf, buf->size, data, n); }
void buffer_remove(BUFFER* buf, size_t pos, size_t n);
BUFFER_INLINE__ void buffer_clear(BUFFER* buf)
        { buffer_remove(buf, 0, buf->size); }

BUFFER_INLINE__ void* buffer_acquire(BUFFER* buf)
        { void* data = buf->data; buffer_init(buf); return data; }


/* Sometimes, it is very useful to use the buffer as a general-purpose stack.
 * structure and, arguably, an explicit API is more readable. */
typedef BUFFER STACK;

#define STACK_INITIALIZER           BUFFER_INITIALIZER

BUFFER_INLINE__ void stack_init(STACK* stack)   { buffer_init(stack); }
BUFFER_INLINE__ void stack_fini(STACK* stack)   { buffer_fini(stack); }

BUFFER_INLINE__ size_t stack_size(STACK* stack) { return buffer_size(stack); }
BUFFER_INLINE__ int stack_is_empty(STACK* stack)   { return buffer_is_empty(stack); }

BUFFER_INLINE__ int stack_push(STACK* stack, const void* data, size_t n)
        { return buffer_append(stack, data, n); }
BUFFER_INLINE__ void* stack_peek(STACK* stack, size_t n)
        { return buffer_data_at(stack, stack_size(stack) - n); }
BUFFER_INLINE__ void* stack_pop(STACK* stack, size_t n)
        { return buffer_data_at(stack, stack_size(stack) - n);
          buffer_remove(stack, stack_size(stack) - n, n); }
BUFFER_INLINE__ void stack_peek_copy(STACK* stack, void* addr, size_t n)
        { memcpy(addr, buffer_data_at(stack, stack_size(stack) - n), n); }
BUFFER_INLINE__ void stack_pop_copy(STACK* stack, void* addr, size_t n)
        { stack_peek_copy(stack, addr, n);
          buffer_remove(stack, stack_size(stack) - n, n); }

BUFFER_INLINE__ int stack_push_int8(STACK* stack, int8_t i8)
        { return stack_push(stack, (void*) &i8, sizeof(int8_t)); }
BUFFER_INLINE__ int stack_push_uint8(STACK* stack, uint8_t u8)
        { return stack_push(stack, (void*) &u8, sizeof(uint8_t)); }
BUFFER_INLINE__ int stack_push_int16(STACK* stack, int16_t i16)
        { return stack_push(stack, (void*) &i16, sizeof(int16_t)); }
BUFFER_INLINE__ int stack_push_uint16(STACK* stack, uint16_t u16)
        { return stack_push(stack, (void*) &u16, sizeof(uint16_t)); }
BUFFER_INLINE__ int stack_push_int32(STACK* stack, int32_t i32)
        { return stack_push(stack, (void*) &i32, sizeof(int32_t)); }
BUFFER_INLINE__ int stack_push_uint32(STACK* stack, uint32_t u32)
        { return stack_push(stack, (void*) &u32, sizeof(uint32_t)); }
BUFFER_INLINE__ int stack_push_int64(STACK* stack, int64_t i64)
        { return stack_push(stack, (void*) &i64, sizeof(int64_t)); }
BUFFER_INLINE__ int stack_push_uint64(STACK* stack, uint64_t u64)
        { return stack_push(stack, (void*) &u64, sizeof(uint64_t)); }
BUFFER_INLINE__ int stack_push_ptr(STACK* stack, void* ptr)
        { return stack_push(stack, (void*) &ptr, sizeof(void*)); }

BUFFER_INLINE__ int8_t stack_peek_int8(STACK* stack)
        { int8_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(int8_t)); return ret; }
BUFFER_INLINE__ uint8_t stack_peek_uint8(STACK* stack)
        { uint8_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(uint8_t)); return ret; }
BUFFER_INLINE__ int16_t stack_peek_int16(STACK* stack)
        { int16_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(int16_t)); return ret; }
BUFFER_INLINE__ uint16_t stack_peek_uint16(STACK* stack)
        { uint16_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(uint16_t)); return ret; }
BUFFER_INLINE__ int32_t stack_peek_int32(STACK* stack)
        { int32_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(int32_t)); return ret; }
BUFFER_INLINE__ uint32_t stack_peek_uint32(STACK* stack)
        { uint32_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(uint32_t)); return ret; }
BUFFER_INLINE__ int64_t stack_peek_int64(STACK* stack)
        { int64_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(int64_t)); return ret; }
BUFFER_INLINE__ uint64_t stack_peek_uint64(STACK* stack)
        { uint64_t ret; stack_peek_copy(stack, (void*) &ret, sizeof(uint64_t)); return ret; }
BUFFER_INLINE__ void* stack_peek_ptr(STACK* stack)
        { void* ret; stack_peek_copy(stack, (void*) &ret, sizeof(void*)); return ret; }

BUFFER_INLINE__ int8_t stack_pop_int8(STACK* stack)
        { int8_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(int8_t)); return ret; }
BUFFER_INLINE__ uint8_t stack_pop_uint8(STACK* stack)
        { uint8_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(uint8_t)); return ret; }
BUFFER_INLINE__ int16_t stack_pop_int16(STACK* stack)
        { int16_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(int16_t)); return ret; }
BUFFER_INLINE__ uint16_t stack_pop_uint16(STACK* stack)
        { uint16_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(uint16_t)); return ret; }
BUFFER_INLINE__ int32_t stack_pop_int32(STACK* stack)
        { int32_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(int32_t)); return ret; }
BUFFER_INLINE__ uint32_t stack_pop_uint32(STACK* stack)
        { uint32_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(uint32_t)); return ret; }
BUFFER_INLINE__ int64_t stack_pop_int64(STACK* stack)
        { int64_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(int64_t)); return ret; }
BUFFER_INLINE__ uint64_t stack_pop_uint64(STACK* stack)
        { uint64_t ret; stack_pop_copy(stack, (void*) &ret, sizeof(uint64_t)); return ret; }
BUFFER_INLINE__ void* stack_pop_ptr(STACK* stack)
        { void* ret; stack_pop_copy(stack, (void*) &ret, sizeof(void*)); return ret; }


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_BUFFER_H */
