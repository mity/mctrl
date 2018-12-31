
#include <tchar.h>
#include <windows.h>

#include <wdl.h>

#include "image-from-buffer.h"

static HMENU hMenu = NULL;


typedef struct {
    UINT uMenuItem;
    int iPixelFormatId;
    UINT uBytesPerPixel;
    BOOL fIsAlphaSupported;
    BOOL fIsBottomUp;
} PIXEL_FORMAT_INFO;

typedef struct {
    UINT uMenuItem;
    COLORREF clr;
} COLOR_INFO;

typedef struct {
    UINT uMenuItem;
    BYTE bAlpha;
} TRANSPARENCY_INFO;


static const PIXEL_FORMAT_INFO gPixelFormatMap[] = {
    { IDM_FORMAT_PALETTE,  WD_PIXELFORMAT_PALETTE,  1, FALSE, FALSE },
    { IDM_FORMAT_R8G8B8,   WD_PIXELFORMAT_R8G8B8,   3, FALSE, FALSE },
    { IDM_FORMAT_R8G8B8A8, WD_PIXELFORMAT_R8G8B8A8, 4, TRUE,  FALSE },
    { IDM_FORMAT_B8G8R8A8, WD_PIXELFORMAT_B8G8R8A8, 4, TRUE,  TRUE  },
    { 0 }
};

static const COLOR_INFO gColorMap[] = {
    { IDM_COLOR_BLACK, RGB(0,0,0)       },
    { IDM_COLOR_GRAY,  RGB(127,127,127) },
    { IDM_COLOR_WHITE, RGB(255,255,255) },
    { IDM_COLOR_RED,   RGB(255,0,0)     },
    { IDM_COLOR_GREEN, RGB(0,255,0)     },
    { IDM_COLOR_BLUE,  RGB(0,0,255)     },
    { 0 }
};

static const TRANSPARENCY_INFO gTransparencyMap[] = {
    { IDM_TRANSPARENT_100, 0   },
    { IDM_TRANSPARENT_80,  51  },
    { IDM_TRANSPARENT_60,  102 },
    { IDM_TRANSPARENT_40,  153 },
    { IDM_TRANSPARENT_20,  204 },
    { IDM_TRANSPARENT_0,   255 },
    { 0 }
};


/* Background image. */
static WD_HIMAGE hBackImage = NULL;
#define BACK_PADDING            30.0f

/* Foreground image. */
static WD_HIMAGE hForeImage = NULL;
static const PIXEL_FORMAT_INFO* pCurrentPixelFormat = &gPixelFormatMap[0];
static const COLOR_INFO* pCurrentColor = &gColorMap[0];
static const TRANSPARENCY_INFO* pCurrentTransparency = &gTransparencyMap[0];


static const PIXEL_FORMAT_INFO*
PixelFormatInfo(UINT uMenuItem)
{
    int i;

    for(i = 0; gPixelFormatMap[i].uMenuItem != 0; i++) {
        if(gPixelFormatMap[i].uMenuItem == uMenuItem)
            return &gPixelFormatMap[i];
    }

    return NULL;
}

static const COLOR_INFO*
ColorInfo(UINT uMenuItem)
{
    int i;

    for(i = 0; gColorMap[i].uMenuItem != 0; i++) {
        if(gColorMap[i].uMenuItem == uMenuItem)
            return &gColorMap[i];
    }

    return NULL;
}

static const TRANSPARENCY_INFO*
TransparencyInfo(UINT uMenuItem)
{
    int i;

    for(i = 0; gTransparencyMap[i].uMenuItem != 0; i++) {
        if(gTransparencyMap[i].uMenuItem == uMenuItem)
            return &gTransparencyMap[i];
    }

    return NULL;
}

static void
SetBufferPixel(BYTE* pBuffer, UINT uWidth, UINT uHeight, UINT x, UINT y, BOOL fAltColor)
{
    if(pCurrentPixelFormat->fIsBottomUp)
        y = uHeight-1 - y;

    switch(pCurrentPixelFormat->iPixelFormatId) {
        case WD_PIXELFORMAT_PALETTE:
            pBuffer[y * uWidth + x] = (fAltColor ? 1 : 0);
            break;

        case WD_PIXELFORMAT_R8G8B8:
            if(fAltColor) {
                pBuffer[3 * (y * uWidth + x) + 0] = 191;
                pBuffer[3 * (y * uWidth + x) + 1] = 191;
                pBuffer[3 * (y * uWidth + x) + 2] = 223;
            } else {
                pBuffer[3 * (y * uWidth + x) + 0] = GetRValue(pCurrentColor->clr);
                pBuffer[3 * (y * uWidth + x) + 1] = GetGValue(pCurrentColor->clr);
                pBuffer[3 * (y * uWidth + x) + 2] = GetBValue(pCurrentColor->clr);
            }
            break;

        case WD_PIXELFORMAT_R8G8B8A8:
            pBuffer[4 * (y * uWidth + x) + 0] = GetRValue(pCurrentColor->clr);
            pBuffer[4 * (y * uWidth + x) + 1] = GetGValue(pCurrentColor->clr);
            pBuffer[4 * (y * uWidth + x) + 2] = GetBValue(pCurrentColor->clr);
            pBuffer[4 * (y * uWidth + x) + 3] = fAltColor ? pCurrentTransparency->bAlpha : 0xff;
            break;

        case WD_PIXELFORMAT_B8G8R8A8:
            pBuffer[4 * (y * uWidth + x) + 0] = GetBValue(pCurrentColor->clr);
            pBuffer[4 * (y * uWidth + x) + 1] = GetGValue(pCurrentColor->clr);
            pBuffer[4 * (y * uWidth + x) + 2] = GetRValue(pCurrentColor->clr);
            pBuffer[4 * (y * uWidth + x) + 3] = fAltColor ? pCurrentTransparency->bAlpha : 0xff;
            break;
    }
}

static WD_HIMAGE
CreateForeImage(UINT uWidth, UINT uHeight)
{
    UINT uBytesPerPixel = pCurrentPixelFormat->uBytesPerPixel;
    BYTE* pBuffer;
    int i, j;
    UINT x, y;
    BOOL fAltColor;
    WD_HIMAGE hImage;

    pBuffer = (BYTE*) malloc(uWidth * uHeight * uBytesPerPixel);
    if(pBuffer == NULL)
        return NULL;

    for(j = 0; j < 4; j++) {
        for(i = 0; i < 4; i++) {
            fAltColor = ((i + j) % 2 != 0);

            for(y = j * (uHeight / 5); y < (j+1) * (uHeight / 5); y++) {
                for(x = i * (uWidth / 5); x < (i+1) * (uWidth / 5); x++) {
                    SetBufferPixel(pBuffer, uWidth, uHeight, x, y, fAltColor);
                }
            }
        }
    }

    /* For the remainder on right/bottom, use just one color.
     * (This is for testing purposes, we have the right image orientation.) */
    for(y = 0; y < 4 * (uHeight / 5); y++) {
        for(x = 4 * (uWidth / 5); x < uWidth; x++) {
            SetBufferPixel(pBuffer, uWidth, uHeight, x, y, TRUE);
        }
    }
    for(y = 4 * (uHeight / 5); y < uHeight; y++) {
        for(x = 0; x < uWidth; x++) {
            SetBufferPixel(pBuffer, uWidth, uHeight, x, y, TRUE);
        }
    }

    if(pCurrentPixelFormat->iPixelFormatId == WD_PIXELFORMAT_PALETTE) {
        COLORREF palette[2];

        palette[0] = pCurrentColor->clr;
        palette[1] = RGB(191,191,223);

        hImage = wdCreateImageFromBuffer(uWidth, uHeight, uWidth * uBytesPerPixel,
                        pBuffer, pCurrentPixelFormat->iPixelFormatId, palette, 2);
    } else {
        hImage = wdCreateImageFromBuffer(uWidth, uHeight, uWidth * uBytesPerPixel,
                        pBuffer, pCurrentPixelFormat->iPixelFormatId, NULL, 0);
    }

    free(pBuffer);
    return hImage;
}

static void
PaintToCanvas(HWND hwnd, WD_HCANVAS hCanvas)
{
    RECT client;
    WD_RECT rect;

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

    /* Blit over it our image created from memory buffer. */
    if(hForeImage == NULL)
        hForeImage = CreateForeImage(client.right, client.bottom);
    if(hForeImage != NULL) {
        rect.x0 = client.left;
        rect.y0 = client.top;
        rect.x1 = client.right;
        rect.y1 = client.bottom;
        wdBitBltImage(hCanvas, hForeImage, &rect, NULL);
    }

    wdEndPaint(hCanvas);
}

static void
MainHandleMenu(HWND hwnd, UINT uMenuItem)
{
    const PIXEL_FORMAT_INFO* pPixelFormat;
    const COLOR_INFO* pColor;
    const TRANSPARENCY_INFO* pTransparency;

    pPixelFormat = PixelFormatInfo(uMenuItem);
    if(pPixelFormat != NULL) {
        CheckMenuItem(hMenu, pCurrentPixelFormat->uMenuItem, MF_BYCOMMAND | MF_UNCHECKED);
        pCurrentPixelFormat = pPixelFormat;
        CheckMenuItem(hMenu, pCurrentPixelFormat->uMenuItem, MF_BYCOMMAND | MF_CHECKED);

        if(pCurrentPixelFormat->fIsAlphaSupported)
            EnableMenuItem(hMenu, 2, MF_BYPOSITION | MF_ENABLED);
        else
            EnableMenuItem(hMenu, 2, MF_BYPOSITION | MF_GRAYED);
        DrawMenuBar(hwnd);
    }

    pColor = ColorInfo(uMenuItem);
    if(pColor != NULL) {
        CheckMenuItem(hMenu, pCurrentColor->uMenuItem, MF_BYCOMMAND | MF_UNCHECKED);
        pCurrentColor = pColor;
        CheckMenuItem(hMenu, pCurrentColor->uMenuItem, MF_BYCOMMAND | MF_CHECKED);
    }

    pTransparency = TransparencyInfo(uMenuItem);
    if(pTransparency != NULL) {
        CheckMenuItem(hMenu, pCurrentTransparency->uMenuItem, MF_BYCOMMAND | MF_UNCHECKED);
        pCurrentTransparency = pTransparency;
        CheckMenuItem(hMenu, pCurrentTransparency->uMenuItem, MF_BYCOMMAND | MF_CHECKED);
    }

    /* Destroy the foreground image so it gets re-created
     * with the new options. */
    if(hForeImage != NULL) {
        wdDestroyImage(hForeImage);
        hForeImage = NULL;
    }

    InvalidateRect(hwnd, NULL, TRUE);
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

        case WM_SIZE:
            if(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) {
                /* Destroy the foreground image so it gets re-created
                 * in the new size. */
                if(hForeImage != NULL) {
                    wdDestroyImage(hForeImage);
                    hForeImage = NULL;
                }
            }
            break;

        case WM_COMMAND:
            if(HIWORD(wParam) == 0)
                MainHandleMenu(hwnd, LOWORD(wParam));
            return 0;

        case WM_CREATE:
            hBackImage = wdLoadImageFromResource(GetModuleHandle(NULL),
                            RT_RCDATA, MAKEINTRESOURCE(ID_LENNA_JPG));
            if(hBackImage == NULL)
                return -1;

            /* Set defaults. */
            MainHandleMenu(hwnd, IDM_FORMAT_R8G8B8A8);
            MainHandleMenu(hwnd, IDM_COLOR_BLACK);
            MainHandleMenu(hwnd, IDM_TRANSPARENT_60);
            return 0;

        case WM_DESTROY:
            if(hBackImage != NULL)
                wdDestroyImage(hBackImage);
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
    hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(ID_MENU));
    hwnd = CreateWindow(
        _T("main_window"), _T("LibWinDraw Example: Image from Memory Buffer"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 550, 350,
        NULL, hMenu, hInstance, NULL
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
