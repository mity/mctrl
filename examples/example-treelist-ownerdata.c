/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */


/* This sample demonstrates usage of dynamically populated TREELIST control.
 * The technique allows to have huge tree hiearchies without eating enormous
 * amounts of memory.
 *
 * The example implements a viewer into the system registry, allocating
 * data only for items for registry keys and values of expanded nodes only.
 */


#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>

#include <mCtrl/treelist.h>
#include "example-treelist-ownerdata.h"

#define IDC_TREELIST    100

static HINSTANCE hInst;
static HIMAGELIST hImgList;
static HWND hwndTreeList;


#define IMG_REG_KEY         0
#define IMG_REG_VALUE_STR   1
#define IMG_REG_VALUE_BIN   2

static HIMAGELIST
CreateImageList(void)
{
    HIMAGELIST hImgList;

    hImgList = ImageList_Create(16, 16, ILC_COLOR32, 3, 0);
    ImageList_AddIcon(hImgList, LoadIcon(hInst, MAKEINTRESOURCE(IDI_REG_KEY)));
    ImageList_AddIcon(hImgList, LoadIcon(hInst, MAKEINTRESOURCE(IDI_REG_VALUE_STR)));
    ImageList_AddIcon(hImgList, LoadIcon(hInst, MAKEINTRESOURCE(IDI_REG_VALUE_BIN)));

    return hImgList;
}

static BOOL
KeyHasChildren(HKEY hKey)
{
    DWORD cSubkeys;
    DWORD cValues;
    LONG status;

    status = RegQueryInfoKey(hKey, NULL, NULL, NULL, &cSubkeys, NULL, NULL,
                    &cValues, NULL, NULL, NULL, NULL);
    if(status != ERROR_SUCCESS)
        return FALSE;

    return (cSubkeys > 0 || cValues > 0);
}

static TCHAR*
ValueTypeName(DWORD dwType)
{
    switch(dwType) {
        case REG_NONE:                 return _T("None");
        case REG_BINARY:               return _T("Binary");
        case REG_LINK:                 return _T("Link");
        case REG_DWORD:                return _T("Dword");
        case REG_DWORD_BIG_ENDIAN:     return _T("Dword (BE)");
        case REG_QWORD:                return _T("Qword");
        case REG_SZ:                   return _T("String");
        case REG_EXPAND_SZ:            return _T("String (expand)");
        case REG_MULTI_SZ:             return _T("String (multi)");
        default:                       return _T("???");
    }
}


#define SWAP32(dw)   (((dw) & 0x000000ff) << 24  |  ((dw) & 0x0000ff00) << 8  |   \
                      ((dw) & 0x00ff0000) >> 8   |  ((dw) & 0xff000000) >> 24)

static void
StringizeData(TCHAR* pszBuffer, DWORD dwBufferLen, DWORD dwType, BYTE* data, DWORD dwDataLen)
{
    switch(dwType) {
        case REG_NONE:
            pszBuffer[0] = _T('\0');
            break;
        case REG_DWORD:
        case REG_DWORD_BIG_ENDIAN:
        {
            DWORD dw = *((DWORD*)data);
            if(dwType == REG_DWORD_BIG_ENDIAN)
                dw = SWAP32(dw);
            _sntprintf(pszBuffer, dwBufferLen, _T("%u (0x%x)"), dw, dw);
            break;
        }

        case REG_QWORD:
        {
            ULONGLONG qw = *((ULONGLONG*)data);
            _sntprintf(pszBuffer, dwBufferLen, _T("%I64u (0x%I64x)"), qw, qw);
            break;
        }

        case REG_SZ:
        case REG_LINK:
        case REG_EXPAND_SZ:
        case REG_MULTI_SZ:
            /* For REG_MULTI_SZ we show only the 1st string. It is enough for
             * purposes of this example. */
            _sntprintf(pszBuffer, dwBufferLen, _T("\"%.*s\""),
                           dwDataLen / sizeof(TCHAR), (TCHAR*) data);
            pszBuffer[dwBufferLen-1] = _T('\0');
            break;

        case REG_BINARY:
        default:
        {
            DWORD i;
            DWORD dwOffset;

            dwOffset = _sntprintf(pszBuffer, dwBufferLen, _T("[%d bytes]"), (int)dwDataLen);
            for(i = 0; i < dwDataLen; i++) {
                if(dwOffset >= dwBufferLen - _tcslen(_T(" 0xAB")) - 1)
                    break;
                _sntprintf(pszBuffer + dwOffset, dwBufferLen - dwDataLen,
                           _T(" 0x%0x"), data[i]);
                dwOffset += _tcslen(_T(" 0xAB"));
            }
            break;
        }
    }
}

static void
SetupTreeList(void)
{
    const struct {
        HKEY hKey;
        TCHAR* pszName;
    } rootKeys[] = {
        { HKEY_CLASSES_ROOT,   _T("HKEY_CLASSES_ROOT") },
        { HKEY_CURRENT_CONFIG, _T("HKEY_CURRENT_CONFIG") },
        { HKEY_CURRENT_USER,   _T("HKEY_CURRENT_USER") },
        { HKEY_LOCAL_MACHINE,  _T("HKEY_LOCAL_MACHINE") },
        { HKEY_USERS,          _T("HKEY_USERS") }
    };

    MC_TLCOLUMN col;
    MC_TLINSERTSTRUCT insert;
    int i, n;

    /* Use EXPLORER window theme class */
    SendMessage(hwndTreeList, CCM_SETWINDOWTHEME, 0, (LPARAM) L"Explorer");

    /* Associate image list with the control */
    SendMessage(hwndTreeList, MC_TLM_SETIMAGELIST, 0, (LPARAM) hImgList);

    /* Insert columns */
    col.fMask = MC_TLCF_TEXT | MC_TLCF_WIDTH;
    col.cx = 250;
    col.pszText = _T("Key/Value name");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 0, (LPARAM) &col);
    col.cx = 65;
    col.pszText = _T("Type");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 1, (LPARAM) &col);
    col.cx = 180;
    col.pszText = _T("Data");
    SendMessage(hwndTreeList, MC_TLM_INSERTCOLUMN, 2, (LPARAM) &col);

    /* Insert root items */
    insert.hParent = MC_TLI_ROOT;
    insert.hInsertAfter = MC_TLI_LAST;
    insert.item.fMask = MC_TLIF_TEXT | MC_TLIF_LPARAM | MC_TLIF_CHILDREN |
                        MC_TLIF_IMAGE | MC_TLIF_SELECTEDIMAGE | MC_TLIF_EXPANDEDIMAGE;
    insert.item.iImage = IMG_REG_KEY;
    insert.item.iSelectedImage = IMG_REG_KEY;
    insert.item.iExpandedImage = IMG_REG_KEY;
    n = sizeof(rootKeys) / sizeof(rootKeys[0]);
    for(i = 0; i < n; i++) {
        insert.item.pszText = rootKeys[i].pszName;
        insert.item.lParam = (LPARAM) rootKeys[i].hKey;
        /* We may need to indicate it is a parent item even though we do not
         * insert and child items. This tells the control to display the
         * expand/collapse button next to the item. When user clicks on it,
         * we will add the child items dynamically. */
        insert.item.cChildren = (KeyHasChildren(rootKeys[i].hKey) ? 1 : 0);
        SendMessage(hwndTreeList, MC_TLM_INSERTITEM, 0, (LPARAM) &insert);
    }
}

static void
InsertChildren(MC_HTREELISTITEM hItem, HKEY hKey)
{
    MC_TLINSERTSTRUCT insert;
    MC_TLSUBITEM subitem;
    DWORD dwIndex;
    LONG status;
    DWORD dwBufferLen;
    TCHAR pszBuffer[260];

    /* This makes this noop for values, if user tries to expand them e.g.
     * by double-click. */
    if(hKey == NULL)
        return;

    insert.hParent = hItem;
    insert.hInsertAfter = MC_TLI_LAST;
    insert.item.fMask = MC_TLIF_TEXT | MC_TLIF_LPARAM | MC_TLIF_CHILDREN |
                        MC_TLIF_IMAGE | MC_TLIF_SELECTEDIMAGE | MC_TLIF_EXPANDEDIMAGE;

    SendMessage(hwndTreeList, WM_SETREDRAW, FALSE, 0);

    /* Enumerate child keys */
    dwIndex = 0;
    while(TRUE) {
        HKEY hSubKey;

        dwBufferLen = 260;
        status = RegEnumKeyEx(hKey, dwIndex, pszBuffer, &dwBufferLen, NULL, NULL, NULL, NULL);
        dwIndex++;
        if(status != ERROR_SUCCESS)
            break;

        status = RegOpenKeyEx(hKey, pszBuffer, 0, KEY_READ, &hSubKey);
        if(status != ERROR_SUCCESS)
            continue;

        insert.item.pszText = pszBuffer;
        insert.item.lParam = (LPARAM) hSubKey;
        insert.item.cChildren = (KeyHasChildren(hSubKey) ? 1 : 0);
        insert.item.iImage = IMG_REG_KEY;
        insert.item.iSelectedImage = IMG_REG_KEY;
        insert.item.iExpandedImage = IMG_REG_KEY;
        SendMessage(hwndTreeList, MC_TLM_INSERTITEM, 0, (LPARAM) &insert);
    }

    /* Enumerate values */
    dwIndex = 0;
    while(TRUE) {
        DWORD dwType;
        BYTE data[512];
        DWORD dwDataLen;
        MC_HTREELISTITEM hChildItem;

        dwDataLen = 512;
        status = RegEnumValue(hKey, dwIndex, pszBuffer, &dwBufferLen, NULL,
                              &dwType, data, &dwDataLen);
        dwIndex++;
        if(status != ERROR_SUCCESS) {
            if(status == ERROR_NO_MORE_ITEMS)
                break;
            else
                continue;
        }

        insert.item.pszText = (dwBufferLen > 0  ?  pszBuffer  :  _T("<default>"));
        insert.item.lParam = 0;
        insert.item.cChildren = 0;
        if(dwType == REG_EXPAND_SZ || dwType == REG_MULTI_SZ || dwType == REG_SZ)
            insert.item.iImage = IMG_REG_VALUE_STR;
        else
            insert.item.iImage = IMG_REG_VALUE_BIN;
        insert.item.iSelectedImage = insert.item.iImage;
        insert.item.iExpandedImage = insert.item.iImage;
        hChildItem = (MC_HTREELISTITEM) SendMessage(hwndTreeList,
                                    MC_TLM_INSERTITEM, 0, (LPARAM) &insert);

        subitem.fMask = MC_TLSIF_TEXT;
        subitem.iSubItem = 1;
        subitem.pszText = ValueTypeName(dwType);
        SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                                    (WPARAM) hChildItem, (LPARAM) &subitem);

        StringizeData(pszBuffer, 260, dwType, data, dwDataLen);
        subitem.iSubItem = 2;
        subitem.pszText = pszBuffer;
        SendMessage(hwndTreeList, MC_TLM_SETSUBITEM,
                                    (WPARAM) hChildItem, (LPARAM) &subitem);
    }

    SendMessage(hwndTreeList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndTreeList, NULL, TRUE);
}

static LRESULT
OnTreeListNotify(NMHDR* hdr)
{
    switch(hdr->code) {
        case MC_TLN_EXPANDING:
        {
            /* This is core of the logic for dynamic populating of the control.
             * We only insert child items when their parent is being expanded,
             * and delete them when it is being collapsed. */
            MC_NMTREELIST* nm = (MC_NMTREELIST*) hdr;
            if(nm->action == MC_TLE_EXPAND) {
                InsertChildren(nm->hItemNew, (HKEY) nm->lParamNew);
            } else if(nm->action == MC_TLE_COLLAPSE) {
                SendMessage(hwndTreeList, MC_TLM_EXPAND,
                            MC_TLE_COLLAPSE | MC_TLE_COLLAPSERESET,
                            (LPARAM) nm->hItemNew);
            }
            break;
        }

        case MC_TLN_DELETEITEM:
        {
            /* If the item being deleted is registry key, we need to close its
             * handle. */
            MC_NMTREELIST* nm = (MC_NMTREELIST*) hdr;
            HKEY hKey = (HKEY) nm->lParamOld;
            if(hKey != NULL) {
                MC_HTREELISTITEM hParent;

                /* The root items use the special key constants, so we cannot
                 * close those. */
                hParent = (MC_HTREELISTITEM) SendMessage(hwndTreeList,
                                MC_TLM_GETNEXTITEM, MC_TLGN_PARENT, (LPARAM) nm->hItemOld);
                if(hParent != NULL)
                    RegCloseKey(hKey);
            }
            break;
        }
    }

    return 0;
}


/* Main window procedure */
static LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_NOTIFY:
        {
            NMHDR* hdr = (NMHDR*) lParam;
            if(hdr->hwndFrom == hwndTreeList)
                return OnTreeListNotify(hdr);
            return 0;
        }

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
            /* Create the property list view */
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
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = { 0 };
    HWND hwndMain;
    MSG msg;

    hInst = hInstance;

    /* Create image list */
    hImgList = CreateImageList();

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
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 550, 300,
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
