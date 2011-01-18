/*
 * Copyright (c) 2008-2011 Martin Mitas
 *
 * This file contains example code for mCtrl library. Code of this example
 * (but not the library itself) has been placed in the public domain.
 */
 
/* This sample demonstrates how to get mCtrl.dll version through the de-facto
 * Windowish standard way with DllGetVersion() function.
 *
 * See MSDN for some more information about DllGetVersion: 
 * http://msdn.microsoft.com/en-us/library/bb776404%28VS.85%29.aspx
 */

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <tchar.h>


int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE instance_prev, 
        LPSTR cmd_line, int cmd_show)
{
    HMODULE dll;
    DLLGETVERSIONPROC fn_DllGetVersion;
    DLLVERSIONINFO vi;
    TCHAR buffer[64];
    
    /* Load mCtrl.dll */
    dll = LoadLibrary(_T("mCtrl.dll"));
    if(dll == NULL) {
        MessageBox(NULL, _T("Cannot load mCtrl.dll library."), 
                _T("mCtrl Sample: DllGetVersion"), MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Get DllGetVersion function address */
    fn_DllGetVersion = (DLLGETVERSIONPROC) GetProcAddress(dll, "DllGetVersion");
    if(fn_DllGetVersion == NULL) {
        MessageBox(NULL, _T("Cannot get DllGetVersion function."), 
                _T("mCtrl Sample: DllGetVersion"), MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Call the function to get the version */
    vi.cbSize = sizeof(DLLVERSIONINFO);
    if(fn_DllGetVersion(&vi) != S_OK) {
        MessageBox(NULL, _T("DllGetVersion failed."), _T("Error"), 
                MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Show the results */
    _sntprintf(buffer, 64, _T("Detected mCtrl.dll version %u.%u.%u"), 
                vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    MessageBox(NULL, buffer, _T("mCtrl Sample: DllGetVersion"), MB_OK);
    return 0;
}
