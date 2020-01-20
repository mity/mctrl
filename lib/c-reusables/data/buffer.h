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
void* buffer_insert_raw(BUFFER* buf, size_t pos, size_t n);
int buffer_insert(BUFFER* buf, size_t pos, const void* data, size_t n);
BUFFER_INLINE__ void* buffer_append_raw(BUFFER* buf, size_t n)
        { return buffer_insert_raw(buf, buf->size, n); }
BUFFER_INLINE__ int buffer_append(BUFFER* buf, const void* data, size_t n)
        { return buffer_insert(buf, buf->size, data, n); }
void buffer_remove(BUFFER* buf, size_t pos, size_t n);
BUFFER_INLINE__ void buffer_clear(BUFFER* buf)
        { buffer_remove(buf, 0, buf->size); }

/* Take over the raw buffer. */
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

BUFFER_INLINE__ void* stack_push_raw(STACK* stack, size_t n)
        { return buffer_append_raw(stack, n); }
BUFFER_INLINE__ int stack_push(STACK* stack, const void* data, size_t n)
        { return buffer_append(stack, data, n); }
BUFFER_INLINE__ void* stack_peek_raw(STACK* stack, size_t n)
        { return buffer_data_at(stack, stack_size(stack) - n); }
BUFFER_INLINE__ void stack_peek(STACK* stack, void* addr, size_t n)
        { memcpy(addr, buffer_data_at(stack, stack_size(stack) - n), n); }
BUFFER_INLINE__ void* stack_pop_raw(STACK* stack, size_t n)
        { return buffer_data_at(stack, stack_size(stack) - n);
          buffer_remove(stack, stack_size(stack) - n, n); }
BUFFER_INLINE__ void stack_pop(STACK* stack, void* addr, size_t n)
        { stack_peek(stack, addr, n);
          buffer_remove(stack, stack_size(stack) - n, n); }
BUFFER_INLINE__ void stack_clear(STACK* stack)
        { buffer_clear(stack); }

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
BUFFER_INLINE__ int stack_push_ptr(STACK* stack, const void* ptr)
        { return stack_push(stack, (void*) &ptr, sizeof(void*)); }

BUFFER_INLINE__ int8_t stack_peek_int8(STACK* stack)
        { int8_t ret; stack_peek(stack, (void*) &ret, sizeof(int8_t)); return ret; }
BUFFER_INLINE__ uint8_t stack_peek_uint8(STACK* stack)
        { uint8_t ret; stack_peek(stack, (void*) &ret, sizeof(uint8_t)); return ret; }
BUFFER_INLINE__ int16_t stack_peek_int16(STACK* stack)
        { int16_t ret; stack_peek(stack, (void*) &ret, sizeof(int16_t)); return ret; }
BUFFER_INLINE__ uint16_t stack_peek_uint16(STACK* stack)
        { uint16_t ret; stack_peek(stack, (void*) &ret, sizeof(uint16_t)); return ret; }
BUFFER_INLINE__ int32_t stack_peek_int32(STACK* stack)
        { int32_t ret; stack_peek(stack, (void*) &ret, sizeof(int32_t)); return ret; }
BUFFER_INLINE__ uint32_t stack_peek_uint32(STACK* stack)
        { uint32_t ret; stack_peek(stack, (void*) &ret, sizeof(uint32_t)); return ret; }
BUFFER_INLINE__ int64_t stack_peek_int64(STACK* stack)
        { int64_t ret; stack_peek(stack, (void*) &ret, sizeof(int64_t)); return ret; }
BUFFER_INLINE__ uint64_t stack_peek_uint64(STACK* stack)
        { uint64_t ret; stack_peek(stack, (void*) &ret, sizeof(uint64_t)); return ret; }
BUFFER_INLINE__ void* stack_peek_ptr(STACK* stack)
        { void* ret; stack_peek(stack, (void*) &ret, sizeof(void*)); return ret; }

BUFFER_INLINE__ int8_t stack_pop_int8(STACK* stack)
        { int8_t ret; stack_pop(stack, (void*) &ret, sizeof(int8_t)); return ret; }
BUFFER_INLINE__ uint8_t stack_pop_uint8(STACK* stack)
        { uint8_t ret; stack_pop(stack, (void*) &ret, sizeof(uint8_t)); return ret; }
BUFFER_INLINE__ int16_t stack_pop_int16(STACK* stack)
        { int16_t ret; stack_pop(stack, (void*) &ret, sizeof(int16_t)); return ret; }
BUFFER_INLINE__ uint16_t stack_pop_uint16(STACK* stack)
        { uint16_t ret; stack_pop(stack, (void*) &ret, sizeof(uint16_t)); return ret; }
BUFFER_INLINE__ int32_t stack_pop_int32(STACK* stack)
        { int32_t ret; stack_pop(stack, (void*) &ret, sizeof(int32_t)); return ret; }
BUFFER_INLINE__ uint32_t stack_pop_uint32(STACK* stack)
        { uint32_t ret; stack_pop(stack, (void*) &ret, sizeof(uint32_t)); return ret; }
BUFFER_INLINE__ int64_t stack_pop_int64(STACK* stack)
        { int64_t ret; stack_pop(stack, (void*) &ret, sizeof(int64_t)); return ret; }
BUFFER_INLINE__ uint64_t stack_pop_uint64(STACK* stack)
        { uint64_t ret; stack_pop(stack, (void*) &ret, sizeof(uint64_t)); return ret; }
BUFFER_INLINE__ void* stack_pop_ptr(STACK* stack)
        { void* ret; stack_pop(stack, (void*) &ret, sizeof(void*)); return ret; }


/* Sometimes, it is very useful to use the buffer as a general-purpose ARRAY.
 * These functions assume all elements have the same size. */
typedef BUFFER ARRAY;

#define ARRAY_INITIALIZER           BUFFER_INITIALIZER

BUFFER_INLINE__ void array_init(ARRAY* array)   { buffer_init(array); }
BUFFER_INLINE__ void array_fini(ARRAY* array)   { buffer_fini(array); }

BUFFER_INLINE__ size_t array_count(ARRAY* array, size_t elem_size)
        { return (buffer_size(array) / elem_size); }
BUFFER_INLINE__ int array_is_empty(ARRAY* array)
        { return buffer_is_empty(array); }

BUFFER_INLINE__ void* array_get_raw(ARRAY* array, size_t index, size_t elem_size)
        { return buffer_data_at(array, index * elem_size); }
BUFFER_INLINE__ void array_get(ARRAY* array, size_t index, void* addr, size_t elem_size)
        { memcpy(addr, buffer_data_at(array, index * elem_size), elem_size); }
BUFFER_INLINE__ void array_set(ARRAY* array, size_t index, const void* data, size_t elem_size)
        { memcpy(buffer_data_at(array, index * elem_size), data, elem_size); }
BUFFER_INLINE__ void* array_insert_raw(ARRAY* array, size_t index, size_t elem_size)
        { return buffer_insert_raw(array, index * elem_size, elem_size); }
BUFFER_INLINE__ int array_insert(ARRAY* array, size_t index, const void* data, size_t elem_size)
        { return buffer_insert(array, index * elem_size, data, elem_size); }
BUFFER_INLINE__ void* array_append_raw(ARRAY* array, size_t elem_size)
        { return buffer_append_raw(array, elem_size); }
BUFFER_INLINE__ int array_append(ARRAY* array, const void* data, size_t elem_size)
        { return buffer_append(array, data, elem_size); }
BUFFER_INLINE__ void array_remove(ARRAY* array, size_t index, size_t elem_size)
        { buffer_remove(array, index * elem_size, elem_size); }
BUFFER_INLINE__ void array_remove_range(ARRAY* array, size_t index, size_t n, size_t elem_size)
        { buffer_remove(array, index * elem_size, n * elem_size); }
BUFFER_INLINE__ void array_clear(ARRAY* array)
        { buffer_clear(array); }

BUFFER_INLINE__ size_t array_int8_count(ARRAY* array)
        { return array_count(array, sizeof(int8_t)); }
BUFFER_INLINE__ size_t array_uint8_count(ARRAY* array)
        { return array_count(array, sizeof(uint8_t)); }
BUFFER_INLINE__ size_t array_int16_count(ARRAY* array)
        { return array_count(array, sizeof(int16_t)); }
BUFFER_INLINE__ size_t array_uint16_count(ARRAY* array)
        { return array_count(array, sizeof(uint16_t)); }
BUFFER_INLINE__ size_t array_int32_count(ARRAY* array)
        { return array_count(array, sizeof(int32_t)); }
BUFFER_INLINE__ size_t array_uint32_count(ARRAY* array)
        { return array_count(array, sizeof(uint32_t)); }
BUFFER_INLINE__ size_t array_int64_count(ARRAY* array)
        { return array_count(array, sizeof(int64_t)); }
BUFFER_INLINE__ size_t array_uint64_count(ARRAY* array)
        { return array_count(array, sizeof(uint64_t)); }
BUFFER_INLINE__ size_t array_ptr_count(ARRAY* array)
        { return array_count(array, sizeof(void*)); }

BUFFER_INLINE__ int8_t array_get_int8(ARRAY* array, size_t index)
        { int8_t ret; array_get(array, index, (void*) &ret, sizeof(int8_t)); return ret; }
BUFFER_INLINE__ uint8_t array_get_uint8(ARRAY* array, size_t index)
        { uint8_t ret; array_get(array, index, (void*) &ret, sizeof(uint8_t)); return ret; }
BUFFER_INLINE__ int16_t array_get_int16(ARRAY* array, size_t index)
        { int16_t ret; array_get(array, index, (void*) &ret, sizeof(int16_t)); return ret; }
BUFFER_INLINE__ uint16_t array_get_uint16(ARRAY* array, size_t index)
        { uint16_t ret; array_get(array, index, (void*) &ret, sizeof(uint16_t)); return ret; }
BUFFER_INLINE__ int32_t array_get_int32(ARRAY* array, size_t index)
        { int32_t ret; array_get(array, index, (void*) &ret, sizeof(int32_t)); return ret; }
BUFFER_INLINE__ uint32_t array_get_uint32(ARRAY* array, size_t index)
        { uint32_t ret; array_get(array, index, (void*) &ret, sizeof(uint32_t)); return ret; }
BUFFER_INLINE__ int64_t array_get_int64(ARRAY* array, size_t index)
        { int64_t ret; array_get(array, index, (void*) &ret, sizeof(int64_t)); return ret; }
BUFFER_INLINE__ uint64_t array_get_uint64(ARRAY* array, size_t index)
        { uint64_t ret; array_get(array, index, (void*) &ret, sizeof(uint64_t)); return ret; }
BUFFER_INLINE__ void* array_get_ptr(ARRAY* array, size_t index)
        { void* ret; array_get(array, index, (void*) &ret, sizeof(void*)); return ret; }

BUFFER_INLINE__ void array_set_int8(ARRAY* array, size_t index, int8_t i8)
        { array_set(array, index, (const void*) &i8, sizeof(int8_t)); }
BUFFER_INLINE__ void array_set_uint8(ARRAY* array, size_t index, uint8_t u8)
        { array_set(array, index, (const void*) &u8, sizeof(uint8_t)); }
BUFFER_INLINE__ void array_set_int16(ARRAY* array, size_t index, int16_t i16)
        { array_set(array, index, (const void*) &i16, sizeof(int16_t)); }
BUFFER_INLINE__ void array_set_uint16(ARRAY* array, size_t index, uint16_t u16)
        { array_set(array, index, (const void*) &u16, sizeof(uint16_t)); }
BUFFER_INLINE__ void array_set_int32(ARRAY* array, size_t index, int32_t i32)
        { array_set(array, index, (const void*) &i32, sizeof(int32_t)); }
BUFFER_INLINE__ void array_set_uint32(ARRAY* array, size_t index, uint32_t u32)
        { array_set(array, index, (const void*) &u32, sizeof(uint32_t)); }
BUFFER_INLINE__ void array_set_int64(ARRAY* array, size_t index, int64_t i64)
        { array_set(array, index, (const void*) &i64, sizeof(int64_t)); }
BUFFER_INLINE__ void array_set_uint64(ARRAY* array, size_t index, uint64_t u64)
        { array_set(array, index, (const void*) &u64, sizeof(uint64_t)); }
BUFFER_INLINE__ void array_set_ptr(ARRAY* array, size_t index, const void* ptr)
        { array_set(array, index, (const void*) &ptr, sizeof(const void*)); }

BUFFER_INLINE__ int array_insert_int8(ARRAY* array, size_t index, int8_t i8)
        { return array_insert(array, index, (const void*) &i8, sizeof(int8_t)); }
BUFFER_INLINE__ int array_insert_uint8(ARRAY* array, size_t index, int8_t u8)
        { return array_insert(array, index, (const void*) &u8, sizeof(uint8_t)); }
BUFFER_INLINE__ int array_insert_int16(ARRAY* array, size_t index, int16_t i16)
        { return array_insert(array, index, (const void*) &i16, sizeof(int16_t)); }
BUFFER_INLINE__ int array_insert_uint16(ARRAY* array, size_t index, int16_t u16)
        { return array_insert(array, index, (const void*) &u16, sizeof(uint16_t)); }
BUFFER_INLINE__ int array_insert_int32(ARRAY* array, size_t index, int32_t i32)
        { return array_insert(array, index, (const void*) &i32, sizeof(int32_t)); }
BUFFER_INLINE__ int array_insert_uint32(ARRAY* array, size_t index, int32_t u32)
        { return array_insert(array, index, (const void*) &u32, sizeof(uint32_t)); }
BUFFER_INLINE__ int array_insert_int64(ARRAY* array, size_t index, int64_t i64)
        { return array_insert(array, index, (const void*) &i64, sizeof(int64_t)); }
BUFFER_INLINE__ int array_insert_uint64(ARRAY* array, size_t index, int64_t u64)
        { return array_insert(array, index, (const void*) &u64, sizeof(uint64_t)); }
BUFFER_INLINE__ int array_insert_ptr(ARRAY* array, size_t index, const void* ptr)
        { return array_insert(array, index, (const void*) &ptr, sizeof(const void*)); }

BUFFER_INLINE__ int array_append_int8(ARRAY* array, int8_t i8)
        { return array_append(array, (const void*) &i8, sizeof(int8_t)); }
BUFFER_INLINE__ int array_append_uint8(ARRAY* array, uint8_t u8)
        { return array_append(array, (const void*) &u8, sizeof(uint8_t)); }
BUFFER_INLINE__ int array_append_int16(ARRAY* array, int16_t i16)
        { return array_append(array, (const void*) &i16, sizeof(int16_t)); }
BUFFER_INLINE__ int array_append_uint16(ARRAY* array, uint16_t u16)
        { return array_append(array, (const void*) &u16, sizeof(uint16_t)); }
BUFFER_INLINE__ int array_append_int32(ARRAY* array, int32_t i32)
        { return array_append(array, (const void*) &i32, sizeof(int32_t)); }
BUFFER_INLINE__ int array_append_uint32(ARRAY* array, uint32_t u32)
        { return array_append(array, (const void*) &u32, sizeof(uint32_t)); }
BUFFER_INLINE__ int array_append_int64(ARRAY* array, int64_t i64)
        { return array_append(array, (const void*) &i64, sizeof(int64_t)); }
BUFFER_INLINE__ int array_append_uint64(ARRAY* array, uint64_t u64)
        { return array_append(array, (const void*) &u64, sizeof(uint64_t)); }
BUFFER_INLINE__ int array_append_ptr(ARRAY* array, const void* ptr)
        { return array_append(array, (const void*) &ptr, sizeof(const void*)); }

BUFFER_INLINE__ void array_remove_int8(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(int8_t)); }
BUFFER_INLINE__ void array_remove_uint8(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(uint8_t)); }
BUFFER_INLINE__ void array_remove_int16(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(int16_t)); }
BUFFER_INLINE__ void array_remove_uint16(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(uint16_t)); }
BUFFER_INLINE__ void array_remove_int32(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(int32_t)); }
BUFFER_INLINE__ void array_remove_uint32(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(uint32_t)); }
BUFFER_INLINE__ void array_remove_int64(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(int64_t)); }
BUFFER_INLINE__ void array_remove_uint64(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(uint64_t)); }
BUFFER_INLINE__ void array_remove_ptr(ARRAY* array, size_t index)
        { array_remove(array, index, sizeof(void*)); }

BUFFER_INLINE__ void array_remove_int8_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(int8_t)); }
BUFFER_INLINE__ void array_remove_uint8_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(uint8_t)); }
BUFFER_INLINE__ void array_remove_int16_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(int16_t)); }
BUFFER_INLINE__ void array_remove_uint16_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(uint16_t)); }
BUFFER_INLINE__ void array_remove_int32_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(int32_t)); }
BUFFER_INLINE__ void array_remove_uint32_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(uint32_t)); }
BUFFER_INLINE__ void array_remove_int64_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(int64_t)); }
BUFFER_INLINE__ void array_remove_uint64_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(uint64_t)); }
BUFFER_INLINE__ void array_remove_ptr_range(ARRAY* array, size_t index, size_t n)
        { array_remove_range(array, index, n, sizeof(void*)); }


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_BUFFER_H */
