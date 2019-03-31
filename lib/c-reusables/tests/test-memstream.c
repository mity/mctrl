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

/* Enable C preprocessor wrappers for COM methods in <objidl.h> */
#ifndef COBJMACROS
    #define COBJMACROS
#endif

#include "acutest.h"
#include "memstream.h"


static const BYTE test_data[] = "Hello world.";
static const size_t test_size = sizeof(test_data);


static void
test_from_mem(void)
{
    IStream* stream;
    HRESULT hr;

    hr = memstream_create(test_data, test_size, &stream);
    if(TEST_CHECK(hr == S_OK  &&  stream != NULL)) {
        BYTE buffer[256];
        ULONG n;
        LARGE_INTEGER li;

        hr = IStream_Read(stream, buffer, 5, &n);
        TEST_CHECK_(hr == S_OK  &&  n == 5, "IStream::Read() few bytes");

        hr = IStream_Read(stream, buffer, 1000, &n);
        TEST_CHECK_(hr == S_OK  &&  n == test_size - 5, "IStream::Read() till end of the stream");

        hr = IStream_Read(stream, buffer, 1000, &n);
        TEST_CHECK_(hr == S_FALSE  &&  n == 0, "IStream::Read() in end-of-file situation.");

        li.QuadPart = 0;
        hr = IStream_Seek(stream, li, STREAM_SEEK_SET, NULL);
        TEST_CHECK_(hr == S_OK, "IStream::Seek(STREAM_SEEK_SET)");

        hr = IStream_Read(stream, buffer, 1, &n);
        TEST_CHECK_(hr == S_OK  &&  n == 1  &&  buffer[0] == 'H', "IStream::Read() after Seek()");

        n = IStream_Release(stream);
        TEST_CHECK_(n == 0, "IStream::Release()");
    }
}

static void
test_from_res(void)
{
    HMODULE dll;
    IStream* stream;
    HRESULT hr;

    dll = LoadLibraryW(L"COMCTL32.DLL");
    if(TEST_CHECK(dll != NULL)) {
        hr = memstream_create_from_resource(dll, MAKEINTRESOURCEW(1), (LPWSTR)RT_VERSION, &stream);

        if(TEST_CHECK(hr == S_OK  &&  stream != NULL)) {
            ULONG n;

            n = IStream_Release(stream);
            TEST_CHECK_(n == 0, "IStream::Release()");
        }

        FreeLibrary(dll);
    }
}


TEST_LIST = {
    { "istream-from-memory", test_from_mem },
    { "istream-from-resource", test_from_res },
    { 0 }
};
