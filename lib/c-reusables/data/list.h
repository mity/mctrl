/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2018-2020 Martin Mitas
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

#ifndef CRE_LIST_H
#define CRE_LIST_H

#ifdef __cplusplus
extern "C" {
#endif


#if defined __cplusplus
    #define LIST_INLINE__       inline
#elif defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
    #define LIST_INLINE__       static inline
#elif defined __GNUC__
    #define LIST_INLINE__       static __inline__
#elif defined _MSC_VER
    #define LIST_INLINE__       static __inline
#else
    #define LIST_INLINE__       static
#endif

#if defined offsetof
    #define LIST_OFFSETOF__(type, member)   offsetof(type, member)
#elif defined __GNUC__ && __GNUC__ >= 4
    #define LIST_OFFSETOF__(type, member)   __builtin_offsetof(type, member)
#else
    #define LIST_OFFSETOF__(type, member)   ((size_t) &((type*)0)->member)
#endif


/* This header implements few simple intrusive linked lists:
 *
 *  - Double linked lists (LIST)
 *  - Singly linked lists (SLIST)
 *  - Singly linked lists which also the tail, aka queue (QLIST)
 *
 * The word intrusive means our node structures (LIST_NODE, SLIST_NODE or
 * QLIST_NODE) don't hold any data on their own. Instead, you are supposed to
 * embed them in your structure.
 *
 * Given the simplicity of all operations, all functions are inline.
 *
 * The naming of all operations follows a common pattern <listtype>_<operation>,
 * so e.g. list_next() is used to advance to the following node in the doubly
 * linked list and slist_next() in singly linked list.
 *
 * For all the manipulations with the lists, you are supposed to use pointer
 * to our node structures (LIST_NODE, SLIST_NODE or QLIST_NODE). To retrieve
 * the payload data, use the macro LIST_DATA, SLIST_DATA or QUEUE_DATA as
 * appropriate (all of these are actually just a synonym for the wildly used
 * container_of() macro.)
 *
 * Note that our lists may use the common little trick and avoid using NULL as
 * the end-of-list sentinel. Instead the first and last nodes point to the
 * dummy structure representing the list as a whole. This simplifies many
 * insert/remove operations because it mitigates the need for branches.
 *
 * The cost of that trick is the caller may not test whether he has reached
 * the end of the list with NULL. For the sake of simplicity we provide the
 * function list_end(), which can be used similarly as std::vector::end() in
 * C++ (both in forward as well as backward iteration).
 *
 * Hence, the typical iteration over e.g. the doubly linked list may look like
 * this:
 *
 * ````
 * typedef struct MyStruct {
 *    ...   // Some data
 *
 *    // The embedded list node structure:
 *    LIST_NODE list_node;
 *
 *    ...   // Some more data
 * } MyStruct;
 *
 *
 * static void
 * walk_my_list(LIST* list)
 * {
 *     LIST_NODE* node;
 *     MyStruct* data_payload;
 *
 *     for(node = list_head(list); node != list_end(list); node = list_next(node)) {
 *         // Retrieve the data pay load from the node:
 *         data_payload = LIST_DATA(node, MyStruct, list_node);
 *
 *         // Process the data payload as desired.
 *         ...
 *     }
 * }
 * ````
 *
 * An overview of available operations, depending on the list type:
 *
 *                              | LIST   | SLIST  | QLIST
 * -----------------------------|--------|--------|-------
 * <listtype>_init()            | yes    | yes    | yes
 * <listtype>_is_empty()        | yes    | yes    | yes
 * <listtype>_head()            | yes    | yes    | yes
 * <listtype>_tail()            | yes    |        | yes
 * <listtype>_next()            | yes    | yes    | yes
 * <listtype>_prev()            | yes    |        |
 * <listtype>_end()             | yes    | yes    | yes
 * <listtype>_insert_after()    | yes    | yes    |
 * <listtype>_insert_before()   | yes    |        |
 * <listtype>_append()          | yes    |        | yes
 * <listtype>_prepend()         | yes    | yes    | yes
 * <listtype>_remove()          | yes    | yes(1) | yes(1)
 * <listtype>_remove_head()     | yes    | yes    | yes
 * <listtype>_remove_tail()     | yes    |        |
 *
 * Notes:
 *  (1): The caller has to additionally provide pointer to the _previous_ node.
 */


/*********************************
 *** LIST (doubly linked list) ***
 *********************************/

/* List node structure. Treat as opaque.
 */
typedef struct LIST_NODE {
    struct LIST_NODE* p;    /* prev */
    struct LIST_NODE* n;    /* next */
} LIST_NODE;


/* List structure. Treat as opaque.
 */
typedef struct LIST {
    struct LIST_NODE main;
} LIST;


/* Macro for getting pointer to the structure holding list node data.
 */
#define LIST_DATA(node_ptr, type, member)  \
                ((type*)((char*)(node_ptr) - LIST_OFFSETOF__(type, member)))


/* The list has to be initialized before it is used by any other function.
 */
LIST_INLINE__ void list_init(LIST* list)
        { list->main.p = list->main.n = &list->main; }


/* Check whether the list is empty or not.
 */
LIST_INLINE__ int list_is_empty(const LIST* list)
        { return (list->main.n == &list->main); }


/* Iterating the list.
 */
LIST_INLINE__ LIST_NODE* list_head(const LIST* list)        { return list->main.n; }
LIST_INLINE__ LIST_NODE* list_tail(const LIST* list)        { return list->main.p; }
LIST_INLINE__ LIST_NODE* list_prev(const LIST_NODE* node)   { return node->p; }
LIST_INLINE__ LIST_NODE* list_next(const LIST_NODE* node)   { return node->n; }
LIST_INLINE__ const LIST_NODE* list_end(const LIST* list)   { return &list->main; }

/* Add the given node into the list.
 *
 * Note that any node can be only in one list at any given time. If you
 * attempt to add the node into multiple lists (or multiple times into the
 * same list), the result is undefined.
 */
LIST_INLINE__ void list_insert_after(LIST* list, LIST_NODE* node_where, LIST_NODE* node)
        { node->p = node_where; node->n = node_where->n; node_where->n = node; node->n->p = node; }
LIST_INLINE__ void list_insert_before(LIST* list, LIST_NODE* node_where, LIST_NODE* node)
        { node->p = node_where->p; node->n = node_where; node_where->p = node; node->p->n = node; }
LIST_INLINE__ void list_append(LIST* list, LIST_NODE* node)
        { list_insert_before(list, &list->main, node); }
LIST_INLINE__ void list_prepend(LIST* list, LIST_NODE* node)
        { list_insert_after(list, &list->main, node); }

/* Disconnect the given node from its list.
 */
LIST_INLINE__ void list_remove(LIST* list, LIST_NODE* node)
        { node->p->n = node->n; node->n->p = node->p; }
LIST_INLINE__ void list_remove_head(LIST* list)
        { list_remove(list, list->main.n); }
LIST_INLINE__ void list_remove_tail(LIST* list)
        { list_remove(list, list->main.p); }


/**********************************
 *** SLIST (singly linked list) ***
 **********************************/

/* List node structure. Treat as opaque.
 */
typedef struct SLIST_NODE {
    struct SLIST_NODE* n;       /* next */
} SLIST_NODE;


/* List structure. Treat as opaque.
 */
typedef struct SLIST {
    struct SLIST_NODE main;
} SLIST;


/* Macro for getting pointer to the structure holding list node data.
 */
#define SLIST_DATA(node_ptr, type, member)  \
                ((type*)((char*)(node_ptr) - LIST_OFFSETOF__(type, member)))


/* The list has to be initialized before it is used by any other function.
 */
LIST_INLINE__ void slist_init(SLIST* list)
        { list->main.n = &list->main; }


/* Check whether the list is empty or not.
 */
LIST_INLINE__ int slist_is_empty(const SLIST* list)
        { return (list->main.n == &list->main); }


/* Iterating the list.
 */
LIST_INLINE__ SLIST_NODE* slist_head(const SLIST* list)         { return list->main.n; }
LIST_INLINE__ SLIST_NODE* slist_next(const SLIST_NODE* node)    { return node->n; }
LIST_INLINE__ const SLIST_NODE* slist_end(const SLIST* list)    { return &list->main; }

/* Add the given node into the list.
 *
 * Note that any node can be only in one list at any given time. If you
 * attempt to add the node into multiple lists (or multiple times into the
 * same list), the result is undefined.
 */
LIST_INLINE__ void slist_insert_after(SLIST* list, SLIST_NODE* node_where, SLIST_NODE* node)
        { node->n = node_where->n; node_where->n = node; }
LIST_INLINE__ void slist_prepend(SLIST* list, SLIST_NODE* node)
        { slist_insert_after(list, &list->main, node); }

/* Disconnect the given node from its list.
 */
LIST_INLINE__ void slist_remove(SLIST* list, SLIST_NODE* node_prev, SLIST_NODE* node)
        { node_prev->n = node->n; }
LIST_INLINE__ void slist_remove_head(SLIST* list)
        { slist_remove(list, &list->main, list->main.n); }


/*****************************************************
 *** QLIST (queue or singly linked list with tail) ***
 *****************************************************/

/* List node structure. Treat as opaque.
 */
typedef struct QLIST_NODE {
    struct QLIST_NODE* n;       /* next */
} QLIST_NODE;


/* List structure. Treat as opaque.
 */
typedef struct QLIST {
    struct QLIST_NODE main;
    struct QLIST_NODE* tail;
} QLIST;


/* Macro for getting pointer to the structure holding list node data.
 */
#define QLIST_DATA(node_ptr, type, member)  \
                ((type*)((char*)(node_ptr) - LIST_OFFSETOF__(type, member)))

/* The list has to be initialized before it is used by any other function.
 */
LIST_INLINE__ void qlist_init(QLIST* list)
        { list->tail = list->main.n = &list->main; }

/* Check whether the list is empty or not.
 */
LIST_INLINE__ int qlist_is_empty(const QLIST* list)
        { return (list->main.n == &list->main); }

/* Iterating the list.
 */
LIST_INLINE__ QLIST_NODE* qlist_head(const QLIST* list)         { return list->main.n; }
LIST_INLINE__ QLIST_NODE* qlist_tail(const QLIST* list)         { return list->tail; }
LIST_INLINE__ QLIST_NODE* qlist_next(const QLIST_NODE* node)    { return node->n; }
LIST_INLINE__ const QLIST_NODE* qlist_end(const QLIST* list)    { return &list->main; }

/* Add the given node into the list.
 *
 * Note that any node can be only in one list at any given time. If you
 * attempt to add the node into multiple lists (or multiple times into the
 * same list), the result is undefined.
 */
LIST_INLINE__ void qlist_insert_after(QLIST* list, QLIST_NODE* node_where, QLIST_NODE* node)
        { node->n = node_where->n; node_where->n = node;
          if(list->tail == node_where) list->tail = node; }
LIST_INLINE__ void qlist_append(QLIST* list, QLIST_NODE* node)
        { node->n = &list->main; list->tail->n = node; list->tail = node; }
LIST_INLINE__ void qlist_prepend(QLIST* list, QLIST_NODE* node)
        { qlist_insert_after(list, &list->main, node); }

/* Disconnect the given node from its list.
 */
LIST_INLINE__ void qlist_remove(QLIST* list, QLIST_NODE* node_prev, QLIST_NODE* node)
        { node_prev->n = node->n;
          if(list->tail == node) list->tail = node_prev; }
LIST_INLINE__ void qlist_remove_head(QLIST* list)
        { qlist_remove(list, &list->main, list->main.n); }


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_LIST_H */
