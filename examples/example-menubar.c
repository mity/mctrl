/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of MENUBAR control. */

#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include <mCtrl/menubar.h>
#include "example-menubar.h"


static HINSTANCE hInst;

static HMENU hMenu;
static HWND hwndRebar;
static HWND hwndMenubar;

static HMENU hMenuSm;
static HWND hwndMenubarSmall;


#define BAND_MENUBAR        1
#define BAND_TOOLBAR        2


/* Create the menubar control */
static void
CreateMenuBar(HWND hWnd)
{
    REBARBANDINFO band = {0};
    SIZE szIdeal;
    DWORD dwBtnSize;
    HWND hwndToolbar;
    HIMAGELIST hImgList;
    int i;

    /* Create ReBar window */
    hwndRebar = CreateWindowEx(WS_EX_TOOLWINDOW, _T("ReBarWindow32"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_BORDER |
            CCS_NODIVIDER | CCS_TOP | RBS_VARHEIGHT | RBS_BANDBORDERS | RBS_AUTOSIZE,
            0, 0, 0, 0, hWnd, (HMENU) 1000, hInst, NULL);

    /* Create the menubar control */
    hwndMenubar = CreateWindow(MC_WC_MENUBAR, _T(""), WS_CHILD | WS_VISIBLE |
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NORESIZE | CCS_NOPARENTALIGN,
            0, 0, 0, 0, hwndRebar, (HMENU) 1001, hInst, (LPVOID) hMenu);
    SendMessage(hwndMenubar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_HIDECLIPPEDBUTTONS);
    SendMessage(hwndMenubar, TB_GETIDEALSIZE, FALSE, (LPARAM) &szIdeal);

    /* Embed the menubar in the ReBar */
    band.cbSize = REBARBANDINFO_V3_SIZE;
    band.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE | RBBIM_ID;
    band.fStyle = RBBS_GRIPPERALWAYS | RBBS_USECHEVRON | RBBS_VARIABLEHEIGHT;
    band.hwndChild = hwndMenubar;
    dwBtnSize = (DWORD)SendMessage(band.hwndChild, TB_GETBUTTONSIZE, 0, 0);
    band.cyChild = HIWORD(dwBtnSize);
    band.cxMinChild = 0;
    band.cyMinChild = HIWORD(dwBtnSize);
    band.cyMaxChild = HIWORD(dwBtnSize);
    band.cyIntegral = HIWORD(dwBtnSize);
    band.cx = 240;
    band.cxIdeal = szIdeal.cx;
    band.wID = BAND_MENUBAR;
    SendMessage(hwndRebar, RB_INSERTBAND, -1, (LPARAM) &band);

    /* Create yet another ReBar band with a dummy toolbar, so user can play by
     * positioning the two bands. */
    hwndToolbar = CreateWindow(TOOLBARCLASSNAME, _T(""), WS_CHILD | WS_VISIBLE |
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NORESIZE | CCS_NOPARENTALIGN |
            CCS_NODIVIDER | TBSTYLE_TRANSPARENT | TBSTYLE_FLAT,
            0, 0, 0, 0, hwndRebar, (HMENU) 1002, hInst, 0);
    hImgList = ImageList_LoadImage(hInst, MAKEINTRESOURCE(ID_IMGLIST),
                    16, 1, RGB(255,0,255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
    SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM) hImgList);
    for(i = 0; i < 6; i++) {
        TBBUTTON button = { 0 };
        button.iBitmap = i;
        button.idCommand = 300 + i;
        button.fsState = TBSTATE_ENABLED;
        SendMessage(hwndToolbar, TB_ADDBUTTONS, 1, (LPARAM) &button);
    }

    /* Embed the toolbar in the ReBar */
    band.hwndChild = hwndToolbar;
    dwBtnSize = (DWORD)SendMessage(band.hwndChild, TB_GETBUTTONSIZE, 0,0);
    band.cyChild = HIWORD(dwBtnSize);
    band.cxMinChild = 6 * LOWORD(dwBtnSize);
    band.cyMinChild = HIWORD(dwBtnSize);
    band.cyMaxChild = HIWORD(dwBtnSize);
    band.cyIntegral = HIWORD(dwBtnSize);
    band.cx = 16 * LOWORD(dwBtnSize);
    band.wID = BAND_TOOLBAR;
    SendMessage(hwndRebar, RB_INSERTBAND, -1, (LPARAM) &band);
}

/* Create smaller menu bar as a child of the main window, demonstrating we
 * can place a menu anywhere. */
static void
CreateMenuBarSm(HWND hWnd)
{
    hwndMenubarSmall = CreateWindow(MC_WC_MENUBAR, _T(""), WS_CHILD | WS_VISIBLE |
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NORESIZE | CCS_NOPARENTALIGN,
            100, 100, 100, 23, hWnd, (HMENU) 1003, hInst, (LPVOID) hMenuSm);
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
            break;

        case WM_NOTIFY:
        {
            /* Handle notifications for the ReBar control */
            NMHDR* hdr = (NMHDR*) lParam;
            if(hdr->hwndFrom == hwndRebar) {
                switch(hdr->code) {
                    /* Cancel drag'n'drop for the menubar's band, so it is
                     * always on the top just below window caption. */
                    case RBN_BEGINDRAG:
                    {
                        NMREBAR* nm = (NMREBAR*) hdr;
                        if(nm->wID == BAND_MENUBAR)
                            return -1;
                        break;
                    }

                    /* Handle chevron for the menubar's band. */
                    case RBN_CHEVRONPUSHED:
                    {
                        NMREBARCHEVRON* nm = (NMREBARCHEVRON*) hdr;
                        if(nm->wID == BAND_MENUBAR)
                            mcMenubar_HandleRebarChevronPushed(hwndMenubar, nm);
                        break;
                    }
                }
            }
            break;
        }

        case WM_SIZE:
            /* Ensure the ReBar is resized on top of the main window */
            SendMessage(hwndRebar, WM_SIZE, 0, 0);
            break;

        case WM_CREATE:
            CreateMenuBar(hWnd);
            CreateMenuBarSm(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = { 0 };
    HWND hWnd;
    MSG msg;

    hInst = hInstance;
    hMenu = LoadMenu(hInst, MAKEINTRESOURCE(ID_MENU));
    hMenuSm = LoadMenu(hInst, MAKEINTRESOURCE(ID_MENU_SM));

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
        if(mcIsMenubarMessage(hwndMenubarSmall, &msg))
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
