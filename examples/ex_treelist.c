/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of TREELIST control. */

#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>

#include <mCtrl/treelist.h>


#define IDC_TREELIST    100

static HINSTANCE hInst;
static HWND hwndTreeList;


/* We use the Solar system to populate the control. Yes, the lists of moons
 * of the outer planets are very incomplete. Many smaller moons were omitted.
 * Remember this is just an example code to show the TREELIST in action, not
 * an astronomic catalogue. */

typedef struct MOON_tag {
    const TCHAR* pszName;
    const TCHAR* pszDistance;
    const TCHAR* pszDiameter;
    const TCHAR* pszMass;
} MOON;

typedef struct PLANET_tag {
    const TCHAR* pszName;
    const TCHAR* pszDistance;
    const TCHAR* pszDiameter;
    const TCHAR* pszMass;
    const MOON* pMoons;
    UINT cMoons;
} PLANET;

#define SIZEOF_ARRAY(array)    (sizeof((array)) / sizeof((array)[0]))

static const MOON earthMoons[] = {
    { _T("Moon"), _T("384399 km"), _T("3474.2 km"), _T("7.3477e22 kg") }
};

static const MOON marsMoons[] = {
    { _T("Phobos"),  _T("9380 km"), _T("22.2 km"), _T("1.0720e16 kg") },
    { _T("Deimos"), _T("23460 km"), _T("12.4 km"), _T("1.4800e15 kg") }
};

static const MOON jupiterMoons[] = {
    { _T("Io"),        _T("421800 km"), _T("3636.2 km"), _T("8.9319e22 kg") },
    { _T("Europa"),    _T("671100 km"), _T("3121.4 km"), _T("4.7998e22 kg") },
    { _T("Ganymede"), _T("1070400 km"), _T("5268.2 km"), _T("1.4819e23 kg") },
    { _T("Callisto"), _T("1882700 km"), _T("4816.8 km"), _T("1.0759e23 kg") }
};

static const MOON saturnMoons[] = {
    { _T("Mimas"),     _T("185540 km"),  _T("397.6 km"), _T("3.7493e19 kg") },
    { _T("Enceladus"), _T("238040 km"),  _T("504.6 km"), _T("1.0802e20 kg") },
    { _T("Tethys"),    _T("294670 km"), _T("1072.6 km"), _T("6.1745e20 kg") },
    { _T("Dione"),     _T("377420 km"), _T("1125.0 km"), _T("1.0955e21 kg") },
    { _T("Rhea"),      _T("527070 km"), _T("1529.0 km"), _T("2.3065e21 kg") },
    { _T("Titan"),    _T("1221870 km"), _T("5151.0 km"), _T("1.3452e23 kg") },
    { _T("Iapetus"),  _T("3560840 km"), _T("1469.0 km"), _T("1.8056e21 kg") }
};

static const MOON uranusMoons[] = {
    { _T("Ariel"),   _T("190900 km"), _T("1157.8 km"), _T("1.3530e21 kg") },
    { _T("Umbriel"), _T("266000 km"), _T("1169.4 km"), _T("1.1720e21 kg") },
    { _T("Titania"), _T("436300 km"), _T("1577.8 km"), _T("3.5270e21 kg") },
    { _T("Oberon"),  _T("583500 km"), _T("1522.8 km"), _T("3.0140e21 kg") },
    { _T("Miranda"), _T("129900 km"),  _T("461.6 km"), _T("6.5900e19 kg") }
};

static const MOON neptuneMoons[] = {
    { _T("Triton"), _T("354800.0 km"), _T("2706.8 km"), _T("2.1400e22 kg") }
};

static const PLANET planets[] = {
    { _T("Mercury"),   _T("57909100 km"),   _T("4879.4 km"), _T("3.3022e23 kg"), NULL,         0 },
    { _T("Venus"),    _T("108208000 km"),  _T("12103.6 km"), _T("4.8685e24 kg"), NULL,         0 },
    { _T("Earth"),    _T("149598261 km"),  _T("12742.0 km"), _T("5.9736e24 kg"), earthMoons,   SIZEOF_ARRAY(earthMoons) },
    { _T("Mars"),     _T("227939100 km"),   _T("6779.0 km"), _T("6.4185e23 kg"), marsMoons,    SIZEOF_ARRAY(marsMoons) },
    { _T("Jupiter"),  _T("778547200 km"), _T("139822.0 km"), _T("1.8986e27 kg"), jupiterMoons, SIZEOF_ARRAY(jupiterMoons) },
    { _T("Saturn"),  _T("1433449370 km"), _T("120536.0 km"), _T("5.6846e26 kg"), saturnMoons,  SIZEOF_ARRAY(saturnMoons) },
    { _T("Uranus"),  _T("2876679082 km"),  _T("50724.0 km"), _T("8.6810e25 kg"), uranusMoons,  SIZEOF_ARRAY(uranusMoons) },
    { _T("Neptune"), _T("4503443661 km"),  _T("49244.0 km"), _T("1.0243e26 kg"), neptuneMoons, SIZEOF_ARRAY(neptuneMoons) }
};


/* Fill the TREELIST control with the planets and moons */
static void
SetupTreeList(void)
{
    MC_TLCOLUMN col;
    MC_TLINSERTSTRUCT insertSun;
    MC_TLINSERTSTRUCT insertPlanet;
    MC_TLINSERTSTRUCT insertMoon;
    MC_HTREELISTITEM hSun;
    MC_TLSUBITEM subitem;
    int i, j;

    col.fMask = MC_TLCF_TEXT | MC_TLCF_WIDTH | MC_TLCF_FORMAT;
    col.cx = 130;
    col.fmt = MC_TLFMT_LEFT;
    col.pszText = _T("Solar system");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 0, (LPARAM) &col);
    col.cx = 100;
    col.fmt = MC_TLFMT_RIGHT;
    col.pszText = _T("Distance");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 1, (LPARAM) &col);
    col.pszText = _T("Diamater");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 2, (LPARAM) &col);
    col.pszText = _T("Mass");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 3, (LPARAM) &col);

    subitem.fMask = MC_TLSIF_TEXT;

    insertSun.hParent = MC_TLI_ROOT;
    insertSun.hInsertAfter = MC_TLI_LAST;
    insertSun.item.fMask = MC_TLIF_TEXT | MC_TLIF_STATE;
    insertSun.item.pszText = _T("Sun");
    insertSun.item.state = MC_TLIS_EXPANDED;
    insertSun.item.stateMask = MC_TLIS_EXPANDED;
    hSun = (MC_HTREELISTITEM) SendMessage(hwndTreeList, MC_TLM_INSERTITEM,
                    0, (LPARAM) &insertSun);

    subitem.iSubItem = 2;
    subitem.pszText = _T("1392684.0 km");
    SendMessage(hwndTreeList, MC_TLM_SETSUBITEM, (WPARAM) hSun, (LPARAM) &subitem);

    subitem.iSubItem = 3;
    subitem.pszText = _T("1.9891e30 kg");
    SendMessage(hwndTreeList, MC_TLM_SETSUBITEM, (WPARAM) hSun, (LPARAM) &subitem);

    insertPlanet.hParent = hSun;
    insertPlanet.hInsertAfter = MC_TLI_LAST;
    insertPlanet.item.fMask = MC_TLIF_TEXT;

    for(i = 0; i < SIZEOF_ARRAY(planets); i++) {
        MC_HTREELISTITEM hPlanet;

        insertPlanet.item.pszText = (TCHAR*) planets[i].pszName;
        hPlanet = (MC_HTREELISTITEM) SendMessage(hwndTreeList,
                    MC_TLM_INSERTITEM, 0, (LPARAM) &insertPlanet);

        subitem.iSubItem = 1;
        subitem.pszText = (TCHAR*) planets[i].pszDistance;
        SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                    (WPARAM) hPlanet, (LPARAM) &subitem);

        subitem.iSubItem = 2;
        subitem.pszText = (TCHAR*) planets[i].pszDiameter;
        SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                    (WPARAM) hPlanet, (LPARAM) &subitem);

        subitem.iSubItem = 3;
        subitem.pszText = (TCHAR*) planets[i].pszMass;
        SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                    (WPARAM) hPlanet, (LPARAM) &subitem);

        insertMoon.hParent = hPlanet;
        insertMoon.hInsertAfter = MC_TLI_LAST;
        insertMoon.item.fMask = MC_TLIF_TEXT;
        for(j = 0; j < planets[i].cMoons; j++) {
            MC_HTREELISTITEM hMoon;

            insertMoon.item.pszText = (TCHAR*) planets[i].pMoons[j].pszName;
            hMoon = (MC_HTREELISTITEM) SendMessage(hwndTreeList,
                        MC_TLM_INSERTITEM, 0, (LPARAM) &insertMoon);

            subitem.iSubItem = 1;
            subitem.pszText = (TCHAR*) planets[i].pMoons[j].pszDistance;
            SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                        (WPARAM) hMoon, (LPARAM) &subitem);

            subitem.iSubItem = 2;
            subitem.pszText = (TCHAR*) planets[i].pMoons[j].pszDiameter;
            SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                        (WPARAM) hMoon, (LPARAM) &subitem);

            subitem.iSubItem = 3;
            subitem.pszText = (TCHAR*) planets[i].pMoons[j].pszMass;
            SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                        (WPARAM) hMoon, (LPARAM) &subitem);
        }
    }
}


/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_SIZE:
            if(wParam == SIZE_RESTORED  ||  wParam == SIZE_MAXIMIZED) {
                SetWindowPos(hwndTreeList, NULL, 5, 5,
                             LOWORD(lParam) - 10, HIWORD(lParam) - 10, SWP_NOZORDER);
            }
            return 0;

        case WM_SETFONT:
            SendMessage(hwndTreeList, uMsg, wParam, lParam);
            break;

        case WM_SETFOCUS:
            SetFocus(hwndTreeList);
            return 0;

        case WM_CREATE:
            /* Create the tree list view */
            hwndTreeList = CreateWindowEx(WS_EX_CLIENTEDGE, MC_WC_TREELIST, NULL,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | MC_TLS_HEADERDRAGDROP |
                    MC_TLS_HASBUTTONS | MC_TLS_HASLINES | MC_TLS_LINESATROOT |
                    MC_TLS_FULLROWSELECT, 0, 0, 0, 0, hwnd, (HMENU) IDC_TREELIST,
                    hInst, NULL);
            SetupTreeList();
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

    /* Register class of TREELIST control. */
    mcTreeList_Initialize();

    /* Prevent linker from ignoring COMCTL32.DLL. We do not use common
     * controls at all in this sample, however MCTRL.DLL enables XP styles
     * in forms for the TREELIST control only if the application uses them,
     * i.e. if the application links against COMCTL32.DLL 6 (or newer) and
     * has the manifest in resources. */
    InitCommonControls();

    /* Register main window class */
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = _T("main_window");
    RegisterClass(&wc);

    /* Create main window */
    hwndMain = CreateWindow(
        _T("main_window"), _T("mCtrl Example: TREELIST Control"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 460, 300,
        NULL, NULL, hInst, NULL
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

    mcTreeList_Terminate();

    /* Return exit code of WM_QUIT */
    return (int)msg.wParam;
}
