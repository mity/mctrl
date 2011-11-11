/*
 * Copyright (c) 2011 Martin Mitas
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

#include "embedit.h"


const TCHAR embedit_wc[] = _T("mCtrl.embEdit");
static WNDPROC orig_edit_proc = NULL;


static void
embedit_close(HWND win, WORD command_code)
{
    SendMessage(GetParent(win), WM_COMMAND, 
                MAKEWPARAM(GetDlgCtrlID(win), command_code), (LPARAM)win);
    DestroyWindow(win);
}

static LRESULT CALLBACK
embedit_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTALLKEYS;

        case WM_KILLFOCUS:
            embedit_close(win, EEN_APPLY);
            break;
        
        case WM_KEYDOWN:
            switch((int)wp) {
                case VK_ESCAPE:  embedit_close(win, EEN_APPLY); return 0;
                case VK_RETURN:  embedit_close(win, EEN_CANCEL); return 0;
                default:         break;
            }
            break;        
    }
    
    return CallWindowProc(orig_edit_proc, win, msg, wp, lp);
}

int
embedit_init(void)
{
    WNDCLASS wc;
    
    mc_init_common_controls(ICC_STANDARD_CLASSES);
    
    if(MC_ERR(!GetClassInfo(NULL, _T("EDIT"), &wc))) {
        MC_TRACE("embedit_init: GetClassInfo() failed [%lu].", GetLastError());
        return -1;
    }
    
    orig_edit_proc = wc.lpfnWndProc;
    
    wc.style &= ~CS_GLOBALCLASS;
    wc.lpfnWndProc = embedit_proc;
    wc.hInstance = mc_instance;
    wc.lpszClassName = embedit_wc;
    
    if(MC_ERR(!RegisterClass(&wc))) {
        MC_TRACE("embedit_init: RegisterClass() failed [%lu].",
                 GetLastError());
        return -1;
    }
    
    return 0;
}

void
embedit_fini(void)
{
    UnregisterClass(embedit_wc, mc_instance);
}
