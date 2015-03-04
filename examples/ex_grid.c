/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
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
    MC_HTABLE hTable;
    MC_TABLECELL tc;
    TCHAR buffer[32];
    int row;
    int height;

    /* Set size of the table to 8 columns and 16 rows. */
    SendMessage(hwndGrid, MC_GM_RESIZE, MAKEWPARAM(8, 16), 0);

    /* Setup first column which serves as row headers. This is due the style
     * MC_GS_ROWHEADERNORMAL. */
    tc.fMask = MC_TCMF_TEXT;
    tc.pszText = buffer;
    for(row = 0; row < 16; row++) {
        _sntprintf(buffer, 32, _T("Row %d"), row+1);
        SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(MC_TABLE_HEADER, row), (LPARAM) &tc);
    }

    /* Set a text of some cell */
    tc.fMask = MC_TCMF_TEXT;
    tc.pszText = buffer;
    _sntprintf(buffer, 32, _T("Hello world!"));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 1), (LPARAM) &tc);

    /* Set few other cells to various value types (see MC_HVALUE) */
    tc.fMask = MC_TCMF_VALUE;
    tc.hValue = mcValue_CreateImmString(_T("imm string"));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 3), (LPARAM) &tc);
    tc.hValue = mcValue_CreateString(_T("string"));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 4), (LPARAM) &tc);
    tc.hValue = mcValue_CreateColor(RGB(200, 0, 0));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 5), (LPARAM) &tc);
    tc.hValue = mcValue_CreateColor(RGB(0, 200, 0));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(2, 5), (LPARAM) &tc);
    tc.hValue = mcValue_CreateColor(RGB(0, 0, 200));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(3, 5), (LPARAM) &tc);
    tc.hValue = mcValue_CreateInt32(42);
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 6), (LPARAM) &tc);
    tc.hValue = mcValue_CreateIcon(LoadImage(hInst, MAKEINTRESOURCE(IDI_BEAR),
                                   IMAGE_ICON, 0, 0, LR_SHARED));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 7), (LPARAM) &tc);
    tc.hValue = mcValue_CreateImmString(_T("This is very long string ")
                                        _T("which does not fit in the cell."));
    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(1, 8), (LPARAM) &tc);


    /* We can also get the data model of the grid control and manipulate it
     * directly. */
    hTable = (MC_HTABLE) SendMessage(hwndGrid, MC_GM_GETTABLE, 0, 0);
    tc.fMask = MC_TCMF_VALUE | MC_TCMF_FLAGS;
    tc.hValue = mcValue_CreateImmString(_T("top left"));
    tc.dwFlags = MC_TCF_ALIGNLEFT | MC_TCF_ALIGNTOP;
    mcTable_SetCell(hTable, 4, 10, &tc);
    tc.hValue = mcValue_CreateImmString(_T("top center"));
    tc.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNTOP;
    mcTable_SetCell(hTable, 5, 10, &tc);
    tc.hValue = mcValue_CreateImmString(_T("top right"));
    tc.dwFlags = MC_TCF_ALIGNRIGHT | MC_TCF_ALIGNTOP;
    mcTable_SetCell(hTable, 6, 10, &tc);
    tc.hValue = mcValue_CreateImmString(_T("middle left"));
    tc.dwFlags = MC_TCF_ALIGNLEFT | MC_TCF_ALIGNVCENTER;
    mcTable_SetCell(hTable, 4, 11, &tc);
    tc.hValue = mcValue_CreateImmString(_T("middle center"));
    tc.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNVCENTER;
    mcTable_SetCell(hTable, 5, 11, &tc);
    tc.hValue = mcValue_CreateImmString(_T("middle right"));
    tc.dwFlags = MC_TCF_ALIGNRIGHT | MC_TCF_ALIGNVCENTER;
    mcTable_SetCell(hTable, 6, 11, &tc);
    tc.hValue = mcValue_CreateImmString(_T("bottom left"));
    tc.dwFlags = MC_TCF_ALIGNLEFT | MC_TCF_ALIGNBOTTOM;
    mcTable_SetCell(hTable, 4, 12, &tc);
    tc.hValue = mcValue_CreateImmString(_T("bottom center"));
    tc.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNBOTTOM;
    mcTable_SetCell(hTable, 5, 12, &tc);
    tc.hValue = mcValue_CreateImmString(_T("bottom right"));
    tc.dwFlags = MC_TCF_ALIGNRIGHT | MC_TCF_ALIGNBOTTOM;
    mcTable_SetCell(hTable, 6, 12, &tc);

    /* Set heights of few rows to a different value. */
    height = LOWORD(SendMessage(hwndGrid, MC_GM_GETROWHEIGHT, 10, 0));
    height *= 2;
    SendMessage(hwndGrid, MC_GM_SETROWHEIGHT, 10, MAKEWPARAM(height, 0));
    SendMessage(hwndGrid, MC_GM_SETROWHEIGHT, 11, MAKEWPARAM(height, 0));
    SendMessage(hwndGrid, MC_GM_SETROWHEIGHT, 12, MAKEWPARAM(height, 0));
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
        {
            MC_GGEOMETRY geom;

            SendMessage(hwndGrid, WM_SETFONT, wParam, lParam);
            /* Reset grid's geometry to defaults according to the font. */
            SendMessage(hwndGrid, MC_GM_SETGEOMETRY, 0, 0);
            /* Make it to use a bit more space. */
            geom.fMask = MC_GGF_ROWHEADERWIDTH | MC_GGF_DEFCOLUMNWIDTH;
            SendMessage(hwndGrid, MC_GM_GETGEOMETRY, 0, (LPARAM)&geom);
            geom.wRowHeaderWidth = 50;
            geom.wDefColumnWidth += geom.wDefColumnWidth / 2;
            SendMessage(hwndGrid, MC_GM_SETGEOMETRY, 0, (LPARAM)&geom);
            return 0;
        }

        case WM_CREATE:
            /* Create grid control and fill it with some data */
            hwndGrid = CreateWindowEx(WS_EX_CLIENTEDGE, MC_WC_GRID, _T(""),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                    MC_GS_COLUMNHEADERALPHABETIC | MC_GS_ROWHEADERNORMAL,
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
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
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
        _T("main_window"), _T("mCtrl Example: GRID Control"),
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
