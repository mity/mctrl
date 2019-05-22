/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2017 Martin Mitas
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
#include "crc32.h"


typedef struct TEST_VECTOR {
    const char* str;
    size_t n;
    uint32_t crc32;
} TEST_VECTOR;

#define LEN(x)      (sizeof(x)-1)
#define TEST(x)     x, LEN(x)

/* The test vector has been taken here:
 * http://reveng.sourceforge.net/crc-catalogue/17plus.htm#crc.cat-bits.32
 */
static const TEST_VECTOR test_vectrors[] = {
    { TEST("123456789"), 0xcbf43926U },
    { 0 }
};


static void
test_crc32(void)
{
    int i;

    for(i = 0; test_vectrors[i].str != NULL; i++) {
        const char* str = test_vectrors[i].str;
        size_t n = test_vectrors[i].n;
        uint32_t expected = test_vectrors[i].crc32;
        uint32_t produced;

        produced = crc32(str, n);
        if(!TEST_CHECK_(produced == expected, "vector '%.*s'", (int)n, str)) {
            TEST_MSG("Expected: %x", (unsigned) expected);
            TEST_MSG("Produced: %x", (unsigned) produced);
        }
    }
}


TEST_LIST = {
    { "crc32",      test_crc32 },
    { 0 }
};
