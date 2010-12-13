/*
 * Copyright (c) 2008-2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This sample demonstrates basic usage of HTML control. */

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>

#include <mCtrl/html.h>


/* The initial URL */
#define INITIAL_URL   _T("res://ex_html.exe/1000")


static HINSTANCE inst;
static HWND html;


static void
set_dynamic_contents(void)
{
    static int counter = 1;
    TCHAR buffer[512];
    
    _sntprintf(buffer, sizeof(buffer) / sizeof(TCHAR), 
               _T("<p>This paragraph is generated dynamically by the application "
                  "and injected via message <tt>MC_HM_SETTAGCONTENTS</tt>. To "
                  "prove that the following number is incremented anytime this "
                  "page is <a href=\"1000\">reloaded</a> or "
                  "<a href=\"app:set_dynamic\">this app link is clicked</a>:</p>"
                  "<div class=\"big\">%d</div>"), 
                  counter);
    /* Our control has the <p id="dynamic"> on it. */
    SendMessage(html, MC_HM_SETTAGCONTENTS, 
                (WPARAM)_T("dynamic"), 
                (LPARAM)buffer);
    counter++;
}


/* Main window procedure */
static CALLBACK LRESULT
win_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
        case WM_NOTIFY:
            if(((NMHDR*)lp)->idFrom == 100 && ((NMHDR*)lp)->code == MC_HN_APPLINK) {
                /* We recieved notification from the HTML control that the user
                 * activated the application link. If it is the link in our 
                 * resource page, greet the user as the link URL suggested. 
                 * Otherwise show URL of the link. */
                MC_NMHTMLURL* nmhtmlurl = (MC_NMHTMLURL*) lp;
                if(_tcscmp(nmhtmlurl->pszUrl, _T("app:SayHello")) == 0)
                    MessageBox(win, _T("Hello World!"), _T("Hello World!"), MB_OK);
                else if(_tcscmp(nmhtmlurl->pszUrl, _T("app:set_dynamic")) == 0)
                    set_dynamic_contents();
                else
                    MessageBox(win, nmhtmlurl->pszUrl, _T("URL of the app link"), MB_OK);
            }
            if(((NMHDR*)lp)->idFrom == 100 && ((NMHDR*)lp)->code == MC_HN_DOCUMENTCOMPLETE) {
                /* We recieved notification from the HTML control that the 
                 * document is completely loaded. If it is the initial URL,
                 * we use this chance to modify the page dynamically to 
                 * demonstrate this feature. */
                MC_NMHTMLURL* nmhtmlurl = (MC_NMHTMLURL*) lp;
                
                if(_tcscmp(nmhtmlurl->pszUrl, INITIAL_URL) == 0)
                    set_dynamic_contents();
            }
            return 0;

        case WM_SIZE:
            if(wp == SIZE_RESTORED  ||  wp == SIZE_MAXIMIZED)
                SetWindowPos(html, NULL, 0, 0, LOWORD(lp), HIWORD(lp), SWP_NOZORDER);
            return 0;

        case WM_SETFOCUS:
            SetFocus(html);
            return 0;

        case WM_CREATE:
            /* Create html control */
            html = CreateWindow(MC_WC_HTML, _T("res://ex_html.exe/1000"),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    0, 0, 0, 0, win, (HMENU) 100, inst, NULL);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(win, msg, wp, lp);
}


int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE instance_prev, LPSTR cmd_line, int cmd_show)
{
    WNDCLASS wc = { 0 };
    HWND win;
    MSG msg;

    mcHtml_Initialize();

    /* Prevent linker from ignoring comctl32.dll */
    InitCommonControls();

    inst = instance;

    /* Register main window class */
    wc.lpfnWndProc = win_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);

    /* Create main window */
    win = CreateWindow(
        _T("main_window"), _T("mCtrl Example: HTML Control"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, instance, NULL
    );
    ShowWindow(win, cmd_show);

    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(win, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mcHtml_Terminate();

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
