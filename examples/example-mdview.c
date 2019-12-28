/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of MDVIEW control. */

#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>

#include <mCtrl/mdview.h>
#include "example-mdview.h"


#define IDC_MDVIEW    100

static HINSTANCE hInst;
static HWND hwndMdView;



/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_SIZE:
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED) {
                SetWindowPos(hwndMdView, NULL, 5, 5,
                             LOWORD(lParam) - 10, HIWORD(lParam) - 10, SWP_NOZORDER);
            }
            return 0;

        case WM_SETFONT:
            SendMessage(hwndMdView, uMsg, wParam, lParam);
            break;

        case WM_SETFOCUS:
            SetFocus(hwndMdView);
            return 0;

        case WM_CREATE:
            /* Create the tree list view */
            hwndMdView = CreateWindowEx(0, MC_WC_MDVIEW, NULL,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER, 0, 0, 0, 0,
                    hwnd, (HMENU) IDC_MDVIEW, hInst, NULL);
            SendMessage(hwndMdView, MC_MDM_GOTOURL, 0, (LPARAM) _T("res://example-mdview.exe/doc.md"));
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
    NONCLIENTMETRICS ncm;
    HFONT hFont;
    WNDCLASS wc = { 0 };
    HWND hwndMain;
    MSG msg;

    hInst = hInstance;

    /* Register class of MDVIEW control. */
    mcMdView_Initialize();

    /* Prevent linker from ignoring COMCTL32.DLL. We do not use common
     * controls at all in this sample, however MCTRL.DLL enables XP styles
     * in forms for the TREELIST control only if the application uses them,
     * i.e. if the application links against COMCTL32.DLL 6 (or newer) and
     * has the manifest in resources. */
    InitCommonControls();

    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);
    hFont = CreateFontIndirect(&ncm.lfMessageFont);

    /* Register main window class */
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);

    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("mCtrl Example: MDVIEW Control"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 460, 300,
        NULL, NULL, hInst, NULL
    );
    SendMessage(hwndMain, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));
    ShowWindow(hwndMain, nCmdShow);

    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(hwndMain, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mcMdView_Terminate();
    DeleteObject(hFont);

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
