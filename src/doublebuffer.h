/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2015-2020 Martin Mitas
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

#ifndef MC_DOUBLEBUFFER_H
#define MC_DOUBLEBUFFER_H

#include "misc.h"


typedef struct doublebuffer_tag doublebuffer_t;
struct doublebuffer_tag {
    HPAINTBUFFER uxtheme_buf;
};

/* Every thread using double buffering should call this.
 * (Usually we do this by calling doublebuffer_init() from control's
 * WM_NCCREATE and doublebuffer_fini() from WM_NCDESTROY). */
void doublebuffer_init(void);
void doublebuffer_fini(void);

/* Simple wrapper for double-buffering controls */
typedef void (*doublebuffer_callback_t)(void* /*control_data*/, HDC /*dc*/,
                                        RECT* /*dirty_rect*/, BOOL /*erase*/);

void doublebuffer(void* control, PAINTSTRUCT* ps, doublebuffer_callback_t callback);


#endif  /* MC_DOUBLEBUFFER_H */
