/*
 * Copyright (c) 2019-2020 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MC_MDTEXT_H
#define MC_MDTEXT_H

#include "misc.h"
#include "xd2d.h"
#include "xdwrite.h"


typedef struct mdtext_tag mdtext_t;


#define MDTEXT_FLAG_NOJUSTIFY       0x0001

mdtext_t* mdtext_create(c_IDWriteTextFormat* text_fmt, const TCHAR* text, UINT text_len, UINT width, DWORD flags);
void mdtext_destroy(mdtext_t* mdtext);

void mdtext_set_width(mdtext_t* mdtext, UINT width);

UINT mdtext_min_width(mdtext_t* mdtext);
void mdtext_size(mdtext_t* mdtext, SIZE* size);

void mdtext_paint(mdtext_t* mdtext, c_ID2D1RenderTarget* rt,
                  int x_offset, int y_offset,
                  int portview_y0, int portview_y1);


#endif  /* MC_MDTEXT_H */
