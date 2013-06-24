/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of HTML control. */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <tchar.h>

#include <mCtrl/html.h>


/* Control ID of controls */
#define IDC_HTML         100
#define IDC_REBAR        101
#define IDC_HISTORY      102
#define IDC_ADDRESS      103
#define IDC_STATUS       104

#define CAPTION       _T("mCtrl Example: HTML Control")

/* The initial URL refers to the HTML document embedded into this example as
 * a resource. */
#define INITIAL_URL   _T("res://ex_html.exe/doc.html")


static HINSTANCE hInst;

static HWND hwndHtml;
static HWND hwndStatus;


/* Set the dynamically generated contents in the embedded HTML page. The HTML
 * page contains a tag <div> with the ID "dynamic". This function replaces its
 * contents with a custom string. */
static void
GenerateDynamicContents(void)
{
    static UINT uCounter = 1;
    TCHAR buffer[512];

    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR),
               _T("<p>This paragraph is generated dynamically by the application "
                  "and injected via message <tt>MC_HM_SETTAGCONTENTS</tt>. To "
                  "prove that the following number is incremented anytime this "
                  "page is <a href=\"doc.html\">reloaded</a> or "
                  "<a href=\"app:set_dynamic\">this app link is clicked</a>:</p>"
                  "<div class=\"big\">%u</div>"),
                  uCounter);

    SendMessage(hwndHtml, MC_HM_SETTAGCONTENTS, (WPARAM)_T("dynamic"), (LPARAM)buffer);
    uCounter++;
}


static void
HandleNotify(HWND hwnd, NMHDR* hdr)
{
    if(hdr->idFrom == IDC_HTML) {
        if(hdr->code == MC_HN_APPLINK) {
            /* User has activated the application link (app: protocol).
             * If it is the "Say Hello" link in our resource page, we greet
             * the user as an URL of the link suggests.
             * If it is the link to change the dynamic contents, we do so.
             * Otherwise as a fallback we just show an URL of the link.
             */
            MC_NMHTMLURL* nmhtmlurl = (MC_NMHTMLURL*) hdr;
            if(_tcscmp(nmhtmlurl->pszUrl, _T("app:SayHello")) == 0)
                MessageBox(hwnd, _T("Hello World!"), _T("Hello World!"), MB_OK);
            else if(_tcscmp(nmhtmlurl->pszUrl, _T("app:set_dynamic")) == 0)
                GenerateDynamicContents();
            else
                MessageBox(hwnd, nmhtmlurl->pszUrl, _T("URL of the app link"), MB_OK);
        }

        if(hdr->code == MC_HN_DOCUMENTCOMPLETE) {
            /* The document is completely loaded. If it is the initial URL,
             * we use this as a chance to generate the dynamic contents for
             * the first time.
             */
            MC_NMHTMLURL* nmhtmlurl = (MC_NMHTMLURL*) hdr;
            if(_tcscmp(nmhtmlurl->pszUrl, INITIAL_URL) == 0)
                GenerateDynamicContents();
        }

        if(hdr->code == MC_HN_STATUSTEXT) {
            /* Update status bar */
            MC_NMHTMLTEXT* nmhtmltext = (MC_NMHTMLTEXT*) hdr;
            SetWindowText(hwndStatus, nmhtmltext->pszText);
        }

        if(hdr->code == MC_HN_TITLETEXT) {
            /* Update title */
            MC_NMHTMLTEXT* nmhtmltext = (MC_NMHTMLTEXT*) hdr;
            if(nmhtmltext->pszText[0] != _T('\0')) {
                TCHAR buffer[512];
                _sntprintf(buffer, 512, _T("%s - %s"), nmhtmltext->pszText, CAPTION);
                SetWindowText(hwnd, buffer);
            } else {
                SetWindowText(hwnd, CAPTION);
            }
        }
    }
}


static void
HandleResize(HWND hwnd, UINT uWidth, UINT uHeight)
{
    RECT rect;
    UINT statusHeight;

    SendMessage(hwndStatus, WM_SIZE, 0, 0);
    GetWindowRect(hwndStatus, &rect);
    statusHeight = rect.bottom - rect.top;

    SetWindowPos(hwndHtml, NULL, 0, 0, uWidth, uHeight - statusHeight, SWP_NOZORDER);
}


/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_NOTIFY:
            HandleNotify(hwnd, (NMHDR*) lParam);
            return 0;

        case WM_SIZE:
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED)
                HandleResize(hwnd, LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_SETFOCUS:
            SetFocus(hwndHtml);
            return 0;

        case WM_CREATE:
            /* Create the html control */
            hwndHtml = CreateWindow(MC_WC_HTML, INITIAL_URL, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    0, 0, 0, 0, hwnd, (HMENU) IDC_HTML, hInst, NULL);
            hwndStatus = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                    0, 0, 0, 0, hwnd, (HMENU) IDC_STATUS, hInst, NULL);
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

    /* Init common controls. */
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
        _T("main_window"), CAPTION, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
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
