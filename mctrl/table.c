/*
 * Copyright (c) 2010-2015 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "table.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define TABLE_DEBUG     1*/

#ifdef TABLE_DEBUG
    #define TABLE_TRACE     MC_TRACE
#else
    #define TABLE_TRACE     MC_NOOP
#endif


#define MC_TCMF_ALL    (MC_TCMF_TEXT | MC_TCMF_PARAM | MC_TCMF_FLAGS)


static inline void
table_cell_clear(table_cell_t* cell)
{
    if(cell->text != NULL)
        free(cell->text);
}

static inline void
table_refresh(table_t* table, table_refresh_detail_t* detail)
{
    view_list_refresh(&table->vlist, detail);
}

static int
table_resize_helper(table_t* table, int col_pos, int col_delta,
                                    int row_pos, int row_delta)
{
    table_cell_t* cols;
    table_cell_t* rows;
    table_cell_t* cells;
    table_region_t copy_src[4];
    table_region_t copy_dst[4];
    table_region_t init_dst[3];
    table_region_t free_src[3];
    int copy_count, init_count, free_count;
    int col_count, row_count;
    int i;
    table_refresh_detail_t refresh_detail;

    TABLE_TRACE("table_resize_helper(%p, %d, %d, %d, %d)",
                table, col_pos, col_delta, row_pos, row_delta);

    if(col_delta == 0  ||  row_delta == 0) {
        if(col_delta == 0  &&  row_delta == 0) {
            /* noop */
            return 0;
        }

        if(col_delta == 0)
            col_pos = 0;
        if(row_delta == 0)
            row_pos = 0;
    }

    col_count = table->col_count + col_delta;
    row_count = table->row_count + row_delta;

    /* Allocate buffer for resized table */
    if(col_count > 0  ||  row_count > 0) {
        size_t size;

        size = col_count * sizeof(table_cell_t) +
               row_count * sizeof(table_cell_t) +
               (col_count*row_count) * sizeof(table_cell_t);
        cells = (table_cell_t*) malloc(size);
        if(MC_ERR(cells == NULL)) {
            MC_TRACE("table_resize: malloc() failed");
            return -1;
        }
        cols = cells + (col_count * row_count);
        rows = cols + col_count;
    }
    if(col_count == 0) {
        cells = NULL;
        cols = NULL;
    }
    if(row_count == 0) {
        cells = NULL;
        rows = NULL;
    }

    /* Analyze which region of the original cells shall be freed, which shall
     * be reused in the reallocated buffer, and which in the new buffer need
     * initialization. */
#define REGSET(reg, c0, r0, c1, r1)                                           \
            do { reg.col0 = c0; reg.row0 = r0;                                \
                 reg.col1 = c1; reg.row1 = r1; } while (0)
    if(col_delta >= 0  &&  row_delta >= 0) {
        /*
         *                     +---+-+---+
         *    +---+---+        | 0 | | 1 |
         *    | 0 | 1 |        +---+-+---+
         *    +---+---+  --->  |         |
         *    | 2 | 3 |        +---+-+---+
         *    +---+---+        | 2 | | 3 |
         *                     +---+-+---+
         */
        REGSET(copy_src[0], 0, 0, col_pos, row_pos);
        REGSET(copy_dst[0], 0, 0, col_pos, row_pos);
        REGSET(copy_src[1], col_pos, 0, table->col_count, row_pos);
        REGSET(copy_dst[1], col_pos + col_delta, 0, col_count, row_pos);
        REGSET(copy_src[2], 0, row_pos, col_pos, table->row_count);
        REGSET(copy_dst[2], 0, row_pos + row_delta, col_pos, row_count);
        REGSET(copy_src[3], col_pos, row_pos, table->col_count, table->row_count);
        REGSET(copy_dst[3], col_pos + col_delta, row_pos + row_delta, col_count, row_count);
        copy_count = 4;
        REGSET(init_dst[0], col_pos, 0, col_pos + col_delta, row_pos);
        REGSET(init_dst[1], 0, row_pos, col_count, row_pos + row_delta);
        REGSET(init_dst[2], col_pos, row_pos + row_delta, col_pos + col_delta, row_count);
        init_count = 3;
        free_count = 0;
    } else if(col_delta >= 0  &&  row_delta < 0) {
        /*
         *    +---+---+
         *    | 0 | 1 |        +---+-+---+
         *    +---+---+        | 0 | | 1 |
         *    |       |  --->  +---+ +---+
         *    +---+---+        | 2 | | 3 |
         *    | 2 | 3 |        +---+-+---+
         *    +---+---+
         */
        REGSET(copy_src[0], 0, 0, col_pos, row_pos);
        REGSET(copy_dst[0], 0, 0, col_pos, row_pos);
        REGSET(copy_src[1], col_pos, 0, table->col_count, row_pos);
        REGSET(copy_dst[1], col_pos + col_delta, 0, col_count, row_pos);
        REGSET(copy_src[2], 0, row_pos - row_delta, col_pos, table->row_count);
        REGSET(copy_dst[2], 0, row_pos, col_pos, row_count);
        REGSET(copy_src[3], col_pos, row_pos - row_delta, table->col_count, table->row_count);
        REGSET(copy_dst[3], col_pos + col_delta, row_pos, col_count, row_count);
        copy_count = 4;
        REGSET(init_dst[0], col_pos, 0, col_delta, row_count);
        init_count = 1;
        REGSET(free_src[0], 0, row_pos, table->col_count, row_pos - row_delta);
        free_count = 1;
    } else if(col_delta < 0  &&  row_delta >= 0) {
        /*
         *                       +---+---+
         *    +---+-+---+        | 0 | 1 |
         *    | 0 | | 1 |        +---+---+
         *    +---+ +---+  --->  |       |
         *    | 2 | | 3 |        +---+---+
         *    +---+-+---+        | 2 | 3 |
         *                       +---+---+
         */
        REGSET(copy_src[0], 0, 0, col_pos, row_pos);
        REGSET(copy_dst[0], 0, 0, col_pos, row_pos);
        REGSET(copy_src[1], col_pos - col_delta, 0, table->col_count, row_pos);
        REGSET(copy_dst[1], col_pos, 0, col_count, row_pos);
        REGSET(copy_src[2], 0, row_pos, col_pos, row_count);
        REGSET(copy_dst[2], 0, row_pos + row_delta, col_pos, row_count);
        REGSET(copy_src[3], col_pos - col_delta, row_pos, table->col_count, table->row_count);
        REGSET(copy_dst[3], col_pos, row_pos + row_delta, col_count, row_count);
        copy_count = 4;
        REGSET(init_dst[0], 0, row_pos, col_count, row_pos + row_delta);
        init_count = 1;
        REGSET(free_src[0], col_pos, 0, col_pos - col_delta, table->row_count);
        free_count = 1;
    } else {
        MC_ASSERT(col_delta < 0  &&  row_delta < 0);
        /*
         *    +---+-+---+
         *    | 0 | | 1 |        +---+---+
         *    +---+-+---+        | 0 | 1 |
         *    |         |  --->  +---+---+
         *    +---+-+---+        | 2 | 3 |
         *    | 2 | | 3 |        +---+---+
         *    +---+-+---+
         */
        REGSET(copy_src[0], 0, 0, col_pos, row_pos);
        REGSET(copy_dst[0], 0, 0, col_pos, row_pos);
        REGSET(copy_src[1], col_pos - col_delta, 0, table->col_count, row_pos);
        REGSET(copy_dst[1], col_pos, 0, col_count, row_pos);
        REGSET(copy_src[2], 0, row_pos - row_delta, col_pos, table->row_count);
        REGSET(copy_dst[2], 0, row_pos, col_pos, row_count);
        REGSET(copy_src[3], col_pos - col_delta, row_pos - row_delta, table->col_count, table->row_count);
        REGSET(copy_dst[3], col_pos, row_pos, col_count, row_count);
        copy_count = 4;
        init_count = 0;
        REGSET(free_src[0], col_pos, 0, col_pos - col_delta, row_pos);
        REGSET(free_src[1], 0, row_pos, table->col_count, row_pos - row_delta);
        REGSET(free_src[2], col_pos, row_pos - row_delta, col_pos - col_delta, table->row_count);
        free_count = 3;
    }
#undef REGSET

    /* Copy cells to be reused */
    if(table->cells != NULL  &&  cells != NULL) {
        for(i = 0; i < copy_count; i++) {
            MC_ASSERT(copy_src[i].col1-copy_src[i].col0 == copy_dst[i].col1-copy_dst[i].col0);
            MC_ASSERT(copy_src[i].row1-copy_src[i].row0 == copy_dst[i].row1-copy_dst[i].row0);

            if(col_delta == 0) {
                memcpy(&cells[copy_dst[i].row0 * col_count],
                       &table->cells[copy_src[i].row0 * col_count],
                       (copy_src[i].row1-copy_src[i].row0) * (copy_src[i].col1-copy_src[i].col0) * sizeof(table_cell_t));
            } else {
                WORD row_src, row_dst;
                for(row_src = copy_src[i].row0, row_dst = copy_dst[i].row0;
                            row_src < copy_src[i].row1; row_src++, row_dst++) {
                    memcpy(&cells[row_dst * col_count + copy_dst[i].col0],
                           &table->cells[row_src * table->col_count + copy_src[i].col0],
                           (copy_src[i].col1-copy_src[i].col0) * sizeof(table_cell_t));
                }
            }
        }
    }

    /* Init new cells in the new buffer */
    if(cells != NULL) {
        for(i = 0; i < init_count; i++) {
            if(col_delta == 0) {
                memset(&cells[col_count * init_dst[i].row0], 0,
                       col_count * (init_dst[i].row1-init_dst[i].row0) * sizeof(table_cell_t));
            } else {
                WORD row;
                for(row = init_dst[i].row0; row < init_dst[i].row1; row++) {
                    memset(&cells[row * col_count + init_dst[i].col0], 0,
                           (init_dst[i].col1-init_dst[i].col0) * sizeof(table_cell_t));
                }
            }
        }
    }

    /* Free bogus cells in the old buffer */
    if(table->cells != NULL) {
        for(i = 0; i < free_count; i++) {
            WORD col, row;
            for(row = free_src[i].row0; row < free_src[i].row1; row++) {
                for(col = free_src[i].col0; col < free_src[i].col1; col++) {
                    table_cell_clear(&table->cells[row * table->col_count + col]);
                }
            }
        }
    }

    /* Handle column headers */
    if(table->cols != NULL  &&  cols != NULL)
        memcpy(&cols[0], &table->cols[0], col_pos * sizeof(table_cell_t));
    if(col_delta >= 0) {
        if(cols != NULL)
            memset(&cols[col_pos], 0, col_delta * sizeof(table_cell_t));
        if(table->cols != NULL) {
            memcpy(&cols[col_pos + col_delta], &table->cols[col_pos],
                   (table->col_count - col_pos) * sizeof(table_cell_t));
        }
    } else {
        WORD col;
        MC_ASSERT(table->cols != NULL);  /* implies from col_delta < 0 */
        for(col = col_pos; col < col_pos - col_delta; col++)
            table_cell_clear(&table->cols[col]);
        if(cols != NULL) {
            memcpy(&cols[col_pos], &table->cols[col_pos - col_delta],
                   (col_count - col_pos) * sizeof(table_cell_t));
        }
    }

    /* Handle row headers */
    if(table->rows != NULL  &&  rows != NULL)
        memcpy(&rows[0], &table->rows[0], row_pos * sizeof(table_cell_t));
    if(row_delta >= 0) {
        if(rows != NULL)
            memset(&rows[row_pos], 0, row_delta * sizeof(table_cell_t));
        if(table->rows != NULL) {
            memcpy(&rows[row_pos + row_delta], &table->rows[row_pos],
                   (table->row_count - row_pos) * sizeof(table_cell_t));
        }
    } else {
        WORD row;
        MC_ASSERT(table->rows != NULL);  /* implies from row_delta < 0 */
        for(row = row_pos; row < row_pos - row_delta; row++)
            table_cell_clear(&table->rows[row]);
        if(rows != NULL) {
            memcpy(&rows[row_pos], &table->rows[row_pos - row_delta],
                   (row_count - row_pos) * sizeof(table_cell_t));
        }
    }

    /* Install the new buffer */
    if(table->cells)
        free(table->cells);
    table->cols = cols;
    table->rows = rows;
    table->cells = cells;
    table->col_count = col_count;
    table->row_count = row_count;

    /* Refresh */
    if(col_delta != 0) {
        refresh_detail.event = TABLE_COLCOUNT_CHANGED;
        refresh_detail.param[0] = table->col_count - col_delta;
        refresh_detail.param[1] = table->col_count;
        refresh_detail.param[2] = col_pos;
        table_refresh(table, &refresh_detail);
    }
    if(row_delta != 0) {
        refresh_detail.event = TABLE_ROWCOUNT_CHANGED;
        refresh_detail.param[0] = table->row_count - row_delta;
        refresh_detail.param[1] = table->row_count;
        refresh_detail.param[2] = row_pos;
        table_refresh(table, &refresh_detail);
    }

    return 0;
}

int
table_resize(table_t* table, WORD col_count, WORD row_count)
{
    int col_pos, col_delta;
    int row_pos, row_delta;

    TABLE_TRACE("table_resize(%p): %hd x %hd --> %hd x %hd", table,
                table->col_count, table->row_count, col_count, row_count);

    if(col_count >= table->col_count) {
        col_pos = table->col_count;
        col_delta = col_count - table->col_count;
    } else {
        col_pos = col_count;
        col_delta = -(int)(table->col_count - col_count);
    }

    if(row_count >= table->row_count) {
        row_pos = table->row_count;
        row_delta = row_count - table->row_count;
    } else {
        row_pos = row_count;
        row_delta = -(int)(table->row_count - row_count);
    }

    return table_resize_helper(table, col_pos, col_delta, row_pos, row_delta);
}

table_t*
table_create(WORD col_count, WORD row_count)
{
    table_t* table;

    TABLE_TRACE("table_create(%hd, %hd)", col_count, row_count);

    table = (table_t*) malloc(sizeof(table_t));
    if(MC_ERR(table == NULL)) {
        MC_TRACE("table_create: malloc() failed.");
        return NULL;
    }

    table->refs = 1;
    table->col_count = 0;
    table->row_count = 0;
    table->cols = NULL;
    table->rows = NULL;
    table->cells = NULL;

    view_list_init(&table->vlist);

    if(MC_ERR(table_resize(table, col_count, row_count) != 0)) {
        MC_TRACE("table_create: table_resize() failed.");
        free(table);
        return NULL;
    }

    return table;
}

void
table_destroy(table_t* table)
{
    TABLE_TRACE("table_destroy(%p)", table);
    MC_ASSERT(table->refs == 0);

    if(table->cells) {
        int i, n;

        n = table->col_count + table->row_count + table->col_count * table->row_count;
        for(i = 0; i < n; i++)
            table_cell_clear(&table->cells[i]);

        free(table->cells);
    }

    view_list_fini(&table->vlist);
    free(table);
}

static table_cell_t*
table_get_cell(table_t* table, WORD col, WORD row)
{
    TABLE_TRACE("table_get_cell(%p, %hd, %hd)", table, col, row);

    if(MC_ERR(col >= table->col_count  &&  col != MC_TABLE_HEADER)) {
        MC_TRACE("table_get_cell: Column ID %hd does not exist", col);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if(MC_ERR(row >= table->row_count  &&  row != MC_TABLE_HEADER)) {
        MC_TRACE("table_get_cell: Row ID %hd does not exist", row);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if(MC_ERR(col == MC_TABLE_HEADER  &&  row == MC_TABLE_HEADER)) {
        MC_TRACE("table_get_cell: The \"dead\" cell requested.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if(col == MC_TABLE_HEADER)
        return &table->rows[row];
    else if(row == MC_TABLE_HEADER)
        return &table->cols[col];
    else
        return table_cell(table, col, row);
}

int
table_set_cell_data(table_t* table, WORD col, WORD row, MC_TABLECELL* cell_data, BOOL unicode)
{
    table_cell_t* cell;
    table_refresh_detail_t refresh_detail;

    TABLE_TRACE("table_set_cell_data(%p, %hd, %hd, %p, %s)",
                table, col, row, cell_data, (unicode ? "unicode" : "ansi"));

    cell = table_get_cell(table, col, row);
    if(MC_ERR(cell == NULL)) {
        MC_TRACE("table_set_cell_data: table_get_cell() failed.");
        return -1;
    }

    if(MC_ERR(cell_data->fMask & ~MC_TCMF_ALL)) {
        MC_TRACE("table_set_cell_data: Unsupported pCell->fMask 0x%x", cell_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    /* Set the cell */
    if(cell_data->fMask & MC_TCMF_TEXT) {
        TCHAR* str;

        if(cell_data->pszText != NULL) {
            str = mc_str(cell_data->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
            if(MC_ERR(str == NULL)) {
                MC_TRACE("table_set_cell_data: mc_str() failed.");
                return -1;
            }
        } else {
            str = NULL;
        }
        table_cell_clear(cell);
        cell->text = str;
    }

    if(cell_data->fMask & MC_TCMF_PARAM)
        cell->lp = cell_data->lParam;

    if(cell_data->fMask & MC_TCMF_FLAGS)
        cell->flags = cell_data->dwFlags;

    /* Refresh */
    refresh_detail.event = TABLE_CELL_CHANGED;
    refresh_detail.param[0] = col;
    refresh_detail.param[1] = row;
    table_refresh(table, &refresh_detail);

    return 0;
}

int
table_get_cell_data(table_t* table, WORD col, WORD row, MC_TABLECELL* cell_data, BOOL unicode)
{
    table_cell_t* cell;

    TABLE_TRACE("table_get_cell_data(%p, %hd, %hd, %p, %s)",
                table, col, row, cell_data, (unicode ? "unicode" : "ansi"));

    cell = table_get_cell(table, col, row);
    if(MC_ERR(cell == NULL)) {
        MC_TRACE("table_set_cell_data: table_get_cell_data() failed.");
        return -1;
    }

    if(MC_ERR(cell_data->fMask & ~MC_TCMF_ALL)) {
        MC_TRACE("table_get_cell_data: Unsupported pCell->fMask 0x%x", cell_data->fMask);
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(cell_data->fMask & MC_TCMF_TEXT) {
        mc_str_inbuf(cell->text, MC_STRT, cell_data->pszText,
                     (unicode ? MC_STRW : MC_STRA), cell_data->cchTextMax);
    }

    if(cell_data->fMask & MC_TCMF_PARAM)
        cell_data->lParam = cell->lp;

    if(cell_data->fMask & MC_TCMF_FLAGS)
        cell_data->dwFlags = cell->flags;

    return 0;
}


/**************************
 *** Exported functions ***
 **************************/

MC_HTABLE MCTRL_API
mcTable_Create(WORD wColumnCount, WORD wRowCount, DWORD dwReserved)
{
    table_t* table;

    table = table_create(wColumnCount, wRowCount);
    if(MC_ERR(table == NULL)) {
        MC_TRACE("mcTable_Create: table_create() failed.");
        return NULL;
    }

    return table;
}

void MCTRL_API
mcTable_AddRef(MC_HTABLE hTable)
{
    if(hTable)
        table_ref((table_t*) hTable);
}

void MCTRL_API
mcTable_Release(MC_HTABLE hTable)
{
    if(hTable)
        table_unref((table_t*) hTable);
}

WORD MCTRL_API
mcTable_ColumnCount(MC_HTABLE hTable)
{
    return (hTable ? ((table_t*) hTable)->col_count : 0);
}

WORD MCTRL_API
mcTable_RowCount(MC_HTABLE hTable)
{
    return (hTable ? ((table_t*) hTable)->row_count : 0);
}

BOOL MCTRL_API
mcTable_Resize(MC_HTABLE hTable, WORD wColumnCount, WORD wRowCount)
{
    if(MC_ERR(table_resize(hTable, wColumnCount, wRowCount) != 0)) {
        MC_TRACE("mcTable_Resize: table_resize() failed.");
        return FALSE;
    }

    return TRUE;
}

void MCTRL_API
mcTable_Clear(MC_HTABLE hTable, DWORD dwWhat)
{
    table_t* table = (table_t*) hTable;
    table_cell_t* ptr[3];
    int count[3];
    int i, j, n = 0;
    table_refresh_detail_t refresh_detail;

    if(dwWhat == 0)
        dwWhat = 0xffffffff;   /* clear everything */

    if(dwWhat & 0x1) {
        ptr[n] = table->cells;
        count[n] = table->col_count * table->row_count;
        n++;
    }
    if(dwWhat & 0x2) {
        ptr[n] = table->cols;
        count[n] = table->col_count;
        n++;
    }
    if(dwWhat & 0x4) {
        ptr[n] = table->rows;
        count[n] = table->row_count;
        n++;
    }

    for(i = 0; i < n; i++) {
        for(j = 0; j < count[i]; j++)
            table_cell_clear(&ptr[i][j]);
        memset(ptr[i], 0, count[i] * sizeof(table_cell_t));
    }

    /* Refresh */
    refresh_detail.event = TABLE_REGION_CHANGED;
    if(dwWhat & 0x1) {
        refresh_detail.param[0] = 0;
        refresh_detail.param[1] = 0;
        refresh_detail.param[2] = table->col_count;
        refresh_detail.param[3] = table->row_count;
        table_refresh(table, &refresh_detail);
    }
    if(dwWhat & 0x2) {
        refresh_detail.param[0] = 0;
        refresh_detail.param[1] = MC_TABLE_HEADER;
        refresh_detail.param[2] = table->col_count;
        refresh_detail.param[3] = 0;
        table_refresh(table, &refresh_detail);
    }
    if(dwWhat & 0x4) {
        refresh_detail.param[0] = MC_TABLE_HEADER;
        refresh_detail.param[1] = 0;
        refresh_detail.param[2] = 0;
        refresh_detail.param[3] = table->row_count;
        table_refresh(table, &refresh_detail);
    }
}

BOOL MCTRL_API
mcTable_SetCellW(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELLW* pCell)
{
    if(MC_ERR(table_set_cell_data(hTable, wCol, wRow, (MC_TABLECELL*)pCell, TRUE) != 0)) {
        MC_TRACE("mcTable_SetCellW: table_set_cell_data() failed.");
        return FALSE;
    }
    return TRUE;
}

BOOL MCTRL_API
mcTable_SetCellA(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELLA* pCell)
{
    if(MC_ERR(table_set_cell_data(hTable, wCol, wRow, (MC_TABLECELL*)pCell, FALSE) != 0)) {
        MC_TRACE("mcTable_SetCellA: table_set_cell_data() failed.");
        return FALSE;
    }
    return TRUE;
}

BOOL MCTRL_API
mcTable_GetCellW(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELLW* pCell)
{
    if(MC_ERR(table_get_cell_data(hTable, wCol, wRow, (MC_TABLECELL*)pCell, TRUE) != 0)) {
        MC_TRACE("mcTable_GetCellA: table_get_cell_data() failed.");
        return FALSE;
    }
    return TRUE;
}

BOOL MCTRL_API
mcTable_GetCellA(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELLA* pCell)
{
    if(MC_ERR(table_get_cell_data(hTable, wCol, wRow, (MC_TABLECELL*)pCell, FALSE) != 0)) {
        MC_TRACE("mcTable_GetCellA: table_get_cell_data() failed.");
        return FALSE;
    }
    return TRUE;
}
