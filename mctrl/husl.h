/*
 * Copyright (c) 2012 Alexei Boronine
 *  -- Idea, design & algorithm
 *  -- Original CoffeeScript & JavaScript implementation
 *  -- See http://boronine.com/husl/
 *
 * Copyright (c) 2012 Lajos Ambrus
 *  -- C/C++ port
 *  -- See http://github.com/boronine/husl/tree/master/ports/c
 *
 * Copyright (c) 2012 Martin Mitas
 *  -- Minor tweaks & incorporation into mCtrl
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MC_HUSL_H
#define MC_HUSL_H

#include "misc.h"


/* Pass in HUSL values and get back RGB values.
 * H ranges from 0 to 360, S and L from 0 to 100.
 * RGB values will range from 0 to 1.
 */
void husl_to_rgb(float* r, float* g, float* b, float h, float s, float l);

/* Pass in RGB values ranging from 0 to 1 and get back HUSL values.
 * H ranges from 0 to 360, S and L from 0 to 100.
 */
void husl_from_rgb(float* h, float* s, float* l, float r, float g, float b);


#endif  /* MC_HUSL_H */
