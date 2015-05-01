/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of IMGVIEW control. */

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>

#include <mCtrl/imgview.h>
#include <mCtrl/dialog.h>
#include "example-imgview.h"


static HINSTANCE hInst;


/* Main window procedure */
static INT_PTR CALLBACK
DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            break;

        case WM_INITDIALOG:
            return TRUE;

        default:
            return FALSE;
    }

    return TRUE;
}


int APIENTRY
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;

    /* Initialize mCtrl control */
    if(!mcImgView_Initialize()) {
        MessageBox(NULL, _T("The function mcImgView_Initialize() has failed. ")
                   _T("Perhaps GDIPLUS.DLL is not available on your machine?"),
                   _T("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    /* Load and show a dialog. */
    mcDialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG), NULL, DlgProc, MC_DF_DEFAULTFONT);
    return 0;
}
