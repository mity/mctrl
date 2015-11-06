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

#include "mousedrag.h"


static CRITICAL_SECTION mousedrag_cs;

static BOOL mousedrag_running = FALSE;
static HWND mousedrag_win = NULL;
int mousedrag_start_x;
int mousedrag_start_y;
int mousedrag_hotspot_x;
int mousedrag_hotspot_y;
int mousedrag_index;
UINT_PTR mousedrag_extra;

BOOL
mousedrag_set_candidate(HWND win, int start_x, int start_y,
                      int hotspot_x, int hotspot_y, int index, UINT_PTR extra)
{
    BOOL ret = FALSE;

    EnterCriticalSection(&mousedrag_cs);
    if(!mousedrag_running) {
        mousedrag_win = win;
        mousedrag_start_x = start_x;
        mousedrag_start_y = start_y;
        mousedrag_hotspot_x = hotspot_x;
        mousedrag_hotspot_y = hotspot_y;
        mousedrag_index = index;
        mousedrag_extra = extra;
        ret = TRUE;
    } else {
        /* Dragging (of different window?) is already running. Actually this
         * should happen normally only if the two windows live in different
         * threads, because the mousedrag_win should have the mouse captured. */
        MC_ASSERT(mousedrag_win != NULL);
        MC_ASSERT(GetWindowThreadProcessId(win, NULL) != GetWindowThreadProcessId(mousedrag_win, NULL));
    }
    LeaveCriticalSection(&mousedrag_cs);

    return ret;
}

mousedrag_state_t
mousedrag_consider_start(HWND win, int x, int y)
{
    mousedrag_state_t ret;

    EnterCriticalSection(&mousedrag_cs);
    if(!mousedrag_running  &&  win == mousedrag_win) {
        int drag_cx, drag_cy;
        RECT rect;

        drag_cx = GetSystemMetrics(SM_CXDRAG);
        drag_cy = GetSystemMetrics(SM_CYDRAG);

        rect.left = mousedrag_start_x - drag_cx;
        rect.top = mousedrag_start_y - drag_cy;
        rect.right = mousedrag_start_x + drag_cx + 1;
        rect.bottom = mousedrag_start_y + drag_cy + 1;

        if(!mc_rect_contains_xy(&rect, x, y)) {
            mousedrag_running = TRUE;
            ret = MOUSEDRAG_STARTED;
        } else {
            /* Still not clear whether we should start the dragging. Maybe
             * next WM_MOUSEMOVE will finally decide it. */
            ret = MOUSEDRAG_CONSIDERING;
        }
    } else {
        ret = MOUSEDRAG_CANCELED;
    }
    LeaveCriticalSection(&mousedrag_cs);

    return ret;
}

mousedrag_state_t
mousedrag_start(HWND win, int start_x, int start_y)
{
    mousedrag_state_t ret;

    EnterCriticalSection(&mousedrag_cs);
    if(!mousedrag_running) {
        mousedrag_running = TRUE;
        mousedrag_win = win;
        mousedrag_start_x = start_x;
        mousedrag_start_y = start_y;
        ret = MOUSEDRAG_STARTED;
    } else {
        ret = MOUSEDRAG_CANCELED;
    }
    LeaveCriticalSection(&mousedrag_cs);

    return ret;
}

void
mousedrag_stop(HWND win)
{
    LeaveCriticalSection(&mousedrag_cs);
    MC_ASSERT(mousedrag_running);
    MC_ASSERT(win == mousedrag_win);
    mousedrag_running = FALSE;
    LeaveCriticalSection(&mousedrag_cs);
}

BOOL
mousedrag_lock(HWND win)
{
    if(win != mousedrag_win)
        return FALSE;

    EnterCriticalSection(&mousedrag_cs);
    if(win != mousedrag_win) {
        LeaveCriticalSection(&mousedrag_cs);
        return FALSE;
    }

    return TRUE;
}

void
mousedrag_unlock(void)
{
    LeaveCriticalSection(&mousedrag_cs);
}

void
mousedrag_dllmain_init(void)
{
    InitializeCriticalSection(&mousedrag_cs);
}

void
mousedrag_dllmain_fini(void)
{
    DeleteCriticalSection(&mousedrag_cs);
}
