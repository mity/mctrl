/*
 * Copyright (c) 2008-2011 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MC_COMPAT_H
#define MC_COMPAT_H


/* This file is here to hide differences between various build environments
 * and toolchains. */


/**********************************
 *** Detect toolchain/compiler  ***
 **********************************/

#if defined __MINGW64_VERSION_MAJOR && defined __MINGW64_VERSION_MINOR
    #define MC_TOOLCHAIN_MINGW64    1
#elif defined __MINGW32__
    #define MC_TOOLCHAIN_MINGW      1
#elif defined _MSC_VER
    #define MC_TOOLCHAIN_MSVC       1
#else
    #define MC_TOOLCHAIN_OTHER      1
#endif


/*********************************
 *** MSVC compatibility hacks  ***
 *********************************/

#if defined MC_TOOLCHAIN_MSVC
    /* Disable warning C4996 ("This function or variable may be unsafe.") */
    #pragma warning( disable : 4996 )

    /* MSVC does not understand inline when building as pure C (not C++) */
    #ifndef __cplusplus
        #define inline
    #endif

    /* MS platform SDK #defines [GS]etWindowLongPtr as plain [GS]etWindowLong
     * for x86 builds, without any casting, hence causing lots of compiler
     * warnings C4312. Lets workaround it. */
    #ifndef _WIN64
        #ifdef GetWindowLongPtrA
            #undef GetWindowLongPtrA
            #define GetWindowLongPtrA(win,ix)  (intptr_t)GetWindowLongA(win,ix)
        #endif
        #ifdef GetWindowLongPtrW
            #undef GetWindowLongPtrW
            #define GetWindowLongPtrW(win,ix)  (intptr_t)GetWindowLongW(win,ix)
        #endif
        #ifdef SetWindowLongPtrA
            #undef SetWindowLongPtrA
            #define SetWindowLongPtrA(win,ix,val)  SetWindowLongA(win,ix,(LONG)val)
        #endif
        #ifdef SetWindowLongPtrW
            #undef SetWindowLongPtrW
            #define SetWindowLongPtrW(win,ix,val)  SetWindowLongW(win,ix,(LONG)val)
        #endif
    #endif
#endif


/*******************
 *** <stdint.h>  ***
 *******************/

#if defined MC_TOOLCHAIN_MINGW || defined MC_TOOLCHAIN_MINGW64
    #include <stdint.h>
#else
    /* Windows SDK/Visual Studio was missing <stdint.h> for a long time, so
     * lets have few types defined here and not rely on it. */
    typedef __int8               int8_t;
    typedef unsigned __int8     uint8_t;
    typedef __int16              int16_t;
    typedef unsigned __int16    uint16_t;
    typedef __int32              int32_t;
    typedef unsigned __int32    uint32_t;
    typedef __int64              int64_t;
    typedef unsigned __int64    uint64_t;

    #ifndef INT8_MIN
        #define INT8_MIN       (-0x7f - 1)
    #endif
    #ifndef INT16_MIN
        #define INT16_MIN      (-0x7fff - 1)
    #endif
    #ifndef INT32_MIN
        #define INT32_MIN      (-0x7fffffff - 1)
    #endif
    #ifndef INT64_MIN
        #define INT64_MIN      (-0x7fffffffffffffff - 1)
    #endif
    #ifndef INT8_MAX
        #define INT8_MAX       (0x7f)
    #endif
    #ifndef INT16_MAX
        #define INT16_MAX      (0x7fff)
    #endif
    #ifndef INT32_MAX
        #define INT32_MAX      (0x7fffffff)
    #endif
    #ifndef INT64_MAX
        #define INT64_MAX      (0x7fffffffffffffff)
    #endif
    #ifndef UINT8_MAX
        #define UINT8_MAX      (0xff)
    #endif
    #ifndef UINT16_MAX
        #define UINT16_MAX     (0xffff)
    #endif
    #ifndef UINT32_MAX
        #define UINT32_MAX     (0xffffffff)
    #endif
    #ifndef UINT64_MAX
        #define UINT64_MAX     (0xffffffffffffffff)
    #endif
#endif


/******************************************
 *** Missing constants in mingw headers ***
 ******************************************/

/* Some of them should be constants from enumeration, other are just #defines.
 * However as we cannot use preprocessor to detect if enums or their members
 * are missing, we always #define it here. */

#if defined MC_TOOLCHAIN_MINGW
    #ifndef BS_TYPEMASK
        #define BS_TYPEMASK     0x0000000fL
    #endif

    #ifndef BST_HOT
        #define BST_HOT         0x0200
    #endif

    #ifndef DT_HIDEPREFIX
        #define DT_HIDEPREFIX   0x00100000
    #endif

    #ifndef UISF_HIDEFOCUS
        #define UISF_HIDEFOCUS  0x1
    #endif

    #ifndef UISF_HIDEACCEL
        #define UISF_HIDEACCEL  0x2
    #endif

    #ifndef UIS_SET
        #define UIS_SET         1
    #endif

    #ifndef UIS_CLEAR
        #define UIS_CLEAR       2
    #endif

    #ifndef UIS_INITIALIZE
        #define UIS_INITIALIZE  3
    #endif
#endif


/********************************************
 *** Hack for broken COM headers in mingw ***
 ********************************************/

/* There is a lot of COM interfaces missing in the w32api package (as of
 * version 3.13) of mingw project. Hence we use copy of those headers
 * from mingw-w64 project which seems to be more complete.
 *
 * These are placed in the com/ subdirectory. The below are hacks
 * hiding incompatibilities between headers from mingw-w64 and mingw.
 */

#if defined MC_TOOLCHAIN_MINGW
    #define SHANDLE_PTR HANDLE_PTR
    #define __MINGW_EXTENSION
#endif


/************************************
 *** _wcstoi64() and _wcstoui64() ***
 ************************************/

/* MSVCRT.DLL on Windows 2000 lacks these symbols, so we have to provide our
 * own implementations for 32bit version of MCTRL.DLL. */
#ifndef _WIN64
    #define COMPAT_NEED_WCSTOI64   1
    int64_t compat_wcstoi64(const wchar_t *nptr, wchar_t **endptr, int base);
    #define _wcstoi64 compat_wcstoi64

    #define COMPAT_NEED_WCSTOUI64  1
    uint64_t compat_wcstoui64(const wchar_t *nptr, wchar_t **endptr, int base);
    #define _wcstoui64 compat_wcstoui64
#endif

/* mingw does not declare them in <ctype.h> and <tchar.h> */
#if defined MC_TOOLCHAIN_MINGW
    #include <tchar.h>
    #ifndef _tcstoui64
        #ifdef UNICODE
            #define _tcstoui64 _wcstoui64
        #else
            #define _tcstoui64 _strtoui64
        #endif
    #endif

    #ifndef _tcstoi64
        #ifdef UNICODE
            #define _tcstoi64 _wcstoi64
        #else
            #define _tcstoi64 _strtoi64
        #endif
    #endif
#endif


/*****************
 *** Intrinsic ***
 *****************/

/* <intrin.h> is only somewhere, so we have implement the functions we want to
 * use ourselfs. */

#if defined MC_TOOLCHAIN_MSVC  ||  defined MC_TOOLCHAIN_MINGW64
    #include <intrin.h>
#else
    #define COMPAT_NEED_STOSD      1
    void compat_stosd(unsigned long* dst, unsigned long val, size_t n);
    #define __stosd compat_stosd
#endif


#endif  /* MC_COMPAT_H */
