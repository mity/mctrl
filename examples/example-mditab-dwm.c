/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of MDITAB control with style
 * MC_MTS_EXTENDWINDOWFRAME. */

#include <windows.h>
#include <commctrl.h>  /* image list */
#include <tchar.h>
#include <stdio.h>
#include <dwmapi.h>

#include <mCtrl/mditab.h>
#include "example-mditab-dwm.h"


static HINSTANCE hInst;
static HIMAGELIST hImgList;  /* Few cute images for the tabs */

static HWND hwndMdiTab;      /* The MDITAB control */
static HWND hwndBtn;         /* A button for creating new tabs */


#define MINIMAL_WIDTH      500   /* Minimal (and also default) window size */
#define MINIMAL_HEIGHT     300


static TCHAR* pszTabNames[] = {
    _T("Lorem"),
    _T("ipsum"),
    _T("dolor"),
    _T("sit"),
    _T("amet"),
    _T("consectetur"),
    _T("adipiscing"),
    _T("elit"),
    _T("Ut"),
    _T("tristique"),
    _T("dui"),
    _T("ex"),
    _T("ut"),
    _T("facilisis"),
    _T("nisl"),
    _T("consequat"),
    _T("sed")
};

static const UINT uTabNames = sizeof(pszTabNames) / sizeof(pszTabNames[0]);


/* Adds new tab */
static void
AddNewTab(void)
{
    static UINT uCounter = 0;
    int i, n;
    MC_MTITEM item;

    /* Setup tab icon and label */
    item.dwMask = MC_MTIF_TEXT | MC_MTIF_IMAGE;
    item.iImage = uCounter % 11; /* we have 11 icons in the image list */
    item.pszText = pszTabNames[uCounter % uTabNames];
    uCounter++;

    /* Add the new tab as last tab */
    n = SendMessage(hwndMdiTab, MC_MTM_GETITEMCOUNT, 0, 0);
    i = SendMessage(hwndMdiTab, MC_MTM_INSERTITEM, (WPARAM) n, (LPARAM) &item);

    /* Activate the new tab */
    SendMessage(hwndMdiTab, MC_MTM_SETCURSEL, (WPARAM) i, 0);
}

/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult;

    /* Let mCtrl to process all the stuff on the extended frame. */
    if(mcMditab_DefWindowProc(hWnd, hwndMdiTab, uMsg, wParam, lParam, &lResult))
        return lResult;

    switch(uMsg) {
        case WM_COMMAND:
            /* Handle clicks to the button: create new tab. */
            if(LOWORD(wParam) == IDC_BUTTON_NEW) {
                AddNewTab();
            }
            break;

        case WM_SIZE:
            if(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) {
                WORD width = LOWORD(lParam);
                WORD height = HIWORD(lParam);

                SetWindowPos(hwndMdiTab, NULL, 0, 0, width, 30, SWP_NOZORDER);
                SetWindowPos(hwndBtn, NULL, 10, height - 34, 80, 24, SWP_NOZORDER);
            }
            break;

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* lpMmi = (MINMAXINFO*) lParam;
            lpMmi->ptMinTrackSize.x = MINIMAL_WIDTH;
            lpMmi->ptMinTrackSize.y = MINIMAL_HEIGHT;
            return 0;
        }

        case WM_CREATE:
            /* Create mditab child window  */
            hwndMdiTab = CreateWindow(MC_WC_MDITAB, _T(""), WS_CHILD | WS_VISIBLE |
                MC_MTS_CLOSEONMCLICK | MC_MTS_DOUBLEBUFFER | MC_MTS_ANIMATE |
                MC_MTS_EXTENDWINDOWFRAME | MC_MTS_DRAGDROP, 0, 0, 0, 0, hWnd,
                (HMENU) IDC_MDITAB, hInst, NULL);

            /* Set an imagelist */
            SendMessage(hwndMdiTab, MC_MTM_SETIMAGELIST, 0, (LPARAM) hImgList);

            /* Create button for creating new tabs */
            hwndBtn = CreateWindow(_T("BUTTON"), _T("New tab"),
                WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                hWnd, (HMENU) IDC_BUTTON_NEW, hInst, NULL);
            return 0;

        case WM_SETFONT:
            SendMessage(hwndMdiTab, WM_SETFONT, wParam, lParam);
            SendMessage(hwndBtn, WM_SETFONT, wParam, lParam);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


#define WINDOW_TITLE        _T("mCtrl Example: MDITAB Control")

int APIENTRY
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    HMODULE hDwmapi;
    HRESULT (WINAPI* funcDwmIsCompositionEnabled)(BOOL*);
    WNDCLASS wc = { 0 };
    HWND hWnd;
    MSG msg;

    mcMditab_Initialize();

    /* Prevent linker from ignoring comctl32.dll */
    InitCommonControls();

    hInst = hInstance;

    /* Check this example can show anything relevant. If not the example
     * provides the same user experience as example-mditab.exe.
     * (Normal apps which do not need this kind of "warning" do not need this
     * at all.) */
    hDwmapi = LoadLibrary(_T("DWMAPI.DLL"));
    if(hDwmapi != NULL) {
        funcDwmIsCompositionEnabled = (HRESULT (WINAPI*)(BOOL*))
                GetProcAddress(hDwmapi, "DwmIsCompositionEnabled");
    } else {
        funcDwmIsCompositionEnabled = NULL;
    }
    if(funcDwmIsCompositionEnabled != NULL) {
        BOOL bEnabled;
        funcDwmIsCompositionEnabled(&bEnabled);
        if(!bEnabled) {
            MessageBox(NULL, _T("DWM composition is disabled. The style ")
                    _T("MC_MTS_EXTENDWINDOWFRAME has no effect."), WINDOW_TITLE,
                    MB_OK | MB_ICONINFORMATION);
        }
    } else {
        MessageBox(NULL, _T("DWM composition is not available. The style ")
                _T("MC_MTS_EXTENDWINDOWFRAME has no effect."), WINDOW_TITLE,
                MB_OK | MB_ICONINFORMATION);
    }
    if(hDwmapi != NULL)
        FreeLibrary(hDwmapi);

    /* Register main window class */
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);

    /* Load image list */
    hImgList = ImageList_LoadImage(hInst, MAKEINTRESOURCE(IDL_IMGLIST),
                    16, 1, RGB(255,0,255), IMAGE_BITMAP, LR_CREATEDIBSECTION);

    /* Create main window */
    hWnd = CreateWindow(_T("main_window"), WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, MINIMAL_WIDTH, MINIMAL_HEIGHT,
            NULL, NULL, hInst, NULL);
    SendMessage(hWnd, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),
            MAKELPARAM(TRUE, 0));
    ShowWindow(hWnd, nCmdShow);

    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(hWnd, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mcMditab_Terminate();

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
