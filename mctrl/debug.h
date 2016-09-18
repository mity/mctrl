/*
 * Copyright (c) 2009-2014 Martin Mitas
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

#ifndef MC_DEBUG_H
#define MC_DEBUG_H

#include "compat.h"


/*********************
 *** Debug Tracing ***
 *********************/

#if defined _MSC_VER  &&  _MSC_VER <= 1200
    /* MSVC 6.0 does not support variadic macros. */
    static inline void MC_NOOP(const char* dummy, ...)      {}
#else
    #define MC_NOOP(dummy, ...)                 do {} while(0)
#endif

#if defined DEBUG && DEBUG >= 1
    #include <windows.h>
    #include <stdlib.h>

    void MC_TRACE(const char* fmt, ...);
    void MC_DUMP(const char* msg, void* addr, size_t n);
#else
    #define MC_TRACE    MC_NOOP
    #define MC_DUMP     MC_NOOP
#endif

/* Helper for tracing message with GetLastError() or HRESULT */
#define MC_TRACE_ERR_(msg, err)          MC_TRACE(msg " [%lu]", (err))
#define MC_TRACE_ERR(msg)                MC_TRACE(msg " [%lu]", GetLastError())
#define MC_TRACE_HR_(msg, hr)            MC_TRACE(msg " [0x%lx]", (hr))
#define MC_TRACE_HR(msg)                 MC_TRACE(msg " [0x%lx]", hr)

/* Helper for tracing GUIDs */
#define MC_TRACE_GUID(msg, guid)                                              \
    MC_TRACE(msg " {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",       \
             (guid)->Data1, (guid)->Data2, (guid)->Data3,                     \
             (guid)->Data4[0], (guid)->Data4[1], (guid)->Data4[2],            \
             (guid)->Data4[3], (guid)->Data4[4], (guid)->Data4[5],            \
             (guid)->Data4[6], (guid)->Data4[7])


/******************
 *** Assertions ***
 ******************/

#if defined DEBUG && DEBUG >= 1
    /* Assertion */
    #define MC_ASSERT(cond)                                                   \
        do {                                                                  \
            if(MC_ERR(!(cond))) {                                             \
                const char msg[] = __FILE__ ":" MC_STRINGIZE(__LINE__) ": "   \
                            "Assertion '" #cond "' failed.";                  \
                MC_TRACE(msg);                                                \
                if(IsDebuggerPresent()) {                                     \
                    __debugbreak();                                           \
                } else {                                                      \
                    MessageBoxA(NULL, msg, "Assert", MB_OK);                  \
                    abort();                                                  \
                }                                                             \
            }                                                                 \
        } while(0)

    /* Static assertion */
    #if defined(MC_COMPILER_GCC)  &&  (MC_COMPILER_GCC >= 40600)
        #define MC_STATIC_ASSERT(cond)                                        \
                _Static_assert(cond, "Static assertion '" #cond "' failed.")
    #elif defined(__COUNTER__)
        #define MC_STATIC_ASSERT__(a, b)         a##b
        #define MC_STATIC_ASSERT_(a, b)          MC_STATIC_ASSERT__(a, b)
        #define MC_STATIC_ASSERT(cond)                                        \
                extern int MC_STATIC_ASSERT_(mc_static_assert_,               \
                                             __COUNTER__)[(cond) ? 1 : -1]
    #endif
#endif

/* Fallback to no-op macros. */
#ifndef MC_ASSERT
    /* Try to utilize the assertions as hints for optimization. */
    #if defined __GNUC__
        #define MC_ASSERT(cond)          \
                    do { if(!(cond)) { __builtin_unreachable(); } } while(0)
    #elif defined _MSC_VER  &&  _MSC_VER > 1200
        #include <intrin.h>
        #define MC_ASSERT(cond)          do { __assume(cond); } while(0)
    #else
        #define MC_ASSERT(cond)          do { } while(0)
    #endif
#endif
#ifndef MC_STATIC_ASSERT
    #define MC_STATIC_ASSERT(cond)       /* empty */
#endif

/* Marking code as unreachable so compiler can optimize accordingly. */
#ifndef MC_UNREACHABLE
    #define MC_UNREACHABLE               MC_ASSERT(FALSE)
#endif


/*****************************
 *** Memory Heap Debugging ***
 *****************************/

/* Functions debug_malloc() and debug_free() are used for debugging internal
 * mCtrl memory management. When used instead of malloc/free, they track
 * each allocation and deallocation, and perform some checks. Finally when
 * MCTRL.DLL is unloaded, it traces out report about detected leaks. */
#if defined DEBUG && DEBUG >= 2
    #include <malloc.h>

    void* debug_malloc(const char* fname, int line, size_t size);
    void* debug_realloc(const char* fname, int line, void* ptr, size_t size);
    void debug_free(const char* fname, int line, void* mem);

    #define malloc(size)       debug_malloc(__FILE__, __LINE__, (size))
    #define realloc(ptr, size) debug_realloc(__FILE__, __LINE__, (ptr), (size))
    #define free(ptr)          debug_free(__FILE__, __LINE__, (ptr))

    #ifdef _malloca
        #undef _malloca
    #endif
    #ifdef _freea
        #undef _freea
    #endif
    #define _malloca(size)     malloc(size)
    #define _freea(ptr)        free(ptr)
#endif


#endif  /* MC_DEBUG_H */
