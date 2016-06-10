/*
 * Unit tests verifying grid control
 * Copyright (c) 2016 Martin Mitas
 */

#include "cutest.h"
#include <windows.h>
#include <mCtrl/grid.h>


static void
init_test(void)
{
    TEST_CHECK(mcGrid_Initialize());
    mcGrid_Terminate();
}


/*****************
 *** Test List ***
 *****************/

TEST_LIST = {
    { "initialization", init_test },
    { 0 }
};
