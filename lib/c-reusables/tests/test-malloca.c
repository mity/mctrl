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

#include "acutest.h"
#include "malloca.h"

#include <stdint.h>


static void
test_malloca_zero(void)
{
    /* MALLOCA with zero size should not return NULL, but something unique
     * what can be passed to FREEA(). */
    void* ptr;

    ptr = MALLOCA(0);
    TEST_CHECK(ptr != NULL);
    FREEA(ptr);
}

static void
test_malloca_small(void)
{
    /* Small MALLOCA allocations should be on stack. I.e. its address should
     * be close to a local variable. */

    int on_stack;
    void* on_heap;
    uintptr_t on_stack_addr;
    uintptr_t on_heap_addr;

    on_heap = MALLOCA(32);
    TEST_CHECK(on_heap != NULL);

    on_stack_addr = (uintptr_t) &on_stack;
    on_heap_addr = (uintptr_t) on_heap;

    /* No assumption whether stack grows up or down. */
    TEST_CHECK(on_stack_addr - on_heap_addr < 0xff  ||  on_heap_addr - on_stack_addr < 0xff);

    FREEA(on_heap);
}

static void
test_malloca_large(void)
{
    /* Similarly, large MALLOCA allocations should be on heap. I.e. its address
     * should be far from any local variable. */

    int on_stack;
    void* on_heap;
    uintptr_t on_stack_addr;
    uintptr_t on_heap_addr;

    on_heap = MALLOCA(32 * 1024);
    TEST_CHECK(on_heap != NULL);

    on_stack_addr = (uintptr_t) &on_stack;
    on_heap_addr = (uintptr_t) on_heap;

    if(on_stack_addr > on_heap_addr)
        TEST_CHECK(on_stack_addr - on_heap_addr > 16384);
    else
        TEST_CHECK(on_heap_addr - on_stack_addr > 16384);

    FREEA(on_heap);
}

static void
test_malloca_ultralarge(void)
{
    /* Attempt to allocate something ultra-large should always fail and return
     * NULL, the same way as malloc() does.
     *
     * The ridiculous expressions are to pass over warning
     * -Walloc-size-larger-than= included in -Wall (gcc 7.2.0 on Linux).
     */
    size_t size = SIZE_MAX / 2 - 8;
    void* ptr;


    /* Verify that malloc() fails. If not, we have chosen too small size
     * and our test needs some tuning... */
    ptr = malloc(size);
    TEST_CHECK(ptr == NULL);
    free(ptr);      /* Should not be needed if it really works ;-) */

    /* And now test that MALLOCA fails the same way. */
    ptr = MALLOCA(size);
    TEST_CHECK(ptr == NULL);
    free(ptr);      /* Should not be needed if it really works ;-) */
}

static void
test_freea_null(void)
{
    /* Just check this does not cause SIGSEGV. */

    FREEA(NULL);
    TEST_CHECK_(1, "FREEA(NULL) is noop that never crashes.");
}


TEST_LIST = {
    { "malloca-zero",       test_malloca_zero },
    { "malloca-small",      test_malloca_small },
    { "malloca-large",      test_malloca_large },
    { "malloca-ultralarge", test_malloca_ultralarge },
    { "freea-null",         test_freea_null },
    { 0 }
};

