/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of BUTTON control. */

#include <tchar.h>
#include <windows.h>
#include <mCtrl/button.h>
#include <mCtrl/dialog.h>
#include "example-button.h"


static HINSTANCE hInst;
static HMENU hMenu;


/* Creates a popup menu, to be shown when user clicks on drop-down part
 * of the split buttons */
static void
CreateSplitMenu(void)
{
    MENUITEMINFO mii = {0};

    hMenu = CreatePopupMenu();

    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_TYPE;
    mii.fType = MFT_STRING;
    mii.dwTypeData = _T("item 1");
    InsertMenuItem(hMenu, 0, TRUE, &mii);

    mii.dwTypeData = _T("item 2");
    InsertMenuItem(hMenu, 1, TRUE, &mii);
}

/* Main dialog procedure */
static INT_PTR CALLBACK
myproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_NOTIFY:
        {
            /* React when user clicks on a drop-down part of any of the two
             * split buttons. We just show a simple menu (which does nothing).
             */
            MC_NMBCDROPDOWN* nm = (MC_NMBCDROPDOWN*)lParam;
            if((nm->hdr.idFrom == ID_BUTTON_SPLIT_1 || nm->hdr.idFrom == ID_BUTTON_SPLIT_2)  &&
               nm->hdr.code == MC_BCN_DROPDOWN)
            {
                ClientToScreen(nm->hdr.hwndFrom, ((POINT*) &nm->rcButton)+1);
                TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_LEFTBUTTON,
                               nm->rcButton.right, nm->rcButton.bottom, 0, hwndDlg, NULL);
            }
            break;
        }

        case WM_COMMAND:
        {
            /* React when user clicks on button (for split buttons, only the
             * main part of the control as drop-down is handled in WM_NOTIFY)
             */
            const TCHAR* lpStr = NULL;
            switch(LOWORD(wParam)) {
                case ID_BUTTON_ICON_1:
                    lpStr = _T("The upper BS_ICON button has been clicked.");
                    break;
                case ID_BUTTON_ICON_2:
                    lpStr = _T("The lower BS_ICON button has been clicked.");
                    break;
                case ID_BUTTON_SPLIT_1:
                    lpStr = _T("The text split button has been clicked.");
                    break;
                case ID_BUTTON_SPLIT_2:
                    lpStr = _T("The icon split button has been clicked.");
                    break;
            }
            if(lpStr)
                MessageBox(hwndDlg, lpStr, _T("mCtrl Sample"), MB_OK);
            break;
        }

        case WM_INITDIALOG:
            /* Setup icons for the buttons with BS%ICON style. */
            SendDlgItemMessage(hwndDlg, ID_BUTTON_ICON_1, BM_SETIMAGE, IMAGE_ICON,
                    (LPARAM)LoadIcon(NULL, IDI_QUESTION));
            SendDlgItemMessage(hwndDlg, ID_BUTTON_ICON_2, BM_SETIMAGE, IMAGE_ICON,
                    (LPARAM)LoadIcon(NULL, IDI_QUESTION));
            SendDlgItemMessage(hwndDlg, ID_BUTTON_SPLIT_2, BM_SETIMAGE, IMAGE_ICON,
                    (LPARAM)LoadIcon(NULL, IDI_QUESTION));
            return TRUE;

        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            break;

        default:
            return FALSE;
    }

    return TRUE;
}


int APIENTRY
_tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;
    CreateSplitMenu();

    /* Initialize mCtrl control */
    mcButton_Initialize();

    /* Load and show a dialog. */
    mcDialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG), NULL, myproc, MC_DF_DEFAULTFONT);
    return 0;
}
