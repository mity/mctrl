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
    if(cell->text != NULL && cell->text != MC_LPSTR_TEXTCALLBACK)
        free(cell->text);
}

static inline void
table_refresh(table_t* table, table_refresh_detail_t* detail)
{
    view_list_refresh(&table->vlist, detail);
}


typedef struct table_cell_buffer_tag table_cell_buffer_t;
struct table_cell_buffer_tag {
    table_cell_t* cells;
    table_cell_t* cols;
    table_cell_t* rows;
    int col_count;
    int row_count;
};

static void
table_init_region(table_cell_buffer_t* buf, const table_region_t* reg,
                  BOOL init_cols, BOOL init_rows)
{
    if(reg->col1 - reg->col0 > 0  &&  reg->row1 - reg->row0 > 0) {
        if(reg->col0 == 0  &&  reg->col1 == buf->col_count) {
            /* Special case: If the region covers whole rows, we can do it with a
             * single memset(). */
            memset(buf->cells + reg->row0 * buf->col_count, 0,
                   (reg->row1 - reg->row0) * buf->col_count * sizeof(table_cell_t));
        } else {
            int row;

            for(row = reg->row0; row < reg->row1; row++) {
                memset(buf->cells + row * buf->col_count + reg->col0, 0,
                       (reg->col1 - reg->col0) * sizeof(table_cell_t));
            }
        }
    }

    if(init_cols  &&  reg->col1 - reg->col0 > 0) {
        memset(buf->cols + reg->col0, 0,
               (reg->col1 - reg->col0) * sizeof(table_cell_t));
    }

    if(init_rows  &&  reg->row1 - reg->row0 > 0) {
        memset(buf->rows + reg->row0, 0,
               (reg->row1 - reg->row0) * sizeof(table_cell_t));
    }
}

static void
table_clear_region(table_cell_buffer_t* buf, const table_region_t* reg,
                   BOOL clear_cols, BOOL clear_rows)
{
    int col, row;

    for(row = reg->row0; row < reg->row1; row++) {
        for(col = reg->col0; col < reg->col1; col++)
            table_cell_clear(buf->cells + row * buf->col_count + col);
    }

    if(clear_cols) {
        for(col = reg->col0; col < reg->col1; col++)
            table_cell_clear(buf->cols + col);
    }

    if(clear_rows) {
        for(row = reg->row0; col < reg->row1; row++)
            table_cell_clear(buf->rows + row);
    }
}

static void
table_copy_region(table_cell_buffer_t* buf_dst, const table_region_t* reg_dst,
                  table_cell_buffer_t* buf_src, const table_region_t* reg_src,
                  BOOL copy_cols, BOOL copy_rows)
{
    MC_ASSERT(reg_dst->col1 - reg_dst->col0 == reg_src->col1 - reg_src->col0);
    MC_ASSERT(reg_dst->row1 - reg_dst->row0 == reg_src->row1 - reg_src->row0);

    if(reg_dst->col1 - reg_dst->col0 > 0  &&  reg_dst->row1 - reg_dst->row0 > 0) {
        if(reg_dst->col0 == 0  &&  reg_dst->col1 == buf_dst->col_count  &&
           reg_src->col0 == 0  &&  reg_src->col1 == buf_src->col_count)
        {
            /* Special case: If the both regions cover whole rows, we can do it
             * with a single memcpy(). */
            memcpy(buf_dst->cells + reg_dst->row0 * buf_dst->col_count,
                   buf_src->cells + reg_src->row0 * buf_src->col_count,
                   (reg_dst->row1 - reg_dst->row0) * buf_dst->col_count * sizeof(table_cell_t));
        } else {
            int row_dst, row_src;

            for(row_dst = reg_dst->row0, row_src = reg_src->row0;
                row_dst < reg_dst->row1;
                row_dst++, row_src++)
            {
                memcpy(buf_dst->cells + row_dst * buf_dst->col_count + reg_dst->col0,
                       buf_src->cells + row_src * buf_src->col_count + reg_src->col0,
                       (reg_dst->col1 - reg_dst->col0) * sizeof(table_cell_t));
            }
        }
    }

    if(copy_cols  &&  reg_dst->col1 - reg_dst->col0 > 0) {
        memcpy(buf_dst->cols + reg_dst->col0,
               buf_src->cols + reg_src->col0,
               (reg_dst->col1 - reg_dst->col0) * sizeof(table_cell_t));
    }

    if(copy_rows  &&  reg_dst->row1 - reg_dst->row0 > 0) {
        memcpy(buf_dst->rows + reg_dst->row0,
               buf_src->rows + reg_src->row0,
               (reg_dst->row1 - reg_dst->row0) * sizeof(table_cell_t));

    }
}

static int
table_resize_helper(table_t* table, int col_pos, int col_delta,
                                    int row_pos, int row_delta)
{
    table_cell_buffer_t buf_dst;
    table_cell_buffer_t buf_src;
    size_t size;

    buf_src.cells = table->cells;
    buf_src.cols = table->cols;
    buf_src.rows = table->rows;
    buf_src.col_count = table->col_count;
    buf_src.row_count = table->row_count;

    buf_dst.col_count = table->col_count + col_delta;
    buf_dst.row_count = table->row_count + row_delta;

    if(col_delta == 0)
        col_pos = table->col_count;
    if(row_delta == 0)
        row_pos = table->row_count;

    size = (buf_dst.col_count * buf_dst.row_count) * sizeof(table_cell_t) +
           buf_dst.col_count * sizeof(table_cell_t) +
           buf_dst.row_count * sizeof(table_cell_t);

    if(size == 0) {
        /* Trivial case: Resizing to an empty table. */
        table_region_t clear_reg = { 0, 0, buf_src.col_count, buf_src.row_count };
        table_clear_region(&buf_src, &clear_reg, TRUE, TRUE);
        free(buf_src.cells);

        buf_dst.cells = NULL;
        buf_dst.cols = NULL;
        buf_dst.rows = NULL;
    } else if(col_delta == 0  &&  row_delta > 0  &&  row_pos == buf_src.row_count) {
        /* Optimized case: When just adding new rows at the end of the table,
         * we can handle it more effectively with a realloc(). */
        table_region_t init_reg = { 0, row_pos, buf_dst.col_count, buf_dst.row_count };

        buf_dst.cells = (table_cell_t*) realloc(table->cells, size);
        if(MC_ERR(buf_dst.cells == NULL)) {
            MC_TRACE("table_resize_helper: realloc() failed");
            return -1;
        }

        /* Move col/row headers to the proper new place. */
        memmove(buf_dst.cells + (buf_dst.col_count * buf_dst.row_count),
                buf_dst.cells + (buf_src.col_count * buf_src.row_count),
                (buf_src.col_count + buf_src.row_count) * sizeof(table_cell_t));
        buf_dst.cols = buf_dst.cells + (buf_dst.col_count * buf_dst.row_count);
        buf_dst.rows = buf_dst.cols + buf_dst.col_count;

        /* Init the new rows (and their headers). */
        table_init_region(&buf_dst, &init_reg, FALSE, TRUE);
    } else if(col_delta == 0  &&  row_delta < 0  &&  row_pos == buf_dst.row_count) {
        /* Optimized case: When just removing some rows  at the end of the table.
         * This is similar to the above but we have to handle the realloc() failure
         * differently: If it fails we are in a situation when we have already
         * destroyed the old stuff. */
        table_region_t clear_reg = { 0, row_pos, buf_src.col_count, buf_src.row_count };
        table_clear_region(&buf_src, &clear_reg, FALSE, TRUE);

        /* Move col/row headers to the proper new place. */
        memmove(buf_src.cells + (buf_dst.col_count * buf_dst.row_count),
                buf_src.cells + (buf_src.col_count * buf_src.row_count),
                (buf_dst.col_count + buf_dst.row_count) * sizeof(table_cell_t));

        buf_dst.cells = (table_cell_t*) realloc(table->cells, size);
        if(MC_ERR(buf_dst.cells == NULL)) {
            MC_TRACE("table_resize_helper: realloc() failed. Cannot shrink.");
            /* Work with the original bigger buffer.
             * We just are just wasting some memory... */
        }

        buf_dst.cols = buf_dst.cells + (buf_dst.col_count * buf_dst.row_count);
        buf_dst.rows = buf_dst.cols + buf_dst.col_count;
    } else {
        /* This is the most general and complex case when we may be adding
         * or removing some columns and/or rows at the same time, anywhere
         * in the table.
         */
        buf_dst.cells = (table_cell_t*) malloc(size);
        if(MC_ERR(buf_dst.cells == NULL)) {
            MC_TRACE("table_resize_helper: malloc() failed");
            return -1;
        }
        buf_dst.cols = buf_dst.cells + (buf_dst.col_count * buf_dst.row_count);
        buf_dst.rows = buf_dst.cols + buf_dst.col_count;

        /*
         *                     +---+-+---+
         *    +---+---+        | A | | B |
         *    | A | B |        +---+-+---+
         *    +---+---+  --->  |         |
         *    | C | D |        +---+-+---+
         *    +---+---+        | C | | D |
         *                     +---+-+---+
         */
        {
            /* Copy the corner areas (A, B, C, D) which do not change */
            table_region_t reg_src_A = { 0, 0, col_pos, row_pos };
            table_region_t reg_dst_A = { 0, 0, col_pos, row_pos };
            table_region_t reg_src_B = { col_pos + (col_delta >= 0 ? 0 : -col_delta), 0, buf_src.col_count, row_pos };
            table_region_t reg_dst_B = { col_pos + (col_delta >= 0 ? col_delta : 0), 0, buf_dst.col_count, row_pos };
            table_region_t reg_src_C = { 0, row_pos + (row_delta >= 0 ? 0 : -row_delta), col_pos, buf_src.row_count };
            table_region_t reg_dst_C = { 0, row_pos + (row_delta >= 0 ? row_delta : 0), col_pos, buf_dst.row_count };
            table_region_t reg_src_D = { col_pos + (col_delta >= 0 ? 0 : -col_delta), row_pos + (row_delta >= 0 ? 0 : -row_delta), buf_src.col_count, buf_src.row_count };
            table_region_t reg_dst_D = { col_pos + (col_delta >= 0 ? col_delta : 0), row_pos + (row_delta >= 0 ? row_delta : 0), buf_dst.col_count, buf_dst.row_count };

            table_copy_region(&buf_dst, &reg_dst_A, &buf_src, &reg_src_A, TRUE, TRUE);
            table_copy_region(&buf_dst, &reg_dst_B, &buf_src, &reg_src_B, TRUE, FALSE);
            table_copy_region(&buf_dst, &reg_dst_C, &buf_src, &reg_src_C, FALSE, TRUE);
            table_copy_region(&buf_dst, &reg_dst_D, &buf_src, &reg_src_D, FALSE, FALSE);

            /* Handle new/removed rows. */
            if(row_delta > 0) {
                table_region_t reg_dst_X = { 0, row_pos, buf_dst.col_count, row_pos + row_delta };
                table_init_region(&buf_dst, &reg_dst_X, FALSE, TRUE);
            } else if(row_delta < 0) {
                table_region_t reg_src_X = { 0, row_pos, buf_src.col_count, row_pos - row_delta };
                table_clear_region(&buf_src, &reg_src_X, FALSE, TRUE);
            }

            /* Handle new/removed columns.
             * Beware they may be "interrupted" by an area of removed or
             * added rows. */
            if(col_delta > 0) {
                table_region_t reg_dst_X0 = { reg_dst_A.col1, reg_dst_A.row0, reg_dst_B.col0, reg_dst_B.row1 };
                table_region_t reg_dst_X1 = { reg_dst_C.col1, reg_dst_C.row0, reg_dst_D.col0, reg_dst_D.row1 };
                table_init_region(&buf_dst, &reg_dst_X0, TRUE, FALSE);
                table_init_region(&buf_dst, &reg_dst_X1, FALSE, FALSE);
            } else if(col_delta < 0) {
                table_region_t reg_src_X0 = { reg_src_A.col1, reg_src_A.row0, reg_src_B.col0, reg_src_B.row1 };
                table_region_t reg_src_X1 = { reg_src_C.col1, reg_src_C.row0, reg_src_D.col0, reg_src_D.row1 };
                table_clear_region(&buf_src, &reg_src_X0, TRUE, FALSE);
                table_clear_region(&buf_src, &reg_src_X1, FALSE, FALSE);
            }
        }

        free(buf_src.cells);
    }

    /* Install the new buffer. */
    table->cells = buf_dst.cells;
    table->cols = buf_dst.cols;
    table->rows = buf_dst.rows;
    table->col_count = buf_dst.col_count;
    table->row_count = buf_dst.row_count;
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

table_cell_t*
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

        if(cell_data->pszText == MC_LPSTR_TEXTCALLBACK) {
            str = MC_LPSTR_TEXTCALLBACK;
        } else if(cell_data->pszText != NULL) {
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
        if(cell->text == MC_LPSTR_TEXTCALLBACK) {
            MC_TRACE("table_get_cell_data: Table cell contains "
                     "MC_LPSTR_TEXTCALLBACK and that cannot be asked for.");
            SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        } else {
            mc_str_inbuf(cell->text, MC_STRT, cell_data->pszText,
                         (unicode ? MC_STRW : MC_STRA), cell_data->cchTextMax);
        }
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
