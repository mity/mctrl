/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2017 Martin Mitas
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
#include "cmdline.h"


static int
test_error_propagation_callback(int optid, const char* arg, void* userdata)
{
    return 0xbeef;
}

static void
test_error_propagation(void)
{
    static const CMDLINE_OPTION optlist[] = { { 0 } };
    static char* argv[] = { "foo", "bar" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    int ret;

    ret = cmdline_read(optlist, argc, argv, test_error_propagation_callback, NULL);
    TEST_CHECK(ret == 0xbeef);
}



static int
test_unknown_short_option_callback(int optid, const char* arg, void* userdata)
{
    switch(optid) {
        case CMDLINE_OPTID_UNKNOWN:
            TEST_CHECK(strcmp(arg, "-x") == 0);
            return -1;

        default:
            TEST_CHECK(0);  /* Should never be here */
            break;
    }

    return 0;
}

static void
test_unknown_short_option(void)
{
    static const CMDLINE_OPTION optlist[] = { { 0 } };
    static char* argv[] = { "foo", "-x" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    int ret;

    ret = cmdline_read(optlist, argc, argv, test_unknown_short_option_callback, NULL);
    TEST_CHECK(ret == -1);
}

/****************************************************************************/

static int
test_unknown_long_option_callback(int optid, const char* arg, void* userdata)
{
    switch(optid) {
        case CMDLINE_OPTID_UNKNOWN:
            TEST_CHECK(strcmp(arg, "--bar") == 0);
            return -1;
            break;

        default:
            TEST_CHECK(0);  /* Should never be here */
            break;
    }

    return 0;
}

static void
test_unknown_long_option(void)
{
    static const CMDLINE_OPTION optlist[] = { { 0 } };
    static char* argv[] = { "foo", "--bar=arg" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    int ret;

    ret = cmdline_read(optlist, argc, argv, test_unknown_long_option_callback, NULL);
    TEST_CHECK(ret == -1);
}

/****************************************************************************/

static int
test_no_options_callback(int optid, const char* arg, void* userdata)
{
    /* Never called when argc == 1. */
    TEST_CHECK(0);
    return 0xbeef;
}

static void
test_no_options(void)
{
    static const CMDLINE_OPTION optlist[] = { { 0 } };
    static char* argv[] = { "foo" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    int ret;

    ret = cmdline_read(optlist, argc, argv, test_no_options_callback, NULL);
    TEST_CHECK(ret == 0);
}

/****************************************************************************/

typedef struct TEST_SHORT_OPTIONS_RESULT {
    int a_used;
    int b_used;
    int c_used;
    int d_used;
    int e_used;
    int f_used;
    int nonoption_arg_used;
} TEST_SHORT_OPTIONS_RESULT;

static int
test_short_options_callback(int optid, const char* arg, void* userdata)
{
    TEST_SHORT_OPTIONS_RESULT* res = (TEST_SHORT_OPTIONS_RESULT*) userdata;

    switch(optid) {
        case 'a':
            res->a_used++;
            TEST_CHECK(arg == NULL);
            break;

        case 'b':
            res->b_used++;
            TEST_CHECK(arg == NULL);
            break;

        case 'c':
            res->c_used++;
            TEST_CHECK(arg == NULL);
            break;

        case 'd':
            res->d_used++;
            TEST_CHECK(strcmp(arg, "arg") == 0);
            break;

        case 'e':
            res->e_used++;
            TEST_CHECK(strcmp(arg, "arg") == 0);
            break;

        case 'f':
            res->f_used++;
            TEST_CHECK(arg == NULL);
            break;

        case 0:
            res->nonoption_arg_used++;
            TEST_CHECK(strcmp(arg, "arg") == 0);
            break;

        default:
            TEST_CHECK(0);  /* should never be called. */
            break;
    }

    return 0;
}

/****************************************************************************/

static void
test_short_options(void)
{
    static const CMDLINE_OPTION optlist[] = {
        { 'a', NULL, 'a', 0 },
        { 'b', NULL, 'b', 0 },
        { 'c', NULL, 'c', 0 },
        { 'd', NULL, 'd', CMDLINE_OPTFLAG_REQUIREDARG },
        { 'e', NULL, 'e', CMDLINE_OPTFLAG_REQUIREDARG },
        { 'f', NULL, 'f', CMDLINE_OPTFLAG_OPTIONALARG },
        { 0 }
    };
    static char* argv[] = { "foo", "-a", "-bc", "-darg", "-e", "arg", "-f", "arg" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    TEST_SHORT_OPTIONS_RESULT res = { 0 };
    int ret;

    ret = cmdline_read(optlist, argc, argv, test_short_options_callback, &res);
    TEST_CHECK_(ret == 0, "return value");
    TEST_CHECK_(res.a_used, "short option handling");
    TEST_CHECK_(res.b_used  &&  res.c_used, "shortoption group handling");
    TEST_CHECK_(res.d_used, "short option with argument");
    TEST_CHECK_(res.e_used, "short option with argument delimited with whitespace");
    TEST_CHECK_(res.f_used  &&  res.nonoption_arg_used, "short option ignores optional arg. flag.");
}

/****************************************************************************/

typedef struct TEST_LONG_OPTIONS_RESULT {
    int a_used;
    int b_used;
    int c_used;
    int d_used;
} TEST_LONG_OPTIONS_RESULT;

static int
test_long_options_callback(int optid, const char* arg, void* userdata)
{
    TEST_LONG_OPTIONS_RESULT* res = (TEST_LONG_OPTIONS_RESULT*) userdata;

    switch(optid) {
        case 'a':
            res->a_used++;
            TEST_CHECK(arg == NULL);
            break;

        case 'b':
            res->b_used++;
            TEST_CHECK(strcmp(arg, "arg") == 0);
            break;

        case 'c':
            res->c_used++;
            TEST_CHECK(arg == NULL);
            break;

        case 'd':
            res->d_used++;
            TEST_CHECK(strcmp(arg, "arg") == 0);
            break;

        default:
            TEST_CHECK(0);  /* should never be called. */
            break;
    }

    return 0;
}

static void
test_long_options(void)
{
    static const CMDLINE_OPTION optlist[] = {
        { 0, "long-a", 'a', 0 },
        { 0, "long-b", 'b', CMDLINE_OPTFLAG_REQUIREDARG },
        { 0, "long-c", 'c', CMDLINE_OPTFLAG_OPTIONALARG },
        { 0, "long-d", 'd', CMDLINE_OPTFLAG_OPTIONALARG },
        { 0 }
    };
    static char* argv[] = { "foo", "--long-a", "--long-b=arg", "--long-c", "--long-d=arg" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    TEST_LONG_OPTIONS_RESULT res = { 0 };
    int ret;

    ret = cmdline_read(optlist, argc, argv, test_long_options_callback, &res);
    TEST_CHECK_(ret == 0, "return value");
    TEST_CHECK_(res.a_used, "long option handling");
    TEST_CHECK_(res.b_used, "long option with required argument");
    TEST_CHECK_(res.c_used, "long option with missing optional argument");
    TEST_CHECK_(res.d_used, "long option with present optional argument");
}

/****************************************************************************/

typedef struct TEST_NONOPTIONS_RESULT {
    int a_used;
    int b_used;
    int c_used;
    int d_used;
    int arg_dash_used;
    int arg_xyz_used;
    int arg_c_used;
    int arg_d_used;
    int arg_doubledash_used;
} TEST_NONOPTIONS_RESULT;

static int
test_nonoptions_callback(int optid, const char* arg, void* userdata)
{
    TEST_NONOPTIONS_RESULT* res = (TEST_NONOPTIONS_RESULT*) userdata;

    switch(optid) {
        case 'a':
            res->a_used++;
            break;

        case 'b':
            res->b_used++;
            break;

        case 'c':
            res->c_used++;
            break;

        case 'd':
            res->d_used++;
            break;

        case 0:
            if(strcmp(arg, "-") == 0)
                res->arg_dash_used++;
            else if(strcmp(arg, "xyz") == 0)
                res->arg_xyz_used++;
            else if(strcmp(arg, "-c") == 0)
                res->arg_c_used++;
            else if(strcmp(arg, "--long-d") == 0)
                res->arg_d_used++;
            else if(strcmp(arg, "--") == 0)
                res->arg_doubledash_used++;
            else
                TEST_CHECK(0);  /* should never happen. */
            break;

        default:
            TEST_CHECK(0);  /* should never happen. */
            break;
    }

    return 0;
}

static void
test_nonoptions(void)
{
    static const CMDLINE_OPTION optlist[] = {
        { 'a', NULL,     'a', 0 },
        { 0,   "long-b", 'b', CMDLINE_OPTFLAG_OPTIONALARG },
        { 'c', NULL,     'c', 0 },
        { 0,   "long-d", 'd', 0 },
        { 0 }
    };
    static char* argv[] = { "foo", "-a", "-", "--long-b", "xyz", "--", "-c", "--long-d", "--" };
    static const int argc = sizeof(argv) / sizeof(argv[0]);
    TEST_NONOPTIONS_RESULT res = { 0 };

    int ret;

    ret = cmdline_read(optlist, argc, argv, test_nonoptions_callback, &res);
    TEST_CHECK_(ret == 0, "return value");
    TEST_CHECK_(res.a_used == 1, "short option before double-dash");
    TEST_CHECK_(res.arg_dash_used == 1, "singleton dash is never option");
    TEST_CHECK_(res.b_used == 1, "long option before double-dash");
    TEST_CHECK_(res.c_used == 0, "short option after double-dash");
    TEST_CHECK_(res.d_used == 0, "long option after double-dash");
    TEST_CHECK_(res.arg_dash_used == 1, "double-dash after double-dash");
}

/****************************************************************************/

TEST_LIST = {
    { "error-propagation",      test_error_propagation },
    { "unknown-short-option",   test_unknown_short_option },
    { "unknown-long-option",    test_unknown_long_option },
    { "no-options",             test_no_options },
    { "short-options",          test_short_options },
    { "long-options",           test_long_options },
    { "non-options",            test_nonoptions },
    { 0 }
};
