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
#include "base64.h"


typedef struct TEST_VECTOR {
    const char* blob;
    const char* base64;
} TEST_VECTOR;


/* Test vectors from RFC 4648. */
static const TEST_VECTOR test_vectors[] = {
    { "",       ""         },
    { "f",      "Zg=="     },
    { "fo",     "Zm8="     },
    { "foo",    "Zm9v"     },
    { "foob",   "Zm9vYg==" },
    { "fooba",  "Zm9vYmE=" },
    { "foobar", "Zm9vYmFy" },
    { 0 }
};

static const TEST_VECTOR test_vectors_nopadding[] = {
    { "",       ""         },
    { "f",      "Zg"       },
    { "fo",     "Zm8"      },
    { "foo",    "Zm9v"     },
    { "foob",   "Zm9vYg"   },
    { "fooba",  "Zm9vYmE"  },
    { "foobar", "Zm9vYmFy" },
    { 0 }
};


static void
test_base64_encode_(const TEST_VECTOR* vectors, const BASE64_OPTIONS* options)
{
    char out_buf[256];
    int i;

    for(i = 0; vectors[i].blob != NULL; i++) {
        const char* blob = vectors[i].blob;
        const char* base64 = vectors[i].base64;
        int out_size;

        out_size = base64_encode(blob, strlen(blob), NULL, 0, options);
        if(!TEST_CHECK_(out_size >= 0  &&
                        out_size == strlen(base64)+1,
                        "expected output size for '%s' -> '%s'", blob, base64))
        {
            TEST_MSG("Expected: %d bytes", (int) strlen(base64));
            TEST_MSG("Produced: %d bytes", (int) out_size);
        }

        out_size = base64_encode(blob, strlen(blob), out_buf, sizeof(out_buf), options);
        if(!TEST_CHECK_(out_size >= 0  &&
                        out_size == strlen(base64)  &&
                        memcmp(out_buf, base64, out_size) == 0,
                        "encoding '%s' -> %s", blob, base64))
        {
            TEST_MSG("Expected: '%s'", base64);
            TEST_MSG("Produced: '%s'", (out_size > 0 ? out_buf : "base64_encode() failed"));
        }
    }
}

static void
test_base64_encode(void)
{
    test_base64_encode_(test_vectors, NULL);
}

static void
test_base64_encode_nopadding(void)
{
    static const BASE64_OPTIONS opts_nopadding = { '+', '/', '\0' };
    test_base64_encode_(test_vectors_nopadding, &opts_nopadding);
}


static void
test_base64_decode_(const TEST_VECTOR* vectors, const BASE64_OPTIONS* options)
{
    char out_buf[256];
    int i;

    for(i = 0; vectors[i].blob != NULL; i++) {
        const char* blob = vectors[i].blob;
        const char* base64 = vectors[i].base64;
        int out_size;

        out_size = base64_decode(base64, strlen(base64), NULL, 0, options);
        if(!TEST_CHECK_(out_size >= 0  &&
                        out_size == strlen(blob),
                        "expected output size for '%s' -> '%s'", base64, blob))
        {
            TEST_MSG("Expected: %d bytes", (int) strlen(blob));
            TEST_MSG("Produced: %d bytes", (int) out_size);
        }

        out_size = base64_decode(base64, strlen(base64), out_buf, sizeof(out_buf), options);
        if(!TEST_CHECK_(out_size >= 0  &&
                        out_size == strlen(blob)  &&
                        memcmp(out_buf, blob, out_size) == 0,
                        "decoding '%s' -> '%s'", base64, blob))
        {
            TEST_MSG("Expected: '%s'", blob);
            TEST_MSG("Produced: '%s'", (out_size > 0 ? out_buf : "base64_decode() failed"));
        }
    }

}

static void
test_base64_decode(void)
{
    test_base64_decode_(test_vectors, NULL);
}

static void
test_base64_decode_nopadding(void)
{
    static const BASE64_OPTIONS opts_nopadding = { '+', '/', '\0' };
    test_base64_decode_(test_vectors_nopadding, &opts_nopadding);
}


TEST_LIST = {
    { "base64-encode-standard",     test_base64_encode },
    { "base64-encode-no-padding",   test_base64_encode_nopadding },
    { "base64-decode-standard",     test_base64_decode },
    { "base64-decode-no-padding",   test_base64_decode_nopadding },
    { 0 }
};
