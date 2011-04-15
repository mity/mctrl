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
#include "ex_grid.h"


static HINSTANCE hInst;
static HWND hwndGrid;


/* Loads some values into the table. Some cells are filled via grid control,
 * some are set directly through table model API after we get a pointer to
 * the table from the grid control. */
static void
LoadGrid(void)
{
    MC_GGEOMETRY geom;
    MC_GCELL cell;
    MC_HTABLE hTable;
    MC_TABLECELL tc;
    
    /* Set size of the table to 8 columns and 16 rows. */
    SendMessage(hwndGrid, MC_GM_RESIZE, MAKEWPARAM(8, 16), 0);

    /* We use custom row headers so we need more space there. */
    geom.fMask = MC_GGF_ROWHEADERWIDTH;
    geom.wRowHeaderWidth = 48;
    SendMessage(hwndGrid, MC_GM_SETGEOMETRY, 0, (LPARAM)&geom);
    
    /* Setup first column which serves as row headers. */
    cell.wCol = 0;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_STRING);
    for(cell.wRow = 0; cell.wRow < 16; cell.wRow++) {
        TCHAR buffer[32];
        _sntprintf(buffer, 32, _T("Row %d"), (int)cell.wRow+1);
        mcValue_CreateFromString(&cell.hValue, buffer);
        SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);
    }
    
    /* Setup few cells and show that the table can contain various kinds
     * of data. */
    cell.wCol = 1;
    cell.wRow = 0;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_IMMSTRING);
    mcValue_CreateFromImmString(&cell.hValue, _T("imm string"));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 1;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_STRING);
    mcValue_CreateFromString(&cell.hValue, _T("string"));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 2;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_COLORREF);
    mcValue_CreateFromColorref(&cell.hValue, RGB(200,0,0));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 2;
    cell.wRow = 2;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_COLORREF);
    mcValue_CreateFromColorref(&cell.hValue, RGB(0,200,0));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 3;
    cell.wRow = 2;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_COLORREF);
    mcValue_CreateFromColorref(&cell.hValue, RGB(0,0,200));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 3;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_INT32);
    mcValue_CreateFromInt32(&cell.hValue, 42);
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 1;
    cell.wRow = 4;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_IMMSTRING);
    mcValue_CreateFromImmString(&cell.hValue, _T("This is very long string which does not fit in the cell."));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);

    cell.wCol = 6;
    cell.wRow = 14;
    cell.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_HICON);
    mcValue_CreateFromHIcon(&cell.hValue, LoadImage(hInst, MAKEINTRESOURCE(IDI_BEAR), IMAGE_ICON, 0, 0, LR_SHARED));
    SendMessage(hwndGrid, MC_GM_SETCELL, 0, (LPARAM)&cell);
    
    /* We can also use API of the table presented by the control directly.
     * first we need a handle of the grid's table. */
    hTable = (MC_HTABLE)SendMessage(hwndGrid, MC_GM_GETTABLE, 0, 0);
    
    tc.fMask = MC_TCM_VALUE | MC_TCM_FLAGS;    
    tc.hType = mcValueType_GetBuiltin(MC_VALUETYPEID_IMMSTRING);
    
    mcValue_CreateFromImmString(&tc.hValue, _T("top left"));
    tc.dwFlags = MC_TCF_ALIGNLEFT | MC_TCF_ALIGNTOP;
    mcTable_SetCellEx(hTable, 4, 6, &tc);

    mcValue_CreateFromImmString(&tc.hValue, _T("top"));
    tc.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNTOP;
    mcTable_SetCellEx(hTable, 5, 6, &tc);

    mcValue_CreateFromImmString(&tc.hValue, _T("top right"));
    tc.dwFlags = MC_TCF_ALIGNRIGHT | MC_TCF_ALIGNTOP;
    mcTable_SetCellEx(hTable, 6, 6, &tc);
    
    mcValue_CreateFromImmString(&tc.hValue, _T("left"));
    tc.dwFlags = MC_TCF_ALIGNLEFT | MC_TCF_ALIGNVCENTER;
    mcTable_SetCellEx(hTable, 4, 7, &tc);
    
    mcValue_CreateFromImmString(&tc.hValue, _T("center"));
    tc.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNVCENTER;
    mcTable_SetCellEx(hTable, 5, 7, &tc);

    mcValue_CreateFromImmString(&tc.hValue, _T("right"));
    tc.dwFlags = MC_TCF_ALIGNRIGHT | MC_TCF_ALIGNVCENTER;
    mcTable_SetCellEx(hTable, 6, 7, &tc);
    
    mcValue_CreateFromImmString(&tc.hValue, _T("bottom left"));
    tc.dwFlags = MC_TCF_ALIGNLEFT | MC_TCF_ALIGNBOTTOM;
    mcTable_SetCellEx(hTable, 4, 8, &tc);
    
    mcValue_CreateFromImmString(&tc.hValue, _T("bottom"));
    tc.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNBOTTOM;
    mcTable_SetCellEx(hTable, 5, 8, &tc);
    
    mcValue_CreateFromImmString(&tc.hValue, _T("bottom right"));
    tc.dwFlags = MC_TCF_ALIGNRIGHT | MC_TCF_ALIGNBOTTOM;
    mcTable_SetCellEx(hTable, 6, 8, &tc);
}


/* Main window procedure */
static LRESULT CALLBACK
win_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_SIZE:
            /* Resize the grid control so it takes all space of the top
             * level window */
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED)
                SetWindowPos(hwndGrid, NULL, 5, 5, LOWORD(lParam)-10, HIWORD(lParam)-10, SWP_NOZORDER);
            return 0;

        case WM_SETFOCUS:
            SetFocus(hwndGrid);
            return 0;
            
        case WM_SETFONT:
            SendMessage(hwndGrid, WM_SETFONT, wParam, lParam);
            return 0;
            
        case WM_CREATE:
            /* Create grid control and fill it with some data */
            hwndGrid = CreateWindowEx(WS_EX_CLIENTEDGE, MC_WC_GRID, _T(""), 
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                    MC_GS_COLUMNHEADERALPHABETIC | MC_GS_ROWHEADERCUSTOM,
                    0, 0, 0, 0, hwnd, (HMENU) IDC_GRID, hInst, NULL);
            LoadGrid();
            return 0;
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = { 0 };
    HWND hwndMain;
    MSG msg;
    
    hInst = hInstance;

    /* Register grid window class */
    mcGrid_Initialize();
    
    /* Prevent linker from ignoring comctl32.dll. */
    InitCommonControls();

    /* Register main window class */
    wc.lpfnWndProc = win_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);
    
    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("mCtrl Example: Grid Control"), 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
        NULL, NULL, hInst, NULL
    );
    SendMessage(hwndMain, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 
            MAKELPARAM(TRUE, 0));
    ShowWindow(hwndMain, nCmdShow);

    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(hwndMain, &msg))
            continue;
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    mcGrid_Terminate();
    
    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
