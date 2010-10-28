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
 
#ifndef MCTRL_VERSION_H
#define MCTRL_VERSION_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file 
 * @brief Main header file of mCtrl.
 *
 * This header includes all the other public mCtrl headers.
 */


/**
 * @brief Structure describing @c MCTRL.DLL version.
 * @see mcGetVersion
 */
typedef struct MC_VERSION_TAG {
    DWORD dwMajor;      /**< @brief Major version number. */
    DWORD dwMinor;      /**< @brief Minor version number. */
    DWORD dwRelease;    /**< @brief Release version number. */
} MC_VERSION;

/** 
 * @brief Retrieve @c MCTRL.DLL version.
 * @param[out] lpVersion Pointer to the version structure.
 *
 * @note @c MCTRL.DLL exports also the function @c DllGetVersion, as a 
 * lot of standard DLLs distributed by Microsoft. Note this function has no 
 * declaration in any mCtrl public header, as it is inteded to use with 
 * @c LoadLibrary only. Prototype of the function looks like this:
 * @code HRESULT MCTRL_API DllGetVersion(DLLVERSIONINFO* pdvi);@endcode
 * The function supports @c DLLVERSIONINFO as well as @c DLLVERSIONINFO2 
 * structure. See documentation of the function on MSDN for further info:
 * http://msdn.microsoft.com/en-us/library/bb776404%28VS.85%29.aspx
 */
void MCTRL_API mcVersion(MC_VERSION* lpVersion);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* #define MCTRL_VERSION_H */
