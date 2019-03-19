
#include <tchar.h>
#include <windows.h>

#include <wdl.h>


static HWND hwndMain = NULL;


static const int strokeStyles[5] = {
    WD_DASHSTYLE_SOLID,
    WD_DASHSTYLE_DASH,
    WD_DASHSTYLE_DOT,
    WD_DASHSTYLE_DASHDOT,
    WD_DASHSTYLE_DASHDOTDOT
};


#define STROKE_WIDTH        1.0f


static void
MainWinPaintToCanvas(WD_HCANVAS hCanvas)
{
    WD_HBRUSH hBrush;
    WD_HSTROKESTYLE hStrokeStyle;
    float x, y;
    int i;

    wdBeginPaint(hCanvas);
    wdClear(hCanvas, WD_RGB(255,255,255));
    hBrush = wdCreateSolidBrush(hCanvas, WD_RGB(0, 0, 0));

    for(i = 0; i < 5; i++) {
        hStrokeStyle = wdCreateStrokeStyle(strokeStyles[i], WD_LINECAP_FLAT, WD_LINEJOIN_MITER);

        x = 10.0f + i * 110.0f;
        y = 10.0f;
        wdDrawRectStyled(hCanvas, hBrush, x, y, x + 90.0f, y + 90.0f, STROKE_WIDTH, hStrokeStyle);
        wdDrawEllipseStyled(hCanvas, hBrush, x + 45.0f, y + 45.0f, 40.0f, 40.0f, STROKE_WIDTH, hStrokeStyle);

        x = 10.0f;
        y = 130.0f + i * 15.0f;
        wdDrawLineStyled(hCanvas, hBrush, x, y, x + 5 * 110.0f - 20.0f, y, STROKE_WIDTH, hStrokeStyle);

        wdDestroyStrokeStyle(hStrokeStyle);
    }

    wdDestroyBrush(hBrush);
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
            MainWinPaintToCanvas(hCanvas);
            wdDestroyCanvas(hCanvas);
            EndPaint(hwndMain, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            HDC dc = (HDC) wParam;
            WD_HCANVAS hCanvas;

            hCanvas = wdCreateCanvasWithHDC(dc, NULL, 0);
            MainWinPaintToCanvas(hCanvas);
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

    wdInitialize(0);

    /* Register main window class */
    wc.lpfnWndProc = MainWinProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);

    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("LibWinDraw Example: Drawing with Stroke Styles"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 580, 250,
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

    wdTerminate(0);

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
