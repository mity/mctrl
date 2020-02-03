/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2016 Martin Mitas
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

#include "acutest.h"
#include "list.h"


/**************************
 *** Doubly-linked list ***
 **************************/

typedef struct DATA {
    int value;
    LIST_NODE list_node;
} DATA;

static DATA*
alloc_data(int value)
{
    DATA* d;

    d = (DATA*) malloc(sizeof(DATA));
    TEST_CHECK(d != NULL);
    d->value = value;
    return d;
}


static void
test_list_empty(void)
{
    LIST list;
    LIST_NODE node;

    list_init(&list);

    TEST_CHECK(list_is_empty(&list));
    list_append(&list, &node);
    TEST_CHECK(!list_is_empty(&list));
}

static void
test_list_iterate(void)
{
    LIST list;
    LIST_NODE* node;
    DATA* data;
    int n;

    /* Create simple list. */
    list_init(&list);
    list_append(&list, &alloc_data(1)->list_node);
    list_append(&list, &alloc_data(2)->list_node);
    list_append(&list, &alloc_data(3)->list_node);

    /* Iterate forward. */
    for(node = list_head(&list), n = 1; node != list_end(&list); node = list_next(node), n++) {
        data = LIST_DATA(node, DATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Iterate backward. */
    for(node = list_tail(&list), n = 3; node != list_end(&list); node = list_prev(node), n--) {
        data = LIST_DATA(node, DATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Clean up. */
    n = 0;
    while(!list_is_empty(&list)) {
        node = list_head(&list);
        list_remove(&list, node);
        free(LIST_DATA(node, DATA, list_node));
        n++;
    }
    TEST_CHECK(n == 3);
}

static void
test_list_insert(void)
{
    LIST list;
    LIST_NODE* node;
    DATA* data;
    DATA xxx;
    int n;

    /* Create simple list. */
    list_init(&list);
    list_append(&list, &alloc_data(5)->list_node);                              /* 5 */
    list_prepend(&list, &alloc_data(2)->list_node);                             /* 2 -> 5 */
    list_insert_after(&list, list_head(&list), &alloc_data(3)->list_node);      /* 1 -> 2 -> 3 */
    list_insert_before(&list, list_head(&list), &alloc_data(1)->list_node);     /* 1 -> 2 -> 3 -> 5 */
    list_insert_before(&list, list_tail(&list), &alloc_data(4)->list_node);     /* 1 -> 2 -> 3 -> 4 -> 5 */
    list_insert_after(&list, list_tail(&list), &alloc_data(6)->list_node);      /* 1 -> 2 -> 3 -> 4 -> 5 -> 6 */

    /* Verify that removals do not break integrity. */
    xxx.value = 666;
    list_append(&list, &xxx.list_node);
    list_remove(&list, &xxx.list_node);
    list_prepend(&list, &xxx.list_node);
    list_remove(&list, &xxx.list_node);
    list_insert_after(&list, list_head(&list), &xxx.list_node);
    list_remove(&list, &xxx.list_node);

    /* Iterate forward. */
    for(node = list_head(&list), n = 1; node != list_end(&list); node = list_next(node), n++) {
        data = LIST_DATA(node, DATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Iterate backward. */
    for(node = list_tail(&list), n = 6; node != list_end(&list); node = list_prev(node), n--) {
        data = LIST_DATA(node, DATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Clean up. */
    n = 0;
    while(!list_is_empty(&list)) {
        node = list_head(&list);
        list_remove(&list, node);
        free(LIST_DATA(node, DATA, list_node));
        n++;
    }
    TEST_CHECK(n == 6);
}


/**************************
 *** Single linked list ***
 **************************/

typedef struct SDATA {
    int value;
    SLIST_NODE list_node;
} SDATA;

static SDATA*
alloc_sdata(int value)
{
    SDATA* d;

    d = (SDATA*) malloc(sizeof(SDATA));
    TEST_CHECK(d != NULL);
    d->value = value;
    return d;
}


static void
test_slist_empty(void)
{
    SLIST list;
    SLIST_NODE node;

    slist_init(&list);

    TEST_CHECK(slist_is_empty(&list));
    slist_prepend(&list, &node);
    TEST_CHECK(!slist_is_empty(&list));
}

static void
test_slist_iterate(void)
{
    SLIST list;
    SLIST_NODE* node;
    SDATA* data;
    int n;

    /* Create simple list. */
    slist_init(&list);
    slist_prepend(&list, &alloc_sdata(3)->list_node);
    slist_prepend(&list, &alloc_sdata(2)->list_node);
    slist_prepend(&list, &alloc_sdata(1)->list_node);

    /* Iterate forward. */
    for(node = slist_head(&list), n = 1; node != slist_end(&list); node = slist_next(node), n++) {
        data = SLIST_DATA(node, SDATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Clean up. */
    n = 0;
    while(!slist_is_empty(&list)) {
        node = slist_head(&list);
        slist_remove_head(&list);
        free(SLIST_DATA(node, SDATA, list_node));
        n++;
    }
    TEST_CHECK(n == 3);
}

static void
test_slist_insert(void)
{
    SLIST list;
    SLIST_NODE* node;
    SDATA* data;
    SDATA xxx;
    int n;

    /* Create simple list. */
    slist_init(&list);
    slist_prepend(&list, &alloc_sdata(5)->list_node);                           /* 5 */
    slist_prepend(&list, &alloc_sdata(3)->list_node);                           /* 3 -> 5 */
    slist_insert_after(&list, slist_head(&list), &alloc_sdata(4)->list_node);   /* 3 -> 4 -> 5 */
    slist_prepend(&list, &alloc_sdata(1)->list_node);                           /* 1 -> 3 -> 4 -> 5 */
    slist_insert_after(&list, slist_head(&list), &alloc_sdata(2)->list_node);   /* 1 -> 2 -> 3 -> 4 -> 5 */

    /* Verify that removals do not break integrity. */
    xxx.value = 666;
    slist_prepend(&list, &xxx.list_node);
    slist_remove_head(&list);
    slist_insert_after(&list, slist_head(&list), &xxx.list_node);
    slist_remove(&list, slist_head(&list), &xxx.list_node);

    /* Iterate forward. */
    for(node = slist_head(&list), n = 1; node != slist_end(&list); node = slist_next(node), n++) {
        data = SLIST_DATA(node, SDATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Clean up. */
    n = 0;
    while(!slist_is_empty(&list)) {
        node = slist_head(&list);
        slist_remove_head(&list);
        free(SLIST_DATA(node, SDATA, list_node));
        n++;
    }
    TEST_CHECK(n == 5);
}


/*************
 *** Queue ***
 *************/

typedef struct QDATA {
    int value;
    QLIST_NODE list_node;
} QDATA;

static QDATA*
alloc_qdata(int value)
{
    QDATA* d;

    d = (QDATA*) malloc(sizeof(QDATA));
    TEST_CHECK(d != NULL);
    d->value = value;
    return d;
}


static void
test_qlist_empty(void)
{
    QLIST list;
    QLIST_NODE node;

    qlist_init(&list);

    TEST_CHECK(qlist_is_empty(&list));
    qlist_append(&list, &node);
    TEST_CHECK(!qlist_is_empty(&list));
}

static void
test_qlist_iterate(void)
{
    QLIST list;
    QLIST_NODE* node;
    QDATA* data;
    int n;

    /* Create simple list. */
    qlist_init(&list);
    qlist_append(&list, &alloc_qdata(1)->list_node);
    qlist_append(&list, &alloc_qdata(2)->list_node);
    qlist_append(&list, &alloc_qdata(3)->list_node);

    /* Iterate forward. */
    for(node = qlist_head(&list), n = 1; node != qlist_end(&list); node = qlist_next(node), n++) {
        data = QLIST_DATA(node, QDATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Clean up. */
    n = 0;
    while(!qlist_is_empty(&list)) {
        node = qlist_head(&list);
        qlist_remove_head(&list);
        free(QLIST_DATA(node, QDATA, list_node));
        n++;
    }
    TEST_CHECK(n == 3);
}

static void
test_qlist_insert(void)
{
    QLIST list;
    QLIST_NODE* node;
    QDATA* data;
    QDATA xxx;
    int n;

    /* Create simple list. */
    qlist_init(&list);
    qlist_prepend(&list, &alloc_qdata(5)->list_node);                           /* 5 */
    qlist_prepend(&list, &alloc_qdata(3)->list_node);                           /* 3 -> 5 */
    qlist_insert_after(&list, qlist_head(&list), &alloc_qdata(4)->list_node);   /* 3 -> 4 -> 5 */
    qlist_prepend(&list, &alloc_qdata(1)->list_node);                           /* 1 -> 3 -> 4 -> 5 */
    qlist_insert_after(&list, qlist_head(&list), &alloc_qdata(2)->list_node);   /* 1 -> 2 -> 3 -> 4 -> 5 */
    qlist_append(&list, &alloc_qdata(6)->list_node);                            /* 1 -> 2 -> 3 -> 4 -> 5 -> 6 */

    /* Verify that removals do not break integrity. */
    xxx.value = 666;
    qlist_prepend(&list, &xxx.list_node);
    qlist_remove_head(&list);
    qlist_insert_after(&list, qlist_head(&list), &xxx.list_node);
    qlist_remove(&list, qlist_head(&list), &xxx.list_node);

    /* Iterate forward. */
    for(node = qlist_head(&list), n = 1; node != qlist_end(&list); node = qlist_next(node), n++) {
        data = QLIST_DATA(node, QDATA, list_node);
        TEST_CHECK(data->value == n);
    }

    /* Clean up. */
    n = 0;
    while(!qlist_is_empty(&list)) {
        node = qlist_head(&list);
        qlist_remove_head(&list);
        free(QLIST_DATA(node, QDATA, list_node));
        n++;
    }
    TEST_CHECK(n == 6);
}


/*********************
 *** List of tests ***
 *********************/

TEST_LIST = {
    { "list-empty",     test_list_empty },
    { "list-iterate",   test_list_iterate },
    { "list-insert",    test_list_insert },

    { "slist-empty",    test_slist_empty },
    { "slist-iterate",  test_slist_iterate },
    { "slist-insert",   test_slist_insert },

    { "qlist-empty",    test_qlist_empty },
    { "qlist-iterate",  test_qlist_iterate },
    { "qlist-insert",   test_qlist_insert },

    { NULL, NULL }
};
