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


#include "acutest.h"
#include "rbtree.h"

#include <stdlib.h>


/* Provided by rbtree.c when built with -DCRE_TEST. */
int rbtree_verify(RBTREE* tree);


/******************************************************
 ***   Helpers for constructing our testing trees   ***
 ******************************************************/

/* Payload structures for our tree. */
typedef struct VAL {
    int x;
    RBTREE_NODE the_node;
} VAL;

/* Comparator of our VAL structures. */
static int
val_cmp(const RBTREE_NODE* node1, const RBTREE_NODE* node2)
{
    const VAL* val1 = RBTREE_DATA(node1, VAL, the_node);
    const VAL* val2 = RBTREE_DATA(node2, VAL, the_node);

    if(val1->x < val2->x)
        return -1;
    if(val1->x > val2->x)
        return +1;
    return 0;
}

/* Factory of our VAL structures. For our convenience, it returns pointer
 * to the RBTREE_NODE, so we can pass it directly into rbtree functions.
 * expecting that. */
static RBTREE_NODE*
make_val(int x)
{
    VAL* v;

    v = (VAL*) malloc(sizeof(VAL));
    TEST_ASSERT(v != NULL);
    v->x = x;

    return &v->the_node;
}

static void
destroy_val(VAL* val)
{
    free(val);
}

static void
clear_tree(RBTREE* tree)
{
    RBTREE_NODE* node;
    while(1) {
        node = rbtree_fini_step(tree);
        if(node == NULL)
            break;
        free(RBTREE_DATA(node, VAL, the_node));
    }
}


/*****************************
 ***   The test routines   ***
 *****************************/

static void
test_empty(void)
{
    RBTREE tree = RBTREE_INITIALIZER;

    TEST_CHECK(rbtree_verify(&tree) == 0);
    TEST_CHECK(rbtree_is_empty(&tree));
    TEST_CHECK(rbtree_insert(&tree, make_val(42), val_cmp) == 0);
    TEST_CHECK(rbtree_verify(&tree) == 0);
    TEST_CHECK(!rbtree_is_empty(&tree));
    clear_tree(&tree);
    TEST_CHECK(rbtree_is_empty(&tree));
}

static void
test_fini(void)
{
    RBTREE tree = RBTREE_INITIALIZER;
    RBTREE_NODE* node;
    VAL* val;
    int i;
    char visit_flag[1000] = { 0 };

    for(i = 0; i < 1000; i++)
        TEST_CHECK(rbtree_insert(&tree, make_val(i), val_cmp) == 0);
    TEST_CHECK(rbtree_verify(&tree) == 0);

    /* Verify rbtree_fini_step() visits exactly once every single node;
     * without assuming any particular order. */
    while(1) {
        node = rbtree_fini_step(&tree);
        if(node == NULL)
            break;

        val = RBTREE_DATA(node, VAL, the_node);
        if(!TEST_CHECK(0 <= val->x  &&  val->x < 1000))
            continue;

        TEST_CHECK(visit_flag[val->x] == 0);
        visit_flag[val->x] = 1;

        destroy_val(val);
    }
    for(i = 0; i < 1000; i++)
        TEST_CHECK(visit_flag[i] != 0);

    /* Verify the tree is in a good shape for reuse. */
    TEST_CHECK(rbtree_verify(&tree) == 0);
    TEST_CHECK(rbtree_is_empty(&tree));
}

static void
test_insert_lookup(void)
{
    static const struct {
        const char* name;
        int values[15];
    } vectors[] = {
        { "Ascending order",    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
        { "Descending order",   { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 } },
        { "Randomized order",   { 8, 1, 12, 6, 4, 14, 11, 9, 10, 15, 2, 13, 3, 5, 7 } }
    };
    VAL key, tmp;

    RBTREE tree = RBTREE_INITIALIZER;
    int vec, i;

    for(vec = 0; vec < sizeof(vectors) / sizeof(vectors[0]); vec++) {
        const char* name = vectors[vec].name;
        const int* values = vectors[vec].values;

        TEST_CASE(name);

        for(i = 0; i < 15; i++) {
            TEST_CHECK(rbtree_insert(&tree, make_val(values[i]), val_cmp) == 0);
            TEST_CHECK(rbtree_verify(&tree) == 0);
        }

        /* Verify all the numbers are there. */
        for(i = 0; i < 15; i++) {
            key.x = values[i];
            TEST_CHECK(rbtree_lookup(&tree, &key.the_node, val_cmp) != NULL);
        }

        /* Verify that other ones are not. */
        key.x = -1;
        TEST_CHECK(rbtree_lookup(&tree, &key.the_node, val_cmp) == NULL);
        key.x = 0xf00d;
        TEST_CHECK(rbtree_lookup(&tree, &key.the_node, val_cmp) == NULL);
        key.x = 0xbeef;
        TEST_CHECK(rbtree_lookup(&tree, &key.the_node, val_cmp) == NULL);

        /* Verify an attempt to insert the same numbers fails. */
        for(i = 0; i < 15; i++) {
            tmp.x = values[i];
            TEST_CHECK(rbtree_insert(&tree, &tmp.the_node, val_cmp) != 0);
            TEST_CHECK(rbtree_verify(&tree) == 0);
        }

        clear_tree(&tree);
    }
}

static void
test_remove(void)
{
    RBTREE tree = RBTREE_INITIALIZER;
    VAL key;
    RBTREE_NODE* removed;
    int i;

    for(i = 0; i < 1000; i++)
        TEST_CHECK(rbtree_insert(&tree, make_val(i), val_cmp) == 0);
    TEST_CHECK(rbtree_verify(&tree) == 0);

    for(i = 0; i < 1000; i += 3) {
        key.x = i;

        /* Check the value is there. */
        TEST_CHECK(rbtree_lookup(&tree, &key.the_node, val_cmp) != NULL);
        /* Check its removal. */
        removed = rbtree_remove(&tree, &key.the_node, val_cmp);
        TEST_CHECK(removed != NULL);
        /* Check it is no longer there. */
        TEST_CHECK(rbtree_lookup(&tree, &key.the_node, val_cmp) == NULL);
        /* Check another attempt to remove it fails. */
        TEST_CHECK(rbtree_remove(&tree, &key.the_node, val_cmp) == NULL);

        /* And the tree is still in a good shape. */
        TEST_CHECK(rbtree_verify(&tree) == 0);

        destroy_val(RBTREE_DATA(removed, VAL, the_node));
    }

    /* Remove all remaining values. */
    while(!rbtree_is_empty(&tree)) {
        /* Cheating a little bit here. We should not take manually the root
         * node from the opaque structure but whatever. */
        removed = rbtree_remove(&tree, tree.root, val_cmp);
        TEST_CHECK(removed != NULL);
        TEST_CHECK(rbtree_verify(&tree) == 0);
    }

    TEST_CHECK(rbtree_is_empty(&tree));
}

static void
test_walk_forward(void)
{
    static const struct {
        const char* name;
        int values[15];
    } vectors[] = {
        { "Ascending order",    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
        { "Descending order",   { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 } },
        { "Randomized order",   { 8, 1, 12, 6, 4, 14, 11, 9, 10, 15, 2, 13, 3, 5, 7 } }
    };

    RBTREE tree = RBTREE_INITIALIZER;
    int vec, i;
    VAL* val;
    VAL key;

    for(vec = 0; vec < sizeof(vectors) / sizeof(vectors[0]); vec++) {
        const char* name = vectors[vec].name;
        const int* values = vectors[vec].values;
        RBTREE_CURSOR cur;
        RBTREE_NODE* node;

        TEST_CASE(name);

        for(i = 0; i < 15; i++)
            TEST_CHECK(rbtree_insert(&tree, make_val(values[i]), val_cmp) == 0);
        TEST_CHECK(rbtree_verify(&tree) == 0);

        /* Verify the cur visits all the nodes and that it happens in the
         * right order. */
        for(node = rbtree_head(&tree, &cur), i = 1;
            node != NULL;
            node = rbtree_next(&cur), i++)
        {
            val = RBTREE_DATA(node, VAL, the_node);
            TEST_CHECK(val->x == i);
        }

        /* Verify any other attempt to go forward still returns NULL. */
        TEST_CHECK(rbtree_next(&cur) == NULL);

        /* Verify we still point to the last node and user can walk backward again. */
        key.x = 15;
        TEST_CHECK(rbtree_current(&cur) == rbtree_lookup(&tree, &key.the_node, val_cmp));
        key.x = 14;
        TEST_CHECK(rbtree_prev(&cur) == rbtree_lookup(&tree, &key.the_node, val_cmp));

        clear_tree(&tree);
    }
}

static void
test_walk_backward(void)
{
    static const struct {
        const char* name;
        int values[15];
    } vectors[] = {
        { "Ascending order",    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
        { "Descending order",   { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 } },
        { "Randomized order",   { 8, 1, 12, 6, 4, 14, 11, 9, 10, 15, 2, 13, 3, 5, 7 } }
    };

    RBTREE tree = RBTREE_INITIALIZER;
    int vec, i;
    VAL* val;
    VAL key;

    for(vec = 0; vec < sizeof(vectors) / sizeof(vectors[0]); vec++) {
        const char* name = vectors[vec].name;
        const int* values = vectors[vec].values;
        RBTREE_CURSOR cur;
        RBTREE_NODE* node;

        TEST_CASE(name);

        for(i = 0; i < 15; i++)
            TEST_CHECK(rbtree_insert(&tree, make_val(values[i]), val_cmp) == 0);
        TEST_CHECK(rbtree_verify(&tree) == 0);

        /* Verify the cur visits all the nodes and that it happens in the
         * right order. */
        for(node = rbtree_tail(&tree, &cur), i = 15;
            node != NULL;
            node = rbtree_prev(&cur), i--)
        {
            val = RBTREE_DATA(node, VAL, the_node);
            TEST_CHECK(val->x == i);
        }

        /* Verify any other attempt to go forward still returns NULL. */
        TEST_CHECK(rbtree_prev(&cur) == NULL);

        /* Verify we still point to the first node and user can walk forward again. */
        key.x = 1;
        TEST_CHECK(rbtree_current(&cur) == rbtree_lookup(&tree, &key.the_node, val_cmp));
        key.x = 2;
        TEST_CHECK(rbtree_next(&cur) == rbtree_lookup(&tree, &key.the_node, val_cmp));

        clear_tree(&tree);
    }
}

static void
test_lookup_ex(void)
{
    RBTREE tree = RBTREE_INITIALIZER;
    RBTREE_CURSOR cur;
    RBTREE_NODE* node;
    VAL key;
    int i;

    for(i = 0; i < 1000; i++)
        TEST_CHECK(rbtree_insert(&tree, make_val(i), val_cmp) == 0);
    TEST_CHECK(rbtree_verify(&tree) == 0);

    /* Verify that the return value and cursor are set consistently. */
    key.x = 42;
    node = rbtree_lookup_ex(&tree, &key.the_node, val_cmp, &cur);
    TEST_CHECK(node != NULL  &&  rbtree_current(&cur) == node);

    /* Ditto for non-existent node. */
    key.x = 0xbeef;
    TEST_CHECK(rbtree_lookup_ex(&tree, &key.the_node, val_cmp, &cur) == NULL);
    TEST_CHECK(rbtree_current(&cur) == NULL);

    clear_tree(&tree);
}


TEST_LIST = {
    { "empty",              test_empty },
    { "fini",               test_fini },
    { "insert-and-lookup",  test_insert_lookup },
    { "remove",             test_remove },
    { "walk-forward",       test_walk_forward },
    { "walk-backward",      test_walk_backward },
    { "lookup-ex",          test_lookup_ex },
    { NULL, NULL }
};
