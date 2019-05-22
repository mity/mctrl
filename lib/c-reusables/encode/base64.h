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

#ifndef CRE_BASE64_H
#define CRE_BASE64_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct BASE64_OPTIONS {
    char ch62;              /* Default: '+' */
    char ch63;              /* Default: '/' */
    char pad;               /* Default: '='  (use '\0' for no padding) */
#if 0
    /* Not yet implemented. */
    unsigned line_width;    /* Default: 0 (no line wrapping) */
#endif
} BASE64_OPTIONS;


/* Encode a block of bytes into Base64 encoding.
 *
 * Desired Base64 dialect is described by the structure BASE64_OPTIONS.
 * If NULL, default values are used.
 * (See https://en.wikipedia.org/wiki/Base64#Variants_summary_table for summary
 * of wide-spread variants.)
 *
 * Note that multi-line output is not supported for now.
 *
 * If there is enough space in the output buffer, the output is zero-terminated.
 *
 * If out_buf is NULL, the function returns ideal size of output buffer
 * (approx. in_size / 3 * 4 + some space for new lines, padding and zero
 * terminator).
 * If out_buf is not NULL, the function returns count of characters written
 * to the output buffer (excluding the zero-terminator).
 * On error, a negative value is returned.
 */
int base64_encode(const void* in_buf, unsigned in_size,
                      char* out_buf, unsigned out_size,
                      const BASE64_OPTIONS* options);


/* Decode Base64 encoded string into a block of bytes.
 *
 * Expected Base64 dialect is described by the structure BASE64_OPTIONS.
 * If NULL, default values are used.
 *
 * Notes:
 *  - The function accepts any padding specified by BASE64_OPTIONS::pad but
 *    it also tolerates if padding is not present.
 *
 * If out_buf is NULL, the function returns ideal size of output buffer
 * (approx. in_size / 4 * 3).
 * If out_buf is not NULL, the function returns count of bytes written
 * to the output buffer.
 * On error, a negative value is returned.
 */

int base64_decode(const char* in_buf, unsigned in_size,
                      void* out_buf, unsigned out_size,
                      const BASE64_OPTIONS* options);


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_BASE64_H */
