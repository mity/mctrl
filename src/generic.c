/*
 * Copyright (c) 2014 Martin Mitas
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

#include "generic.h"
#include "tooltip.h"


LRESULT
generic_ncpaint(HWND win, HTHEME theme, HRGN orig_clip)
{
    HDC dc;
    int edge_h, edge_v;
    RECT rect;
    HRGN clip;
    HRGN tmp;
    LRESULT ret;

    if(theme == NULL)
        return DefWindowProc(win, WM_NCPAINT, (WPARAM)orig_clip, 0);

    edge_h = GetSystemMetrics(SM_CXEDGE);
    edge_v = GetSystemMetrics(SM_CYEDGE);
    GetWindowRect(win, &rect);

    /* Prepare the clip region for DefWindowProc() so that it does not repaint
     * what we do here. */
    if(orig_clip == (HRGN) 1)
        clip = CreateRectRgnIndirect(&rect);
    else
        clip = orig_clip;
    tmp = CreateRectRgn(rect.left + edge_h, rect.top + edge_v,
                        rect.right - edge_h, rect.bottom - edge_v);
    CombineRgn(clip, clip, tmp, RGN_AND);
    DeleteObject(tmp);

    mc_rect_offset(&rect, -rect.left, -rect.top);
    dc = GetWindowDC(win);
    ExcludeClipRect(dc, edge_h, edge_v, rect.right - 2*edge_h, rect.bottom - 2*edge_v);
    if(mcIsThemeBackgroundPartiallyTransparent(theme, 0, 0))
        mcDrawThemeParentBackground(win, dc, &rect);
    mcDrawThemeBackground(theme, dc, 0, 0, &rect, NULL);
    ReleaseDC(win, dc);

    /* Use DefWindowProc() to paint scrollbars */
    ret = DefWindowProc(win, WM_NCPAINT, (WPARAM)clip, 0);
    if(clip != orig_clip)
        DeleteObject(clip);
    return ret;
}

LRESULT
generic_erasebkgnd(HWND win, HTHEME theme, HDC dc)
{
    RECT rect;
    HBRUSH brush;

    GetClientRect(win, &rect);
    brush = mcGetThemeSysColorBrush(theme, COLOR_WINDOW);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
    return TRUE;
}

LRESULT
generic_settooltips(HWND win, HWND* tooltip_storage, HWND tooltip_win, BOOL tracking)
{
    HWND old_tooltip = *tooltip_storage;

    if(old_tooltip != NULL)
        tooltip_uninstall(old_tooltip, win);
    if(tooltip_win != NULL)
        tooltip_install(tooltip_win, win, tracking);

    *tooltip_storage = tooltip_win;
    return (LRESULT) old_tooltip;
}
