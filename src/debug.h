/*
 * Copyright (c) 2009 - 2010 Martin Mitas
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

#ifndef MC_DEBUG_H
#define MC_DEBUG_H


#if defined DEBUG && DEBUG >= 1
    #include <windows.h>
    #include <stdlib.h>
    #include <stdio.h>  /* _snprintf() */

    /* Assertion */
    #define MC_ASSERT(condition)                                              \
        do {                                                                  \
            if(!(condition)) {                                                \
                OutputDebugStringA(__FILE__ ":" MC_STRINGIZE(__LINE__) ": "   \
                                   "Assertion '" #condition "' failed.");     \
                MessageBoxA(NULL, __FILE__ ":" MC_STRINGIZE(__LINE__) ": "    \
                                   "Assertion '" #condition "' failed.",      \
                                   "Assert", MB_OK);                          \
                if(IsDebuggerPresent())                                       \
                    DebugBreak();                                             \
                else                                                          \
                    exit(EXIT_FAILURE);                                       \
            }                                                                 \
        } while(0)

    /* Logging */
    #define MC_TRACE(...)                                                     \
        do {                                                                  \
            char mc_trace_buf_[512];                                          \
            _snprintf(mc_trace_buf_, sizeof(mc_trace_buf_), __VA_ARGS__);     \
            OutputDebugStringA(mc_trace_buf_);                                \
        } while(0)
#endif


/* Fallback to no-op macros */
#ifndef MC_ASSERT
    #define MC_ASSERT(condition)        do { } while(0)
#endif
#ifndef MC_TRACE
    #define MC_TRACE(...)               do { } while(0)
#endif


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
    void debug_free(const char* fname, int line, void* mem);
    
    #define malloc(size)   debug_malloc(__FILE__, __LINE__, (size))
    #define free(ptr)      debug_free(__FILE__, __LINE__, (ptr))
    
    void debug_init(void);
    void debug_fini(void);
#endif


#endif  /* MC_DEBUG_H */
