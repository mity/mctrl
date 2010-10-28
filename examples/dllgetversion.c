/*
 * Copyright (c) 2008-2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
