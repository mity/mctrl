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

#include "tooltip.h"


HWND
tooltip_create(HWND control_win, HWND notify_win, BOOL tracking)
{
    HWND tooltip_win;

    tooltip_win = CreateWindow(TOOLTIPS_CLASS, NULL, WS_POPUP, 0, 0, 0, 0,
                               control_win, NULL, NULL, NULL);
    if(MC_ERR(tooltip_win == NULL)) {
        MC_TRACE_ERR("tooltip_create: CreateWindow() failed.");
        return NULL;
    }

    if(notify_win != NULL) {
        NMTOOLTIPSCREATED ttc = { { 0 }, 0 };

        ttc.hdr.hwndFrom = control_win;
        ttc.hdr.idFrom = GetWindowLong(control_win, GWL_ID);
        ttc.hdr.code = NM_TOOLTIPSCREATED;
        ttc.hwndToolTips = tooltip_win;
        MC_SEND(notify_win, WM_NOTIFY, ttc.hdr.idFrom, &ttc);
    }

    tooltip_install(tooltip_win, control_win, tracking);

    return tooltip_win;
}

void
tooltip_destroy(HWND tooltip_win)
{
    DestroyWindow(tooltip_win);
}

void
tooltip_install(HWND tooltip_win, HWND control_win, BOOL tracking)
{
    TTTOOLINFO info = { 0 };

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_TRANSPARENT | TTF_IDISHWND;
    if(tracking)
        info.uFlags = TTF_TRACK | TTF_ABSOLUTE;
    info.uId = (UINT_PTR) control_win;
    info.hwnd = control_win;
    MC_SEND(tooltip_win, TTM_ADDTOOL, 0, &info);
}

void
tooltip_uninstall(HWND tooltip_win, HWND control_win)
{
    TTTOOLINFO info = { 0 };

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_IDISHWND;
    info.uId = (UINT_PTR) control_win;
    info.hwnd = control_win;
    MC_SEND(tooltip_win, TTM_DELTOOL, 0, &info);
}

void
tooltip_forward_msg(HWND tooltip_win, HWND control_win, UINT msg, WPARAM wp, LPARAM lp)
{
    /* Accordingly to docs of TTM_RELAYEVENT, only these messages are really
     * needed. */
    if(msg == WM_LBUTTONDOWN  ||  msg == WM_LBUTTONUP  ||
       msg == WM_MBUTTONDOWN  ||  msg == WM_MBUTTONUP  ||
       msg == WM_RBUTTONDOWN  ||  msg == WM_RBUTTONUP  ||
       msg == WM_MOUSEMOVE)
    {
        DWORD pos;
        MSG m;

        pos = GetMessagePos();

        m.hwnd = control_win;
        m.message = msg;
        m.wParam = wp;
        m.lParam = lp;
        m.time = GetMessageTime();
        m.pt.x = LOWORD(pos);
        m.pt.y = HIWORD(pos);
        MC_SEND(tooltip_win, TTM_RELAYEVENT, 0, &m);
    }
}

void
tooltip_show_tracking(HWND tooltip_win, HWND control_win, BOOL show)
{
    TTTOOLINFO info = { 0 };

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_IDISHWND;
    info.uId = (UINT_PTR) control_win;
    info.hwnd = control_win;
    MC_SEND(tooltip_win, TTM_TRACKACTIVATE, show, &info);
}

void
tooltip_move_tracking(HWND tooltip_win, HWND control_win, int x, int y)
{
    POINT pt;

    pt.x = x;
    pt.y = y;
    ClientToScreen(control_win, &pt);
    MC_SEND(tooltip_win, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x, pt.y));
}

void
tooltip_update_text(HWND tooltip_win, HWND control_win, const TCHAR* str)
{
    TTTOOLINFO info = { 0 };

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_IDISHWND;
    info.uId = (UINT_PTR) control_win;
    info.hwnd = control_win;
    info.lpszText = (TCHAR*) str;
    MC_SEND(tooltip_win, TTM_UPDATETIPTEXT, 0, &info);
}

void
tooltip_size(HWND tooltip_win, HWND control_win, SIZE* size)
{
    TTTOOLINFO info = { 0 };
    DWORD sz;

    info.cbSize = sizeof(TTTOOLINFO);
    info.uFlags = TTF_IDISHWND;
    info.uId = (UINT_PTR) control_win;
    info.hwnd = control_win;
    sz = MC_SEND(tooltip_win, TTM_GETBUBBLESIZE, 0, &info);
    size->cx = LOWORD(sz);
    size->cy = HIWORD(sz);
}

