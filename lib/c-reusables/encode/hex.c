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

#include "hex.h"


int
hex_encode(const void* in_buf, unsigned in_size,
           char* out_buf, unsigned out_size, int lowercase)
{
    static const char lower_xdigits[16] = "0123456789abcdef";
    static const char upper_xdigits[16] = "0123456789ABCDEF";

    const char* xdigits = (lowercase ? lower_xdigits : upper_xdigits);
    const unsigned char* in;
    const unsigned char* in_end;
    char* out;
    char* out_end;

    if(out_buf == NULL)
        return in_size * 2 + 1;

    if(out_size < in_size * 2)
        return -1;

    in = (const unsigned char*) in_buf;
    in_end = in + in_size;
    out = out_buf;
    out_end = out + out_size;

    while(in < in_end  &&  out + 1 < out_end) {
        *out++ = xdigits[(*in & 0xf0) >> 4];
        *out++ = xdigits[(*in & 0x0f)];
        in++;
    }

    /* If enough space in the output buffer, append zero terminator. */
    if(out < out_end)
        *out = '\0';

    return out - out_buf;
}

int
hex_decode(const char* in_buf, unsigned in_size, void* out_buf, unsigned out_size)
{
    const char* in = in_buf;
    const char* in_end = in + in_size;
    unsigned char* out = (unsigned char*) out_buf;
    unsigned char* out_end = out + out_size;

    if(out_buf == NULL)
        return in_size / 2;

    if(in_size % 2 != 0)
        return -1;
    if(out_size < in_size / 2)
        return -1;

    in = in_buf;
    in_end = in + in_size;
    out = (unsigned char*) out_buf;
    out_end = out + out_size;

    while(in < in_end  &&  out < out_end) {
        if('0' <= *in  &&  *in <= '9')
            *out = (*in - '0') << 4;
        else if('a' <= *in  &&  *in <= 'z')
            *out = (*in - 'a' + 10) << 4;
        else if('A' <= *in  &&  *in <= 'Z')
            *out = (*in - 'A' + 10) << 4;
        else
            return -1;
        in++;

        if('0' <= *in  &&  *in <= '9')
            *out |= (*in - '0');
        else if('a' <= *in  &&  *in <= 'z')
            *out |= (*in - 'a' + 10);
        else if('A' <= *in  &&  *in <= 'Z')
            *out |= (*in - 'A' + 10);
        else
            return -1;
        in++;

        out++;
    }

    return out - (const unsigned char*) out_buf;
}
