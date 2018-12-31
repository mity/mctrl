
#include <tchar.h>
#include <windows.h>

#include <wdl.h>


static HWND hwndMain = NULL;


static WCHAR pszLoremIpsum[] =
        L"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Duis "
        L"ante orci, molestie vitae vehicula venenatis, tincidunt ac pede. "
        L"Proin in  tellus sit amet nibh dignissim sagittis. Pellentesque "
        L"arcu. Etiam dui sem, fermentum vitae, sagittis id, malesuada in, "
        L"quam. Nullam dapibus fermentum ipsum. Nam quis nulla.";


static void
PaintToCanvas(WD_HCANVAS hCanvas)
{
    RECT client;
    WD_HBRUSH hBrush;
    WD_HFONT hFont;
    WD_RECT rect;

    GetClientRect(hwndMain, &client);

    wdBeginPaint(hCanvas);
    wdClear(hCanvas, WD_RGB(255,255,255));
    hFont = wdCreateFontWithGdiHandle(GetStockObject(DEFAULT_GUI_FONT));
    hBrush = wdCreateSolidBrush(hCanvas, WD_RGB(0,0,0));

    rect.x0 = 10.0f;
    rect.y0 = 10.0f;
    rect.x1 = client.right - 10.0f;
    rect.y1 = client.bottom / 2 - 5.0f;
    wdDrawString(hCanvas, hFont, &rect, pszLoremIpsum, -1, hBrush, 0);

    rect.y0 = client.bottom / 2 + 5.0f;
    rect.y1 = client.bottom - 10.0f;
    wdDrawString(hCanvas, hFont, &rect, L"Left top", -1, hBrush, WD_STR_LEFTALIGN | WD_STR_TOPALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Center top", -1, hBrush, WD_STR_CENTERALIGN | WD_STR_TOPALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Right top", -1, hBrush, WD_STR_RIGHTALIGN | WD_STR_TOPALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Left center", -1, hBrush, WD_STR_LEFTALIGN | WD_STR_MIDDLEALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Right center", -1, hBrush, WD_STR_RIGHTALIGN | WD_STR_MIDDLEALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Left bottom", -1, hBrush, WD_STR_LEFTALIGN | WD_STR_BOTTOMALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Center bottom", -1, hBrush, WD_STR_CENTERALIGN | WD_STR_BOTTOMALIGN);
    wdDrawString(hCanvas, hFont, &rect, L"Right bottom", -1, hBrush, WD_STR_RIGHTALIGN | WD_STR_BOTTOMALIGN);
    wdSetSolidBrushColor(hBrush, WD_RGB(191,191,191));
    wdDrawRect(hCanvas, hBrush, rect.x0, rect.y0, rect.x1, rect.y1, 1.0f);

    wdDestroyBrush(hBrush);
    wdDestroyFont(hFont);
    wdEndPaint(hCanvas);
 }

/* Main window procedure */
static LRESULT CALLBACK
MainWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            WD_HCANVAS hCanvas;

            BeginPaint(hwndMain, &ps);
            hCanvas = wdCreateCanvasWithPaintStruct(hwndMain, &ps, 0);
            PaintToCanvas(hCanvas);
            wdDestroyCanvas(hCanvas);
            EndPaint(hwndMain, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            HDC dc = (HDC) wParam;
            WD_HCANVAS hCanvas;

            hCanvas = wdCreateCanvasWithHDC(dc, NULL, 0);
            PaintToCanvas(hCanvas);
            wdDestroyCanvas(hCanvas);
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
    MSG msg;

    wdInitialize(WD_INIT_STRINGAPI);

    /* Register main window class */
    wc.lpfnWndProc = MainWinProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClass(&wc);

    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("LibWinDraw Example: Draw Text"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 550, 350,
        NULL, NULL, hInstance, NULL
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

    wdTerminate(WD_INIT_STRINGAPI);

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
