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

#include "acutest.h"
#include "buffer.h"


static void
test_init(void)
{
    BUFFER buf;

    buffer_init(&buf);

    buffer_append(&buf, "hello", 5);

    TEST_CHECK(buf.size == 5);
    TEST_CHECK(buf.alloc >= buf.size);
    TEST_CHECK(memcmp(buf.data, "hello", 5) == 0);

    buffer_fini(&buf);
}

static void
test_grow(void)
{
    BUFFER buf = BUFFER_INITIALIZER;
    size_t i, n = 100;
    char c;

    for(i = 0; i < n; i++) {
        TEST_CHECK(buf.size == i);
        TEST_CHECK(buf.alloc >= buf.size);

        c = (char) i;
        buffer_append(&buf, &c, 1);
    }

    TEST_CHECK(buf.size == n);
    TEST_CHECK(buf.alloc >= buf.size);

    buffer_fini(&buf);
}

static void
test_reserve(void)
{
    BUFFER buf = BUFFER_INITIALIZER;
    size_t i, n = 100;
    char c;

    buffer_reserve(&buf, n);
    TEST_CHECK(buf.alloc == n);

    for(i = 0; i < n; i++) {
        TEST_CHECK(buf.size == i);
        TEST_CHECK(buf.alloc == n);

        c = (char) i;
        buffer_append(&buf, &c, 1);
    }

    TEST_CHECK(buf.size == n);
    TEST_CHECK(buf.alloc == n);

    buffer_fini(&buf);
}

static void
test_shrink(void)
{
    BUFFER buf = BUFFER_INITIALIZER;

    buffer_append(&buf, "1234567890", 10);
    buffer_reserve(&buf, 1000);
    TEST_CHECK(buf.alloc >= 1000);
    TEST_CHECK(buf.size == 10);

    buffer_shrink(&buf);
    TEST_CHECK(buf.alloc == 10);
    TEST_CHECK(buf.size == 10);

    buffer_fini(&buf);
}

static void
test_insert(void)
{
    BUFFER buf = BUFFER_INITIALIZER;

    buffer_append(&buf, "1234567890", 10);
    buffer_insert(&buf, 3, "foo", strlen("foo"));
    TEST_CHECK(memcmp(buf.data, "123foo4567890", buf.size) == 0);

    buffer_fini(&buf);
}

static void
test_remove(void)
{
    BUFFER buf = BUFFER_INITIALIZER;

    buffer_append(&buf, "1234567890", 10);
    buffer_remove(&buf, 3, 4);
    TEST_CHECK(memcmp(buf.data, "123890", buf.size) == 0);

    buffer_fini(&buf);
}


TEST_LIST = {
    { "init",       test_init },
    { "grow",       test_grow },
    { "reserve",    test_reserve },
    { "shrink",     test_shrink },
    { "insert",     test_insert },
    { "remove",     test_remove },
    { 0 }
};
