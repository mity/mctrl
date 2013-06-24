/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of PROPVIEW control. */

#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>

#include <mCtrl/propview.h>


#define IDC_PROPVIEW    100

static HINSTANCE hInst;
static HWND hwndPropView;


/* Fills the PROPVIEW control with some contents */
static void
SetupPropView(void)
{
    int i;
    TCHAR buffer[32];
    MC_PROPSETITEM item;

    /* Insert few items */
    item.fMask = MC_PSIMF_TEXT | MC_PSIMF_VALUE;
    srand(GetTickCount());
    for(i = 0; i < 16; i++) {
        item.iItem = i;
        _sntprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), _T("Value %d"), i+1);
        item.pszText = buffer;
        item.hValue = mcValue_CreateInt32(rand() % 200);
        SendMessage(hwndPropView, MC_PVM_INSERTITEM, 0, (LPARAM) &item);
    }
}


/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_SIZE:
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED) {
                SetWindowPos(hwndPropView, NULL, 0, 0,
                             LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
            }
            return 0;

        case WM_SETFONT:
            SendMessage(hwndPropView, uMsg, wParam, lParam);
            break;

        case WM_SETFOCUS:
            SetFocus(hwndPropView);
            return 0;

        case WM_CREATE:
            /* Create the property list view */
            hwndPropView = CreateWindow(MC_WC_PROPVIEW, NULL,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
                    (HMENU) IDC_PROPVIEW, hInst, NULL);
            SetupPropView();
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

    /* Register class of PROPVIEW control. */
    mcPropView_Initialize();

    /* Prevent linker from ignoring COMCTL32.DLL. We do not use common
     * controls at all in this sample, however MCTRL.DLL enables XP styles
     * in forms for the PROPVIEW control only if the application uses them,
     * i.e. if the application links against COMCTL32.DLL 6 (or newer) and
     * has the manifest in resources. */
    InitCommonControls();

    /* Register main window class */
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);

    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("mCtrl Example: PROPVIEW Control"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 200, 300,
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

    mcPropView_Terminate();

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
