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

#ifndef MC_DOUBLEBUFFER_H
#define MC_DOUBLEBUFFER_H

#include "misc.h"
#include "theme.h"


typedef struct doublebuffer_tag doublebuffer_t;
struct doublebuffer_tag {
    HPAINTBUFFER uxtheme_buf;
};

/* Every thread using double buffering should call this.
 * (Usually we do this by calling doublebuffer_init() from control's
 * WM_NCCREATE and doublebuffer_fini() from WM_NCDESTROY). */
void doublebuffer_init(void);
void doublebuffer_fini(void);

HDC doublebuffer_open(doublebuffer_t* dblbuf, HDC dc, const RECT* rect);
void doublebuffer_close(doublebuffer_t* dblbuf, BOOL blit);


/* Simple wrapper for double-buffering controls */

typedef void (*doublebuffer_callback_t)(void* /*control_data*/, HDC /*dc*/,
                                        RECT* /*dirty_rect*/, BOOL /*erase*/);

static inline void
doublebuffer_simple(void* control, PAINTSTRUCT* ps,
                    doublebuffer_callback_t callback)
{
    doublebuffer_t dblbuf;
    HDC dc;

    dc = doublebuffer_open(&dblbuf, ps->hdc, &ps->rcPaint);
    callback(control, dc, &ps->rcPaint, (ps->fErase || dc != ps->hdc));
    doublebuffer_close(&dblbuf, TRUE);
}


#endif  /* MC_DOUBLEBUFFER_H */
