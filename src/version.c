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

#include "version.h"
#include "mCtrl/version.h"
#include "misc.h"


void MCTRL_API
mcVersion(MC_VERSION* version)
{
    version->dwMajor = MC_VERSION_MAJOR;
    version->dwMinor = MC_VERSION_MINOR;
    version->dwRelease = MC_VERSION_RELEASE;
}

HRESULT MCTRL_API
DllGetVersion(union { DWORD cbSize;
                      DLLVERSIONINFO info1;
                      DLLVERSIONINFO2 info2; }* dvi)
{
    switch(dvi->cbSize) {
        case sizeof(DLLVERSIONINFO2):
            dvi->info2.dwFlags = 0;
            dvi->info2.ullVersion = MAKEDLLVERULL(MC_VERSION_MAJOR,
                            MC_VERSION_MINOR, MC_VERSION_RELEASE, 0);
            /* no break, fall through */
            
        case sizeof(DLLVERSIONINFO):
            dvi->info1.dwMajorVersion = MC_VERSION_MAJOR;
            dvi->info1.dwMinorVersion = MC_VERSION_MINOR;
            dvi->info1.dwBuildNumber = MC_VERSION_RELEASE;
#ifdef UNICODE
            dvi->info1.dwPlatformID = DLLVER_PLATFORM_NT;
#else
            dvi->info1.dwPlatformID = DLLVER_PLATFORM_WINDOWS;
#endif
            return S_OK;

        default:
            MC_TRACE("DllGetVersion: invalid cbSize");
            return E_INVALIDARG;
    }
}
