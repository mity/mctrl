/*
 * Copyright (c) 2009-2013 Martin Mitas
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


#if defined DEBUG && DEBUG >= 1
    #include <windows.h>
    #include <stdlib.h>
    #include <stdio.h>  /* _snprintf() */

    void debug_dump(void* addr, size_t n);

    /* Assertion */
    #define MC_ASSERT(cond)                                                   \
        do {                                                                  \
            if(!(cond)) {                                                     \
                OutputDebugStringA(__FILE__ ":" MC_STRINGIZE(__LINE__) ": "   \
                                   "Assertion '" #cond "' failed.");          \
                MessageBoxA(NULL, __FILE__ ":" MC_STRINGIZE(__LINE__) ": "    \
                                   "Assertion '" #cond "' failed.",           \
                                   "Assert", MB_OK);                          \
                if(IsDebuggerPresent())                                       \
                    DebugBreak();                                             \
                else                                                          \
                    exit(EXIT_FAILURE);                                       \
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

    /* Unreachable branch */
    #define MC_UNREACHABLE               do { MC_ASSERT(FALSE); } while(0)

    /* Logging */
    #define MC_TRACE(...)                                                     \
        do {                                                                  \
            char mc_trace_buf_[512];                                          \
            DWORD mc_last_err_;                                               \
            mc_last_err_ = GetLastError();                                    \
            _snprintf(mc_trace_buf_, sizeof(mc_trace_buf_), __VA_ARGS__);     \
            OutputDebugStringA(mc_trace_buf_);                                \
            SetLastError(mc_last_err_);                                       \
        } while(0)

    #define MC_DUMP(msg,addr,n)                                               \
        do {                                                                  \
            DWORD mc_last_err_;                                               \
            mc_last_err_ = GetLastError();                                    \
            MC_TRACE(msg);                                                    \
            debug_dump((void*)(addr), (size_t)(n));                           \
            SetLastError(mc_last_err_);                                       \
        } while(0)
#endif


/* Fallback to no-op macros */
#ifndef MC_ASSERT
    #ifdef _MSC_VER
        #include <intrin.h>
        #define MC_ASSERT(cond)          do { __assume(cond); } while(0)
    #else
        #define MC_ASSERT(cond)          do { } while(0)
    #endif
#endif
#ifndef MC_STATIC_ASSERT
    #define MC_STATIC_ASSERT(cond)       /* empty */
#endif
#ifndef MC_TRACE
    #define MC_TRACE(...)                do { } while(0)
#endif
#ifndef MC_DUMP
    #define MC_DUMP(...)                 do { } while(0)
#endif
#ifndef MC_UNREACHABLE
    #if defined __GNUC__
        #define MC_UNREACHABLE           __builtin_unreachable()
    #elif defined _MSC_VER
        #define MC_UNREACHABLE           __assume(0)
    #else
        #define MC_UNREACHABLE           do { } while(0)
    #endif
#endif

/* Helper for tracing message with GetLastError() */
#define MC_TRACE_ERR(msg)                                                     \
    MC_TRACE(msg " [%lu]", GetLastError())

/* Helper for tracing GUIDs */
#define MC_TRACE_GUID(msg, guid)                                              \
    MC_TRACE(msg " {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",       \
             (guid)->Data1, (guid)->Data2, (guid)->Data3,                     \
             (guid)->Data4[0], (guid)->Data4[1], (guid)->Data4[2],            \
             (guid)->Data4[3], (guid)->Data4[4], (guid)->Data4[5],            \
             (guid)->Data4[6], (guid)->Data4[7])


/* Functions debug_malloc() and debug_free() are used for debugging internal
 * mCtrl memory management. When used instead of malloc/free, they track
 * each allocation and deallocation, and perform some checks. Finally when
 * MCTRL.DLL is unloaded, it traces out report about detected leaks. */
#if defined DEBUG && DEBUG >= 2
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
    #define _freea(size)       free(size)

    void debug_init(void);
    void debug_fini(void);
#endif


#endif  /* MC_DEBUG_H */
