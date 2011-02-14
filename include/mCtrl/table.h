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
 * The table is actually a container which manages set of values (@ref MC_HVALUE)
 * in two-dimensional matrix. It serves as a back-end for the grid control
 * (@ref MC_WC_GRID).
 *
 * When the table is created with the function @ref mcTable_Create(), the
 * caller determines whether the table shall be @b homogenous or @b
 * heterogenous, and optionally also what other attributes the table won't
 * store.
 *
 * Using homogenous table and/or setting one or more of those flags can be
 * used to save some memory if you know your application never sets certain
 * attributes of table cells.
 *
 * If application sets some of such flags (e.g. @c MC_TF_NOCELLFOREGROUND) and
 * later it attempts to set he corresponding attribute of the cell (e.g. 
 * foreground color), the behavior is undefined (setting of some attributes
 * is silantly ignored, or the operation as a whole can fail but application
 * should not rely on exact behavior).
 *
 * If appplication attemps to insert a value of another type into a homogenous
 * table expecting other value type, the operation fails.
 *
 *
 * @section sec_grid_homo Homogenous tables
 *
 * Homogenous tables can hold only values of the same type (@ref MC_HVALUETYPE),
 * specified during creation of the table. Homogenous table is less memory
 * consuming, as it just holds @ref MC_HVALUE handle for each cell.
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
 *
 * @section sec_grid_hetero Heterogenous tables
 *
 * When @c NULL is used as the value type during table creation, then a
 * heterogenous table is created.
 * 
 * Heterogenous table holds pair of @ref MC_HVALUETYPE and @ref MC_HVALUE for
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
typedef void* MC_HTABLE;


/**
 * @anchor MC_TF_xxxx
 * @name Table Flags
 * @sa mcTable_Create()
 */
/*@{*/
/** @brief If set, the table does not store foreground colors for each cell. */
#define MC_TF_NOCELLFOREGROUND      0x00000001
/** @brief If set, the table does not store background colors for each cell. */
#define MC_TF_NOCELLBACKGROUND      0x00000002
/** @brief If set, the table does not store flags for each cell. */
#define MC_TF_NOCELLFLAGS           0x00000004
/*@}*/

/**
 * @brief Create new table.
 *
 * The table is initially empty and has its reference counter set to 1.
 *
 * @param[in] wColumnCount Column count.
 * @param[in] wRowCount Row count.
 * @param[in] hType Type of all cells for homogenous table, or @c NULL for
 * heterogenous table.
 * @param[in] dwFlags Table flags. See @ref MC_TF_xxxx.
 * @return Handle of the new table or @c NULL on failure.
 */
MC_HTABLE MCTRL_API mcTable_Create(WORD wColumnCount, WORD wRowCount,
                                   MC_HVALUETYPE hType, DWORD dwFlags);

/**
 * @brief Increment reference counter of the table.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_AddRef(MC_HTABLE hTable);

/**
 * @brief Decrement reference counter of the table.
 *
 * If the reference counter drops to zero, all resources allocated for 
 * the table are released.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_Release(MC_HTABLE hTable);

/**
 * @brief Retrieve count of table columns.
 *
 * @param[in] hTable The table.
 * @return The count.
 */
WORD MCTRL_API mcTable_ColumnCount(MC_HTABLE hTable);

/**
 * @brief Retrieve count of table rows.
 *
 * @param[in] hTable The table.
 * @return The count.
 */
WORD MCTRL_API mcTable_RowCount(MC_HTABLE hTable);

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
BOOL MCTRL_API mcTable_Resize(MC_HTABLE hTable, WORD wColumnCount, WORD wRowCount);

/**
 * @brief Clear the table.
 *
 * All values are destroyed. In case of homogenous table, all cells are reset
 * to @c NULL values of the particular value type, in case of homogenous
 * table the table becomes empty.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_Clear(MC_HTABLE hTable);

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
 *
 * @sa mcTable_SetCellEx
 */
BOOL MCTRL_API mcTable_SetCell(MC_HTABLE hTable, WORD wCol, WORD wRow, 
                               MC_HVALUETYPE type, MC_HVALUE value);

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
BOOL MCTRL_API mcTable_GetCell(MC_HTABLE hTable, WORD wCol, WORD wRow, 
                               MC_HVALUETYPE* phType, MC_HVALUE* phValue);

/**
 * @anchor MC_TCM_xxxx
 * @name MC_TABLECELL::fMask Bits
 * @memberof MC_TABLECELL
 */
/*@{*/
/** @brief Set if @ref MC_TABLECELL::hType and @ref MC_TABLECELL::hValue are valid. */
#define MC_TCM_VALUE                0x00000001
/** @brief Set if @ref MC_TABLECELL::crForeground is valid. */
#define MC_TCM_FOREGROUND           0x00000002
/** @brief Set if @ref MC_TABLECELL::crBackground is valid. */
#define MC_TCM_BACKGROUND           0x00000004
/** @brief Set if @ref MC_TABLECELL::dwFlags is valid. */
#define MC_TCM_FLAGS                0x00000008
/*@}*/

/**
 * @anchor MC_TCF_xxxx
 * @name MC_TABLECELL::dwFlags Bits
 * @memberof MC_TABLECELL
 */
/*@{*/
/** @brief Paint the cell value aligned horizontally as default for the value type. */
#define MC_TCF_ALIGNDEFAULT         0x00000000
/** @brief Paint the cell value aligned horizontally to left. */
#define MC_TCF_ALIGNLEFT            0x00000001
/** @brief Paint the cell value aligned horizontally to center. */
#define MC_TCF_ALIGNCENTER          0x00000003
/** @brief Paint the cell value aligned horizontally to right. */
#define MC_TCF_ALIGNRIGHT           0x00000002
/** @brief Paint the cell value aligned vertically as normal for the value type. */
#define MC_TCF_ALIGNVDEFAULT        0x00000000
/** @brief Paint the cell value aligned vertically to top. */
#define MC_TCF_ALIGNTOP             0x00000004
/** @brief Paint the cell value aligned vertically to center. */
#define MC_TCF_ALIGNVCENTER         0x0000000C
/** @brief Paint the cell value aligned vertically to bottom. */
#define MC_TCF_ALIGNBOTTOM          0x00000008
/*@}*/

/**
 * @brief Structure describing a table cell.
 *
 * Note that only members corresponding to the set bits of the @c fMask
 * are considered valid. (@c fMask itself is always valid of course.)
 *
 * @sa mcTable_SetCellEx mcTable_GetCellEx
 */
typedef struct MC_TABLECELL_tag {
    /** @brief Bitmask specifying what other members are valid. See @ref MC_TCM_xxxx. */
    DWORD fMask;
    /** @brief Handle of value type. */
    MC_HVALUETYPE hType;
    /** @brief Handle of value. */
    MC_HVALUE hValue;
    /** @brief Foreground color. It's up to the value type if it respects it. */
    COLORREF crForeground;
    /** @brief Background color. */
    COLORREF crBackground;
    /** @brief Cell flags. See @ref MC_TCF_xxxx. */
    DWORD dwFlags;
} MC_TABLECELL;


/**
 * @brief Set contents of a cell.
 *
 * If @c pCell->fMask includes the bit @c MC_TCM_VALUE, then
 * (in case of success) the table takes responsibility for the value.
 * I.e. when the table is later deallocated, or if the particular cell is later
 * reset, the value is then destroyed.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] pCell Specifies attributes of the cell to set.
 * @return @c TRUE on success, @c FALSE otherwise.
 *
 * @sa mcTable_SetCell
 */
BOOL MCTRL_API mcTable_SetCellEx(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                 MC_TABLECELL* pCell);

/**
 * @brief Get contents of a cell.
 *
 * Before calling this function, the member @c pCell->fMask must specify what
 * members of the structure to retrieve.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[out] pCell Specifies retrieved attributes of the cell.
 * @return @c TRUE on success, @c FALSE otherwise.
 *
 * @sa mcTable_GetCell
 */
BOOL MCTRL_API mcTable_GetCellEx(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                 MC_TABLECELL* pCell);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_TABLE_H */
