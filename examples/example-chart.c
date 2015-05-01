/*
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been released into the public domain.
 */

/* This sample demonstrates basic usage of CHART control. */

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>

#include <mCtrl/chart.h>
#include <mCtrl/dialog.h>
#include "example-chart.h"


static HINSTANCE hInst;


static void
SetupPieChart(HWND hwndChart)
{
    const struct {
        const TCHAR* name;
        int value;
    } data[] = {
        { _T("Work"),    11 },
        { _T("Eat"),      2 },
        { _T("Commute"),  2 },
        { _T("Watch TV"), 2 },
        { _T("Sleep"),    7 }
    };
    MC_CHDATASET dataSet;
    int i;

    SetWindowText(hwndChart, _T("Daily Activities"));

    for(i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
        /* Pie chart expects only one (non-negative) value per data set. */
        dataSet.dwCount = 1;
        dataSet.piValues = (int*) &data[i].value;
        SendMessage(hwndChart, MC_CHM_INSERTDATASET, i, (LPARAM) &dataSet);
        SendMessage(hwndChart, MC_CHM_SETDATASETLEGEND, i, (LPARAM) data[i].name);
    }
}

static void
SetupScatterChart(HWND hwndChart)
{
    /* Note that each data set has even size. They are in fact interpreted
     * as a sets of [X,Y] pairs by the scatter chart. */
    const int maleData[] =   { 185,  97,   169,  70,   182,  83,   191, 102,
                               174,  87,   163,  98,   201,  98 };

    const int femaleData[] = { 155,  51,   178,  65,   170,  74,   162,  44,
                               181,  88,   158,  54,   163,  49,   163,  56 };
    MC_CHDATASET dataSet;

    SetWindowText(hwndChart, _T("Height vs. Weight"));

    SendMessage(hwndChart, MC_CHM_SETAXISLEGEND, 1, (LPARAM) _T("Height [cm]"));
    SendMessage(hwndChart, MC_CHM_SETAXISLEGEND, 2, (LPARAM) _T("Weight [kg]"));

    dataSet.dwCount = sizeof(maleData) / sizeof(maleData[0]);
    dataSet.piValues = (int*) maleData;
    SendMessage(hwndChart, MC_CHM_INSERTDATASET, 0, (LPARAM) &dataSet);
    SendMessage(hwndChart, MC_CHM_SETDATASETLEGEND, 0, (LPARAM) _T("Males"));
    SendMessage(hwndChart, MC_CHM_SETDATASETCOLOR, 0, RGB(0,0,200));

    dataSet.dwCount = sizeof(femaleData) / sizeof(femaleData[0]);
    dataSet.piValues = (int*) femaleData;
    SendMessage(hwndChart, MC_CHM_INSERTDATASET, 1, (LPARAM) &dataSet);
    SendMessage(hwndChart, MC_CHM_SETDATASETLEGEND, 1, (LPARAM) _T("Females"));
    SendMessage(hwndChart, MC_CHM_SETDATASETCOLOR, 1, RGB(200,0,0));
}

static void
SetupCommonChart(HWND hwndChart)
{
    const int austriaData[] = { 1336060, 1538156, 1576579, 1600652, 1968113 };
    const int denmarkData[] = { 1001582, 1119450,  993360, 1004163,  979198 };
    const int greeceData[] =  {  997974,  941795,  930593,  897127, 1080887 };

    MC_CHDATASET dataSet;

    SetWindowText(hwndChart, _T("Yearly Coffee Consumption by Country"));

    SendMessage(hwndChart, MC_CHM_SETAXISLEGEND, 1, (LPARAM) _T("Year"));
    SendMessage(hwndChart, MC_CHM_SETAXISLEGEND, 2, (LPARAM) _T("Amount [tons]"));

    /* The data are since year 2003 */
    SendMessage(hwndChart, MC_CHM_SETAXISOFFSET, 1, 2003);

    dataSet.dwCount = sizeof(austriaData) / sizeof(austriaData[0]);
    dataSet.piValues = (int*) austriaData;
    SendMessage(hwndChart, MC_CHM_INSERTDATASET, 0, (LPARAM) &dataSet);
    SendMessage(hwndChart, MC_CHM_SETDATASETLEGEND, 0, (LPARAM) _T("Austria"));

    dataSet.dwCount = sizeof(denmarkData) / sizeof(denmarkData[0]);
    dataSet.piValues = (int*) denmarkData;
    SendMessage(hwndChart, MC_CHM_INSERTDATASET, 1, (LPARAM) &dataSet);
    SendMessage(hwndChart, MC_CHM_SETDATASETLEGEND, 1, (LPARAM) _T("Denmark"));

    dataSet.dwCount = sizeof(greeceData) / sizeof(greeceData[0]);
    dataSet.piValues = (int*) greeceData;
    SendMessage(hwndChart, MC_CHM_INSERTDATASET, 2, (LPARAM) &dataSet);
    SendMessage(hwndChart, MC_CHM_SETDATASETLEGEND, 2, (LPARAM) _T("Greece"));
}

/* Main window procedure */
static INT_PTR CALLBACK
DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            break;

        case WM_INITDIALOG:
            SetupPieChart(GetDlgItem(hwndDlg, IDC_CHART_PIE));
            SetupScatterChart(GetDlgItem(hwndDlg, IDC_CHART_SCATTER));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_LINE));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_AREA));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_STACKEDAREA));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_COLUMN));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_STACKEDCOLUMN));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_BAR));
            SetupCommonChart(GetDlgItem(hwndDlg, IDC_CHART_STACKEDBAR));
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
    if(!mcChart_Initialize()) {
        MessageBox(NULL, _T("The function mcChart_Initialize() has failed. ")
                   _T("Perhaps GDIPLUS.DLL is not available on your machine?"),
                   _T("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    /* Load and show a dialog. */
    mcDialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG), NULL, DlgProc, MC_DF_DEFAULTFONT);
    return 0;
}
