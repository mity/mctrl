
#include <tchar.h>
#include <windows.h>

#include <wdl.h>


/* Id of the JPG image resource. */
#define ID_LENNA_JPG        50


static HWND hwndMain = NULL;

static WD_HIMAGE hImage = NULL;             /* Loaded once when starting the program. */

static WD_HCANVAS hCachedCanvas = NULL;
static WD_HCACHEDIMAGE hCachedImage = NULL; /* Re-created from hImage every time we re-create a canvas. */


static void
MainWinPaintToCanvas(WD_HCANVAS hCanvas, BOOL* pCanCache)
{
    wdBeginPaint(hCanvas);
    wdClear(hCanvas, WD_RGB(255,255,255));
    wdBitBltCachedImage(hCanvas, hCachedImage, 0.0f, 0.0f);
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

    if(hCanvas != NULL  &&  hCachedImage == NULL)
        hCachedImage = wdCreateCachedImage(hCanvas, hImage);

    if(hCanvas != NULL  &&  hCachedImage != NULL) {
        BOOL bCanCache = FALSE;

        MainWinPaintToCanvas(hCanvas, &bCanCache);

        if(bCanCache) {
            hCachedCanvas = hCanvas;
        } else {
            /* When destroying canvas, we also destroy the cached image as it
             * is only reusable with compatible canvases. */
            wdDestroyCachedImage(hCachedImage);
            hCachedImage = NULL;
            wdDestroyCanvas(hCanvas);
            hCachedCanvas = NULL;
        }
    }
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
            /* If we are caching the canvas for WM_PAINT, we need to resize it. */
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED) {
                if(hCachedCanvas != NULL)
                    wdResizeCanvas(hCachedCanvas, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_DISPLAYCHANGE:
            if(hCachedCanvas != NULL) {
                wdDestroyCanvas(hCachedCanvas);
                hCachedCanvas = NULL;
            }
            InvalidateRect(hwndMain, NULL, FALSE);
            break;

        case WM_CREATE:
            hImage = wdLoadImageFromResource(GetModuleHandle(NULL),
                            RT_RCDATA, MAKEINTRESOURCE(ID_LENNA_JPG));
            if(hImage == NULL)
                return -1;
            return 0;

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

    /* Note that for image API working, we need to ask for it with the flag
     * WD_INIT_IMAGEAPI. */
    wdInitialize(WD_INIT_IMAGEAPI);

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

    wdTerminate(WD_INIT_IMAGEAPI);

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
