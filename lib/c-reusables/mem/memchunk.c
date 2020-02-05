/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2020 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "memchunk.h"


struct MEMCHUNK_BLOCK {
    struct MEMCHUNK_BLOCK* next;
};


void
memchunk_init(MEMCHUNK* chunk, size_t block_size)
{
    if(block_size == 0)
        block_size = 1024;  /* Default block size. */

    chunk->block_size = block_size;
    chunk->free_off = block_size;
    chunk->head = NULL;
}

void*
memchunk_alloc(MEMCHUNK* chunk, size_t size)
{
    void* ptr;

    /* Not enough free space in the head block? */
    if(chunk->free_off + size > chunk->block_size) {
        if(8 * size > chunk->block_size) {
            /* A big allocation.
             *
             * If application asks for more memory then the block size, we
             * have no other option then to malloc a special block dedicated
             * just for the big request.
             *
             * We reuse this code path also for requests < block_size if their
             * size is still quite big and there is not enough space in the
             * head block. This is a simple policy to prevent wasting memory
             * at the end of the chunk->head which still can possibly serve
             * future smaller requests. */
            MEMCHUNK_BLOCK* block;

            block = malloc(sizeof(MEMCHUNK_BLOCK) + size);
            if(block == NULL)
                return NULL;
            if(chunk->head != NULL) {
                /* Insert _after_ the current chunk->head so that chunk->head
                 * is still available for the future (smaller) requests. */
                chunk->next = chunk->head->next;
                chunk->head->next = block;
            } else {
                chunk->head = block;
                chunk->next = NULL;
            }
            return (void*)(block + 1);
        } else {
            /* Allocate a new block. */
            MEMCHUNK_BLOCK* block;

            block = malloc(sizeof(MEMCHUNK_BLOCK) + chunk->block_size);
            if(block == NULL)
                return NULL;

            /* Insert the new block as the 1st block in our list so that all
             * (non-big) requests are served by it. */
            block->next = chunk->head;
            chunk->head = block;
            chunk->free_off = 0;
        }
    }

    /* The allocation itself: Simply take the requested amount of bytes
     * from the chunk->head block. */
    ptr = (void*) (((char*)(chunk->block + 1)) + chunk->free_off);
    chunk->free_off += size;
    return ptr;
}

void
memchunk_fini(MEMCHUNK* chunk)
{
    MEMCHUNK_BLOCK* block = chunk->head;

    while(block != NULL) {
        chunk->head = block->next;
        free(block);
        block = chunk->head;
    }
}
