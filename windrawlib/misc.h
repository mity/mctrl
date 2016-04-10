/*
 * Copyright (c) 2016 Martin Mitas
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

#ifndef WD_MISC_H
#define WD_MISC_H


#include <float.h>
#include <math.h>
#include <tchar.h>
#include <windows.h>
#include <malloc.h>

#include "wdl.h"


/***********************
 ***  Debug Logging  ***
 ***********************/

#ifdef DEBUG
    void wd_log(const char* fmt, ...);
    #define WD_TRACE(...)       do { wd_log(__VA_ARGS__); } while(0)
#else
    #define WD_TRACE(...)       do { } while(0)
#endif

#define WD_TRACE_ERR_(msg, err)          WD_TRACE(msg " [%lu]", (err))
#define WD_TRACE_ERR(msg)                WD_TRACE(msg " [%lu]", GetLastError())
#define WD_TRACE_HR_(msg, hr)            WD_TRACE(msg " [0x%lx]", (hr))
#define WD_TRACE_HR(msg)                 WD_TRACE(msg " [0x%lx]", hr)


/**************************
 ***  Helper Functions  ***
 **************************/

#define WD_PI               3.14159265358979323846f

#define WD_MIN(a,b)         ((a) < (b) ? (a) : (b))
#define WD_MAX(a,b)         ((a) > (b) ? (a) : (b))

#define WD_ABS(a)           ((a) > 0 ? (a) : -(a))

#define WD_SIZEOF_ARRAY(a)  (sizeof((a)) / sizeof((a)[0]))

#define WD_OFFSETOF(type, member)           ((size_t) &((type*)0)->member)
#define WD_CONTAINEROF(ptr, type, member)   ((type*)((BYTE*)(ptr) - WD_OFFSETOF(type, member)))


/* Safer LoadLibrary() replacement for system DLLs. */
HMODULE wd_load_system_dll(const TCHAR* dll_name);


#endif  /* WD_MISC_H */
