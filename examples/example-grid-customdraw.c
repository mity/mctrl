/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates grid control with some customized painting. */

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>

#include <mCtrl/grid.h>


#define IDC_GRID    100


static HINSTANCE hInst;
static HWND hwndGrid;


/* Handling of NM_CUSTOMDRAW notification. We customize some cells to show
 * several ways how the control may be customized. */
static LRESULT
HandleCustomDraw(HWND hwndMain, MC_NMGCUSTOMDRAW* pCD)
{
    switch(pCD->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            /* Tell the control we are interested in per-item notifications.
             * (We need it just to tell the control we want per-subitem
             * notifications.) */
            return CDRF_DODEFAULT | CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
        {
            WORD col = LOWORD(pCD->nmcd.dwItemSpec);
            WORD row = HIWORD(pCD->nmcd.dwItemSpec);

            /* For all column and row header, use different font */
            if(col == MC_TABLE_HEADER || row == MC_TABLE_HEADER) {
                SelectObject(pCD->nmcd.hdc, GetStockObject(OEM_FIXED_FONT));
                return CDRF_DODEFAULT | CDRF_NEWFONT;
            }

            /* Make a chessboard effect in a top left corner of the grid. */
            if(col < 5 && row < 5) {
                if(col % 2 == row % 2) {
                    pCD->clrText = RGB(255,255,255);
                    pCD->clrTextBk = RGB(95,95,95);
                }
                return CDRF_DODEFAULT;
            }

            /* Paint bottom right corner of the grid on our own. */
            if(col >= 10 && row >= 10) {
                MoveToEx(pCD->nmcd.hdc, pCD->nmcd.rc.left, pCD->nmcd.rc.top, NULL);
                LineTo(pCD->nmcd.hdc, pCD->nmcd.rc.right, pCD->nmcd.rc.bottom);
                MoveToEx(pCD->nmcd.hdc, pCD->nmcd.rc.left, pCD->nmcd.rc.bottom, NULL);
                LineTo(pCD->nmcd.hdc, pCD->nmcd.rc.right, pCD->nmcd.rc.top);
                return CDRF_SKIPDEFAULT;
            }

            /* We let the control to paint all the other cells in a normal way. */
            return CDRF_DODEFAULT;
        }

        default:
            /* For all unhandled cases, we let the control do the default. */
            return CDRF_DODEFAULT;
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
            if(hdr->idFrom == IDC_GRID  &&  hdr->code == NM_CUSTOMDRAW)
                return HandleCustomDraw(hwnd, (MC_NMGCUSTOMDRAW*) hdr);
            break;
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
            SendMessage(hwndGrid, WM_SETFONT, wParam, lParam);
            /* Reset grid's geometry to defaults according to the font. */
            SendMessage(hwndGrid, MC_GM_SETGEOMETRY, 0, 0);
            return 0;
        }

        case WM_CREATE:
        {
            WORD col, row;
            MC_TABLECELL cell;
            TCHAR buffer[8];

            /* Create grid control. Note we use the style MC_GS_OWNERDATA.
             * Therefore the control does not hold any data and instead it
             * asks us via WM_NOTIFY. */
            hwndGrid = CreateWindowEx(WS_EX_CLIENTEDGE, MC_WC_GRID, _T(""),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                    MC_GS_COLUMNHEADERNUMBERED | MC_GS_ROWHEADERNUMBERED,
                    0, 0, 0, 0, hwnd, (HMENU) IDC_GRID, hInst, NULL);

            /* Set size of the table. */
            SendMessage(hwndGrid, MC_GM_RESIZE, MAKEWPARAM(15, 15), 0);

            /* Fill the table with some contents. */
            cell.fMask = MC_TCMF_TEXT | MC_TCMF_FLAGS;
            cell.pszText = buffer;
            cell.dwFlags = MC_TCF_ALIGNCENTER | MC_TCF_ALIGNVCENTER;
            for(row = 0; row < 15; row++) {
                for(col = 0; col < 15; col++) {
                    _sntprintf(buffer, 8, _T("[%d, %d]"), (int) col+1, (int) row+1);
                    SendMessage(hwndGrid, MC_GM_SETCELL, MAKEWPARAM(col, row), (LPARAM) &cell);
                }
            }

            return 0;
        }

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
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 350,
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
