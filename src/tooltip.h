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

#ifndef MC_TOOLTIP_H
#define MC_TOOLTIP_H

#include "misc.h"


/* Notes:
 *  - tooltip_create() calls tooltip_install() automatically.
 *  - notify_win (if not NULL), it gets NM_TOOLTIPSCREATED) but all
 *    notifications from the tooltip itself are sent to the control_win.
 */
HWND tooltip_create(HWND control_win, HWND notify_win, BOOL tracking);
void tooltip_destroy(HWND tooltip_win);

/* Add/remove a tooltip tool for complete control_win. */
void tooltip_install(HWND tooltip_win, HWND control_win, BOOL tracking);
void tooltip_uninstall(HWND tooltip_win, HWND control_win);

/* Functions to be used only for tracking tooltip. */
void tooltip_show_tracking(HWND tooltip_win, HWND control_win, BOOL show);
void tooltip_move_tracking(HWND tooltip_win, HWND control_win, int x, int y);

/* str may be LPSTR_TEXTCALLBACK if control_win handles TTN_GETDISPINFO. */
void tooltip_update_text(HWND tooltip_win, HWND control_win, const TCHAR* str);

/* Get current tooltip window size. */
void tooltip_size(HWND tooltip_win, SIZE* size);


#endif  /* MC_TOOLTIP_H */
