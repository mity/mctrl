/*
 * Copyright (c) 2008-2011 Martin Mitas
 *
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been placed in the public domain.
 */

/* This sample demonstrates basic usage of BUTTON control. */

#include <windows.h>
#include <tchar.h>
#include <mCtrl/button.h>


static HINSTANCE inst;
static HMENU menu;


/* Creates a popup menu, to be shown when user clicks on drop-down part
 * of the split buttons */
static void
create_menu(void)
{
    MENUITEMINFO mii;

    menu = CreatePopupMenu();
    
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_TYPE;
    mii.fType = MFT_STRING;
    mii.dwTypeData = _T("item 1");
    InsertMenuItem(menu, 0, TRUE, &mii);

    mii.dwTypeData = _T("item 2");
    InsertMenuItem(menu, 1, TRUE, &mii);
}


static HWND button_icon1;
static HWND button_icon2;
static HWND button_split1;
static HWND button_split2;


/* Main dialog procedure */
static CALLBACK BOOL
dlg_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
        case WM_NOTIFY:
        {
            /* React when user clicks on drop-down of a split button. */
            MC_NMBCDROPDOWN* notify = (MC_NMBCDROPDOWN*)lp;
            if((notify->hdr.idFrom == 102  ||  notify->hdr.idFrom == 103)  &&
                notify->hdr.code == MC_BCN_DROPDOWN) {
                ClientToScreen(notify->hdr.hwndFrom, ((POINT*) &notify->rcButton)+1);
                TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_LEFTBUTTON, 
                        notify->rcButton.right, notify->rcButton.bottom, 0, win, NULL);
            }
            break;
        }
            
        case WM_COMMAND:
        {
            /* React when user clicks on button 
             * (for split buttons, only the main part of the control) */
            const TCHAR* msg = NULL;
            switch(LOWORD(wp)) {
                case 100: 
                    msg = _T("The upper BS_ICON button has been clicked."); 
                    break;
                case 101: 
                    msg = _T("The lower BS_ICON button has been clicked.");
                    break;
                case 102: 
                    msg = _T("The text split button has been clicked.");
                    break;
                case 103: 
                    msg = _T("The icon split button has been clicked.");
                    break;
            }
            if(msg)
                MessageBox(win, msg, _T("mCtrl Sample"), MB_OK);
            break;
        }       
        
        case WM_INITDIALOG:
            /* Get handles of the child controls */
            button_icon1 = GetDlgItem(win, 100);
            button_icon2 = GetDlgItem(win, 101);
            button_split1 = GetDlgItem(win, 102);
            button_split2 = GetDlgItem(win, 103);

            /* Set some icon of the icon buttons. */
            SendMessage(button_icon1, BM_SETIMAGE, IMAGE_ICON, 
                    (LPARAM)LoadIcon(NULL, IDI_QUESTION));
            SendMessage(button_icon2, BM_SETIMAGE, IMAGE_ICON, 
                    (LPARAM)LoadIcon(NULL, IDI_QUESTION));
            SendMessage(button_split2, BM_SETIMAGE, IMAGE_ICON, 
                    (LPARAM)LoadIcon(NULL, IDI_QUESTION ));
            return TRUE;
        
        case WM_CLOSE:  
            EndDialog(win, 0);
            break;
            
        default:
            return FALSE;
    }
    
    return TRUE;
}


int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE instance_prev, LPSTR cmd_line, int cmd_show)
{
    inst = instance;
    create_menu();
    
    /* Initialize mCtrl control */
    mcButton_Initialize();
    
    /* Load and show a dialog. */
    DialogBox(inst, MAKEINTRESOURCE(1000), NULL, dlg_proc);
    return 0;
}
