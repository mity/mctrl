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
#include "hex.h"

#include <string.h>


static void
test_hex_encode(void)
{
    char blob[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0xff };
    char buffer[256];
    char expect_lower[] = "000102030405060708090a0b0c0d0e0fff";
    char expect_upper[] = "000102030405060708090A0B0C0D0E0FFF";

    TEST_CHECK(hex_encode(blob, sizeof(blob), buffer, sizeof(buffer), 1) == strlen(expect_lower));
    TEST_CHECK(strcmp(buffer, expect_lower) == 0);

    TEST_CHECK(hex_encode(blob, sizeof(blob), buffer, sizeof(buffer), 0) == strlen(expect_upper));
    TEST_CHECK(strcmp(buffer, expect_upper) == 0);
}

static void
test_hex_decode(void)
{
    /* Keep the visually ugly mixed lower and upper case here. */
    char hex[] = "000102030405060708090a0B0c0D0e0fFf";
    char buffer[256];
    char expect[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0xff };

    TEST_CHECK(hex_decode(hex, strlen(hex), buffer, sizeof(buffer)) == sizeof(expect));
    TEST_CHECK(memcmp(buffer, expect, sizeof(expect)) == 0);
}

TEST_LIST = {
    { "hex-encode", test_hex_encode },
    { "hex-decode", test_hex_decode },
    { 0 }
};
