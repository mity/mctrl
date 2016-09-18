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


#include "debug.h"
#include "misc.h"

#include <stdio.h>
#include <stdarg.h>


/*********************
 *** Debug Tracing ***
 *********************/

#if defined DEBUG && DEBUG >= 1

#define DEBUG_DUMP_PER_LINE         16

void
MC_TRACE(const char* fmt, ...)
{
    DWORD last_error;
    va_list args;
    char buffer[512];
    int offset = 0;
    int n;

    last_error = GetLastError();

    offset += sprintf(buffer, "[%08x] ", (unsigned) GetCurrentThreadId());

    va_start(args, fmt);
    n = _vsnprintf(buffer + offset, sizeof(buffer)-offset-2, fmt, args);
    va_end(args);
    if(n < 0  ||  n > sizeof(buffer)-offset-2)
        n = sizeof(buffer)-offset-2;
    offset += n;
    buffer[offset++] = '\n';
    buffer[offset++] = '\0';
    OutputDebugStringA(buffer);
    SetLastError(last_error);
}

void
MC_DUMP(const char* msg, void* addr, size_t n)
{
    DWORD last_error;
    BYTE* bytes = (BYTE*) addr;
    size_t offset = 0;
    size_t count;
    char buffer[32 + 3 * DEBUG_DUMP_PER_LINE];
    char* ptr;
    int i;

    last_error = GetLastError();
    MC_TRACE(msg);

    while(offset < n) {
        count = MC_MIN(n - offset, DEBUG_DUMP_PER_LINE);

        ptr = buffer;
        ptr += sprintf(ptr, "    %04lx:  ", (ULONG) offset);

        for(i = 0; i < count; i++) {
            ptr += sprintf(ptr, " %02x", bytes[offset + i]);
            if(i == DEBUG_DUMP_PER_LINE/2 - 1)
                ptr += sprintf(ptr, "  ");
        }

        MC_TRACE(buffer);
        offset += count;
    }

    MC_TRACE("            (%lu bytes)", (ULONG)n);
    SetLastError(last_error);
}

#endif  /* #if defined DEBUG && DEBUG >= 1 */


/*****************************
 *** Memory Heap Debugging ***
 *****************************/

#if defined DEBUG && DEBUG >= 2

/* Trace out all malloc() and free() calls? */
#if DEBUG >= 3
    #define DEBUG_TRACE     MC_TRACE
#else
    #define DEBUG_TRACE     MC_NOOP
#endif

/* Undefine the replacing macros from debug.h */
#undef malloc
#undef realloc
#undef free


/* For each allocated memory chunk we have a memory info with some info
 * about it. */
typedef struct mem_info_tag mem_info_t;
struct mem_info_tag {
    void* mem;
    ULONG size;         /* size of the allocated memory chunk */
    const char* fname;  /* source file name where it has been allocated */
    int line;           /* source file line where it has been allocated */
    mem_info_t* next;
};


/* Here we keep all allocated mem_info_t instances, hashed by the memory chunk
 * address. Keep the hash table size not dividable by four, so that all slots
 * are used approximately evenly. (The dynamic allocator usually tends to
 * allocate on DWORD or QUADWORD boundaries ;-).
 *
 * The hash table lives in its own heap, so it's somewhat separated from
 * other memory usage. This lowers the probability these core data will be
 * overwritten by some bug. (That would make this tool for memory debugging
 * a bit useless...) */
#define MEM_HASHTABLE_SIZE       ((16 * 1024) - 1)
#define MEM_HASHTABLE_INDEX(mem) ((ULONG_PTR)(mem) % MEM_HASHTABLE_SIZE)
static mem_info_t* mem_hashtable[MEM_HASHTABLE_SIZE] = { 0 };
static HANDLE mem_heap;
static mc_mutex_t mem_mutex;


/* Head and tail bytes are prepended/appended to the allocated memory
 * chunk, so that buffer over/underruns can be detected. */
static const BYTE head_guard[] = { 0xaf, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9, 0xa8,
                                   0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0 };
static const BYTE tail_guard[] = { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
                                   0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf };


void*
debug_malloc(const char* fname, int line, size_t size)
{
    BYTE* buffer;
    void* mem;
    mem_info_t* mi;

    /* We never attempt to allocate zero bytes in mCtrl */
    MC_ASSERT(size > 0);

    /* Allocate */
    buffer = (BYTE*) malloc(size + sizeof(head_guard) + sizeof(tail_guard));
    if(MC_ERR(buffer == NULL)) {
        MC_TRACE("%s:%d: \tdebug_malloc(%lu) failed.", fname, line, (ULONG)size);
        return NULL;
    }

    /* Setup over/underrun guards */
    memcpy(buffer, head_guard, sizeof(head_guard));
    memcpy(buffer + sizeof(head_guard) + size, tail_guard, sizeof(tail_guard));

    /* Fill the memory chunk with some non-zero bytes
     * (this can help to debug (mis)uses of uninitialized memory) */
    mem = (void*)(buffer + sizeof(head_guard));
    memset(mem, 0xff, size);

    /* Register info about the allocated memory */
    mc_mutex_lock(&mem_mutex);
    mi = (mem_info_t*) HeapAlloc(mem_heap, 0, sizeof(mem_info_t));
    MC_ASSERT(mi != NULL);
    mi->next = mem_hashtable[MEM_HASHTABLE_INDEX(mem)];
    mem_hashtable[MEM_HASHTABLE_INDEX(mem)] = mi;
    mi->mem = mem;
    mi->size = size;
    mi->fname = fname;
    mi->line = line;
    mc_mutex_unlock(&mem_mutex);

    DEBUG_TRACE("%s:%d: \tdebug_malloc(%lu) -> %p", fname, line, mi->size, mem);
    return mem;
}

void*
debug_realloc(const char* fname, int line, void* mem, size_t size)
{
    void* new_mem;

    new_mem = debug_malloc(fname, line, size);
    if(MC_ERR(new_mem == NULL))
        return NULL;

    /* Copy contents from the old memory chunk */
    if(mem != NULL) {
        mem_info_t* mi;
        mi = mem_hashtable[MEM_HASHTABLE_INDEX(mem)];
        while(mi->mem != mem) {
            if(MC_ERR(mi == NULL)) {
                /* Not registered? */
                MC_TRACE("%s:%d: \tdebug_realloc(%p): Attempting to realloc "
                         "non-allocated memory.", fname, line, mem);
                MC_ASSERT(1 == 0);
            }
            mi = mi->next;
        }
        memcpy(new_mem, mem, MC_MIN(size, mi->size));
        debug_free(fname, line, mem);
    }

    return new_mem;
}

void
debug_free(const char* fname, int line, void* mem)
{
    mem_info_t* mi_prev = NULL;
    mem_info_t* mi;
    BYTE* head;
    BYTE* tail;

    MC_ASSERT(mem != NULL);

    mc_mutex_lock(&mem_mutex);

    /* Find memory info for the memory chunk */
    mi = mem_hashtable[MEM_HASHTABLE_INDEX(mem)];
    while(TRUE) {
        if(MC_ERR(mi == NULL)) {
            /* Not registered? */
            MC_TRACE("%s:%d: \tdebug_free(%p): Attempting to release "
                     "non-allocated memory.", fname, line, mem);
            MC_ASSERT(1 == 0);
        }

        if(mi->mem == mem)
            break;

        mi_prev = mi;
        mi = mi->next;
    }

    DEBUG_TRACE("%s:%d: \tdebug_free(%p) [size=%lu]", fname, line, mem, mi->size);

    /* Check that the over/underrun guards are intact */
    head = ((BYTE*)mem) - sizeof(head_guard);
    tail = ((BYTE*)mem) + mi->size;
    if(memcmp(head, head_guard, sizeof(head_guard)) != 0) {
        MC_TRACE("%s:%d: \tdebug_free(%p) detected buffer underrun "
                 "[guard={%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x}, "
                 "size=%lu]. Was allocated here: %s:%d",
                 fname, line, mem,
                 head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
                 head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15],
                 mi->size, mi->fname, mi->line);
        MC_ASSERT(2 == 0);
    }
    if(memcmp(tail, tail_guard, sizeof(tail_guard)) != 0) {
        MC_TRACE("%s:%d: \tdebug_free(%p) detected buffer overrun "
                 "[guard={%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x}, "
                 "size=%lu]. Was allocated here: %s:%d",
                 fname, line, mem,
                 tail[0], tail[1], tail[2], tail[3], tail[4], tail[5], tail[6], tail[7],
                 tail[8], tail[9], tail[10], tail[11], tail[12], tail[13], tail[14], tail[15],
                 mi->size, mi->fname, mi->line);
        MC_ASSERT(3 == 0);
    }

    /* Rewrite all the memory with 'invalid-memory' mark, including the guards.
     * (this can help to debug (mis)uses of released memory) */
    memset(head, 0xee, mi->size + 2 * sizeof(head_guard));

    /* Unregister the memory info */
    if(mi_prev != NULL)
        mi_prev->next = mi->next;
    else
        mem_hashtable[MEM_HASHTABLE_INDEX(mem)] = mi->next;
    HeapFree(mem_heap, 0, mi);

    mc_mutex_unlock(&mem_mutex);

    /* Finally we can free it */
    free(head);
}

void
debug_dllmain_init(void)
{
    mc_mutex_init(&mem_mutex);

    /* We guard the heap with our own locking as we need the critical section
     * around it anyway. Hence HEAP_NO_SERIALIZE. */
    mem_heap = HeapCreate(HEAP_NO_SERIALIZE, 1024 * 16 * sizeof(mem_info_t), 0);
    MC_ASSERT(mem_heap != NULL);
}

void
debug_dllmain_fini(void)
{
    int i;
    int n = 0;
    size_t size = 0;
    mem_info_t* mi;

    /* Generate report about memory leaks */
    mc_mutex_lock(&mem_mutex);
    for(i = 0; i < MEM_HASHTABLE_SIZE; i++) {
        for(mi = mem_hashtable[i]; mi != NULL; mi = mi->next) {
            if(n == 0) {
                MC_TRACE("");
                MC_TRACE("debug_dllmain_fini: LEAK REPORT:");
                MC_TRACE("debug_dllmain_fini: --------------------------------------------------");
#ifdef _WIN64
                MC_TRACE("debug_dllmain_fini: Address              Size       Where");
#else
                MC_TRACE("debug_dllmain_fini: Address      Size       Where");
#endif
                MC_TRACE("debug_dllmain_fini: --------------------------------------------------");
            }

#ifdef _WIN64
            MC_TRACE("debug_dllmain_fini: 0x%16p   %8lu   %s:%d", mi->mem, mi->size, mi->fname, mi->line);
#else
            MC_TRACE("debug_dllmain_fini: 0x%8p   %8lu   %s:%d", mi->mem, mi->size, mi->fname, mi->line);
#endif

            n++;
            size += mi->size;
        }
    }
    mc_mutex_unlock(&mem_mutex);
    if(n > 0) {
        MC_TRACE("debug_dllmain_fini: --------------------------------------------------");
        MC_TRACE("debug_dllmain_fini: Lost %lu bytes in %d leaks.", size, n);
        MC_TRACE("");
    }

    MC_ASSERT(n == 0);

    /* Uninitialize */
    HeapDestroy(mem_heap);
    mc_mutex_fini(&mem_mutex);
}

#else  /* #if defined DEBUG && DEBUG >= 2 */

void
debug_dllmain_init(void)
{
    /* noop */
}

void
debug_dllmain_fini(void)
{
    /* noop */
}

#endif  /* #if defined DEBUG && DEBUG >= 2 */
