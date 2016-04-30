/*
 * WinDrawLib
 * Copyright (c) 2015-2016 Martin Mitas
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

#ifndef WD_MEMSTREAM_H
#define WD_MEMSTREAM_H

#include "misc.h"


/* The caller is responsible to ensure the data (or the resource) are valid
 * and immutable for the lifetime of the stream. The stream does not copy the
 * data and reads them directly from the given source.
 *
 * The caller should release the stream as a standard COM object, i.e. via
 * method IStream::Release().
 */

IStream* memstream_create(const BYTE* buffer, ULONG size);

IStream* memstream_create_from_resource(HINSTANCE instance,
                        const WCHAR* res_type, const WCHAR* res_name);


#endif  /* WD_MEMSTREAM_H */
