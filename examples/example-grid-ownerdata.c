/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates grid control with virtual table, i.e. grid
 * with style MC_GS_OWNERDATA. */

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>

#include <mCtrl/grid.h>


#define IDC_GRID    100


static HINSTANCE hInst;
static HWND hwndGrid;


static void
HandleNotify(HWND hwndMain, NMHDR* hdr)
{
    switch(hdr->code) {
#if 0
        /* The control may inform us what data for MC_GN_GETDISPINFO are worth
         * of caching. But as "calculating" the data in the way we do it is
         * so cheap operation, we may just omit any caching altogether. */
        case MC_GN_ODCACHEHINT:
            ...
            break;
#endif

        /* As we created the grid with style MC_GS_OWNERDATA, we need to
         * provide a cell data whenever the control asks for them.
         */
        case MC_GN_GETDISPINFO:
        {
            MC_NMGDISPINFO* lpDispInfo = (MC_NMGDISPINFO*) hdr;

            if(lpDispInfo->cell.fMask & MC_TCMF_TEXT) {
                static TCHAR buffer[64];
                _sntprintf(buffer, 64, _T("%d, %d"),
                           (int)lpDispInfo->wColumn+1, (int)lpDispInfo->wRow+1);
                lpDispInfo->cell.pszText = buffer;
            }

            if(lpDispInfo->cell.fMask & MC_TCMF_FLAGS) {
                lpDispInfo->cell.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNVCENTER;
            }
            break;
        }
    }
}

/* Main window procedure */
static LRESULT CALLBACK
MainWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_NOTIFY:
        {
            /* Handle notifications from the grid control. */
            NMHDR* hdr = (NMHDR*) lParam;
            if(hdr->idFrom == IDC_GRID)
                HandleNotify(hwnd, hdr);
            return 0;
        }

        case WM_SIZE:
            /* Resize the grid control so it takes all space of the top
             * level window. */
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
            /* Make it to have square cells. */
            geom.fMask = MC_GGF_DEFCOLUMNWIDTH | MC_GGF_DEFROWHEIGHT;
            SendMessage(hwndGrid, MC_GM_GETGEOMETRY, 0, (LPARAM)&geom);
            geom.wDefRowHeight = geom.wDefColumnWidth;
            SendMessage(hwndGrid, MC_GM_SETGEOMETRY, 0, (LPARAM)&geom);
            return 0;
        }

        case WM_CREATE:
            /* Create grid control. Note we use the style MC_GS_OWNERDATA.
             * Therefore the control does not hold any data and instead it
             * asks us via WM_NOTIFY. */
            hwndGrid = CreateWindowEx(WS_EX_CLIENTEDGE, MC_WC_GRID, _T(""),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | MC_GS_OWNERDATA |
                    MC_GS_COLUMNHEADERNUMBERED | MC_GS_ROWHEADERNUMBERED,
                    0, 0, 0, 0, hwnd, (HMENU) IDC_GRID, hInst, NULL);

            /* Set size of the virtual table to 100 x 100. */
            SendMessage(hwndGrid, MC_GM_RESIZE, MAKEWPARAM(100, 100), 0);
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
    wc.lpfnWndProc = MainWinProc;
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
