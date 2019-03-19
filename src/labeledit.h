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

#ifndef MC_LABELEDIT_H
#define MC_LABELEDIT_H

#include "misc.h"


typedef void (*labeledit_callback_t)(void* /*data*/, const TCHAR* /*text*/, BOOL /*save*/);


HWND labeledit_start(HWND parent_win, const TCHAR* text, labeledit_callback_t callback, void* callback_data);
void labeledit_end(HWND parent_win, BOOL save);

HWND labeledit_win(HWND parent_win);


#endif  /* MC_LABELEDIT_H */
