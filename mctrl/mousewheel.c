/*
 * Copyright (c) 2015 Martin Mitas
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

#include "mousewheel.h"


static mc_mutex_t mousewheel_mutex;


int
mousewheel_scroll(HWND win, int delta, int page, BOOL is_vertical)
{
    /* We accumulate the wheel_delta until there is enough to scroll for
     * at least a single line. This improves the feel for strange values
     * of SPI_GETWHEELSCROLLLINES and for some mouses. */
    static HWND last_win = NULL;
    static DWORD last_time[2] = { 0, 0 };   /* horizontal, vertical */
    static int accum_delta[2] = { 0, 0 };   /* horizontal, vertical */

    DWORD now;
    UINT param;
    int dir = (is_vertical ? 1 : 0); /* index into accum_delta[], last_time[] */
    UINT lines_per_WHEEL_DELTA;
    int lines;

    now = GetTickCount();

    if(page < 1)
        page = 1;

    /* Ask the system for scroll speed. */
    param = (is_vertical ? SPI_GETWHEELSCROLLLINES : SPI_GETWHEELSCROLLCHARS);
    if(MC_ERR(!SystemParametersInfo(param, 0, &lines_per_WHEEL_DELTA, 0)))
        lines_per_WHEEL_DELTA = 3;

    /* But never scroll more then complete page. */
    if(lines_per_WHEEL_DELTA == WHEEL_PAGESCROLL  ||  lines_per_WHEEL_DELTA >= page)
        lines_per_WHEEL_DELTA = page;

    mc_mutex_lock(&mousewheel_mutex);

    /* Reset the accumulated value(s) when switching to another window, when
     * changing scrolling direction, or when the wheel was not used for some
     * time. */
    if(win != last_win) {
        last_win = win;
        accum_delta[0] = 0;
        accum_delta[1] = 0;
    } else if((now - last_time[dir] > GetDoubleClickTime() * 2)  ||
              ((delta > 0) == (accum_delta[dir] < 0))) {
        accum_delta[dir] = 0;
    }

    /* Compute lines to scroll. */
    if(lines_per_WHEEL_DELTA > 0) {
        accum_delta[dir] += delta;
        lines = (accum_delta[dir] * (int)lines_per_WHEEL_DELTA) / WHEEL_DELTA;
        accum_delta[dir] -= (lines * WHEEL_DELTA) / (int)lines_per_WHEEL_DELTA;
    } else {
        /* lines_per_WHEEL_DELTA == 0 --> We just don't scroll at all. */
        lines = 0;
        accum_delta[dir] = 0;
    }
    last_time[dir] = now;

    mc_mutex_unlock(&mousewheel_mutex);
    return (is_vertical ? -lines : lines);
}

void
mousewheel_dllmain_init(void)
{
    mc_mutex_init(&mousewheel_mutex);
}

void
mousewheel_dllmain_fini(void)
{
    mc_mutex_fini(&mousewheel_mutex);
}
