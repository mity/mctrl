/*
 * Copyright (c) 2019 Martin Mitas
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

#ifndef MC_XDWRITE_H
#define MC_XDWRITE_H

#include "misc.h"
#include "c-dwrite.h"


c_IDWriteTextFormat* xdwrite_create_text_format(HFONT gdi_font, c_DWRITE_FONT_METRICS* p_metrics);


/* FLags for xdwrite_create_text_layout() */
#define XDWRITE_ALIGN_LEFT      0x0000
#define XDWRITE_ALIGN_CENTER    0x0001
#define XDWRITE_ALIGN_RIGHT     0x0002
#define XDWRITE_VALIGN_TOP      0x0000
#define XDWRITE_VALIGN_CENTER   0x0004
#define XDWRITE_VALIGN_BOTTOM   0x0008
#define XDWRITE_ELLIPSIS_NONE   0x0000
#define XDWRITE_ELLIPSIS_END    0x0010
#define XDWRITE_ELLIPSIS_WORD   0x0020
#define XDWRITE_ELLIPSIS_PATH   0x0040
#define XDWRITE_NOWRAP          0x0100

#define XDWRITE_ALIGN_MASK      (XDWRITE_ALIGN_LEFT | XDWRITE_ALIGN_CENTER | XDWRITE_ALIGN_RIGHT)
#define XDWRITE_VALIGN_MASK     (XDWRITE_VALIGN_TOP | XDWRITE_VALIGN_CENTER | XDWRITE_VALIGN_BOTTOM)
#define XDWRITE_ELLIPSIS_MASK   (XDWRITE_ELLIPSIS_NONE | XDWRITE_ELLIPSIS_END | XDWRITE_ELLIPSIS_WORD | XDWRITE_ELLIPSIS_PATH)

c_IDWriteTextLayout* xdwrite_create_text_layout(const TCHAR* str, UINT len,
            c_IDWriteTextFormat* tf, float max_width, float max_height, DWORD flags);


#endif  /* MC_XDWRITE_H */
