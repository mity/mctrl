/*
 * Copyright (c) 2010-2011 Martin Mitas
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

#ifndef MCTRL_TABLE_H
#define MCTRL_TABLE_H

#include <mCtrl/defs.h>
#include <mCtrl/value.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Table (data model for grid control)
 *
 * The table is actually a container which manages set of values (@ref MC_VALUE)
 * in two-dimensional matrix. It serves as a back-end for the grid control
 * (@ref MC_WC_GRID).
 *
 * When the table is created with the function @ref mcTable_Create(), the
 * caller determines whether the table shall be @b homogenous or @b
 * heterogenous.
 *
 * @section sec_grid_homo Homogenous tables
 *
 * Homogenous tables can hold only values of the same type (@ref MC_VALUETYPE),
 * specified during creation of the table. Homogenous table is less memory
 * consuming, as it just holds @ref MC_VALUE handle for each cell.
 *
 * However it severly limits flexibility of the table. Any attempt to cet any
 * cell of the table to other type then specified will fail.
 *
 * After the homogenous table is created, you can never reset it to be
 * heterogenous, or to use other value type for its cells.
 *
 * Furthermore even in the initial state after the creation, the table cannot
 * generally be considered empty: All the values (for each cell) are just
 * set to @c NULL. It depends on particular value type how this value is
 * interpreted.
 *
 * @section sec_grid_hetero Heterogenous tables
 *
 * When @c NULL is used as the value type during table creation, then a
 * heterogenous table is created.
 * 
 * Heterogenous table holds pair of @ref MC_VALUETYPE and @ref MC_VALUE for
 * each cell. Initially the table is empty (both the handles of each cell are
 * @c NULL).
 *
 * This allows to set values of any type arbitrarily and types of cells can
 * change dynamically during the table lifetime as different types are
 * specified in @ref mcTable_SetCell().
 */


/**
 * @brief Opaque table handle.
 */
typedef void* MC_TABLE;


/**
 * @brief Create new table.
 *
 * The table is initially empty and has its reference counter set to 1.
 *
 * @param[in] wColumnCount Column count.
 * @param[in] wRowCount Row count.
 * @param[in] hType Type of all cells for homogenous table, or @c NULL for
 * heterogenous table.
 * @param dwFlags Reserved, set to zero.
 * @return Handle of the new table or @c NULL on failure.
 */
MC_TABLE MCTRL_API mcTable_Create(WORD wColumnCount, WORD wRowCount,
                                  MC_VALUETYPE hType, DWORD dwFlags);

/**
 * @brief Increment reference counter of the table.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_AddRef(MC_TABLE hTable);

/**
 * @brief Decrement reference counter of the table.
 *
 * If the reference counter drops to zero, all resources allocated for 
 * the table are released.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_Release(MC_TABLE hTable);

/**
 * @brief Retrieve count of table columns.
 *
 * @param[in] hTable The table.
 * @return The count.
 */
WORD MCTRL_API mcTable_ColumnCount(MC_TABLE hTable);

/**
 * @brief Retrieve count of table rows.
 *
 * @param[in] hTable The table.
 * @return The count.
 */
WORD MCTRL_API mcTable_RowCount(MC_TABLE hTable);

/**
 * @brief Resize the table.
 *
 * If a table dimension decreases, the values from excessive cells are destroyed.
 * If a table dimension increases, the new cells are initialized to empty
 * values (in case of heterogenous table) or to @c NULL values (for homogenous
 * table).
 *
 * @param[in] hTable The table.
 * @param[in] wColumnCount Column count.
 * @param[in] wRowCount Row count.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_Resize(MC_TABLE hTable, WORD wColumnCount, WORD wRowCount);

/**
 * @brief Clear the table.
 *
 * All values are destroyed. In case of homogenous table, all cells are reset
 * to @c NULL values of the particular value type, in case of homogenous
 * table the table becomes empty.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_Clear(MC_TABLE hTable);

/**
 * @brief Set contents of a cell.
 *
 * Note that (in case of success) the table takes responsibility for the value.
 * I.e. when the table is later deallocated, or if the particular cell is later
 * reset, the value is destroyed.
 *
 * To reset the cell, set both the parameters @c type and @c value to @c NULL.
 * The reset leads to destroying of a value actually stored in the cell as
 * a side-effect.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] type Value type. In case of homogenous table the type must match the
 * type used during table creation.
 * @param[in] value The value.
 * @return @c TRUE on success, @c FALSE otherwise. 
 */
BOOL MCTRL_API mcTable_SetCell(MC_TABLE hTable, WORD wCol, WORD wRow, 
                               MC_VALUETYPE type, MC_VALUE value);

/**
 * @brief Get contents of a cell.
 *
 * Note that you don't get a responsibility for the value. If you want to
 * store it elsewhere and/or modify it, you should do so on your copy.
 * You may use @ref mcValue_Duplicate() for that purpose.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[out] phType Value type is filled here.
 * @param[out] phValue Value handle is filled here.
 * @return @c TRUE on success, @c FALSE otherwise. 
 */
BOOL MCTRL_API mcTable_GetCell(MC_TABLE hTable, WORD wCol, WORD wRow, 
                               MC_VALUETYPE* phType, MC_VALUE* phValue);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_TABLE_H */
