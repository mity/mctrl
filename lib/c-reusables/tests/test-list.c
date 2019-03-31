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
        list_remove(node);
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
    list_append(&list, &alloc_data(5)->list_node);                      /* 5 */
    list_prepend(&list, &alloc_data(2)->list_node);                     /* 2 -> 5 */
    list_insert_after(list_head(&list), &alloc_data(3)->list_node);     /* 1 -> 2 -> 3 */
    list_insert_before(list_head(&list), &alloc_data(1)->list_node);    /* 1 -> 2 -> 3 -> 5 */
    list_insert_before(list_tail(&list), &alloc_data(4)->list_node);    /* 1 -> 2 -> 3 -> 4 -> 5 */
    list_insert_after(list_tail(&list), &alloc_data(6)->list_node);     /* 1 -> 2 -> 3 -> 4 -> 5 -> 6 */

    /* Verify that removals do not break integrity. */
    xxx.value = 666;
    list_append(&list, &xxx.list_node);
    list_remove(&xxx.list_node);
    list_prepend(&list, &xxx.list_node);
    list_remove(&xxx.list_node);
    list_insert_after(list_head(&list), &xxx.list_node);
    list_remove(&xxx.list_node);

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
        list_remove(node);
        free(LIST_DATA(node, DATA, list_node));
        n++;
    }
    TEST_CHECK(n == 6);
}


TEST_LIST = {
    { "list-empty",     test_list_empty },
    { "list-iterate",   test_list_iterate },
    { "list-insert",    test_list_insert },
    { 0 }
};
