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

#ifndef CRE_HEX_H
#define CRE_HEX_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Encodes the BLOB in_buf into the trivial hexadecimal notation: Two
 * hexadecimal digits per every byte of in_buf.
 *
 * If there is enough space in out_buf (out_size >= in_size * 2 + 1), 
 * the output is zero-terminated.
 *
 * If the output buffer is too small, the function returns -1.
 * If out_buf is NULL, the function returns ideal size of output buffer
 * (in_size * 2 + 1).
 * If out_buf is not NULL, it returns the number of written hexadecimal digits.
 */
int hex_encode(const void* in_buf, unsigned in_size,
                   char* out_buf, unsigned out_size, int lowercase);

/* Decodes string of hexadecimal notation (even count of digits) to a memory
 * pointed by out_buf. The function accepts both lowercase and uppercase
 * hexadecimal digits on input.
 *
 * If out_buf is NULL, the function returns ideal size of output buffer
 * (in_size / 2).
 * If out_buf is not NULL, it returns the number of written bytes or -1
 * in case of error (e.g. parsing error or too small output buffer).
 */
int hex_decode(const char* in_buf, unsigned in_size,
                   void* out_buf, unsigned out_size);


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_HEX_H */
