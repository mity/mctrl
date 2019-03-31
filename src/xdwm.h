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

#ifndef MC_XDWM_H
#define MC_XDWM_H

#include "misc.h"


BOOL xdwm_is_composition_enabled(void);

void xdwm_extend_frame(HWND win, int margin_left, int margin_top,
                                int margin_right, int margin_bottom);

BOOL xdwm_def_window_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp, LRESULT* res);


#endif  /* MC_XDWM_H */
