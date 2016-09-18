/*
 * Copyright (c) 2008-2015 Martin Mitas
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


/* This file is here to hide differences and incompatibles among various
 * build environments and toolchains. */


/**********************************
 *** Detect toolchain/compiler  ***
 **********************************/

#if defined __MINGW64_VERSION_MAJOR && defined __MINGW64_VERSION_MINOR
    #define MC_TOOLCHAIN_MINGW64    1
#elif defined _MSC_VER
    #define MC_TOOLCHAIN_MSVC       1
#else
    #define MC_TOOLCHAIN_OTHER      1
#endif

#if defined __GNUC__
    #define MC_COMPILER_GCC         (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined _MSC_VER
    #define MC_COMPILER_MSVC        _MSC_VER
#else
    #define MC_COMPILER_OTHER       1
#endif


/*******************
 *** <stdint.h>  ***
 *******************/

#if defined MC_COMPILER_MSVC  &&  MC_COMPILER_MSVC < 1600
    /* Old MSVC versions miss <stdint.h> so lets provide our own (incomplete)
     * replacement. */
    typedef signed __int8  int8_t;
    typedef signed __int16 int16_t;
    typedef signed __int32 int32_t;
    typedef signed __int64 int64_t;

    typedef signed long intptr_t;

    typedef unsigned __int8  uint8_t;
    typedef unsigned __int16 uint16_t;
    typedef unsigned __int32 uint32_t;
    typedef unsigned __int64 uint64_t;

    typedef unsigned long uintptr_t;

    #define INT8_MIN  (-0x7f - 1)
    #define INT16_MIN (-0x7fff - 1)
    #define INT32_MIN (-0x7fffffff - 1)
    #define INT64_MIN (-0x7fffffffffffffff - 1)

    #define INT8_MAX  (0x7f)
    #define INT16_MAX (0x7fff)
    #define INT32_MAX (0x7fffffff)
    #define INT64_MAX (0x7fffffffffffffff)

    #define UINT8_MAX  (0xff)
    #define UINT16_MAX (0xffff)
    #define UINT32_MAX (0xffffffff)
    #define UINT64_MAX (0xffffffffffffffff)
#else
    #include <stdint.h>
#endif


/***************************
 *** restrict specifier  ***
 ***************************/

/* "restrict" has been introduced in C99, but gcc and MSVC (version 2013 and
 * newer) provide __restrict as compiler extension. We use it throughout our
 * code for better optimizations. */
#if !defined(__STDC_VERSION__)  ||  __STDC_VERSION__ < 199901L
    #if defined MC_COMPILER_GCC
        #define restrict __restrict
    #elif defined MC_COMPILER_MSVC  &&  MC_COMPILER_MSVC >= 1700
        #define restrict __restrict
    #else
        #define restrict
    #endif
#endif


/*********************************
 *** MSVC compatibility hacks  ***
 *********************************/

#if defined MC_COMPILER_MSVC
    /* Disable warning C4018 ("'>' : signed/unsigned mismatch") */
    #pragma warning( disable : 4018 )

    /* Disable warning C4244 ("conversion from 'LONG' to 'WORD', possible loss of data.") */
    #pragma warning( disable : 4244 )

    /* Disable warning C4244 ("conversion from 'size_t' to 'UINT', possible loss of data.") */
    #pragma warning( disable : 4267 )

    /* Disable warning C4996 ("This function or variable may be unsafe.") */
    #pragma warning( disable : 4996 )

    /* MSVC does not understand "inline" when building as pure C (not C++).
     * However it understands "__inline" */
    #ifndef __cplusplus
        #define inline __inline
    #endif

    /* MS platform SDK #defines [GS]etWindowLongPtr as plain [GS]etWindowLong
     * for x86 builds without any casting, hence causing lots of compiler
     * warnings C4312. Lets workaround it. */
    #ifndef _WIN64
        #ifdef GetWindowLongPtrA
            #undef GetWindowLongPtrA
            #define GetWindowLongPtrA(win,ix)      ((intptr_t)GetWindowLongA((win),(ix)))
        #endif
        #ifdef GetWindowLongPtrW
            #undef GetWindowLongPtrW
            #define GetWindowLongPtrW(win,ix)      ((intptr_t)GetWindowLongW((win),(ix)))
        #endif
        #ifdef SetWindowLongPtrA
            #undef SetWindowLongPtrA
            #define SetWindowLongPtrA(win,ix,val)  SetWindowLongA((win),(ix),(LONG)(val))
        #endif
        #ifdef SetWindowLongPtrW
            #undef SetWindowLongPtrW
            #define SetWindowLongPtrW(win,ix,val)  SetWindowLongW((win),(ix),(LONG)(val))
        #endif
    #endif

    #include <math.h>

    #if MC_COMPILER_MSVC <= 1200
        /* With MSVC 6.0, these are missing in <malloc.h>. */
        #define _malloca    malloc
        #define _freea      free

        /* With MSVC 6.0, these are missing in <math.h>. */
        static inline float floorf(float x)             { return (float)floor((double)x); }
        static inline float ceilf(float x)              { return (float)ceil((double)x); }
        static inline float powf(float x, float y)      { return (float)pow((double)x, (double)y); }
        static inline float cosf(float x)               { return (float)cos((double)x); }
        static inline float sinf(float x)               { return (float)sin((double)x); }
        static inline float atan2f(float x, float y)    { return (float)atan2((double)x, (double)y); }
        static inline float sqrtf(float x)              { return (float)sqrt((double)x); }

        /* With MSVC 6.0, these is swprintf() and not _swprintf(). */
        #undef _swprintf
        #define _swprintf swprintf

        /* With MSVC 6.0, we have to use AlphaBlend() et al. from MSIMG32.DLL,
         * instead of GdiAlphaBlend() et al. from GDI32.DLL. */
        #define GdiAlphaBlend AlphaBlend
    #endif

    #if MC_COMPILER_MSVC < 1700
        /* MSVC older then 2013 does not know cbrtf() */
        static inline float cbrtf(float x)
            { return powf(x, 1.0f / 3.0f); }

        /* Windows 7.1 SDK lacks some constants. */
        #include <windows.h>
        #ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
            #define LOAD_LIBRARY_SEARCH_SYSTEM32    0x800
        #endif
        #include <commctrl.h>
        #ifndef TB_SETBOUNDINGSIZE
            #define TB_SETBOUNDINGSIZE              (WM_USER+93)
        #endif
    #endif

    /* MSVC does not know roundf() */
    static inline float roundf(float x)
        { return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f); }

    /* With recent SDK versions, <shlwapi.h> started to #undefine COM C wrapper
     * macros IStream_Read and IStream_Write and instead it provides its own
     * functions of the same name (but with incompatible interface).
     * We cannot use the functions (they are only available since Vista) so
     * lets resuscitate the macros.
     */
    #ifdef COBJMACROS
        #include <shlwapi.h>
        #ifndef IStream_Read
            #define IStream_Read(self,a,b,c)   ((self)->lpVtbl->Read(self,a,b,c))
        #endif
        #ifndef IStream_Write
            #define IStream_Write(self,a,b,c)  ((self)->lpVtbl->Write(self,a,b,c))
        #endif
    #endif
#endif


/*************************************
 *** Mingw-w64 compatibility hacks ***
 *************************************/

#ifdef MC_TOOLCHAIN_MINGW64
    /* In some mingw-w64 versions, WM_DWMCOMPOSITIONCHANGED is located in
     * incorrect header. */
    #include <winuser.h>
    #ifndef WM_DWMCOMPOSITIONCHANGED
        #include <dwmapi.h>
    #endif

    /* In some mingw-w64 versions, the following constants are missing. */
    #include <commctrl.h>
    #ifndef TB_SETBOUNDINGSIZE
        #define TB_SETBOUNDINGSIZE          (WM_USER + 93)
    #endif
    #ifndef TB_SETPRESSEDIMAGELIST
        #define TB_SETPRESSEDIMAGELIST      (WM_USER + 104)
    #endif
#endif


/************************************
 *** _wcstoi64() and _wcstoui64() ***
 ************************************/

/* MSVCRT.DLL on Windows 2000 lacks these symbols, so in 32-bit version we
 * always use own implementation. */
#ifndef _WIN64
    /* Include system header prior to the (re)definition, otherwise
     * we may cause some side-effects inside of it. */
    #include <wchar.h>

    #define COMPAT_NEED_WCSTOI64   1
    int64_t compat_wcstoi64(const wchar_t *nptr, wchar_t **endptr, int base);
    #define _wcstoi64 compat_wcstoi64

    #define COMPAT_NEED_WCSTOUI64  1
    uint64_t compat_wcstoui64(const wchar_t *nptr, wchar_t **endptr, int base);
    #define _wcstoui64 compat_wcstoui64
#endif


#endif  /* MC_COMPAT_H */
