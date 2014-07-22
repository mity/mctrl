/*
 * Copyright (c) 2012-2013 Martin Mitas
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

#include "color.h"
#include "husl.h"

#include <math.h>


COLORREF
color_seq(UINT index)
{
    float h, s, l;
    float r, g, b;

    /* We generate hue by a method of pie cutting. Initially we cut 3 times
     * (red, green and blue), and then in N-th round we cut 3*(2^N) times,
     * just between each two neighbor cuts from the set of all cuts already
     * made in previous rounds.
     *
     * We also mix up the cuts of each round by cycling between 3 segments
     * of the color cycle (0-120, 120-240 and 240-360 degrees).
     */
    if(index < 3) {
        h = 120.0 * index;
    } else {
        int i, n, base;

        n = 31 - mc_clz((int32_t)(index / 3));
        base = 3 * (1<<n);
        i = index - base;
        h = 180.0/base + (i/3) * (360.0/base) + (i%3) * 120.0;
    }

    s = 80.0 + 20.0 * cosf((float)(unsigned int)(index / 4));
    l = 50.0 + 30.0 * sinf((float)(unsigned int)(index / 3));

    husl_to_rgb(&r, &g, &b, h, s, l);
    return RGB(255.0 * r, 255.0 * g, 255.0 * b);
}

COLORREF
color_hint(COLORREF color)
{
    float r, g, b;
    float h, s, l;

    r = GetRValue(color) / 255.0;
    g = GetGValue(color) / 255.0;
    b = GetBValue(color) / 255.0;

    husl_from_rgb(&h, &s, &l, r, g, b);
    l = (l + 100.0) / 2.0;
    s = s / 2.0;
    husl_to_rgb(&r, &g, &b, h, s, l);
    return RGB(255.0 * r, 255.0 * g, 255.0 * b);
}
