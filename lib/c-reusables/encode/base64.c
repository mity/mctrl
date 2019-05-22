/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2016-2017 Martin Mitas
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

#include "base64.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>


static const BASE64_OPTIONS base64_def_options = { '+', '/', '=' };

static const char base64_table_core[62] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";


int
base64_encode(const void* in_buf, unsigned in_size,
              char* out_buf, unsigned out_size,
              const BASE64_OPTIONS* options)
{
    char table[64];
    const uint8_t* in = (const uint8_t*) in_buf;
    char* out = out_buf;
    unsigned out_size_needed;
    unsigned in_off = 0;
    unsigned out_off = 0;
    unsigned v;

    if(options == NULL)
        options = &base64_def_options;

    /* Every three bytes are encoded into 4 characters of output.
     * Finally add +1 for zero terminator. */
    out_size_needed = ((in_size + 2) / 3) * 4;

    /* With padding disabled, we may need little less. */
    if(!options->pad) {
        switch(in_size % 3) {
            case 0: break;
            case 1: out_size_needed -= 2; break;
            case 2: out_size_needed -= 1; break;
        }
    }

    if(out_buf == NULL) {
        /* Recommend caller +1 for zero terminator. */
        return out_size_needed + 1;
    }

    /* Check we have enough space in the output. */
    if(out_size < out_size_needed)
        return -ENOBUFS;

    /* Build table. */
    memcpy(table, base64_table_core, 62);
    table[62] = options->ch62;
    table[63] = options->ch63;

    /* Main loop. We process triples of input bytes at once to avoid branches
     * inside the loop and handle indivisible tail below specially. */
    while(in_off + 3 <= in_size) {
        v  = ((unsigned) in[in_off++]) << 16;
        v |= ((unsigned) in[in_off++]) <<  8;
        v |= ((unsigned) in[in_off++]);

        out[out_off++] = table[(uint8_t)(v >> 18) & 0x3f];
        out[out_off++] = table[(uint8_t)(v >> 12) & 0x3f];
        out[out_off++] = table[(uint8_t)(v >>  6) & 0x3f];
        out[out_off++] = table[(uint8_t)(v      ) & 0x3f];
    }

    /* Process a tail of one or two remaining input bytes.
     * Note one-byte tail corresponds to two characters in output,
     * and two-byte tail to three characters of output. */
    if(in_off + 1 == in_size) {                     /* One-byte tail. */
        v = ((unsigned) in[in_off++]) << 16;
        out[out_off++] = table[(uint8_t)(v >> 18) & 0x3f];
        out[out_off++] = table[(uint8_t)(v >> 12) & 0x3f];

        /* Add two padding chars. */
        if(options->pad) {
            out[out_off++] = options->pad;
            out[out_off++] = options->pad;
        }
    } else if(in_off + 2 == in_size) {              /* Two-byte tail. */
        v = ((unsigned) in[in_off++]) << 16;
        v |= ((unsigned) in[in_off++]) << 8;
        out[out_off++] = table[(uint8_t)(v >> 18) & 0x3f];
        out[out_off++] = table[(uint8_t)(v >> 12) & 0x3f];
        out[out_off++] = table[(uint8_t)(v >>  6) & 0x3f];

        /* Add one padding char. */
        if(options->pad)
            out[out_off++] = options->pad;
    }

    /* Add terminator. */
    if(out_off < out_size)
        out[out_off] = '\0';

    return out_off;
}

int
base64_decode(const char* in_buf, unsigned in_size,
              void* out_buf, unsigned out_size,
              const BASE64_OPTIONS* options)
{
    uint8_t table[256];
    int i;
    const char* in = in_buf;
    uint8_t* out = (uint8_t*) out_buf;
    unsigned out_size_needed;
    unsigned in_off = 0;
    unsigned out_off = 0;
    unsigned v;

    if(options == NULL)
        options = &base64_def_options;

    /* Ignore any padding. */
    if(options->pad != '\0'  &&  in_size > 0  &&  in_size % 4 == 0) {
        /* Valid Base64 can have up to two characters of padding. */
        if(in_buf[in_size - 1] == options->pad)
            in_size--;
        if(in_buf[in_size - 1] == options->pad)
            in_size--;
    }

    /* Build table. */
    memset(table, 0xff, 256);
    for(i = 0; i < 62; i++)
        table[(int) base64_table_core[i]] = i;
    table[(int) options->ch62] = 62;
    table[(int) options->ch63] = 63;

    /* Validate the input. */
    for(in_off = 0; in_off < in_size; in_off++) {
        if(table[(int) in[in_off]] == 0xff)
            return -EINVAL;
    }

    /* Every four characters are decoded into 3 bytes of output.
     * Note that on the end only certain values may appear from encoding
     * a tail of one or two bytes in base64_encode(). */
    out_size_needed = (in_size / 4) * 3;
    switch(in_size % 4) {
        case 0:
            break;
        case 1:
            return -EINVAL;
            break;

        case 2:
            if(table[(int) in[in_size-1]] & 0x4f)
                return -EINVAL;
            out_size_needed += 1;
            break;

        case 3:
            if(table[(int) in[in_size-1]] & 0xc3)
                return -EINVAL;
            out_size_needed += 2;
            break;
    }

    if(out_buf == NULL)
        return out_size_needed;

    /* Check we have enough space in the output. */
    if(out_size < out_size_needed)
        return -ENOBUFS;

    /* Main loop. */
    in_off = 0;
    while(in_off + 4 <= in_size) {
        v  = (unsigned)table[(int) in[in_off++]] << 18;
        v |= (unsigned)table[(int) in[in_off++]] << 12;
        v |= (unsigned)table[(int) in[in_off++]] << 6;
        v |= (unsigned)table[(int) in[in_off++]];

        out[out_off++] = (v >> 16) & 0xff;
        out[out_off++] = (v >>  8) & 0xff;
        out[out_off++] = (v      ) & 0xff;
    }

    if(in_off + 2 == in_size) {
        v  = (unsigned)table[(int) in[in_off++]] << 18;
        v |= (unsigned)table[(int) in[in_off++]] << 12;
        out[out_off++] = (v >> 16) & 0xff;
    } else if(in_off + 3 == in_size) {
        v  = (unsigned)table[(int) in[in_off++]] << 18;
        v |= (unsigned)table[(int) in[in_off++]] << 12;
        v |= (unsigned)table[(int) in[in_off++]] << 6;
        out[out_off++] = (v >> 16) & 0xff;
        out[out_off++] = (v >>  8) & 0xff;
    }

    return out_off;
}
