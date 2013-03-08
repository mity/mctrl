/*
 * Copyright (c) 2010-2013 Martin Mitas
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
    #define TABLE_TRACE         MC_TRACE
#else
    #define TABLE_TRACE(...)    do { } while(0)
#endif


static void
table_init_cells_helper(table_cell_t* cells, size_t cell_count)
{
    static const table_cell_t cell_empty = { NULL, RGB(0,0,0), MC_CLR_NONE, 0 };
    size_t n;

    mc_inlined_memcpy(cells, &cell_empty, sizeof(table_cell_t));
    for(n = 1; 2*n < cell_count; n = 2*n)
        memcpy(cells + n, cells, n * sizeof(table_cell_t));
    if(n < cell_count)
        memcpy(cells + n, cells, (cell_count - n) * sizeof(table_cell_t));
}

static void
table_init_cells(table_cell_t* cells, WORD col_count, WORD row_count,
                 const table_region_t* reg)
{
    DWORD offset0, offset, count;
    WORD row;

    TABLE_TRACE("table_init_cells: Region %d,%d,%d,%d",
                reg->col0, reg->row0, reg->col1, reg->row1);

    if(reg->col1 - reg->col0 == 0  ||  reg->row1 - reg->row0 == 0)
        return;

    /* Optimized case if the region contains complete lines */
    if(reg->col0 == 0  &&  reg->col1 == col_count) {
        offset0 = reg->row0 * col_count;
        count = (reg->row1 - reg->row0) * col_count;
        table_init_cells_helper(cells + offset0, count);
        return;
    }

    /* Setup 1st row */
    offset0 = reg->row0 * col_count + reg->col0;
    count = reg->col1 - reg->col0;
    table_init_cells_helper(cells + offset0, count);

    /* Copy it into all the other rows */
    for(row = reg->row0 + 1; row < reg->row1; row++) {
        offset = row * col_count + reg->col0;
        memcpy(cells + offset, cells + offset0, count * sizeof(table_cell_t));
    }
}

static void
table_free_cells(table_cell_t* cells, WORD col_count, WORD row_count,
                 const table_region_t* reg)
{
    WORD col, row;
    value_t* value;

    TABLE_TRACE("table_free_cells: Region %d,%d,%d,%d",
                reg->col0, reg->row0, reg->col1, reg->row1);

    for(row = reg->row0; row < reg->row1; row++) {
        for(col = reg->col0; col < reg->col1; col++) {
            value = cells[row * col_count + col].value;
            if(value != NULL)
                value_destroy(value);
        }
    }
}

static void
table_move_cells(table_cell_t* dst_cells, WORD dst_col_count, WORD dst_row_count,
                 const table_region_t* dst_reg,
                 table_cell_t* src_cells, WORD src_col_count, WORD src_row_count,
                 const table_region_t* src_reg)
{
    DWORD src_offset, dst_offset, count;
    WORD i;

    TABLE_TRACE("table_move_cells: Region %d,%d,%d,%d -> %d,%d,%d,%d",
                src_reg->col0, src_reg->row0, src_reg->col1, src_reg->row1,
                dst_reg->col0, dst_reg->row0, dst_reg->col1, dst_reg->row1);

    MC_ASSERT(src_reg->col1 - src_reg->col0 == dst_reg->col1 - dst_reg->col0);
    MC_ASSERT(src_reg->row1 - src_reg->row0 == dst_reg->row1 - dst_reg->row0);

    if(src_reg->col1 - src_reg->col0 == 0  || src_reg->row1 - src_reg->row0 == 0)
        return;

    /* Optimized case if the region contains complete lines
     * (in both the source and the destination) */
    if(src_col_count == dst_col_count  &&
       src_reg->col0 == 0  &&  src_reg->col1 == src_col_count)
    {
        src_offset = src_reg->row0 * src_col_count;
        count = (src_reg->row1 - src_reg->row0) * src_col_count;
        memcpy(dst_cells + src_offset, src_cells + src_offset, count * sizeof(table_cell_t));
        return;
    }

    /* General case */
    count = src_reg->col1 - src_reg->col0;
    for(i = 0; i < src_reg->col1 - src_reg->col0; i++) {
        src_offset = (src_reg->row0 + i) * src_col_count + src_reg->col0;
        dst_offset = (dst_reg->row0 + i) * dst_col_count + dst_reg->col0;
        count = src_reg->col1 - src_reg->col0;
        memcpy(dst_cells + dst_offset, src_cells + src_offset, count * sizeof(table_cell_t));
    }
}

static inline void
table_refresh_views(table_t* table, table_region_t* region)
{
    view_list_refresh(&table->vlist, region);
}

table_t*
table_create(WORD col_count, WORD row_count)
{
    table_t* table;
    table_region_t reg;

    TABLE_TRACE("table_create(%d, %d)", col_count, row_count);

    table = (table_t*) malloc(sizeof(table_t));
    if(MC_ERR(table == NULL)) {
        MC_TRACE("table_create: malloc() failed.");
        return NULL;
    }

    if(col_count > 0  &&  row_count > 0) {
        table->cells = (table_cell_t*) malloc((size_t)col_count * (size_t)row_count * sizeof(table_cell_t));
        if(MC_ERR(table->cells == NULL)) {
            MC_TRACE("table_create: malloc() failed.");
            free(table);
            return NULL;
        }
    } else {
        table->cells = NULL;
    }

    reg.col0 = 0;
    reg.row0 = 0;
    reg.col1 = col_count;
    reg.row1 = row_count;
    table_init_cells(table->cells, col_count, row_count, &reg);

    table->refs = 1;
    table->col_count = col_count;
    table->row_count = row_count;

    view_list_init(&table->vlist);

    return table;
}

void
table_destroy(table_t* table)
{
    TABLE_TRACE("table_destroy");

    if(table->cells) {
        table_region_t reg;

        reg.col0 = 0;
        reg.row0 = 0;
        reg.col1 = table->col_count;
        reg.row1 = table->row_count;
        table_free_cells(table->cells, table->col_count, table->row_count, &reg);

        free(table->cells);
    }

    free(table);
}

int
table_resize(table_t* table, WORD col_count, WORD row_count)
{
    table_cell_t* cells;
    table_region_t reg;

    TABLE_TRACE("table_resize: %dx%d -> %dx%d",
                table->col_count, table->row_count, col_count, row_count);

    if(col_count == table->col_count  &&  row_count == table->row_count)
        return 0;

    if(col_count > 0  &&  row_count > 0) {
        cells = (table_cell_t*) malloc((size_t)col_count * (size_t)row_count * sizeof(table_cell_t));
        if(MC_ERR(cells == NULL)) {
            MC_TRACE("table_resize: malloc() failed.");
            return -1;
        }
    } else {
        cells = NULL;
    }

    /* Move intersected contents */
    reg.col0 = 0;
    reg.row0 = 0;
    reg.col1 = MC_MIN(col_count, table->col_count);
    reg.row1 = MC_MIN(row_count, table->row_count);
    table_move_cells(cells, col_count, row_count, &reg,
                     table->cells, table->col_count, table->row_count, &reg);

    /* Handle difference in col_count */
    reg.col0 = reg.col1;
    if(col_count > table->col_count) {
        reg.col1 = col_count;
        table_init_cells(cells, col_count, row_count, &reg);
    } else if(col_count < table->col_count) {
        reg.col1 = table->col_count;
        table_free_cells(table->cells, table->col_count, table->row_count, &reg);
    }

    /* Handle difference in row_count */
    reg.col0 = 0;
    reg.row0 = reg.row1;
    if(row_count > table->row_count) {
        reg.col1 = col_count;
        reg.row1 = row_count;
        table_init_cells(cells, col_count, row_count, &reg);
    } else if(row_count < table->row_count) {
        reg.col1 = table->col_count;
        reg.row1 = table->row_count;
        table_free_cells(table->cells, table->col_count, table->row_count, &reg);
    }

    /* Install new contents */
    if(table->cells)
        free(table->cells);
    table->cells = cells;
    table->col_count = col_count;
    table->row_count = row_count;

    table_refresh_views(table, NULL);
    return 0;
}

void
table_clear(table_t* table)
{
    table_region_t reg;

    TABLE_TRACE("table_clear");

    reg.col0 = 0;
    reg.row0 = 0;
    reg.col1 = table->col_count;
    reg.row1 = table->row_count;
    table_free_cells(table->cells, table->col_count, table->row_count, &reg);
    table_init_cells(table->cells, table->col_count, table->row_count, &reg);
}

void
table_get_cell(const table_t* table, WORD col, WORD row, MC_TABLECELL* cell)
{
    table_cell_t* c;

    c = &table->cells[row * table->col_count + col];

    if(cell->fMask & MC_TCMF_VALUE)
        cell->hValue = c->value;

    if(cell->fMask & MC_TCMF_FOREGROUND)
        cell->crForeground = c->fg_color;

    if(cell->fMask & MC_TCMF_BACKGROUND)
        cell->crBackground = c->bg_color;

    if(cell->fMask & MC_TCMF_FLAGS)
        cell->dwFlags = c->flags;
}

void
table_set_cell(table_t* table, WORD col, WORD row, MC_TABLECELL* cell)
{
    table_cell_t* c;
    table_region_t reg;

    TABLE_TRACE("table_set_cell(%dx%d)", col, row);

    c = &table->cells[row * table->col_count + col];

    if(cell->fMask & MC_TCMF_VALUE) {
        if(c->value != NULL)
            value_destroy(c->value);
        c->value = cell->hValue;
    }

    if(cell->fMask & MC_TCMF_FOREGROUND)
        c->fg_color = cell->crForeground;

    if(cell->fMask & MC_TCMF_BACKGROUND)
        c->bg_color = cell->crBackground;

    if(cell->fMask & MC_TCMF_FLAGS)
        c->flags = cell->dwFlags;

    reg.col0 = col;
    reg.row0 = row;
    reg.col1 = col + 1;
    reg.row1 = row + 1;
    table_refresh_views(table, &reg);
}


/**************************
 *** Exported functions ***
 **************************/

#define MC_TCMF_ALL    (MC_TCMF_VALUE | MC_TCMF_FOREGROUND | MC_TCMF_BACKGROUND | MC_TCMF_FLAGS)


MC_HTABLE MCTRL_API
mcTable_Create(WORD wColumnCount, WORD wRowCount, DWORD dwReserved)
{
    return (MC_HTABLE) table_create(wColumnCount, wRowCount);
}

void MCTRL_API
mcTable_AddRef(MC_HTABLE hTable)
{
    if(hTable)
        table_ref((table_t*)hTable);
}

void MCTRL_API
mcTable_Release(MC_HTABLE hTable)
{
    if(hTable)
        table_unref((table_t*)hTable);
}

WORD MCTRL_API
mcTable_ColumnCount(MC_HTABLE hTable)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(table == NULL)) {
        MC_TRACE("mcTable_ColumnCount: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return table->col_count;
}

WORD MCTRL_API
mcTable_RowCount(MC_HTABLE hTable)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(table == NULL)) {
        MC_TRACE("mcTable_RowCount: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return table->row_count;
}

BOOL MCTRL_API
mcTable_Resize(MC_HTABLE hTable, WORD wColumnCount, WORD wRowCount)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(table == NULL)) {
        MC_TRACE("mcTable_Resize: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return (table_resize(table, wColumnCount, wRowCount) == 0 ? TRUE : FALSE);
}

void MCTRL_API
mcTable_Clear(MC_HTABLE hTable)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(table == NULL)) {
        MC_TRACE("mcTable_Clear: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return;
    }

    table_clear(table);
}

BOOL MCTRL_API
mcTable_SetValue(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_HVALUE hValue)
{
    MC_TABLECELL cell;

    cell.fMask = MC_TCMF_VALUE;
    cell.hValue = hValue;
    return mcTable_SetCell(hTable, wCol, wRow, &cell);
}

const MC_HVALUE MCTRL_API
mcTable_GetValue(MC_HTABLE hTable, WORD wCol, WORD wRow)
{
    MC_TABLECELL cell;

    cell.fMask = MC_TCMF_VALUE;
    if(MC_ERR(!mcTable_GetCell(hTable, wCol, wRow, &cell)))
        return NULL;
    return cell.hValue;
}

BOOL MCTRL_API
mcTable_SetCell(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELL* pCell)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_SetCell: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(wCol >= table->col_count || wRow >= table->row_count)) {
        MC_TRACE("mcTable_SetCell: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(pCell->fMask & ~MC_TCMF_ALL)) {
        MC_TRACE("mcTable_SetCell: Unsupported pCell->fMask");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    table_set_cell(table, wCol, wRow, pCell);
    return TRUE;
}

BOOL MCTRL_API
mcTable_GetCell(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELL* pCell)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_GetCell: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(wCol >= table->col_count || wRow >= table->row_count)) {
        MC_TRACE("mcTable_GetCell: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(pCell->fMask & ~MC_TCMF_ALL)) {
        MC_TRACE("mcTable_GetCell: Unsupported pCell->fMask");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    table_get_cell(table, wCol, wRow, pCell);
    return TRUE;
}

MC_HVALUE MCTRL_API
mcTable_SwapValue(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_HVALUE value)
{
    table_t* table = (table_t*) hTable;
    table_cell_t* c;
    value_t* old_value;

    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_SwapValue: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if(MC_ERR(wCol >= table->col_count || wRow >= table->row_count)) {
        MC_TRACE("mcTable_SwapValue: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    c = &table->cells[wRow * table->col_count + wCol];
    old_value = c->value;
    c->value = (value_t*) value;

    return (MC_HVALUE) old_value;
}
