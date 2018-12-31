
#include <tchar.h>
#include <windows.h>

#include <wdl.h>


/* Id of the JPG image resource. */
#define ID_LENNA_JPG        50


static HWND hwndMain = NULL;
static WD_HIMAGE hImage = NULL;

#define PADDING             30.0f;

static void
PaintToCanvas(WD_HCANVAS hCanvas)
{
    RECT client;
    WD_RECT rect;

    GetClientRect(hwndMain, &client);
    rect.x0 = client.left + PADDING;
    rect.y0 = client.top + PADDING;
    rect.x1 = client.right - PADDING;
    rect.y1 = client.bottom - PADDING;

    wdRotateWorld(hCanvas, client.right / 2.0f, client.bottom / 2.0f, 22.5f);

    wdBeginPaint(hCanvas);
    wdClear(hCanvas, WD_RGB(255,255,255));

    if(rect.x0 < rect.x1  &&  rect.y0 < rect.y1)
        wdBitBltImage(hCanvas, hImage, &rect, NULL);

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

        case WM_CREATE:
            hImage = wdLoadImageFromResource(GetModuleHandle(NULL),
                            RT_RCDATA, MAKEINTRESOURCE(ID_LENNA_JPG));
            if(hImage == NULL)
                return -1;
            return 0;

        case WM_DESTROY:
            if(hImage != NULL)
                wdDestroyImage(hImage);
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
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClass(&wc);

    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("LibWinDraw Example: Load Image"),
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
