/*
 * Copyright (c) 2008-2012 Martin Mitas
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


/* This file is here to hide differences and incompabilties among various
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
    typedef __int8  int8_t;
    typedef __int16 int16_t;
    typedef __int32 int32_t;
    typedef __int64 int64_t;

    typedef unsigned __int8  uint8_t;
    typedef unsigned __int16 uint16_t;
    typedef unsigned __int32 uint32_t;
    typedef unsigned __int64 uint64_t;

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


/*********************************
 *** MSVC compatibility hacks  ***
 *********************************/

#if defined MC_COMPILER_MSVC
    /* Disable warning C4018 ("'>' : signed/unsigned mismatch") */
    #pragma warning( disable : 4018 )

    /* Disable warning C4244 ("conversion from 'LONG' to 'WORD', possible loss of data.") */
    #pragma warning( disable : 4244 )

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
#endif


/*************************************
 *** Mingw-w64 compatibility hacks ***
 *************************************/

/* <wingdi.h> misses this prototype */
#ifdef MC_TOOLCHAIN_MINGW64
    BOOL WINAPI __declspec(dllimport) GdiAlphaBlend(
                            HDC dc1, int x1, int y1, int w1, int h1,
                            HDC dc0, int x0, int y0, int w0, int h0,
                            BLENDFUNCTION fn);
#endif


/* various missing constants */
#ifndef TB_SETBOUNDINGSIZE
    #define TB_SETBOUNDINGSIZE      (WM_USER + 93)
#endif

#ifndef TB_SETPRESSEDIMAGELIST
    #define TB_SETPRESSEDIMAGELIST  (WM_USER + 104)
#endif


/************************************
 *** _wcstoi64() and _wcstoui64() ***
 ************************************/

/* MSVCRT.DLL on Windows 2000 lacks these symbols, so in 32-bit version we
 * always use own implementation. */
#ifndef _WIN64
    #define COMPAT_NEED_WCSTOI64   1
    int64_t compat_wcstoi64(const wchar_t *nptr, wchar_t **endptr, int base);
    #define _wcstoi64 compat_wcstoi64

    #define COMPAT_NEED_WCSTOUI64  1
    uint64_t compat_wcstoui64(const wchar_t *nptr, wchar_t **endptr, int base);
    #define _wcstoui64 compat_wcstoui64
#endif


/*****************
 *** Intrinsic ***
 *****************/

/* <intrin.h> is only provided by some toolchains and many functions are
 * available only for some architectures, so we may need to provide fallback
 * implementations for those few functions we use. */

#if defined MC_TOOLCHAIN_MSVC  ||  defined MC_TOOLCHAIN_MINGW64
    #include <intrin.h>
    #define MC_HAVE_INTRIN_H
#endif


static inline void
mc_stosd(uint32_t* dst, uint32_t val, size_t n)
{
#ifdef MC_HAVE_INTRIN_H
    __stosd((unsigned long*) dst, (unsigned long)val, n);
#else
    size_t i;
    for(i = 0; i < n; i++)
        dst[i] = val;
#endif
}

static inline unsigned
mc_clz(uint32_t val)
{
#ifdef MC_COMPILER_GCC
    return __builtin_clz(val);
#else
    unsigned n = 0;
    while(val > 0) {
        val = val >> 1;
        n++;
    }
    return (32 - n);
#endif
}


#endif  /* MC_COMPAT_H */
