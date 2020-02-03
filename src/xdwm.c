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

#include "xdwm.h"

#include <dwmapi.h>


static HMODULE xdwm_dll;

static HRESULT (WINAPI* xdwm_DwmIsCompositionEnabled)(BOOL*);
static HRESULT (WINAPI* xdwm_DwmExtendFrameIntoClientArea)(HWND, const MARGINS*);
static BOOL    (WINAPI* xdwm_DwmDefWindowProc)(HWND, UINT, WPARAM, LPARAM, LRESULT*);


BOOL
xdwm_is_composition_enabled(void)
{
    HRESULT hr;
    BOOL enabled;

    if(xdwm_DwmIsCompositionEnabled == NULL)
        return FALSE;

    hr = xdwm_DwmIsCompositionEnabled(&enabled);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xdwm_is_composition_enabled: DwmIsCompositionEnabled().");
        return FALSE;
    }

    return enabled;
}

void
xdwm_extend_frame(HWND win, int margin_left, int margin_top,
                           int margin_right, int margin_bottom)
{
    MARGINS margins = { margin_left, margin_right, margin_top, margin_bottom };

    MC_ASSERT(xdwm_DwmExtendFrameIntoClientArea != NULL);
    xdwm_DwmExtendFrameIntoClientArea(win, &margins);
}

BOOL
xdwm_def_window_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp, LRESULT* res)
{
    if(xdwm_DwmDefWindowProc == NULL)
        return FALSE;

    return xdwm_DwmDefWindowProc(win, msg, wp, lp, res);
}


int
xdwm_init_module(void)
{
    xdwm_dll = mc_load_sys_dll(_T("DWMAPI.DLL"));
    if(xdwm_dll == NULL)
        goto done;

    xdwm_DwmIsCompositionEnabled = (HRESULT (WINAPI*)(BOOL*))
                GetProcAddress(xdwm_dll, "DwmIsCompositionEnabled");
    if(MC_ERR(xdwm_DwmIsCompositionEnabled == NULL)) {
        MC_TRACE_ERR("xdwm_init_module: GetProcAddress(DwmIsCompositionEnabled) failed.");
        goto no_dwm;
    }
    xdwm_DwmExtendFrameIntoClientArea = (HRESULT (WINAPI*)(HWND, const MARGINS*))
                GetProcAddress(xdwm_dll, "DwmExtendFrameIntoClientArea");
    if(MC_ERR(xdwm_DwmExtendFrameIntoClientArea == NULL)) {
        MC_TRACE_ERR("xdwm_init_module: GetProcAddress(DwmExtendFrameIntoClientArea) failed.");
        goto no_dwm;
    }
    xdwm_DwmDefWindowProc = (BOOL (WINAPI*)(HWND, UINT, WPARAM, LPARAM, LRESULT*))
                GetProcAddress(xdwm_dll, "DwmDefWindowProc");
    if(MC_ERR(xdwm_DwmDefWindowProc == NULL)) {
        MC_TRACE_ERR("xdwm_init_module: GetProcAddress(DwmDefWindowProc) failed.");
        goto no_dwm;
    }

    /* Success. */
    return 0;

    /* "Error" path. */
no_dwm:
    xdwm_DwmIsCompositionEnabled = NULL;
    xdwm_DwmExtendFrameIntoClientArea = NULL;
    xdwm_DwmDefWindowProc = NULL;
    FreeLibrary(xdwm_dll);
    xdwm_dll = NULL;
done:
    return 0;
}

void
xdwm_fini_module(void)
{
    if(xdwm_dll != NULL)
        FreeLibrary(xdwm_dll);
}
