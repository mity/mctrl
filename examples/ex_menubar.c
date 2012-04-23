/*
 * Copyright (c) 2012 Martin Mitas
 *
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been placed in the public domain.
 */

/* This sample demonstrates basic usage of MENUBAR control. */

#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include <mCtrl/menubar.h>
#include "ex_menubar.h"


static HINSTANCE hInst;
static HMENU hMenu;
static HWND hwndRebar;
static HWND hwndMenubar;


/* Create the menubar control */
static void
CreateMenuBar(HWND hWnd)
{
    REBARBANDINFO band = {0};
    DWORD dwBtnSize;

    /* Create ReBar window */
    hwndRebar = CreateWindowEx(WS_EX_TOOLWINDOW, "ReBarWindow32", _T(""), 
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_BORDER |
            CCS_NODIVIDER | CCS_TOP | RBS_VARHEIGHT | RBS_BANDBORDERS | RBS_AUTOSIZE,
            0, 0, 0, 0, hWnd, (HMENU) 1000, hInst, NULL);

    /* Create the menubar control */
    hwndMenubar = CreateWindow(MC_WC_MENUBAR, _T(""), WS_CHILD | WS_VISIBLE |
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NORESIZE | CCS_NOPARENTALIGN,
            0, 0, 0, 0, hwndRebar, (HMENU) 1001, hInst, (LPVOID) hMenu);

    /* Embed the menubar in the ReBar */
    band.cbSize = sizeof(REBARBANDINFO);
    band.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
    band.fStyle = RBBS_GRIPPERALWAYS | RBBS_TOPALIGN | RBBS_USECHEVRON;
    band.hwndChild = hwndMenubar;
    dwBtnSize = (DWORD)SendMessage(band.hwndChild, TB_GETBUTTONSIZE, 0,0);
    band.cyChild = HIWORD(dwBtnSize);
    band.cxMinChild = LOWORD(dwBtnSize);
    band.cyMinChild = HIWORD(dwBtnSize);
    band.cx = 0;
    SendMessage(hwndRebar, RB_INSERTBAND, -1, (LPARAM) &band);
    
    /* Create yet another (empty) ReBar band */
    band.hwndChild = NULL;
    SendMessage(hwndRebar, RB_INSERTBAND, -1, (LPARAM) &band);

    /* Create some dummy child windows, so it can just be seen how focus
     * is handled with the respect to the menubar. */
    CreateWindow("BUTTON", _T("f&oo"), WS_CHILD | WS_TABSTOP | WS_VISIBLE | BS_DEFPUSHBUTTON,
                 10, 60, 100, 25, hWnd, (HMENU) 1002, hInst, NULL);
    CreateWindow("BUTTON", _T("&bar"), WS_CHILD | WS_TABSTOP | WS_VISIBLE | BS_PUSHBUTTON,
                 10, 90, 100, 25, hWnd, (HMENU) 1003, hInst, NULL);
}

/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_COMMAND:
            if(HIWORD(wParam) == 0  &&  lParam == 0) {   /* The message came from the menu bar */
                if(LOWORD(wParam) >= 100) {              /* Ignore std. messages like IDCANCEL et al. */
                    TCHAR buffer[64];
                    _sntprintf(buffer, 64, _T("Received WM_COMMAND from menuitem ID %d."), (int) LOWORD(wParam));
                    MessageBox(hWnd, buffer, _T("Click!"), MB_ICONINFORMATION | MB_OK);
                    return 0;
                }
            }
            
            if(LOWORD(wParam) == 1002 || LOWORD(wParam) == 1003) {
                TCHAR buffer[64];
                _sntprintf(buffer, 64, _T("Hot item: %d"), 
                           (int) SendMessage(hwndMenubar, TB_GETHOTITEM, 0, 0));
                MessageBox(hWnd, buffer, _T("Button!"), MB_ICONINFORMATION | MB_OK);
            }
            break;
            
        case WM_SIZE:
            /* Ensure the ReBar is resized on top of the main window */
            SendMessage(hwndRebar, WM_SIZE, 0, 0);
            break;

        case WM_CREATE:
            CreateMenuBar(hWnd);
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = { 0 };
    HWND hWnd;
    MSG msg;

    hInst = hInstance;
    hMenu = LoadMenu(hInst, MAKEINTRESOURCE(ID_MENU));
    
    /* Initialize mCtrl control */
    mcMenubar_Initialize();
    
    /* Prevent linker from ignoring comctl32.dll */
    InitCommonControls();
    
    /* Register main window class */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);
    
    /* Create main window */
    hWnd = CreateWindow(_T("main_window"), _T("mCtrl Example: MENUBAR Control"), 
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 310,
            NULL, NULL, hInst, NULL);
    ShowWindow(hWnd, nCmdShow);
    
    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(mcIsMenubarMessage(hwndMenubar, &msg))
            continue;
        if(IsDialogMessage(hWnd, &msg))
            continue;
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    mcMenubar_Terminate();
    
    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
