
#include <tchar.h>
#include <windows.h>

#include <wdl.h>

#include "image-from-hbitmap.h"


/* Background and foreground images. */
static WD_HIMAGE hBackImage = NULL;
static HBITMAP hForeBmp = NULL;
static HBITMAP hForeBmpPremultiplied = NULL;
static WD_HIMAGE hForeImageNoAlpha = NULL;
static WD_HIMAGE hForeImageUseAlpha = NULL;
static WD_HIMAGE hForeImageUseAlphaPremultiplied = NULL;

#define BACK_PADDING            30.0f


static void
PaintToCanvas(HWND hwnd, WD_HCANVAS hCanvas)
{
    RECT client;
    WD_RECT rect;
    UINT w, h;

    GetClientRect(hwnd, &client);

    /* Blit lenna.jpg as a background.
     * This is the same code as in image-from-res.c
     */
    rect.x0 = client.left + BACK_PADDING;
    rect.y0 = client.top + BACK_PADDING;
    rect.x1 = client.right - BACK_PADDING;
    rect.y1 = client.bottom - BACK_PADDING;
    wdRotateWorld(hCanvas, client.right / 2.0f, client.bottom / 2.0f, 22.5f);
    wdBeginPaint(hCanvas);
    wdClear(hCanvas, WD_RGB(255,255,255));
    if(rect.x0 < rect.x1  &&  rect.y0 < rect.y1)
        wdBitBltImage(hCanvas, hBackImage, &rect, NULL);
    wdResetWorld(hCanvas);

    /* Blit over our foreground images. */
    wdGetImageSize(hForeImageNoAlpha, &w, &h);
    rect.y0 = client.bottom / 2 - h / 2;
    rect.y1 = rect.y0 + h;
    rect.x0 = client.right / 4 - w / 2;
    rect.x1 = rect.x0 + w;
    wdBitBltImage(hCanvas, hForeImageNoAlpha, &rect, NULL);

    rect.x0 = 2 * client.right / 4 - w / 2;
    rect.x1 = rect.x0 + w;
    wdBitBltImage(hCanvas, hForeImageUseAlpha, &rect, NULL);

    rect.x0 = 3 * client.right / 4 - w / 2;
    rect.x1 = rect.x0 + w;
    wdBitBltImage(hCanvas, hForeImageUseAlphaPremultiplied, &rect, NULL);

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

            BeginPaint(hwnd, &ps);
            hCanvas = wdCreateCanvasWithPaintStruct(hwnd, &ps, 0);
            PaintToCanvas(hwnd, hCanvas);
            wdDestroyCanvas(hCanvas);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
        {
            HDC dc = (HDC) wParam;
            WD_HCANVAS hCanvas;

            hCanvas = wdCreateCanvasWithHDC(dc, NULL, 0);
            PaintToCanvas(hwnd, hCanvas);
            wdDestroyCanvas(hCanvas);
            return 0;
        }

        case WM_CREATE:
            hBackImage = wdLoadImageFromResource(GetModuleHandle(NULL),
                            RT_RCDATA, MAKEINTRESOURCE(ID_LENNA_JPG));
            if(hBackImage == NULL)
                return -1;

            hForeImageNoAlpha = wdCreateImageFromHBITMAP(hForeBmp);
            if(hForeImageNoAlpha == NULL)
                return -1;

            hForeImageUseAlpha = wdCreateImageFromHBITMAPWithAlpha(hForeBmp, WD_ALPHA_USE);
            if(hForeImageUseAlpha == NULL)
                return -1;

            hForeImageUseAlphaPremultiplied = wdCreateImageFromHBITMAPWithAlpha(hForeBmpPremultiplied, WD_ALPHA_USE_PREMULTIPLIED);
            if(hForeImageUseAlphaPremultiplied == NULL)
                return -1;

            return 0;

        case WM_DESTROY:
            if(hBackImage != NULL)
                wdDestroyImage(hBackImage);
            if(hForeImageNoAlpha != NULL)
                wdDestroyImage(hForeImageNoAlpha);
            if(hForeImageUseAlpha != NULL)
                wdDestroyImage(hForeImageUseAlpha);
            if(hForeImageUseAlphaPremultiplied != NULL)
                wdDestroyImage(hForeImageUseAlphaPremultiplied);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int APIENTRY
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = { 0 };
    HWND hwnd;
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
    hForeBmp = LoadBitmap(hInstance, MAKEINTRESOURCE(ID_ALPHA_BMP));
    hForeBmpPremultiplied = LoadBitmap(hInstance, MAKEINTRESOURCE(ID_ALPHA_PREMULTIPLIED_BMP));
    hwnd = CreateWindow(
        _T("main_window"), _T("LibWinDraw Example: Image from HBITMAP"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 550, 350,
        NULL, NULL, hInstance, NULL
    );
    SendMessage(hwnd, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),
            MAKELPARAM(TRUE, 0));
    ShowWindow(hwnd, nCmdShow);

    /* Message loop */
    while(GetMessage(&msg, NULL, 0, 0)) {
        if(IsDialogMessage(hwnd, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    wdTerminate(WD_INIT_IMAGEAPI);

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
