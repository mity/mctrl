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

#include "value.h"

#include <malloc.h>
#include <string.h>


#define TYPE_MASK       0x0f
#define IS_NEW          0x10    /* only for VALUE_NULL */
#define HAS_REDCOLOR    0x10    /* only for VALUE_STRING (when used as RBTREE::key) */
#define HAS_ORDERLIST   0x10    /* only for VALUE_DICT */
#define HAS_CUSTOMCMP   0x20    /* only for VALUE_DICT */
#define IS_MALLOCED     0x80


typedef struct ARRAY_tag ARRAY;
struct ARRAY_tag {
    VALUE* value_buf;
    size_t size;
    size_t alloc;
};

typedef struct RBTREE_tag RBTREE;
struct RBTREE_tag {
    /* We store color by using the flag HAS_REDCOLOR of the key. */
    VALUE key;
    VALUE value;
    RBTREE* left;
    RBTREE* right;

    /* These are present only if HAS_ORDERLIST. */
    RBTREE* order_prev;
    RBTREE* order_next;
};

/* Maximal height of the RB-tree. Given we can never allocate more nodes
 * then 2^(sizeof(void*) * 8) and given the longest root<-->leaf path cannot
 * be longer then twice the shortest one in the RB-tree, the following
 * is guaranteed to be large enough. */
#define RBTREE_MAX_HEIGHT       (2 * 8 * sizeof(void*))

typedef struct DICT_tag DICT;
struct DICT_tag {
    RBTREE* root;
    size_t size;

    /* These are present only when flags VALUE_DICT_MAINTAINORDER or
     * custom_cmp_func is used. */
    RBTREE* order_head;
    RBTREE* order_tail;
    int (*cmp_func)(const char*, size_t, const char*, size_t);
};


#if defined offsetof
    #define OFFSETOF(type, member)      offsetof(type, member)
#elif defined __GNUC__ && __GNUC__ >= 4
    #define OFFSETOF(type, member)      __builtin_offsetof(type, member)
#else
    #define OFFSETOF(type, member)      ((size_t) &((type*)0)->member)
#endif


/***************
 *** Helpers ***
 ***************/

/* We don't want to include <math.h> just because of roundf() and round().
 * Especially as on some systems it requires explicit linking with math
 * library (-lm). */
#define ROUNDF(inttype, x)   ((inttype)((x) >= 0.0f ? (x) + 0.5f : (x) - 0.5f))
#define ROUNDD(inttype, x)   ((inttype)((x) >= 0.0 ? (x) + 0.5 : (x) - 0.5))


static void*
value_init_ex(VALUE* v, VALUE_TYPE type, size_t size, size_t align)
{
    v->data[0] = (uint8_t) type;

    if(size + align <= sizeof(VALUE)) {
        return &v->data[align];
    } else {
        void* buf;

        v->data[0] |= IS_MALLOCED;
        buf = malloc(size);
        if(buf == NULL) {
            v->data[0] = (uint8_t) VALUE_NULL;
            return NULL;
        }

        *((void**) &v->data[sizeof(void*)]) = buf;
        return buf;
    }
}

static void*
value_init(VALUE* v, VALUE_TYPE type, size_t size)
{
    return value_init_ex(v, type, size, 1);
}

static int
value_init_simple(VALUE* v, VALUE_TYPE type, const void* data, size_t size)
{
    void* payload;

    payload = value_init(v, type, size);
    if(payload == NULL)
        return -1;

    memcpy(payload, data, size);
    return 0;
}

static uint8_t*
value_payload_ex(VALUE* v, size_t align)
{
    if(v == NULL)
        return NULL;

    if(!(v->data[0] & IS_MALLOCED))
        return (void*)(v->data + align);
    else
        return *(void**)(v->data + sizeof(void*));
}

static uint8_t*
value_payload(VALUE* v)
{
    return value_payload_ex(v, 1);
}


/********************
 *** Generic info ***
 ********************/

VALUE_TYPE
value_type(const VALUE* v)
{
    if(v == NULL)
        return VALUE_NULL;
    return (VALUE_TYPE)(v->data[0] & TYPE_MASK);
}

int
value_is_compatible(const VALUE* v, VALUE_TYPE type)
{
    if(value_type(v) == type)
        return 1;

    /* We say any numeric value is compatible with another numeric value as
     * long as the conversion does not loose much information. */
    switch(value_type(v)) {
        case VALUE_INT32:
            return (type == VALUE_INT64 || type == VALUE_FLOAT || type == VALUE_DOUBLE)  ||
                   (type == VALUE_UINT32 && value_int32(v) >= 0)  ||
                   (type == VALUE_UINT64 && value_int32(v) >= 0);
            break;

        case VALUE_UINT32:
            return (type == VALUE_INT64 || type == VALUE_UINT64 || type == VALUE_FLOAT || type == VALUE_DOUBLE)  ||
                   (type == VALUE_INT32 && value_uint32(v) <= INT32_MAX);
            break;

        case VALUE_INT64:
            return (type == VALUE_FLOAT || type == VALUE_DOUBLE)  ||
                   (type == VALUE_INT32 && value_int64(v) >= INT32_MIN && value_int64(v) <= INT32_MAX)  ||
                   (type == VALUE_UINT32 && value_int64(v) >= 0 && value_int64(v) <= UINT32_MAX)  ||
                   (type == VALUE_UINT64 && value_int64(v) >= 0);
            break;

        case VALUE_UINT64:
            return (type == VALUE_FLOAT || type == VALUE_DOUBLE)  ||
                   (type == VALUE_INT32 && value_uint64(v) <= INT32_MAX)  ||
                   (type == VALUE_UINT32 && value_uint64(v) <= UINT32_MAX)  ||
                   (type == VALUE_INT64 && value_uint64(v) <= INT64_MAX);
            break;

        case VALUE_FLOAT:
            return (type == VALUE_DOUBLE)  ||
                   (type == VALUE_INT32 && value_float(v) == (float)value_int32(v))  ||
                   (type == VALUE_UINT32 && value_float(v) == (float)value_uint32(v))  ||
                   (type == VALUE_INT64 && value_float(v) == (float)value_int64(v))  ||
                   (type == VALUE_UINT64 && value_float(v) == (float)value_uint64(v));
            break;

        case VALUE_DOUBLE:
            return (type == VALUE_FLOAT)  ||
                   (type == VALUE_INT32 && value_double(v) == (double)value_int32(v))  ||
                   (type == VALUE_UINT32 && value_double(v) == (double)value_uint32(v))  ||
                   (type == VALUE_INT64 && value_double(v) == (double)value_int64(v))  ||
                   (type == VALUE_UINT64 && value_double(v) == (double)value_uint64(v));
            break;

        default:
            break;
    }

    return 0;
}

int
value_is_new(const VALUE* v)
{
    return (v != NULL  &&  value_type(v) == VALUE_NULL  &&  (v->data[0] & IS_NEW));
}

VALUE*
value_path(VALUE* root, const char* path)
{
    const char* token_beg = path;
    const char* token_end;
    VALUE* v = root;

    while(1) {
        token_end = token_beg;
        while(*token_end != '\0'  &&  *token_end != '/')
            token_end++;

        if(token_end - token_beg > 2  &&  token_beg[0] == '['  &&  token_end[-1] == ']') {
            size_t index = 0;

            token_beg++;
            while('0' <= *token_beg  &&  *token_beg <= '9') {
                index = index * 10 + (*token_beg - '0');
                token_beg++;
            }
            if(*token_beg != ']')
                return NULL;

            v = value_array_get(v, index);
        } else if(token_end - token_beg > 0) {
            v = value_dict_get_(v, token_beg, token_end - token_beg);
        }

        if(v == NULL)
            return NULL;

        if(*token_end == '\0')
            return v;

        token_beg = token_end+1;
    }
}


/********************
 *** Initializers ***
 ********************/

void
value_init_null(VALUE* v)
{
    if(v != NULL)
        v->data[0] = (uint8_t) VALUE_NULL;
}

static void
value_init_new(VALUE* v)
{
    v->data[0] = ((uint8_t) VALUE_NULL) | IS_NEW;
}

int
value_init_bool(VALUE* v, int b)
{
    if(v == NULL)
        return -1;

    v->data[0] = (uint8_t) VALUE_BOOL;
    v->data[1] = (b != 0) ? 1 : 0;

    return 0;
}

int
value_init_int32(VALUE* v, int32_t i32)
{
    if(v == NULL)
        return -1;

    return value_init_simple(v, VALUE_INT32, &i32, sizeof(int32_t));
}

int
value_init_uint32(VALUE* v, uint32_t u32)
{
    if(v == NULL)
        return -1;

    return value_init_simple(v, VALUE_UINT32, &u32, sizeof(uint32_t));
}

int
value_init_int64(VALUE* v, int64_t i64)
{
    if(v == NULL)
        return -1;

    return value_init_simple(v, VALUE_INT64, &i64, sizeof(int64_t));
}

int
value_init_uint64(VALUE* v, uint64_t u64)
{
    if(v == NULL)
        return -1;

    return value_init_simple(v, VALUE_UINT64, &u64, sizeof(uint64_t));
}

int
value_init_float(VALUE* v, float f)
{
    if(v == NULL)
        return -1;

    return value_init_simple(v, VALUE_FLOAT, &f, sizeof(float));
}

int
value_init_double(VALUE* v, double d)
{
    if(v == NULL)
        return -1;

    return value_init_simple(v, VALUE_DOUBLE, &d, sizeof(double));
}

int
value_init_string_(VALUE* v, const char* str, size_t len)
{
    uint8_t* payload;
    size_t tmplen;
    size_t off;

    if(v == NULL)
        return -1;

    tmplen = len;
    off = 0;
    while(tmplen >= 128) {
        off++;
        tmplen = tmplen >> 7;
    }
    off++;

    payload = value_init(v, VALUE_STRING, off + len + 1);
    if(payload == NULL)
        return -1;

    tmplen = len;
    off = 0;
    while(tmplen >= 128) {
        payload[off++] = 0x80 | (tmplen & 0x7f);
        tmplen = tmplen >> 7;
    }
    payload[off++] = tmplen & 0x7f;

    memcpy(payload + off, str, len);
    payload[off + len] = '\0';
    return 0;
}

int
value_init_string(VALUE* v, const char* str)
{
    return value_init_string_(v, str, (str != NULL) ? strlen(str) : 0);
}

int
value_init_array(VALUE* v)
{
    uint8_t* payload;

    if(v == NULL)
        return -1;

    payload = value_init_ex(v, VALUE_ARRAY, sizeof(ARRAY), sizeof(void*));
    if(payload == NULL)
        return -1;
    memset(payload, 0, sizeof(ARRAY));

    return 0;
}

int
value_init_dict(VALUE* v)
{
    return value_init_dict_ex(v, NULL, 0);
}

int
value_init_dict_ex(VALUE* v,
                   int (*custom_cmp_func)(const char*, size_t, const char*, size_t),
                   unsigned flags)
{
    uint8_t* payload;
    size_t payload_size;

    if(v == NULL)
        return -1;

    if(custom_cmp_func != NULL  ||  (flags & VALUE_DICT_MAINTAINORDER))
        payload_size = sizeof(DICT);
    else
        payload_size = OFFSETOF(DICT, order_head);

    payload = value_init_ex(v, VALUE_DICT, payload_size, sizeof(void*));
    if(payload == NULL)
        return -1;
    memset(payload, 0, payload_size);

    if(custom_cmp_func != NULL) {
        v->data[0] |= HAS_CUSTOMCMP;
        ((DICT*)payload)->cmp_func = custom_cmp_func;
    }

    if(flags & VALUE_DICT_MAINTAINORDER)
        v->data[0] |= HAS_ORDERLIST;

    return 0;
}

void
value_fini(VALUE* v)
{
    if(v == NULL)
        return;

    if(value_type(v) == VALUE_ARRAY)
        value_array_clean(v);

    if(value_type(v) == VALUE_DICT)
        value_dict_clean(v);

    if(v->data[0] & 0x80)
        free(value_payload(v));

    v->data[0] = VALUE_NULL;
}


/**************************
 *** Basic type getters ***
 **************************/

int
value_bool(const VALUE* v)
{
    if(value_type(v) != VALUE_BOOL)
        return -1;

    return v->data[1];
}

int32_t
value_int32(const VALUE* v)
{
    uint8_t* payload = value_payload((VALUE*) v);
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
    } ret;

    switch(value_type(v)) {
        case VALUE_INT32:     memcpy(&ret.i32, payload, sizeof(int32_t)); return (int32_t) ret.i32;
        case VALUE_UINT32:    memcpy(&ret.u32, payload, sizeof(uint32_t)); return (int32_t) ret.u32;
        case VALUE_INT64:     memcpy(&ret.i64, payload, sizeof(int64_t)); return (int32_t) ret.i64;
        case VALUE_UINT64:    memcpy(&ret.u64, payload, sizeof(uint64_t)); return (int32_t) ret.u64;
        case VALUE_FLOAT:     memcpy(&ret.f, payload, sizeof(float)); return ROUNDF(int32_t, ret.f);
        case VALUE_DOUBLE:    memcpy(&ret.d, payload, sizeof(double)); return ROUNDD(int32_t, ret.d);
        default:            return -1;
    }
}

uint32_t
value_uint32(const VALUE* v)
{
    uint8_t* payload = value_payload((VALUE*) v);
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
    } ret;

    switch(value_type(v)) {
        case VALUE_INT32:     memcpy(&ret.i32, payload, sizeof(int32_t)); return (uint32_t) ret.i32;
        case VALUE_UINT32:    memcpy(&ret.u32, payload, sizeof(uint32_t)); return (uint32_t) ret.u32;
        case VALUE_INT64:     memcpy(&ret.i64, payload, sizeof(int64_t)); return (uint32_t) ret.i64;
        case VALUE_UINT64:    memcpy(&ret.u64, payload, sizeof(uint64_t)); return (uint32_t) ret.u64;
        case VALUE_FLOAT:     memcpy(&ret.f, payload, sizeof(float)); return ROUNDF(uint32_t, ret.f);
        case VALUE_DOUBLE:    memcpy(&ret.d, payload, sizeof(double)); return ROUNDD(uint32_t, ret.d);
        default:            return UINT32_MAX;
    }
}

int64_t
value_int64(const VALUE* v)
{
    uint8_t* payload = value_payload((VALUE*) v);
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
    } ret;

    switch(value_type(v)) {
        case VALUE_INT32:     memcpy(&ret.i32, payload, sizeof(int32_t)); return (int64_t) ret.i32;
        case VALUE_UINT32:    memcpy(&ret.u32, payload, sizeof(uint32_t)); return (int64_t) ret.u32;
        case VALUE_INT64:     memcpy(&ret.i64, payload, sizeof(int64_t)); return (int64_t) ret.i64;
        case VALUE_UINT64:    memcpy(&ret.u64, payload, sizeof(uint64_t)); return (int64_t) ret.u64;
        case VALUE_FLOAT:     memcpy(&ret.f, payload, sizeof(float)); return ROUNDF(int64_t, ret.f);
        case VALUE_DOUBLE:    memcpy(&ret.d, payload, sizeof(double)); return ROUNDD(int64_t, ret.d);
        default:            return -1;
    }
}

uint64_t
value_uint64(const VALUE* v)
{
    uint8_t* payload = value_payload((VALUE*) v);
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
    } ret;

    switch(value_type(v)) {
        case VALUE_INT32:     memcpy(&ret.i32, payload, sizeof(int32_t)); return (uint64_t) ret.i32;
        case VALUE_UINT32:    memcpy(&ret.u32, payload, sizeof(uint32_t)); return (uint64_t) ret.u32;
        case VALUE_INT64:     memcpy(&ret.i64, payload, sizeof(int64_t)); return (uint64_t) ret.i64;
        case VALUE_UINT64:    memcpy(&ret.u64, payload, sizeof(uint64_t)); return (uint64_t) ret.u64;
        case VALUE_FLOAT:     memcpy(&ret.f, payload, sizeof(float)); return ROUNDF(uint64_t, ret.f);
        case VALUE_DOUBLE:    memcpy(&ret.d, payload, sizeof(double)); return ROUNDD(uint64_t, ret.d);
        default:            return UINT64_MAX;
    }
}

float
value_float(const VALUE* v)
{
    uint8_t* payload = value_payload((VALUE*) v);
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
    } ret;

    switch(value_type(v)) {
        case VALUE_INT32:     memcpy(&ret.i32, payload, sizeof(int32_t)); return (float) ret.i32;
        case VALUE_UINT32:    memcpy(&ret.u32, payload, sizeof(uint32_t)); return (float) ret.u32;
        case VALUE_INT64:     memcpy(&ret.i64, payload, sizeof(int64_t)); return (float) ret.i64;
        case VALUE_UINT64:    memcpy(&ret.u64, payload, sizeof(uint64_t)); return (float) ret.u64;
        case VALUE_FLOAT:     memcpy(&ret.f, payload, sizeof(float)); return ret.f;
        case VALUE_DOUBLE:    memcpy(&ret.d, payload, sizeof(double)); return (float) ret.d;
        default:            return -1.0f;   /* FIXME: NaN would be likely better but we do not include <math.h> */
    }
}

double
value_double(const VALUE* v)
{
    uint8_t* payload = value_payload((VALUE*) v);
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
    } ret;

    switch(value_type(v)) {
        case VALUE_INT32:     memcpy(&ret.i32, payload, sizeof(int32_t)); return (double) ret.i32;
        case VALUE_UINT32:    memcpy(&ret.u32, payload, sizeof(uint32_t)); return (double) ret.u32;
        case VALUE_INT64:     memcpy(&ret.i64, payload, sizeof(int64_t)); return (double) ret.i64;
        case VALUE_UINT64:    memcpy(&ret.u64, payload, sizeof(uint64_t)); return (double) ret.u64;
        case VALUE_FLOAT:     memcpy(&ret.f, payload, sizeof(float)); return (double) ret.f;
        case VALUE_DOUBLE:    memcpy(&ret.d, payload, sizeof(double)); return ret.d;
        default:            return -1.0;    /* FIXME: NaN would be likely better but we do not include <math.h> */
    }
}

const char*
value_string(const VALUE* v)
{
    uint8_t* payload;
    size_t off = 0;

    if(value_type(v) != VALUE_STRING)
        return NULL;

    payload = value_payload((VALUE*) v);
    while(payload[off] & 0x80)
        off++;
    off++;

    return (char*) payload + off;
}

size_t
value_string_length(const VALUE* v)
{
    uint8_t* payload;
    size_t off = 0;
    size_t len = 0;
    unsigned shift = 0;

    if(value_type(v) != VALUE_STRING)
        return 0;

    payload = value_payload((VALUE*) v);
    while(payload[off] & 0x80) {
        len |= (payload[off] & 0x7f) << shift;
        shift += 7;
        off++;
    }
    len |= payload[off] << shift;

    return len;
}


/*******************
 *** VALUE_ARRAY ***
 *******************/

static ARRAY*
value_array_payload(VALUE* v)
{
    if(value_type(v) != VALUE_ARRAY)
        return NULL;

    return (ARRAY*) value_payload_ex(v, sizeof(void*));
}

static int
value_array_realloc(ARRAY* a, size_t alloc)
{
    VALUE* value_buf;

    value_buf = (VALUE*) realloc(a->value_buf, alloc * sizeof(VALUE));
    if(value_buf == NULL)
        return -1;

    a->value_buf = value_buf;
    a->alloc = alloc;
    return 0;
}

VALUE*
value_array_get(const VALUE* v, size_t index)
{
    ARRAY* a = value_array_payload((VALUE*) v);

    if(a != NULL  &&  index < a->size)
        return &a->value_buf[index];
    else
        return NULL;
}

VALUE*
value_array_get_all(const VALUE* v)
{
    ARRAY* a = value_array_payload((VALUE*) v);

    if(a != NULL)
        return a->value_buf;
    else
        return NULL;
}

size_t
value_array_size(const VALUE* v)
{
    ARRAY* a = value_array_payload((VALUE*) v);

    if(a != NULL)
        return a->size;
    else
        return 0;
}

VALUE*
value_array_append(VALUE* v)
{
    return value_array_insert(v, value_array_size(v));
}

VALUE*
value_array_insert(VALUE* v, size_t index)
{
    ARRAY* a = value_array_payload(v);

    if(a == NULL  ||  index > a->size)
        return NULL;

    if(a->size >= a->alloc) {
        if(value_array_realloc(a, (a->alloc > 0) ? a->alloc * 2 : 1) != 0)
            return NULL;
    }

    if(index < a->size) {
        memmove(a->value_buf + index + 1, a->value_buf + index,
                (a->size - index) * sizeof(VALUE));
    }
    value_init_new(&a->value_buf[index]);
    a->size++;
    return &a->value_buf[index];
}

int
value_array_remove(VALUE* v, size_t index)
{
    return value_array_remove_range(v, index, 1);
}

int
value_array_remove_range(VALUE* v, size_t index, size_t count)
{
    ARRAY* a = value_array_payload(v);
    size_t i;

    if(a == NULL  ||  index + count > a->size)
        return -1;

    for(i = index; i < index + count; i++)
        value_fini(&a->value_buf[i]);

    if(index + count < a->size) {
        memmove(a->value_buf + index, a->value_buf + index + count,
                (a->size - (index + count)) * sizeof(VALUE));
    }
    a->size -= count;

    if(4 * a->size < a->alloc)
        value_array_realloc(a, a->alloc / 2);

    return 0;
}

void
value_array_clean(VALUE* v)
{
    ARRAY* a = value_array_payload(v);
    size_t i;

    if(a == NULL)
        return;

    for(i = 0; i < a->size; i++)
        value_fini(&a->value_buf[i]);

    free(a->value_buf);
    memset(a, 0, sizeof(ARRAY));
}


/******************
 *** VALUE_DICT ***
 ******************/

#define MAKE_RED(node)      do { (node)->key.data[0] |= HAS_REDCOLOR; } while(0)
#define MAKE_BLACK(node)    do { (node)->key.data[0] &= ~HAS_REDCOLOR; } while(0)
#define TOGGLE_COLOR(node)  do { (node)->key.data[0] ^= HAS_REDCOLOR; } while(0)
#define IS_RED(node)        ((node)->key.data[0] & HAS_REDCOLOR)
#define IS_BLACK(node)      (!IS_RED(node))

static DICT*
value_dict_payload(VALUE* v)
{
    if(value_type(v) != VALUE_DICT)
        return NULL;

    return (DICT*) value_payload_ex(v, sizeof(void*));
}

static int
value_dict_default_cmp(const char* key1, size_t len1, const char* key2, size_t len2)
{
    /* Comparing lengths 1st might be in general especially if the keys are
     * long, but it would break value_dict_walk_sorted().
     *
     * In most apps keys are short and ASCII. It is nice to follow
     * lexicographic order at least in such cases as that's what most
     * people expect. And real world, the keys are usually quite short so
     * the cost should be acceptable.
     */

    size_t min_len = (len1 < len2) ? len1 : len2;
    int cmp;

    cmp = memcmp(key1, key2, min_len);
    if(cmp == 0 && len1 != len2)
        cmp = (len1 < len2) ? -1 : +1;

    return cmp;
}

static int
value_dict_cmp(const VALUE* v, const DICT* d,
               const char* key1, size_t len1, const char* key2, size_t len2)
{
    if(!(v->data[0] & HAS_CUSTOMCMP))
        return value_dict_default_cmp(key1, len1, key2, len2);
    else
        return d->cmp_func(key1, len1, key2, len2);
}

static int
value_dict_leftmost_path(RBTREE** path, RBTREE* node)
{
    int n = 0;

    while(node != NULL) {
        path[n++] = node;
        node = node->left;
    }

    return n;
}

unsigned
value_dict_flags(const VALUE* v)
{
    DICT* d = value_dict_payload((VALUE*) v);
    unsigned flags = 0;

    if(d != NULL  &&  (v->data[0] & HAS_ORDERLIST))
        flags |= VALUE_DICT_MAINTAINORDER;

    return flags;
}

size_t
value_dict_size(const VALUE* v)
{
    DICT* d = value_dict_payload((VALUE*) v);

    if(d != NULL)
        return d->size;
    else
        return 0;
}

size_t
value_dict_keys_sorted(const VALUE* v, const VALUE** buffer, size_t buffer_size)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* stack[RBTREE_MAX_HEIGHT];
    int stack_size = 0;
    RBTREE* node;
    size_t n = 0;

    if(d == NULL)
        return 0;

    stack_size = value_dict_leftmost_path(stack, d->root);

    while(stack_size > 0  &&  n < buffer_size) {
        node = stack[--stack_size];
        buffer[n++] = &node->key;
        stack_size += value_dict_leftmost_path(stack + stack_size, node->right);
    }

    return n;
}

size_t
value_dict_keys_ordered(const VALUE* v, const VALUE** buffer, size_t buffer_size)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* node;
    size_t n = 0;

    if(d == NULL  ||  !(v->data[0] & HAS_ORDERLIST))
        return 0;

    node = d->order_head;
    while(node != NULL  &&  n < buffer_size) {
        buffer[n++] = &node->key;
        node = node->order_next;
    }

    return n;
}

VALUE*
value_dict_get_(const VALUE* v, const char* key, size_t key_len)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* node = (d != NULL) ? d->root : NULL;
    int cmp;

    while(node != NULL) {
        cmp = value_dict_cmp(v, d, key, key_len, value_string(&node->key), value_string_length(&node->key));

        if(cmp < 0)
            node = node->left;
        else if(cmp > 0)
            node = node->right;
        else
            return &node->value;
    }

    return NULL;
}

VALUE*
value_dict_get(const VALUE* v, const char* key)
{
    return value_dict_get_(v, key, (key != NULL) ? strlen(key) : 0);
}

static void
value_dict_rotate_left(DICT* d, RBTREE* parent, RBTREE* node)
{
    RBTREE* tmp = node->right;
    node->right = tmp->left;
    tmp->left = node;

    if(parent != NULL) {
        if(parent->left == node)
            parent->left = tmp;
        else if(parent->right == node)
            parent->right = tmp;
    } else {
        d->root = tmp;
    }
}

static void
value_dict_rotate_right(DICT* d, RBTREE* parent, RBTREE* node)
{
    RBTREE* tmp = node->left;
    node->left = tmp->right;
    tmp->right = node;

    if(parent != NULL) {
        if(parent->right == node)
            parent->right = tmp;
        else if(parent->left == node)
            parent->left = tmp;
    } else {
        d->root = tmp;
    }
}

/* Fixes the tree after inserting (red) node path[path_len-1]. */
static void
value_dict_fix_after_insert(DICT* d, RBTREE** path, int path_len)
{
    RBTREE* node;
    RBTREE* parent;
    RBTREE* grandparent;
    RBTREE* grandgrandparent;
    RBTREE* uncle;

    while(1) {
        node = path[path_len-1];
        parent = (path_len > 1) ? path[path_len-2] : NULL;
        if(parent == NULL) {
            MAKE_BLACK(node);
            d->root = node;
            break;
        }

        if(IS_BLACK(parent))
            break;

        /* If we reach here, there is a double-red issue: The node as well as
         * the parent are red.
         *
         * Note grandparent has to exist (implied from red parent).
         */
        grandparent = path[path_len-3];
        uncle = (grandparent->left == parent) ? grandparent->right : grandparent->left;
        if(uncle == NULL || IS_BLACK(uncle)) {
            /* Black uncle. */
            grandgrandparent = (path_len > 3) ? path[path_len-4] : NULL;
            if(grandparent->left != NULL  &&  grandparent->left->right == node) {
                value_dict_rotate_left(d, grandparent, parent);
                parent = node;
                node = node->left;
            } else if(grandparent->right != NULL  &&  grandparent->right->left == node) {
                value_dict_rotate_right(d, grandparent, parent);
                parent = node;
                node = node->right;
            }
            if(parent->left == node)
                value_dict_rotate_right(d, grandgrandparent, grandparent);
            else
                value_dict_rotate_left(d, grandgrandparent, grandparent);

            /* Note that `parent` now,  after the rotations, points to where
             * the grand-parent was originally in the tree hierarchy, and
             * `grandparent` is now its child and also parent of the `uncle`.
             *
             * We switch their colors and hence make sure the upper `parent`
             * is now black. */
            MAKE_BLACK(parent);
            MAKE_RED(grandparent);
            break;
        }

        /* Red uncle. This allows us to make both the parent and the uncle
         * black and propagate the red up to grandparent. */
        MAKE_BLACK(parent);
        MAKE_BLACK(uncle);
        MAKE_RED(grandparent);

        /* But it means we could just move the double-red issue two levels
         * up, so we have to continue re-balancing there. */
        path_len -= 2;
    }
}

VALUE*
value_dict_add_(VALUE* v, const char* key, size_t key_len)
{
    VALUE* res;

    res = value_dict_get_or_add_(v, key, key_len);
    return (value_is_new(res) ? res : NULL);
}

VALUE* value_dict_add(VALUE* v, const char* key)
{
    return value_dict_add_(v, key, strlen(key));
}

VALUE*
value_dict_get_or_add_(VALUE* v, const char* key, size_t key_len)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* node = (d != NULL) ? d->root : NULL;
    RBTREE* path[RBTREE_MAX_HEIGHT];
    int path_len = 0;
    int cmp;

    if(d == NULL)
        return NULL;

    while(node != NULL) {
        cmp = value_dict_cmp(v, d, key, key_len,
                value_string(&node->key), value_string_length(&node->key));

        path[path_len++] = node;

        if(cmp < 0)
            node = node->left;
        else if(cmp > 0)
            node = node->right;
        else
            return &node->value;
    }

    /* Add new node into the tree. */
    node = (RBTREE*) malloc((v->data[0] & HAS_ORDERLIST) ?
                sizeof(RBTREE) : OFFSETOF(RBTREE, order_prev));
    if(node == NULL)
        return NULL;
    if(value_init_string_(&node->key, key, key_len) != 0) {
        free(node);
        return NULL;
    }
    value_init_new(&node->value);
    node->left = NULL;
    node->right = NULL;
    MAKE_RED(node);

    /* Update order_list. */
    if(v->data[0] & HAS_ORDERLIST) {
        node->order_prev = d->order_tail;
        node->order_next = NULL;

        if(d->order_tail != NULL)
            d->order_tail->order_next = node;
        else
            d->order_head = node;
        d->order_tail = node;
    }

    /* Insert the new node. */
    if(path_len > 0) {
        if(cmp < 0)
            path[path_len - 1]->left = node;
        else
            path[path_len - 1]->right = node;
    } else {
        d->root = node;
    }

    /* Re-balance. */
    path[path_len++] = node;
    value_dict_fix_after_insert(d, path, path_len);

    d->size++;

    return &node->value;
}

VALUE*
value_dict_get_or_add(VALUE* v, const char* key)
{
    return value_dict_get_or_add_(v, key, (key != NULL) ? strlen(key) : 0);
}

/* Fixes the tree after making the given path one black node shorter.
 * (Note that that the path may end with NULL if the removed node had no child.) */
static void
value_dict_fix_after_remove(DICT* d, RBTREE** path, int path_len)
{
    RBTREE* node;
    RBTREE* parent;
    RBTREE* grandparent;
    RBTREE* sibling;

    while(1) {
        node = path[path_len-1];
        if(node != NULL  &&  IS_RED(node)) {
            MAKE_BLACK(node);
            break;
        }

        parent = (path_len > 1) ? path[path_len-2] : NULL;
        if(parent == NULL)
            break;

        /* Sibling has to exist because its subtree must have black
         * count as our subtree + 1. */
        sibling = (parent->left == node) ? parent->right : parent->left;
        grandparent = (path_len > 2) ? path[path_len-3] : NULL;
        if(IS_RED(sibling)) {
            /* Red sibling: Convert to black sibling case. */
            if(parent->left == node)
                value_dict_rotate_left(d, grandparent, parent);
            else
                value_dict_rotate_right(d, grandparent, parent);

            MAKE_BLACK(sibling);
            MAKE_RED(parent);
            path[path_len-2] = sibling;
            path[path_len-1] = parent;
            path[path_len++] = node;
            continue;
        }

        if((sibling->left != NULL && IS_RED(sibling->left))  ||
           (sibling->right != NULL && IS_RED(sibling->right))) {
            /* Black sibling having at least one red child. */
            if(node == parent->left && (sibling->right == NULL || IS_BLACK(sibling->right))) {
                MAKE_RED(sibling);
                MAKE_BLACK(sibling->left);
                value_dict_rotate_right(d, parent, sibling);
                sibling = parent->right;
            } else if(node == parent->right && (sibling->left == NULL || IS_BLACK(sibling->left))) {
                MAKE_RED(sibling);
                MAKE_BLACK(sibling->right);
                value_dict_rotate_left(d, parent, sibling);
                sibling = parent->left;
            }

            if(IS_RED(sibling) != IS_RED(parent))
                TOGGLE_COLOR(sibling);
            MAKE_BLACK(parent);
            if(node == parent->left) {
                MAKE_BLACK(sibling->right);
                value_dict_rotate_left(d, grandparent, parent);
            } else {
                MAKE_BLACK(sibling->left);
                value_dict_rotate_right(d, grandparent, parent);
            }
            break;
        }

        /* Black sibling with both children black. Make sibling subtree one
         * black shorter to match our subtree and try to resolve the black
         * deficit at the parent level. */
        if(IS_RED(parent)) {
            MAKE_RED(sibling);
            MAKE_BLACK(parent);
            break;
        } else {
            /* Fix the black deficit higher in the tree. */
            MAKE_RED(sibling);
            path_len--;
        }
    }
}

int
value_dict_remove_(VALUE* v, const char* key, size_t key_len)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* node = (d != NULL) ? d->root : NULL;
    RBTREE* single_child;
    RBTREE* path[RBTREE_MAX_HEIGHT];
    int path_len = 0;
    int cmp;

    /* Find the node to remove. */
    while(node != NULL) {
        cmp = value_dict_cmp(v, d, key, key_len,
                value_string(&node->key), value_string_length(&node->key));

        path[path_len++] = node;

        if(cmp < 0)
            node = node->left;
        else if(cmp > 0)
            node = node->right;
        else
            break;
    }
    if(node == NULL)
        return -1;

    /* It is far more easier to remove a node at the bottom of the tree, if it
     * has at most one child. Therefore, if we are not at the bottom, we switch
     * our place with another node, which is our direct successor; i.e. with
     * the minimal value of the right subtree. */
    if(node->right != NULL) {
        RBTREE* successor;
        int node_index = path_len-1;

        if(node->right->left != NULL) {
            RBTREE* tmp;

            path_len += value_dict_leftmost_path(path + path_len, node->right);
            successor = path[path_len-1];

            tmp = successor->right;
            successor->right = node->right;
            node->right = tmp;

            if(path[path_len-2]->left == successor)
                path[path_len-2]->left = node;
            else
                path[path_len-2]->right = node;

            path[node_index] = successor;
            path[path_len-1] = node;
        } else if(node->left != NULL) {
            /* node->right is the successor. Must be handled specially as the
             * code above would entangle the pointers. */
            successor = node->right;

            node->right = successor->right;
            successor->right = node;

            path[path_len-1] = successor;
            path[path_len++] = node;
        } else {
            /* node->left == NULL; i.e. node has at most one child.
             * The code below is capable to handle this. */
            successor = NULL;
        }

        if(successor != NULL) {
            /* Common work for the two active code paths above. */
            successor->left = node->left;
            node->left = NULL;

            if(node_index > 0) {
                if(path[node_index-1]->left == node)
                    path[node_index-1]->left = successor;
                else
                    path[node_index-1]->right = successor;
            } else {
                d->root = successor;
            }

            if(IS_RED(successor) != IS_RED(node)) {
                TOGGLE_COLOR(successor);
                TOGGLE_COLOR(node);
            }
        }
    }

    /* The node now cannot have more then one child. Move it upwards
     * to the node's place. */
    single_child = (node->left != NULL) ? node->left : node->right;
    if(path_len > 1) {
        if(path[path_len-2]->left == node)
            path[path_len-2]->left = single_child;
        else
            path[path_len-2]->right = single_child;
    } else {
        d->root = single_child;
    }
    path[path_len-1] = single_child;

    /* Node is now successfully disconnected. But the tree may need
     * re-balancing if we have removed black node. */
    if(IS_BLACK(node))
        value_dict_fix_after_remove(d, path, path_len);

    /* Kill the node */
    if(v->data[0] & HAS_ORDERLIST) {
        if(node->order_prev != NULL)
            node->order_prev->order_next = node->order_next;
        else
            d->order_head = node->order_next;

        if(node->order_next != NULL)
            node->order_next->order_prev = node->order_prev;
        else
            d->order_tail = node->order_prev;
    }
    value_fini(&node->key);
    value_fini(&node->value);
    free(node);
    d->size--;

    return 0;
}

int
value_dict_remove(VALUE* v, const char* key)
{
    return value_dict_remove_(v, key, (key != NULL) ? strlen(key) : 0);
}

int
value_dict_walk_ordered(const VALUE* v, int (*visit_func)(const VALUE*, VALUE*, void*), void* ctx)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* node;
    int ret;

    if(d == NULL  ||  !(v->data[0] & HAS_ORDERLIST))
        return -1;

    node = d->order_head;
    while(node != NULL) {
        ret = visit_func(&node->key, &node->value, ctx);
        if(ret != 0)
            return ret;
        node = node->order_next;
    }

    return 0;
}

int
value_dict_walk_sorted(const VALUE* v, int (*visit_func)(const VALUE*, VALUE*, void*), void* ctx)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* stack[RBTREE_MAX_HEIGHT];
    int stack_size = 0;
    RBTREE* node;
    int ret;

    if(d == NULL)
        return -1;

    stack_size = value_dict_leftmost_path(stack, d->root);

    while(stack_size > 0) {
        node = stack[--stack_size];
        ret = visit_func(&node->key, &node->value, ctx);
        if(ret != 0)
            return ret;
        stack_size += value_dict_leftmost_path(stack + stack_size, node->right);
    }

    return 0;
}

void
value_dict_clean(VALUE* v)
{
    DICT* d = value_dict_payload((VALUE*) v);
    RBTREE* stack[RBTREE_MAX_HEIGHT];
    int stack_size;
    RBTREE* node;
    RBTREE* right;

    if(d == NULL)
        return;

    stack_size = value_dict_leftmost_path(stack, d->root);

    while(stack_size > 0) {
        node = stack[--stack_size];
        right = node->right;

        value_fini(&node->key);
        value_fini(&node->value);
        free(node);

        stack_size += value_dict_leftmost_path(stack + stack_size, right);
    }

    if(v->data[0] & HAS_ORDERLIST)
        memset(d, 0, OFFSETOF(DICT, cmp_func));
    else
        memset(d, 0, OFFSETOF(DICT, order_head));
}



#ifdef CRE_TEST
/* Verification of RB-tree correctness. */

/* Returns black height of the tree, or -1 on an error. */
static int
value_dict_verify_recurse(RBTREE* node)
{
    int left_black_height;
    int right_black_height;
    int black_height;

    if(node->left != NULL) {
        if(IS_RED(node) && IS_RED(node->left))
            return -1;

        left_black_height = value_dict_verify_recurse(node->left);
        if(left_black_height < 0)
            return left_black_height;
    } else {
        left_black_height = 1;
    }

    if(node->right != NULL) {
        if(IS_RED(node) && IS_RED(node->right))
            return -1;

        right_black_height = value_dict_verify_recurse(node->right);
        if(right_black_height < 0)
            return right_black_height;
    } else {
        right_black_height = 1;
    }

    if(left_black_height != right_black_height)
        return -1;

    black_height = left_black_height;
    if(IS_BLACK(node))
        black_height++;
    return black_height;
}

/* Returns 0 if ok, or -1 on an error. */
int
value_dict_verify(VALUE* v)
{
    DICT* d = value_dict_payload(v);
    if(d == NULL)
        return -1;

    if(d->root == NULL)
        return 0;

    if(IS_RED(d->root))
        return -1;

    return (value_dict_verify_recurse(d->root) > 0) ? 0 : -1;
}

#endif  /* #ifdef CRE_TEST */
