
#include <tchar.h>
#include <windows.h>

#include <wdl.h>


static HWND hwndMain = NULL;

/* The cached canvas (if not NULL).
 *
 * Note that if the caching is enabled, it remembers all its painted contents
 * across all the WM_PAINT messages so as long as we do not need to paint it
 * with different contents, we may just call wdBeginPaint() + wdEndPaint() to
 * make it blit into the window.
 *
 * If only some sub-region needs to be painted differently, then only that
 * part of it needs to be repainted.
 */
static WD_HCANVAS hCachedCanvas = NULL;


static const WD_COLOR drawColors[3] =
        { WD_RGB(255,0,0), WD_RGB(0,255,0), WD_RGB(0,0,255) };
static const WD_COLOR fillColors[3] =
        { WD_ARGB(63,255,0,0), WD_ARGB(63,0,255,0), WD_ARGB(63,0,0,255) };

static void
MainWinPaintToCanvas(WD_HCANVAS hCanvas, BOOL* pCanCache)
{
    WD_HBRUSH hBrush;
    int i;

    wdBeginPaint(hCanvas);

    /* This very simple example never changes what it paints; i.e. if we
     * already cached the completely painted canvas, we may skip the paint
     * code altogether.
     *
     * In real applications, we would have to paint only parts of the canvas
     * which needs to be painted differently since the last time; e.g. to
     * reflect a change in the application's state.
     */
    if(hCanvas != hCachedCanvas) {
        wdClear(hCanvas, WD_RGB(255,255,255));
        hBrush = wdCreateSolidBrush(hCanvas, 0);

        for(i = 0; i < 3; i++) {
            float x = 10.0f + i * 20.0f;
            float y = 10.0f + i * 20.0f;

            wdSetSolidBrushColor(hBrush, fillColors[i]);
            wdFillRect(hCanvas, hBrush, x, y, x + 100.0f, y + 100.0f);

            wdSetSolidBrushColor(hBrush, drawColors[i]);
            wdDrawRect(hCanvas, hBrush, x, y, x + 100.0f, y + 100.0f, 3.0f);
        }

        for(i = 0; i < 3; i++) {
            float x = 250.0f + i * 20.0f;
            float y = 60.0f + i * 20.0f;

            wdSetSolidBrushColor(hBrush, fillColors[i]);
            wdFillCircle(hCanvas, hBrush, x, y, 55.0f);

            wdSetSolidBrushColor(hBrush, drawColors[i]);
            wdDrawCircle(hCanvas, hBrush, x, y, 55.0f, 3.0f);
        }

        wdDestroyBrush(hBrush);
    }

    *pCanCache = wdEndPaint(hCanvas);
}


static void
MainWinPaint(void)
{
    PAINTSTRUCT ps;
    WD_HCANVAS hCanvas;

    BeginPaint(hwndMain, &ps);
    if(hCachedCanvas != NULL)
        hCanvas = hCachedCanvas;
    else
        hCanvas = wdCreateCanvasWithPaintStruct(hwndMain, &ps, WD_CANVAS_DOUBLEBUFFER);

    if(hCanvas != NULL) {
        BOOL bCanCache = FALSE;

        MainWinPaintToCanvas(hCanvas, &bCanCache);

        /* Cache the completely painted canvas if we are allowed to. */
        if(bCanCache) {
            hCachedCanvas = hCanvas;
        } else {
            wdDestroyCanvas(hCanvas);
            hCachedCanvas = NULL;
        }
    }
    EndPaint(hwndMain, &ps);
}

/* Main window procedure */
static LRESULT CALLBACK
MainWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_PAINT:
            MainWinPaint();
            break;

        case WM_SIZE:
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED) {
                if(hCachedCanvas != NULL) {
                    /* We could call wdResizeCanvas() here. But that would
                     * loose the painted stuff on the canvas anyway, and save
                     * us "only" the reallocation of the canvas.
                     *
                     * For the sake of simplicity, we just destroy it all. */
                    wdDestroyCanvas(hCachedCanvas);
                    hCachedCanvas = NULL;
                }
                InvalidateRect(hwndMain, NULL, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_DISPLAYCHANGE:
            /* Some relevant graphics settings has been changed and we cannot
             * use the cached canvas as it can use incompatible pixel format,
             * so discard it. */
            if(hCachedCanvas != NULL) {
                wdDestroyCanvas(hCachedCanvas);
                hCachedCanvas = NULL;
            }
            InvalidateRect(hwndMain, NULL, FALSE);
            break;

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
        _T("main_window"), _T("LibWinDraw Example"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 150, 350,
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
