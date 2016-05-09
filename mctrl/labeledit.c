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

#include "labeledit.h"


#define LABELEDIT_PROPNAME      _T("mCtrl.labeledit.data")


typedef struct labeledit_data_tag labeledit_data_t;
struct labeledit_data_tag {
    HWND edit_win;
    HWND parent_win;
    labeledit_callback_t callback;
    void* callback_data;
    BOOL want_save;
};

static mc_mutex_t labeledit_mutex;
static labeledit_data_t* labeledit_current = NULL;
static HWND labeledit_current_parent_win = NULL;
static WNDPROC labeledit_orig_proc = NULL;


static void
labeledit_call_callback(labeledit_data_t* data, BOOL save)
{
    TCHAR* buffer = NULL;

    if(data->callback == NULL)
        return;

    if(save) {
        DWORD bufsize;

        bufsize = GetWindowTextLength(data->edit_win) + 1;
        buffer = (TCHAR*) _malloca(sizeof(TCHAR*) * bufsize);
        if(buffer != NULL) {
            GetWindowText(data->edit_win, buffer, bufsize);
        } else {
            MC_TRACE("labeledit_call_callback: _malloca() failed.");
            save = FALSE;
        }
    }

    data->callback(data->callback_data, buffer, save);
    data->callback = NULL;

    if(buffer != NULL)
        _freea(buffer);

    mc_mutex_lock(&labeledit_mutex);
    if(data == labeledit_current) {
        labeledit_current = NULL;
        labeledit_current_parent_win = NULL;
    }
    mc_mutex_unlock(&labeledit_mutex);
}

static LRESULT CALLBACK
labeledit_subclass_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    labeledit_data_t* data;

    switch(msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTALLKEYS;

        case WM_KEYDOWN:
            if(wp == VK_ESCAPE  ||  wp == VK_RETURN) {
                data  = (labeledit_data_t*) GetProp(win, LABELEDIT_PROPNAME);
                labeledit_call_callback(data, (wp == VK_RETURN));
                MC_SEND(win, WM_CLOSE, 0, 0);
                return 0;
            }
            break;

        case WM_NCDESTROY:
            data = RemoveProp(win, LABELEDIT_PROPNAME);
            labeledit_call_callback(data, data->want_save);
            free(data);
            break;
    }

    return CallWindowProc(labeledit_orig_proc, win, msg, wp, lp);
}

HWND
labeledit_start(HWND parent_win, const TCHAR* text,
                labeledit_callback_t callback, void* callback_data)
{
    labeledit_data_t* data;

    data = (labeledit_data_t*) malloc(sizeof(labeledit_data_t));
    if(MC_ERR(data == NULL)) {
        MC_TRACE("labeledit_start: malloc() failed.");
        goto err_malloc;
    }

    data->edit_win = CreateWindow(WC_EDIT, text, WS_CHILD | WS_CLIPSIBLINGS |
            WS_BORDER | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 0, 0, 0, 0,
            parent_win, 0, mc_instance, 0);
    if(MC_ERR(data->edit_win == NULL)) {
        MC_TRACE_ERR("labeledit_start: CreateWindow() failed.");
        goto err_CreateWindow;
    }
    data->parent_win = parent_win;
    data->callback = callback;
    data->callback_data = callback_data;
    data->want_save = FALSE;

    if(MC_ERR(SetProp(data->edit_win, LABELEDIT_PROPNAME, (HANDLE) data) == 0)) {
        MC_TRACE_ERR("labeledit_start: SetProp() failed.");
        goto err_SetProp;
    }

    /* Success. */
    mc_mutex_lock(&labeledit_mutex);
    if(labeledit_current != NULL)
        MC_SEND(labeledit_current->edit_win, WM_CLOSE, 0, 0);

    labeledit_orig_proc = (WNDPROC) SetWindowLongPtr(data->edit_win,
                GWLP_WNDPROC, (DWORD_PTR) labeledit_subclass_proc);
    labeledit_current_parent_win = parent_win;
    labeledit_current = data;
    mc_mutex_unlock(&labeledit_mutex);
    return data->edit_win;

    /* Error paths */
err_SetProp:
    DestroyWindow(data->edit_win);
    return NULL;    /* <-- The data in released in WM_NCDESTROY. */
err_CreateWindow:
    free(data);
err_malloc:
    return NULL;
}

void
labeledit_end(HWND parent_win, BOOL save)
{
    HWND edit_win = NULL;

    if(parent_win == labeledit_current_parent_win) {
        mc_mutex_lock(&labeledit_mutex);
        if(parent_win == labeledit_current_parent_win) {
            labeledit_current->want_save = save;
            edit_win = labeledit_current->edit_win;
        }
        mc_mutex_unlock(&labeledit_mutex);
    }

    if(edit_win != NULL)
        MC_SEND(edit_win, WM_CLOSE, 0, 0);
}

HWND
labeledit_win(HWND parent_win)
{
    HWND edit_win = NULL;

    if(parent_win == labeledit_current_parent_win) {
        mc_mutex_lock(&labeledit_mutex);
        if(parent_win == labeledit_current_parent_win)
            edit_win = labeledit_current->edit_win;
        mc_mutex_unlock(&labeledit_mutex);
    }

    return edit_win;
}

void
labeledit_dllmain_init(void)
{
    mc_mutex_init(&labeledit_mutex);
}

void
labeledit_dllmain_fini(void)
{
    mc_mutex_fini(&labeledit_mutex);
}
