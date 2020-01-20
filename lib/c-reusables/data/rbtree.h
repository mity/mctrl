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

#ifndef CRE_RBTREE_H
#define CRE_RBTREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>


#if defined __cplusplus
    #define RBTREE_INLINE__     inline
#elif defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
    #define RBTREE_INLINE__     static inline
#elif defined __GNUC__
    #define RBTREE_INLINE__     static __inline__
#elif defined _MSC_VER
    #define RBTREE_INLINE__     static __inline
#else
    #define RBTREE_INLINE__     static
#endif

#if defined offsetof
    #define RBTREE_OFFSETOF__(type, member)     offsetof(type, member)
#elif defined __GNUC__ && __GNUC__ >= 4
    #define RBTREE_OFFSETOF__(type, member)     __builtin_offsetof(type, member)
#else
    #define RBTREE_OFFSETOF__(type, member)     ((size_t) &((type*)0)->member)
#endif


/* This header implements (intrusive) red-black tree.
 *
 * See e.g. https://en.wikipedia.org/wiki/Red–black_tree if you are unfamiliar
 * with the concept of red-black tree.
 *
 * To manipulate or query the tree, the tree as a whole is represented by the
 * RBTREE structure and its nodes are represented by the RBTREE_NODE structure.
 *
 * The word "intrusive" in the title means our RBTREE_NODE structure does not
 * hold any payload data on its own. Instead, you are supposed to embed the
 * node in your own data structure.
 *
 * Also note we do not distinguish any "key" from "data". Caller has to
 * provide his own comparator function for defining the order of the data in
 * the tree and it's up to application to decide which data serve as the "key",
 * i.e. are used for the ordering.
 *
 * The design decision has some consequences:
 *
 * - To get a pointer to the enclosing application specific data structure, 
 *   use the macro RBTREE_DATA (which is just the the typical "container_of()"
 *   macro implementation.
 *
 * - For some operations like e.g. lookup, the caller has to provide a pointer
 *   to the same dummy structure, initialized enough to serve as the "key" so
 *   the comparator function may do its job.
 *
 * - It also means our implementation actually never allocates/frees any memory
 *   on the heap. All the tree operations like e.g. "insert" or "remove" just
 *   update the pointers in the node structure (and those in other nodes of
 *   the tree).
 *
 *   Caller has to allocate and initialize the data structure (at least to the
 *   degree needed by the comparator function) the enclosing data structure
 *   _before_ he inserts it into the tree. Similarly, caller is responsible for
 *   freeing any resources the payload structure holds _after_ it is removed
 *   from the tree.
 *
 * - As long as the node is part of a tree, it must not be modified in any way
 *   which would make the comparator function order it differently with respect
 *   to the other nodes in the tree.
 *
 * Note of warning: The RBTREE_NODE stores its color in the least significant
 * bit of the RBTREE_NODE::left pointer. This means that all the RBTREE_NODE
 * instances must be reasonably aligned.
 */


/* Tree node structure. Treat as opaque.
 */
typedef struct RBTREE_NODE {
    struct RBTREE_NODE* lc; /* combination of left ptr and color bit */
    struct RBTREE_NODE* r;  /* right ptr */
} RBTREE_NODE;


/* Tree structure. Treat as opaque.
 */
typedef struct RBTREE {
    RBTREE_NODE* root;
} RBTREE;


/* Comparator function type.
 *
 * The comparator function defines the order of the data stored in the tree.
 * As such, we suppose the same comparator function is used throughout the life
 * time of the tree.
 *
 * WARNING: When different comparator functions are used during the life time
 * of the tree, the behavior is undefined.
 *
 * It has to return:
 *   - negative value if the 1st argument is lower then the 2nd one;
 *   - positive value if the 1st argument is greater then the 2nd one;
 *   - zero if they are equal.
 */
typedef int (*RBTREE_CMP_FUNC)(const RBTREE_NODE*, const RBTREE_NODE*);


/* Macro for getting pointer to the structure holding the rbtree node data.
 *
 * (If you use the RBTREE_NODE as the first member of your structure, you
 * use just a cast instead.)
 */
#define RBTREE_DATA(node_ptr, type, member)     \
                ((type*)((char*)(node_ptr) - RBTREE_OFFSETOF__(type, member)))


/* The tree has to be initialized before it is used by any other function.
 */
RBTREE_INLINE__ void rbtree_init(RBTREE* tree)
        { tree->root = NULL; }

#define RBTREE_INITIALIZER      { NULL }


/* Cleaning a (non-empty) tree can be a more complex operation. Usually, caller
 * needs to release some resources associated with each node (e.g. to free the
 * data structure).
 *
 * We provide these specialized function for iterating over the nodes in order
 * to destroy them.
 *
 * ```
 * while(1) {
 *     RBTREE_NODE* node = rbtree_fini_step(tree);
 *     if(node == NULL)
 *         break;
 *
 *     // Release the per-node resources:
 *     free(RBTREE_DATA(node, MyStruct, the_node_member_name);
 * }
 *
 * ```
 *
 * Note that once the operation starts, the tree must not be used anymore for
 * anything unless it is complete as its internal state gets broken for
 * any other purpose then finishing the clean-up. After the clean-up is
 * completely done, you get a valid empty tree.
 *
 * The function is actually just a specialized light-weight iteration over all
 * nodes, ripping them one by one out from the tree, without any re-balancing.
 *
 * It does not release any resources on its own. (Our implementation does
 * not allocate any at the first place. We only connect the nodes together or
 * as here, disconnect them).
 *
 * I.e., if you are able kill all the nodes more effectively by any other means
 * without iterating over them (for example because all the nodes live in a
 * single memory buffer, which can be freed at once) then you do not need to
 * use this function at all: Instead, simply free the buffer.
 *
 * (Compatibility note: You should not rely on any particular order when using
 * this function. If we find more efficient algorithm for the given purpose,
 * future versions may present the nodes in a different order.)
 */
RBTREE_NODE* rbtree_fini_step(RBTREE* tree);


/* Check whether the tree is empty. Returns non-zero if empty, zero otherwise.
 */
RBTREE_INLINE__ int rbtree_is_empty(const RBTREE* tree)
        { return (tree->root == NULL); }


/* Insert a new node into the tree.
 *
 * Returns 0 on success or -1 on failure (which may happen only if an equal
 * node is already present in the tree).
 */
int rbtree_insert(RBTREE* tree, RBTREE_NODE* node, RBTREE_CMP_FUNC cmp_func);

/* Remove a node equal to the key (as defined by the comparator function).
 *
 * Returns pointer to the node disconnected from the tree (so that caller can
 * e.g. to destroy it), or NULL if no such item has been found in the tree.
 */
RBTREE_NODE* rbtree_remove(RBTREE* tree, const RBTREE_NODE* key, RBTREE_CMP_FUNC cmp_func);

/* Find a node equal to the key (as defined by the comparator function).
 *
 * Returns pointer to the found node or NULL if no such node has been found in
 * the tree.
 */
RBTREE_NODE* rbtree_lookup(RBTREE* tree, const RBTREE_NODE* key, RBTREE_CMP_FUNC cmp_func);


/* Walking over all nodes in the tree. When reaching end of the iteration, the
 * functions return NULL.
 *
 * Walking over the complete tree can be implemented as follows:
 *
 * ```
 * static void walk_over_my_tree(RBTREE* tree)
 * {
 *     RBTREE_CURSOR cur;
 *     RBTREE* node;
 *
 *     for(node = rbtree_head(tree, &cur);
 *         node != NULL;
 *         node = rbtree_next(&cur))
 *     {
 *         ...
 *     }
 * }
 * ```
 *
 * However note any cursor becomes invalid and must not be used anymore when
 * any nodes are added into the tree or removed from it.
 */
typedef struct RBTREE_CURSOR {
    /* (2 * 8 * sizeof(void*)) is good enough to handle RB-trees of _any_ size.
     * Consider there cannot be more then 2^(8*sizeof(void*)) nodes in the
     * process memory and the longest root<-->leaf path in any RB-tree cannot
     * be longer then twice the shortest one. */
    RBTREE_NODE* stack[2 * 8 * sizeof(void*)];
    unsigned n;
} RBTREE_CURSOR;

/* Initializer for a cursor pointing to nowhere. */
#define RBTREE_CURSOR_INITIALIZER       { { 0 }, 0 }


/* This is similar to rbtree_lookup() but it also initializes the cursor to the
 * corresponding position, so caller may navigate from the node to other ones
 * via the rbtree_prev() and/or rbtree_next().
 */
RBTREE_NODE* rbtree_lookup_ex(RBTREE* tree, const RBTREE_NODE* key,
                              RBTREE_CMP_FUNC cmp_func, RBTREE_CURSOR* cur);

/* Get the node corresponding to the current position of the cursor; or NULL.
 */
RBTREE_NODE* rbtree_current(RBTREE_CURSOR* cur);

/* The functions rbtree_head() and rbtree_tail() retrieve the first or last
 * node in the tree, the functions rbtree_next() and rbtree_prev() move to
 * the next or the previous node (in the order as defined by the comparator
 * function used during construction of the tree).
 */
RBTREE_NODE* rbtree_head(RBTREE* tree, RBTREE_CURSOR* cur);
RBTREE_NODE* rbtree_tail(RBTREE* tree, RBTREE_CURSOR* cur);
RBTREE_NODE* rbtree_next(RBTREE_CURSOR* cur);
RBTREE_NODE* rbtree_prev(RBTREE_CURSOR* cur);


#ifdef __cplusplus
}
#endif

#endif  /* CRE_RBTREE_H */
