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

#ifndef CRE_MALLOCA_H
#define CRE_MALLOCA_H

#include <malloc.h>

#ifdef _WIN32
    /* Windows do not have <alloca.h>.
     * There is _alloca() in <malloc.h> instead. */
    #define CRE_alloca__ _alloca
#else
    #include <alloca.h>
    #define CRE_alloca__ alloca
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* On resource-limited hardware with smaller stacks, e.g. on embedded ones,
 * you may want to lower this threshold. MALLOCA allocations lower then this
 * are allocated on stack, larger on heap. */
#ifndef MALLOCA_THRESHOLD
    #define MALLOCA_THRESHOLD         1024
#endif


/* Helper. Do not use directly. */
static
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
    inline
#elif defined __GNUC__
    __inline__
#elif defined _MSC_VER
    __inline
#endif
void*
malloca_set_mark__(void* ptr, int mark)
{
    if(ptr != NULL) {
        int* x = (int*)ptr;
        *x = mark;
        ptr = (void*)((char*)ptr + sizeof(void*));
    }
    return ptr;
}


/* Allocate block of memory via malloc() or alloca(), depending on the
 * requested amount of memory.
 *
 * MALLOCA uses the default threshold defined by MALLOCA_THRESHOLD,
 * while MALLOCA_ allows you to specify it with its second parameter explicitly
 * on each call site.
 *
 * Returns pointer to the memory block or NULL on failure. When not needed
 * anymore, release it with FREEA().
 */
#define MALLOCA_(size, threshold)                                           \
    ((size) < (threshold) - sizeof(void*)                                   \
        ? malloca_set_mark__(CRE_alloca__(size + sizeof(void*)), 0xcccc)    \
        : malloca_set_mark__(malloc(size + sizeof(void*)), 0xdddd))

#define MALLOCA(size)       MALLOCA_((size), MALLOCA_THRESHOLD)

/* Release any memory allocated with MALLOCA() or MALLOCA_().
 */
#define FREEA(ptr)                                                          \
    do {                                                                    \
        if((ptr) != NULL) {                                                 \
            int* x = (int*)((char*)(ptr) - sizeof(void*));                  \
            if(*x == 0xdddd)                                                \
                free(x);                                                    \
        }                                                                   \
    } while(0)


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_MALLOCA_H */
