/*
 * Copyright (c) 2008-2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This sample demonstrates basic usage of MDITAB control. */

#include <windows.h>
#include <commctrl.h>  /* image list */
#include <tchar.h>
#include <stdio.h>

#include <mCtrl/mditab.h>


static HINSTANCE inst;
static HIMAGELIST img_list;  /* Few cute images for the tabs */
static HWND mditab;          /* MDITAB control */
static HWND btn_new;         /* Button for creating new tabs */


/* Adds new tab */
static void
add_tab(void)
{
    static UINT counter = 0;
    int i, n;
    TCHAR buffer[32];
    mc_MT_ITEM item = {MC_MTIF_TEXT | MC_MTIF_IMAGE, buffer, 0, 0, 0};
    
    /* Setup tab icon and label */
    counter++;
    item.iImage = counter % 11; /* we have 11 icons in the image list */
    _sntprintf(buffer, 32, _T("Tab %u"), counter);
    
    /* Add the new tab as last tab */
    n = SendMessage(mditab, MC_MTM_GETITEMCOUNT, 0, 0);
    i = SendMessage(mditab, MC_MTM_INSERTITEM, (WPARAM) n, (LPARAM) &item);
    
    /* Activate the new tab */
    SendMessage(mditab, MC_MTM_SETCURSEL, (WPARAM) i, 0);
}


/* Main window procedure */
static CALLBACK LRESULT
win_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
        case WM_COMMAND:
            if(LOWORD(wp) == 101) {
                /* We are here after the user has clicked the helper button
                 * to add new tab */
                add_tab();
            }
            break;
        
        case WM_CREATE:
        {
            RECT rect;
            mc_MT_TABWIDTH tw;
            
            GetClientRect(win, &rect);
            
            /* Create mditab child window  */
            mditab = CreateWindow(MC_WC_MDITAB, _T(""),
                WS_CHILD | WS_VISIBLE | MC_MTS_CLOSEONMCLICK, 
                0, 0, rect.right, 30, win, (HMENU) 100, inst, NULL);
                
            /* Set an imagelist */
            SendMessage(mditab, MC_MTM_SETIMAGELIST, 0, (LPARAM) img_list);
            
            /* Change minimal tab width */
            SendMessage(mditab, MC_MTM_GETTABWIDTH, 0, (LPARAM) &tw);
            tw.dwMinWidth += 30;
            SendMessage(mditab, MC_MTM_SETTABWIDTH, 0, (LPARAM) &tw);
                
            /* Create button for creating new tabs */
            btn_new = CreateWindow(_T("BUTTON"), _T("New tab"),
                WS_CHILD | WS_VISIBLE, 10, 250, 80, 24, 
                win, (HMENU) 101, inst, NULL);
            return 0;
        }
        
        case WM_SETFONT:
            SendMessage(mditab, WM_SETFONT, wp, lp);
            SendMessage(btn_new, WM_SETFONT, wp, lp);
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(win, msg, wp, lp);
}


int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE instance_prev, LPSTR cmd_line, int cmd_show)
{
    WNDCLASS wc = { 0 };
    HWND win;
    MSG msg;
    
    mcMditab_Initialize();

    /* Prevent linker from ignoring comctl32.dll */
    InitCommonControls();

    inst = instance;
    
    /* Register main window class */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = win_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);
    
    /* Load image list */
    img_list = ImageList_LoadImage(instance, MAKEINTRESOURCE(100), 16, 1, 
                            RGB(255,0,255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
    
    /* Create main window */
    win = CreateWindow(
            _T("main_window"), _T("mCtrl Example: MDITAB Control"), 
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 
            CW_USEDEFAULT, CW_USEDEFAULT, 500, 310, 
            NULL, NULL, instance, NULL
    );
    SendMessage(win, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 
            MAKELPARAM(TRUE, 0));
    ShowWindow(win, cmd_show);
    
    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(win, &msg))
            continue;
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mcMditab_Terminate();
    
    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
