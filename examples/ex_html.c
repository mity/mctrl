/*
 * Copyright (c) 2008-2011 Martin Mitas
 *
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been placed in the public domain.
 */

/* This sample demonstrates basic usage of HTML control. */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <tchar.h>

#include <mCtrl/html.h>


/* Control ID of the HTML control */
#define IDC_HTML      100

/* The initial URL. Note that 1000 is ID of the HTML resource in html.rc. */
#define INITIAL_URL   _T("res://ex_html.exe/1000")


static HINSTANCE hInst;
static HWND hwndHtml;


/* Set the dynamically generated contents in the HTML page with resource
 * ID 1000. The HTML page contains a tag <div> with the ID "dynamic".
 * This function replaces its contents with a custom string.
 */
static void
GenerateDynamicContents(void)
{
    static UINT uCounter = 1;
    TCHAR buffer[512];
    
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR), 
               _T("<p>This paragraph is generated dynamically by the application "
                  "and injected via message <tt>MC_HM_SETTAGCONTENTS</tt>. To "
                  "prove that the following number is incremented anytime this "
                  "page is <a href=\"1000\">reloaded</a> or "
                  "<a href=\"app:set_dynamic\">this app link is clicked</a>:</p>"
                  "<div class=\"big\">%u</div>"), 
                  uCounter);

    SendMessage(hwndHtml, MC_HM_SETTAGCONTENTS, (WPARAM)_T("dynamic"), (LPARAM)buffer);
    uCounter++;
}


/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_NOTIFY:
            if(((NMHDR*)lParam)->idFrom == IDC_HTML && ((NMHDR*)lParam)->code == MC_HN_APPLINK) {
                /* We recieved notification from the HTML control: user has
                 * activated the application link (app: protocol).
                 *
                 * If it is the "Say Hello" link in our resource page, we greet
                 * the user as the link as the URL suggested.
                 *
                 * If it is the link to change the dynamic contents, we do so.
                 *
                 * Otherwise as a fallback we just show a URL of the link.
                 */
                MC_NMHTMLURL* nmhtmlurl = (MC_NMHTMLURL*) lParam;
                if(_tcscmp(nmhtmlurl->pszUrl, _T("app:SayHello")) == 0)
                    MessageBox(hwnd, _T("Hello World!"), _T("Hello World!"), MB_OK);
                else if(_tcscmp(nmhtmlurl->pszUrl, _T("app:set_dynamic")) == 0)
                    GenerateDynamicContents();
                else
                    MessageBox(hwnd, nmhtmlurl->pszUrl, _T("URL of the app link"), MB_OK);
            }
            if(((NMHDR*)lParam)->idFrom == IDC_HTML && ((NMHDR*)lParam)->code == MC_HN_DOCUMENTCOMPLETE) {
                /* We received notification from the HTML control that the 
                 * document is completely loaded. If it is the initial URL,
                 * we use this chance to generate the dynamiccontents for
                 * the first time. */
                MC_NMHTMLURL* nmhtmlurl = (MC_NMHTMLURL*) lParam;

                if(_tcscmp(nmhtmlurl->pszUrl, INITIAL_URL) == 0)
                    GenerateDynamicContents();
            }
            return 0;

        case WM_SIZE:
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED)
                SetWindowPos(hwndHtml, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
            return 0;

        case WM_SETFOCUS:
            SetFocus(hwndHtml);
            return 0;

        case WM_CREATE:
            /* Create the html control */
            hwndHtml = CreateWindow(MC_WC_HTML, INITIAL_URL, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    0, 0, 0, 0, hwnd, (HMENU) IDC_HTML, hInst, NULL);
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

    /* Register class of HTML control. */
    mcHtml_Initialize();

    /* Prevent linker from ignoring COMCTL32.DLL. We do not use common
     * controls at all in this sample, however MCTRL.DLL enables XP styles
     * in forms for the HTML control only oif the application uses them,
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
        _T("main_window"), _T("mCtrl Example: HTML Control"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInst, NULL
    );
    ShowWindow(hwndMain, nCmdShow);

    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(hwndMain, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mcHtml_Terminate();

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
