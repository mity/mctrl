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

#ifndef MCTRL_TABLE_H
#define MCTRL_TABLE_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>
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
 * If application sets some of such flags (e.g. @c MC_TF_NOCELLFOREGROUND) and
 * later it attempts to set he corresponding attribute of the cell (e.g.
 * foreground color), the behavior is undefined (setting of some attributes
 * is silently ignored, or the operation as a whole can fail but application
 * should not rely on exact behavior).
 */


/**
 * @brief Opaque table handle.
 */
typedef void* MC_HTABLE;


/**
 * @anchor MC_TCMF_xxxx
 * @name MC_TABLECELL::fMask Bits
 */
/*@{*/

/** @brief Set if @ref MC_TABLECELL::hValue are valid. */
#define MC_TCMF_VALUE                0x00000001
/** @brief Set if @ref MC_TABLECELL::crForeground is valid. */
#define MC_TCMF_FOREGROUND           0x00000002
/** @brief Set if @ref MC_TABLECELL::crBackground is valid. */
#define MC_TCMF_BACKGROUND           0x00000004
/** @brief Set if @ref MC_TABLECELL::dwFlags is valid. */
#define MC_TCMF_FLAGS                0x00000008

/*@}*/


/**
 * @anchor MC_TCF_xxxx
 * @name MC_TABLECELL::dwFlags Bits
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
 * @sa mcTable_SetCell mcTable_GetCell
 */
typedef struct MC_TABLECELL_tag {
    /** Bit-mask specifying what other members are valid. See @ref MC_TCMF_xxxx. */
    DWORD fMask;
    /** Cell value. */
    MC_HVALUE hValue;
    /** Foreground color. It's up to the value type if it respects it. */
    COLORREF crForeground;
    /** Background color. */
    COLORREF crBackground;
    /** Cell flags. See @ref MC_TCF_xxxx. */
    DWORD dwFlags;
} MC_TABLECELL;


/**
 * @name Functions
 */
/*@{*/

/**
 * @brief Create new table.
 *
 * The table is initially empty and has its reference counter set to 1.
 *
 * @param[in] wColumnCount Column count.
 * @param[in] wRowCount Row count.
 * @param dwReserved Reserved. Set to zero.
 * @return Handle of the new table or @c NULL on failure.
 */
MC_HTABLE MCTRL_API mcTable_Create(WORD wColumnCount, WORD wRowCount,
                                   DWORD dwReserved);

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
 * values (in case of heterogeneous table) or to @c NULL values (for homogeneous
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
 * All values are destroyed. In case of homogeneous table, all cells are reset
 * to @c NULL values of the particular value type, in case of homogeneous
 * table the table becomes empty.
 *
 * @param[in] hTable The table.
 */
void MCTRL_API mcTable_Clear(MC_HTABLE hTable);

/**
 * @brief Set value.
 *
 * Note that (in case of success) the table takes responsibility for the value.
 * I.e. when the table is later deallocated, or if the particular cell is later
 * reset, the value is destroyed.
 *
 * To reset the cell, set value to @c NULL. The reset leads to destroying of
 * a value actually stored in the cell as a side-effect.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] value The value.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_SetValue(MC_HTABLE hTable, WORD wCol, WORD wRow,
                               MC_HVALUE value);

/**
 * @brief Get value.
 *
 * Note that you don't get a responsibility for the value. If you want to
 * store it elsewhere and/or modify it, you should do so on your copy.
 * You may use @ref mcValue_Duplicate() for that purpose.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @return Value of the cell, or @c NULL on failure.
 */
const MC_HVALUE MCTRL_API mcTable_GetValue(MC_HTABLE hTable, WORD wCol, WORD wRow);

/**
 * @brief Swap contents of a cell.
 *
 * This function swaps value of the cell for the one provided. The table
 * becomes responsible for the new cell, while the application gets
 * responsibility for the returned value in the exchange.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] value The new value of the cell.
 * @return The previous value of the cell, or @c NULL on failure.
 */
MC_HVALUE MCTRL_API mcTable_SwapValue(MC_HTABLE hTable, WORD wCol, WORD wRow,
                                      MC_HVALUE value);

/**
 * @brief Set contents of a cell.
 *
 * If @c pCell->fMask includes the bit @c MC_TCMF_VALUE, then
 * (in case of success) the table takes responsibility for the value.
 * I.e. when the table is later deallocated, or if the particular cell is later
 * reset, the value is then destroyed.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[in] pCell Specifies attributes of the cell to set.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_SetCell(MC_HTABLE hTable, WORD wCol, WORD wRow,
                               MC_TABLECELL* pCell);

/**
 * @brief Get contents of a cell.
 *
 * Before calling this function, the member @c pCell->fMask must specify what
 * attributes of the cell to retrieve.
 *
 * @param[in] hTable The table.
 * @param[in] wCol Column index.
 * @param[in] wRow Row index.
 * @param[out] pCell Specifies retrieved attributes of the cell.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
BOOL MCTRL_API mcTable_GetCell(MC_HTABLE hTable, WORD wCol, WORD wRow,
                               MC_TABLECELL* pCell);

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_TABLE_H */
