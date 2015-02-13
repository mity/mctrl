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


/* Window caption */
#define CAPTION       _T("mCtrl Example: HTML Control")

/* Control ID of controls */
#define ID_HTML         100
#define ID_TOOLBAR      101
#define ID_STATUS       102

/* Toolbar button IDs */
#define IDM_BACK        200
#define IDM_FORWARD     201

/* The initial URL refers to the HTML document embedded into this example as
 * a resource. */
#define INITIAL_URL   _T("res://ex_html.exe/doc.html")


static HINSTANCE hInst;

static HWND hwndHtml;
static HWND hwndToolbar;
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
    if(hdr->idFrom == ID_HTML) {
        /* The HTML control has sent us info about some change in its internal
         * state so we may need to update user interface to reflect it. */

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
            else if (_tcscmp(nmhtmlurl->pszUrl, _T("app:calljsfn")) == 0) {
                MC_HMCALLSCRIPTFN argStruct;
                argStruct.pszFnName = _T("alerter");
                argStruct.pszArguments = _T("alerter arg string from C code");
                TCHAR result[256];
                argStruct.iResultBufCharCount = sizeof(result) / sizeof(TCHAR);
                LRESULT res = SendMessage(hwndHtml, MC_HM_CALLSCRIPTFN, (WPARAM)&argStruct, (LPARAM)result);
                if (res != 0) {
                    MessageBox(hwnd, _T("MC_HM_CALLSCRIPTFN returned error?!?!"), _T("MC_HM_CALLSCRIPTFN result"), MB_OK);
                }
                else {
                    MessageBox(hwnd, result, _T("MC_HM_CALLSCRIPTFN result"), MB_OK);
                }

            }
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
            /* The HTML control asks us to update status bar. */
            MC_NMHTMLTEXT* nmhtmltext = (MC_NMHTMLTEXT*) hdr;
            SetWindowText(hwndStatus, nmhtmltext->pszText);
        }

        if(hdr->code == MC_HN_TITLETEXT) {
            /* The HTML control asks us to update document title. We show it
             * in the caption of the main window. */
            MC_NMHTMLTEXT* nmhtmltext = (MC_NMHTMLTEXT*) hdr;
            if(nmhtmltext->pszText[0] != _T('\0')) {
                TCHAR buffer[512];
                _sntprintf(buffer, 512, _T("%s - %s"), nmhtmltext->pszText, CAPTION);
                SetWindowText(hwnd, buffer);
            } else {
                SetWindowText(hwnd, CAPTION);
            }
        }

        if(hdr->code == MC_HN_HISTORY) {
            /* Update status of history buttons */
            MC_NMHTMLHISTORY* nmhtmlhistory = (MC_NMHTMLHISTORY*) hdr;
            SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_BACK,
                        MAKELPARAM(nmhtmlhistory->bCanBack, 0));
            SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_FORWARD,
                        MAKELPARAM(nmhtmlhistory->bCanForward, 0));
        }
    }
}


static void
HandleResize(HWND hwnd, UINT uWidth, UINT uHeight)
{
    RECT rect;
    UINT toolbarHeight;
    UINT statusHeight;
    UINT htmlHeight;

    SendMessage(hwndStatus, WM_SIZE, 0, 0);
    GetWindowRect(hwndStatus, &rect);
    statusHeight = rect.bottom - rect.top;

    SendMessage(hwndToolbar, WM_SIZE, 0, 0);
    GetWindowRect(hwndToolbar, &rect);
    toolbarHeight = rect.bottom - rect.top;

    htmlHeight = uHeight - statusHeight - toolbarHeight;
    SetWindowPos(hwndHtml, NULL, 0, toolbarHeight, uWidth, htmlHeight, SWP_NOZORDER);
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

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case IDM_BACK:    SendMessage(hwndHtml, MC_HM_GOBACK, TRUE, 0); break;
                case IDM_FORWARD: SendMessage(hwndHtml, MC_HM_GOBACK, FALSE, 0); break;
            }
            break;

        case WM_CREATE:
        {
            HIMAGELIST tbImgList;
            TBBUTTON tbButtons[2];

            /* Create the html control */
            hwndHtml = CreateWindow(MC_WC_HTML, INITIAL_URL, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    0, 0, 0, 0, hwnd, (HMENU) ID_HTML, hInst, NULL);

            /* Create toolbar control. It provides the 'back' and 'forward'
             * buttons for walking the browser history. */
            hwndToolbar = CreateWindow(TOOLBARCLASSNAME, NULL, WS_CHILD | WS_BORDER | WS_VISIBLE,
                                    0, 0, 0, 0, hwnd, (HMENU) ID_TOOLBAR, hInst, NULL);
            SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
            memset(tbButtons, 0, sizeof(tbButtons));
            tbButtons[0].iBitmap = 0;
            tbButtons[0].idCommand = IDM_BACK;
            tbButtons[0].fsStyle = BTNS_BUTTON;
            tbButtons[1].iBitmap = 1;
            tbButtons[1].idCommand = IDM_FORWARD;
            tbButtons[1].fsStyle = BTNS_BUTTON;
            SendMessage(hwndToolbar, TB_ADDBUTTONS, 2, (LPARAM) tbButtons);
            tbImgList = ImageList_LoadImage(hInst, _T("toolbar"), 24, 1, RGB(255,0,255),
                                    IMAGE_BITMAP, LR_CREATEDIBSECTION);
            SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM) tbImgList);

            /* Create status control. We show status info the HTML control
             * sends to us via WM_NOTIFY. */
            hwndStatus = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                    0, 0, 0, 0, hwnd, (HMENU) ID_STATUS, hInst, NULL);
            return 0;
        }

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
