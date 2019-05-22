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
#include "defs.h"

#include <ctype.h>
#include <string.h>


static void
test_MIN(void)
{
    TEST_CHECK(MIN(1, 1) == 1);
    TEST_CHECK(MIN(-2, 2) == -2);
    TEST_CHECK(MIN(-2, -3) == -3);
    TEST_CHECK(MIN(0U, 0xffffffffU) == 0U);
    TEST_CHECK(MIN(-0x7fffffff-1, 0x7fffffff) == -0x7fffffff-1);
}

static void
test_MAX(void)
{
    TEST_CHECK(MAX(1, 1) == 1);
    TEST_CHECK(MAX(-2, 2) == 2);
    TEST_CHECK(MAX(-4, -3) == -3);
    TEST_CHECK(MAX(0U, 0xffffffffU) == 0xffffffffU);
    TEST_CHECK(MAX(-0x7fffffff-1, 0x7fffffff) == 0x7fffffff);
}

static void
test_MIN3(void)
{
    TEST_CHECK(MIN3(-0x7fffffff-1, 0, 0x7fffffff) == -0x7fffffff-1);
}

static void
test_MAX3(void)
{
    TEST_CHECK(MAX3(-0x7fffffff-1, 0, 0x7fffffff) == 0x7fffffff);
}

static void
test_CLAMP(void)
{
    TEST_CHECK(CLAMP(-2, 4, 8) == 4);
    TEST_CHECK(CLAMP(7, 4, 8) == 7);
    TEST_CHECK(CLAMP(9, 4, 8) == 8);
}

static void
test_ABS(void)
{
    TEST_CHECK(ABS(-1) == 1);
    TEST_CHECK(ABS(0) == 0);
    TEST_CHECK(ABS(2) == 2);
    TEST_CHECK(ABS(-0x7fffffff) == 0x7fffffff);
    TEST_CHECK(ABS(-0.0f) == 0.0f);
    TEST_CHECK(ABS(0.0f) == 0.0f);
    TEST_CHECK(ABS(-1.0f) == 1.0f);
}

static void
test_SIZEOF_ARRAY(void)
{
    char hello[] = "hello";
    char hello5[5] = "hello";
    char hello16[16] = "hello";
    int i[] = { 0, 1, 2 };
    int i5[5];

    TEST_CHECK(SIZEOF_ARRAY(hello) == strlen(hello) + 1);
    TEST_CHECK(SIZEOF_ARRAY(hello5) == 5);
    TEST_CHECK(SIZEOF_ARRAY(hello16) == 16);
    TEST_CHECK(SIZEOF_ARRAY(i) == 3);
    TEST_CHECK(SIZEOF_ARRAY(i5) == 5);
}

static void
test_OFFSETOF(void)
{
    typedef struct {
        int a;
        int b;
    } X;

    TEST_CHECK(OFFSETOF(X, a) == 0);
    TEST_CHECK(OFFSETOF(X, b) == sizeof(int));
}

static void
test_CONTAINEROF(void)
{
    typedef struct {
        int a;
        int member;
    } C;

    C c;
    int* ptr = &c.member;

    TEST_CHECK(CONTAINEROF(ptr, C, member) == &c);
}

static void
test_STRINGIZE(void)
{
#define FOO     1
#define BAR     2

    const char hello[] = STRINGIZE(hello world);
    const char foobar[] = STRINGIZE(FOO) "." STRINGIZE(BAR);
    const char foobar2[] = STRINGIZE(FOO.BAR);
    const char expr[] = STRINGIZE(1 + 2);
    const char line[] = STRINGIZE(__LINE__);
    size_t i;

    TEST_CHECK(strcmp(hello, "hello world") == 0);
    TEST_CHECK(strcmp(foobar, "1.2") == 0);
    TEST_CHECK(strcmp(foobar2, "1.2") == 0);
    TEST_CHECK(strcmp(expr, "1 + 2") == 0);

    /* Depending on building environment, there might be some directory
     * prepended, so lets just verify the base name is somewhere in it. */
    TEST_CHECK(strstr(STRINGIZE(__FILE__), "test-defs.c") != NULL);

    for(i = 0; line[i] != '\0'; i++)
        TEST_CHECK(isdigit(line[i]));
}


TEST_LIST = {
    { "min",            test_MIN },
    { "max",            test_MAX },
    { "min3",           test_MIN3 },
    { "max3",           test_MAX3 },
    { "clamp",          test_CLAMP },
    { "abs",            test_ABS },
    { "sizeof-array",   test_SIZEOF_ARRAY },
    { "offsetof",       test_OFFSETOF },
    { "containerof",    test_CONTAINEROF },
    { "stringize",      test_STRINGIZE },
    { 0 }
};
