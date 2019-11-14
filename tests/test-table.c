/*
 * Unit tests verifying table data model.
 * Copyright (c) 2018 Martin Mitas
 */


#include "acutest.h"
#include <windows.h>
#include <mCtrl/table.h>


/***************
 *** Helpers ***
 ***************/

static MC_HTABLE
create_and_populate(int cols, int rows)
{
    MC_HTABLE table;
    MC_TABLECELLA cell;
    int c, r;
    char buffer[32];

    table = mcTable_Create(cols, rows, 0);
    TEST_CHECK(table != NULL);

    r = MC_TABLE_HEADER;
    for(c = 0; c < cols; c++) {
        sprintf(buffer, "[ %d, %d ]", c, r);
        cell.fMask = MC_TCMF_TEXT | MC_TCMF_PARAM;
        cell.pszText = buffer;
        cell.lParam = MAKELPARAM(c, r);

        TEST_CHECK(mcTable_SetCellA(table, c, r, &cell) == TRUE);
    }

    c = MC_TABLE_HEADER;
    for(r = 0; r < rows; r++) {
        sprintf(buffer, "[ %d, %d ]", c, r);
        cell.fMask = MC_TCMF_TEXT | MC_TCMF_PARAM;
        cell.pszText = buffer;
        cell.lParam = MAKELPARAM(c, r);

        TEST_CHECK(mcTable_SetCellA(table, c, r, &cell) == TRUE);
    }

    for(r = 0; r < rows; r++) {
        for(c = 0; c < cols; c++) {
            sprintf(buffer, "[ %d, %d ]", c, r);
            cell.fMask = MC_TCMF_TEXT | MC_TCMF_PARAM;
            cell.pszText = buffer;
            cell.lParam = MAKELPARAM(c, r);

            TEST_CHECK(mcTable_SetCellA(table, c, r, &cell) == TRUE);
        }
    }

    return table;
}

static void
check(MC_HTABLE table, int c, int r, LPARAM lp)
{
    MC_TABLECELLA cell;

    cell.fMask = MC_TCMF_PARAM;

    if(TEST_CHECK(mcTable_GetCellA(table, c, r, &cell) == TRUE))
        TEST_CHECK_(cell.lParam == lp, " cell [%d, %d]", c, r);
}


/******************
 *** Unit Tests ***
 ******************/

static void
test_simple(void)
{
    MC_HTABLE table;

    table = create_and_populate(4, 4);

    check(table, MC_TABLE_HEADER, 0, MAKELPARAM(MC_TABLE_HEADER, 0));
    check(table, 3, MC_TABLE_HEADER, MAKELPARAM(3, MC_TABLE_HEADER));

    check(table, 0, 0, MAKELPARAM(0, 0));
    check(table, 1, 2, MAKELPARAM(1, 2));
    check(table, 3, 3, MAKELPARAM(3, 3));

    mcTable_Release(table);
}

static void
test_no_columns(void)
{
    MC_HTABLE table;

    table = create_and_populate(0, 4);

    check(table, MC_TABLE_HEADER, 0, MAKELPARAM(MC_TABLE_HEADER, 0));

    mcTable_Release(table);
}

static void
test_no_rows(void)
{
    MC_HTABLE table;

    table = create_and_populate(4, 0);

    check(table, 3, MC_TABLE_HEADER, MAKELPARAM(3, MC_TABLE_HEADER));

    mcTable_Release(table);
}

static void
test_resize(void)
{
    MC_HTABLE table;

    table = create_and_populate(4, 4);

    TEST_CHECK(mcTable_Resize(table, 3, 5) == TRUE);
    TEST_CHECK(mcTable_ColumnCount(table) == 3);
    TEST_CHECK(mcTable_RowCount(table) == 5);

    check(table, MC_TABLE_HEADER, 0, MAKELPARAM(MC_TABLE_HEADER, 0));
    check(table, 2, MC_TABLE_HEADER, MAKELPARAM(2, MC_TABLE_HEADER));

    check(table, 0, 0, MAKELPARAM(0, 0));
    check(table, 1, 2, MAKELPARAM(1, 2));

    /* Check the new row is zeroed. */
    check(table, MC_TABLE_HEADER, 4, MAKELPARAM(0, 0));
    check(table, 0, 4, MAKELPARAM(0, 0));
    check(table, 2, 4, MAKELPARAM(0, 0));

    mcTable_Release(table);
}

static void
test_append_column(void)
{
    MC_HTABLE table;

    table = create_and_populate(4, 4);

    TEST_CHECK(mcTable_Resize(table, 5, 4) == TRUE);
    TEST_CHECK(mcTable_ColumnCount(table) == 5);
    TEST_CHECK(mcTable_RowCount(table) == 4);

    check(table, MC_TABLE_HEADER, 0, MAKELPARAM(MC_TABLE_HEADER, 0));
    check(table, 2, MC_TABLE_HEADER, MAKELPARAM(2, MC_TABLE_HEADER));

    check(table, 0, 0, MAKELPARAM(0, 0));
    check(table, 1, 1, MAKELPARAM(1, 1));
    check(table, 2, 2, MAKELPARAM(2, 2));
    check(table, 3, 3, MAKELPARAM(3, 3));

    /* Check the new column is zeroed. */
    check(table, 4, MC_TABLE_HEADER, MAKELPARAM(0, 0));
    check(table, 4, 0, MAKELPARAM(0, 0));
    check(table, 4, 1, MAKELPARAM(0, 0));
    check(table, 4, 2, MAKELPARAM(0, 0));
    check(table, 4, 3, MAKELPARAM(0, 0));

    mcTable_Release(table);
}

static void
test_append_row(void)
{
    MC_HTABLE table;

    table = create_and_populate(4, 4);

    TEST_CHECK(mcTable_Resize(table, 4, 5) == TRUE);
    TEST_CHECK(mcTable_ColumnCount(table) == 4);
    TEST_CHECK(mcTable_RowCount(table) == 5);

    check(table, MC_TABLE_HEADER, 0, MAKELPARAM(MC_TABLE_HEADER, 0));
    check(table, 2, MC_TABLE_HEADER, MAKELPARAM(2, MC_TABLE_HEADER));

    check(table, 0, 0, MAKELPARAM(0, 0));
    check(table, 1, 1, MAKELPARAM(1, 1));
    check(table, 2, 2, MAKELPARAM(2, 2));
    check(table, 3, 3, MAKELPARAM(3, 3));

    /* Check the new row is zeroed. */
    check(table, MC_TABLE_HEADER, 4, MAKELPARAM(0, 0));
    check(table, 0, 4, MAKELPARAM(0, 0));
    check(table, 1, 4, MAKELPARAM(0, 0));
    check(table, 2, 4, MAKELPARAM(0, 0));
    check(table, 3, 4, MAKELPARAM(0, 0));

    mcTable_Release(table);
}

static void
test_remove_row(void)
{
    MC_HTABLE table;

    table = create_and_populate(4, 4);

    TEST_CHECK(mcTable_Resize(table, 4, 3) == TRUE);
    TEST_CHECK(mcTable_ColumnCount(table) == 4);
    TEST_CHECK(mcTable_RowCount(table) == 3);

    check(table, MC_TABLE_HEADER, 0, MAKELPARAM(MC_TABLE_HEADER, 0));
    check(table, 2, MC_TABLE_HEADER, MAKELPARAM(2, MC_TABLE_HEADER));

    check(table, 0, 0, MAKELPARAM(0, 0));
    check(table, 1, 1, MAKELPARAM(1, 1));
    check(table, 2, 2, MAKELPARAM(2, 2));

    mcTable_Release(table);
}



/*****************
 *** Test List ***
 *****************/

TEST_LIST = {
    { "simple",                 test_simple },
    { "no-columns",             test_no_columns },
    { "no-rows",                test_no_rows },
    { "resize",                 test_resize },
    { "resize-append-column",   test_append_column },
    { "resize-append-row",      test_append_row },
    { "resize-remove-row",      test_remove_row },
    { 0 }
};
