/*
 * Copyright (c) 2011 Martin Mitas
 *
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been placed in the public domain.
 */

/* This sample demonstrates basic usage of grid control. */
 
#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>

#include <mCtrl/grid.h>

static HINSTANCE inst;
static HWND grid;


static void
load_grid(void)
{
    MC_GGEOMETRY geom;
    MC_GCELL cell;
    
    SendMessage(grid, MC_GM_RESIZE, MAKEWPARAM(8, 16), 0);

    /* Make space for row headers larger for custom headers */
    geom.fMask = MC_GGF_ROWHEADERWIDTH;
    geom.wRowHeaderWidth = 48;
    SendMessage(grid, MC_GM_SETGEOMETRY, 0, (LPARAM)&geom);
    
    /* Setup first column which serves as row headers */
    cell.wCol = 0;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_STRING);
    for(cell.wRow = 0; cell.wRow < 16; cell.wRow++) {
        TCHAR buffer[32];
        _sntprintf(buffer, 32, _T("Row %d"), (int)cell.wRow+1);
        mcValue_CreateFromString(&cell.hValue, buffer);
        SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);
    }
    
    /* Setup few other cells */
    cell.wCol = 1;
    cell.wRow = 0;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_IMMSTRING);
    mcValue_CreateFromImmString(&cell.hValue, _T("imm string"));
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 1;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_STRING);
    mcValue_CreateFromString(&cell.hValue, _T("string"));
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 2;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_COLORREF);
    mcValue_CreateFromColorref(&cell.hValue, RGB(200,0,0));
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 2;
    cell.wRow = 2;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_COLORREF);
    mcValue_CreateFromColorref(&cell.hValue, RGB(0,200,0));
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 3;
    cell.wRow = 2;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_COLORREF);
    mcValue_CreateFromColorref(&cell.hValue, RGB(0,0,200));
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 3;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_INT32);
    mcValue_CreateFromInt32(&cell.hValue, 42);
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 6;
    cell.wRow = 14;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_HICON);
    mcValue_CreateFromHIcon(&cell.hValue, LoadImage(inst, MAKEINTRESOURCE(100), IMAGE_ICON, 0, 0, LR_SHARED));
    SendMessage(grid, MC_GM_SETCELL, 0, (LPARAM)&cell);
}


/* Main window procedure */
static CALLBACK LRESULT
win_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
        case WM_SIZE:
            if(wp == SIZE_RESTORED  ||  wp == SIZE_MAXIMIZED)
                SetWindowPos(grid, NULL, 5, 5, LOWORD(lp)-10, HIWORD(lp)-10, SWP_NOZORDER);
            return 0;

        case WM_SETFOCUS:
            SetFocus(grid);
            return 0;
            
        case WM_SETFONT:
            SendMessage(grid, WM_SETFONT, wp, lp);
            return 0;
            
        case WM_CREATE:
            /* Create grid control */
            grid = CreateWindowEx(WS_EX_CLIENTEDGE, MC_WC_GRID, _T(""), 
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                    MC_GS_COLUMNHEADERALPHABETIC | MC_GS_ROWHEADERCUSTOM,
                    0, 0, 0, 0, win, (HMENU) 100, inst, NULL);
            load_grid();
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
    
    mcGrid_Initialize();
    
    /* Prevent linker from ignoring comctl32.dll */
    InitCommonControls();

    inst = instance;
    
    /* Register main window class */
    wc.lpfnWndProc = win_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);
    
    /* Create main window */
    win = CreateWindow(
        _T("main_window"), _T("mCtrl Example: Grid Control"), 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
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
    
    mcGrid_Terminate();
    
    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
