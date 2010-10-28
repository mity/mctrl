/*
 * Copyright (c) 2009 Martin Mitas
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

#include "debug.h"
#include "misc.h"


#if defined DEBUG && DEBUG >= 2

#undef malloc
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

/* Here we keep all alocated mem_info_t instances, hashed by the memory chunk
 * address. Keep the hashtable size not dividable by four, so that all slots 
 * are used approximately evenly. (The dynamic allocator usually tends to 
 * allocate on DWORD or QUADWORD boudnaries ;-). 
 *
 * The hashtable lives in its own heap, so it's somewhat separated from 
 * other memory usage. This makes lower probability these core data will be 
 * oberwritten by some bug. (That would make this tool for memory debugging
 * a bit useless...)
 */
#define MEM_HASHTABLE_SIZE       ((16 * 1024) - 1)
#define MEM_HASHTABLE_INDEX(mem) ((ULONG_PTR)(mem) % MEM_HASHTABLE_SIZE)
static mem_info_t* mem_hashtable[MEM_HASHTABLE_SIZE] = { 0 };
static HANDLE mem_heap;
static CRITICAL_SECTION mem_lock;


/* Head and tail bytes are prepended/appended to the allocated memory
 * chunk, so that buffer over/underruns can be detected. 
 */
static const BYTE head_guard[] = 
        { 0xaf, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9, 0xa8, 
          0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0 };
static const BYTE tail_guard[] = 
        { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 
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
        MC_TRACE("%s:%d: \tdebug_malloc(%lu) failed.", 
                 fname, line, (ULONG)size);
        return NULL;
    }

    /* Setup over/underrun guards */
    memcpy(buffer, head_guard, sizeof(head_guard));
    memcpy(buffer + sizeof(head_guard) + size, tail_guard, sizeof(tail_guard));

    /* Fill the memory chunk with some non-zero bytes 
     * (this can help to debug (mis)uses of uninitialized memory) */
    mem = (void*)(buffer + sizeof(head_guard));
    memset(mem, 0xf0, size);
    
    /* Register info about the allocated memory */
    EnterCriticalSection(&mem_lock);
    mi = (mem_info_t*) HeapAlloc(mem_heap, 0, sizeof(mem_info_t));
    MC_ASSERT(mi != NULL);
    mi->next = mem_hashtable[MEM_HASHTABLE_INDEX(mem)];
    mem_hashtable[MEM_HASHTABLE_INDEX(mem)] = mi;
    LeaveCriticalSection(&mem_lock);
    mi->mem = mem;
    mi->size = size;
    mi->fname = fname;
    mi->line = line;
    
    MC_TRACE("%s:%d: \tdebug_malloc(%lu) -> %p", fname, line, mi->size, mem);
    return mem;
}

void
debug_free(const char* fname, int line, void* mem)
{
    mem_info_t* mi_prev = NULL;
    mem_info_t* mi;
    DWORD* head;
    DWORD* tail;
    
    MC_ASSERT(mem != NULL);
    
    /* Find memory info for the memory chunk */
    mi = mem_hashtable[MEM_HASHTABLE_INDEX(mem)];
    while(mi != NULL && mi->mem != mem) {
        mi_prev = mi;
        mi = mi->next;
    }
    if(mi == NULL) {
        /* Not registered? */
        MC_TRACE("%s:%d: \tdebug_free(%p): Attempting to release "
                 "non-allocated memory.", fname, line, mem);
        MC_ASSERT(1 == 0);
    }

    MC_TRACE("%s:%d: \tdebug_free(%p) [size=%lu]", fname, line, mem, mi->size);

    /* Check that the over/underrun guards are intact */
    head = (DWORD*)((BYTE*)mem - sizeof(head_guard));
    tail = (DWORD*)((BYTE*)mem + mi->size);
    if(memcmp(head, head_guard, sizeof(head_guard)) != 0) {
        MC_TRACE("%s:%d: \tdebug_free(%p) detected buffer underrun "
                 "[guard={%x,%x,%x,%x}, size=%lu]. Was allocated here: %s:%d",
                 fname, line, mem, head[0], head[1], head[2], head[3], 
                 mi->size, mi->fname, mi->line);
        MC_ASSERT(2 == 0);
    }
    if(memcmp(tail, tail_guard, sizeof(tail_guard)) != 0) {
        MC_TRACE("%s:%d: \tdebug_free(%p) detected buffer overrun "
                 "[guard={%x,%x,%x,%x}, size=%lu]. Was allocated here: %s:%d",
                 fname, line, mem, tail[0], tail[1], tail[2], tail[3], 
                 mi->size, mi->fname, mi->line);
        MC_ASSERT(3 == 0);
    }

    /* Rewrite all the memory with 'invalid-memory' mark. 
     * (this can help to debug (mis)uses of released memory) */
    memset(head, 0xee, mi->size + sizeof(head_guard) + sizeof(tail_guard));
    
    /* Unregister the memory info */
    EnterCriticalSection(&mem_lock);
    if(mi_prev != NULL)
        mi_prev->next = mi->next;
    else
        mem_hashtable[MEM_HASHTABLE_INDEX(mem)] = mi->next;
    HeapFree(mem_heap, 0, mi);
    LeaveCriticalSection(&mem_lock);

    /* Finally we can free it */
    free(head);
}

void
debug_init(void)
{
    InitializeCriticalSection(&mem_lock);    
    mem_heap = HeapCreate(HEAP_NO_SERIALIZE, 1024 * sizeof(mem_info_t), 0);
    MC_ASSERT(mem_heap != NULL);
}

void
debug_fini(void)
{
    int i;
    int n = 0;
    mem_info_t* mi;
    
    /* Generate report about memory leaks */
    MC_TRACE("debug_fini: Memory leaks report:", n);
    MC_TRACE("debug_fini: ----------------------------------------", n);
    EnterCriticalSection(&mem_lock);
    for(i = 0; i < MEM_HASHTABLE_SIZE; i++) {
        for(mi = mem_hashtable[i]; mi != NULL; mi = mi->next) {
            MC_TRACE("debug_fini:   leak on addr %p (%lu bytes). "
                     "Was allocated here: %s:%d",
                     mi->mem, mi->size, mi->fname, mi->line);
            n++;
        }
    }
    LeaveCriticalSection(&mem_lock);
    MC_TRACE("debug_fini:   [%d leaks detected]", n);
    MC_TRACE("debug_fini: ----------------------------------------", n);
    MC_ASSERT(n == 0);
    
    /* Uninitialize */
    HeapDestroy(mem_heap);
    DeleteCriticalSection(&mem_lock);
}

#endif  /* #if defined DEBUG && DEBUG >= 2 */

