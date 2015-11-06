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

#ifndef MC_MOUSEDRAG_H
#define MC_MOUSEDRAG_H

#include "misc.h"



/* Dragging helpers.
 *
 * Usage:
 * (1) Call mousedrag_set_candidate() on WM_LBUTTONDOWN.
 * (2) If mousedrag_set_candidate() returns TRUE, caller should call
 *     mousedrag_consider_start() on WM_MOUSEMOVE until it gives up, until it is
 *     canceled (mousedrag_CANCELED) or until it is started (mousedrag_STARTED).
 * (3) When started, caller should capture mouse with SetCapture().
 * (4) The dragging holds until mousedrag_release() is called which happens
 *     usually on WM_LBUTTONUP and also must happen on WM_CAPTURECHANGED.
 *
 * Note: mousedrag_set_candidate() may return mousedrag_CANCELED when, in the mean
 * time, some other HWND registered itself as an candidate (and possibly even
 * started its own dragging).
 *
 * Alternatively (if no considering is desired):
 * (1) Call mousedrag_start() on WM_LBUTTONDOWN. It returns mousedrag_STARTED or
 *     mousedrag_CANCELED (never mousedrag_CONSIDERING).
 * (2) When started, caller should capture mouse with SetCapture().
 * (3) The dragging holds until mousedrag_release() is called which happens
 *     usually on WM_LBUTTONUP and also must happen on WM_CAPTURECHANGED.
 */
typedef enum mousedrag_state_tag mousedrag_state_t;
enum mousedrag_state_tag {
    MOUSEDRAG_CANCELED = -1,
    MOUSEDRAG_CONSIDERING = 0,
    MOUSEDRAG_STARTED = 1
};

BOOL mousedrag_set_candidate(HWND win, int start_x, int start_y,
                                     int hotspot_x, int hotspot_y,
                                     int index, UINT_PTR extra);
mousedrag_state_t mousedrag_consider_start(HWND win, int x, int y);
mousedrag_state_t mousedrag_start(HWND win, int start_x, int start_y);
void mousedrag_stop(HWND win);

BOOL mousedrag_lock(HWND win);
void mousedrag_unlock(void);

/* All of the below can be read (and especially written) only when locked
 * with mousedrag_lock(), or when really started the dragging.
 *
 * The mousedrag_start_x and mousedrag_start_y have to be mouse position at the
 * time of mousedrag_set_candidate(), the rest are not interpreted here but serve
 * just as a storage for few values for the caller.
 */
extern int mousedrag_start_x;
extern int mousedrag_start_y;
extern int mousedrag_hotspot_x;
extern int mousedrag_hotspot_y;
extern int mousedrag_index;
extern UINT_PTR mousedrag_extra;


#endif  /* MC_MOUSEDRAG_H */
