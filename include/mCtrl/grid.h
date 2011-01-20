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

#ifndef MCTRL_GRID_H
#define MCTRL_GRID_H

#include <mCtrl/defs.h>
#include <mCtrl/value.h>
#include <mCtrl/table.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Grid control (@c MC_WC_GRID).
 *
 * The grid control provides user interface for presentation of table data
 * model (see @c MC_TABLE).
 *
 * Actually all messages manipulating with contents of the table exist just
 * for convenience: they just call corresponding function manipulating with
 * the underlying @c MC_TABLE. Thus the table can be resized, its contents can
 * be changed and so on without use of the @c MC_TABLE API.
 *
 * By default, the control creates an empty heterogenous table during
 * its creation. You can avoid that by the style @c MC_GS_NOTABLECREATE.
 * In that case however you have to attach some table to the control manually
 * with the message @c MC_GM_SETTABLE. Until you do so, all messages attempting
 * to modify the underlying table will just fail.
 *
 * @c MC_GM_SETTABLE together with @c MC_GM_GETTABLE allows attaching one table
 * to multiple controls.
 *
 * Messages which do not manipulate with the table determine how the table
 * is presented and these are tied to the control. I.e. if any table is
 * attached to multiple control, each of the controls can present the table
 * in other way (e.g. have another dimensions for each cell etc.).
 *
 * These standard messages are handled by @c MC_WC_GRID control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 */


/**
 * @brief Registers window class of the grid control.
 * @return @c TRUE on success, @c FALSE on failure.
 * @sa @ref sec_init
 */
BOOL MCTRL_API mcGrid_Initialize(void);

/**
 * @brief Unregisters window class of the grid control.
 *
 * @sa @ref sec_init
 */
void MCTRL_API mcGrid_Terminate(void);


/**
 * @name Window Class
 */
/*@{*/
/** @brief Window class name (unicode variant). */
#define MC_WC_GRIDW            L"mCtrl.grid"
/** @brief Window class name (ANSI variant). */
#define MC_WC_GRIDA             "mCtrl.grid"
#ifdef UNICODE
    /** @brief Unicode-resolution alias. */
    #define MC_WC_GRID          MC_WC_GRIDW
#else
    #define MC_WC_GRID          MC_WC_GRIDA
#endif
/*@}*/


/**
 * @name Control Styles 
 */
/*@{*/

/** @brief Do not automatically create empty table. */
#define MC_GS_NOTABLECREATE          (0x00000001L)

/** @brief Do not paint grid lines. */
#define MC_GS_NOGRIDLINES            (0x00000002L)

/** @brief Columns have no header. This is default. */
#define MC_GS_COLUMNHEADERNONE       (0x00000000L)
/** @brief Columns have numerical headers (i.e. "1", "2", "3" etc.) */
#define MC_GS_COLUMNHEADERNUMBERED   (0x00001000L)
/** @brief Columns have alphabetical headers (i.e. "A", "B", "C" etc.) */
#define MC_GS_COLUMNHEADERALPHABETIC (0x00002000L)
/** @brief First row is interpreted as column headers. */
#define MC_GS_COLUMNHEADERCUSTOM     (0x00003000L)

/** @brief Rows have no header. This is default. */
#define MC_GS_ROWHEADERNONE          (0x00000000L)
/** @brief Rows have numerical headers (i.e. "1", "2", "3" etc.) */
#define MC_GS_ROWHEADERNUMBERED      (0x00004000L)
/** @brief Rows have alphabetical headers (i.e. "A", "B", "C" etc.) */
#define MC_GS_ROWHEADERALPHABETIC    (0x00008000L)
/** @brief First columns is interpreted as row headers. */
#define MC_GS_ROWHEADERCUSTOM        (0x0000C000L)

/*@}*/

/**
 * @brief Structure for setting and getting cell of the table.
 * @sa @ref MC_GM_SETCELL @ref MC_GM_GETCELL
 */
typedef struct MC_GCELL_tag {
    /** @brief Column index */
    WORD wCol;
    /** @brief Row index */
    WORD wRow;
    /** @brief Handle of value type */
    MC_VALUETYPE hType;
    /** @brief Handle of the value */
    MC_VALUE hValue;
} MC_GCELL;

/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Gets handle of table attached to the controlo or @c NULL if none is
 * attached.
 *
 * Note that calling the message does not change reference counter of the 
 * returned table. If you want to preserve the handle, you should call 
 * @ref mcTable_AddRef() on it and then @ref mcTable_Release() when you no 
 * longer need it.
 * 
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (&ref MC_TABLE) Handle of the table, or @c NULL.
 */
#define MC_GM_GETTABLE            (WM_USER + 100)

/**
 * @brief Attaches a table to the control.
 *
 * Reference counter of the table is incremented. Previously attached table
 * (if any) is detached and its reference counter is decremented.
 *
 * If the @c wParam is @c NULL, the control creates new table (with reference
 * count set to one), unless the control has style @ref MC_GS_NOTABLECREATE.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_TABLE) Handle of the table, or @c NULL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETTABLE            (WM_USER + 101)

/**
 * @brief Gets count of columns in table attached to the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c WORD) Returns count of table columns.
 */
#define MC_GM_GETCOLUMNCOUNT      (WM_USER + 102)

/**
 * @brief Gets count of rows in table attached to the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c WORD) Returns count of table rows.
 */
#define MC_GM_GETROWCOUNT         (WM_USER + 103)

/** 
 * @brief Resizes table attached to the control.
 *
 * @param[in] wParam (@c DWORD) The low-order word specifies count of columns, 
 * high-order word specifies count of rows.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_RESIZE              (WM_USER + 104)

/**
 * @brief Clears the table.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return Not defined, do not rely on return value.
 */
#define MC_GM_CLEAR               (WM_USER + 109)

/**
 * @brief Gets a table cell.
 *
 * Caller has to fill @c MC_GCELL::wCol and @c MC_GCELL::wRow before sending
 * this message.
 *
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_GCELL*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETCELL             (WM_USER + 111)

/**
 * @brief Sets a table cell.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam[in] (@ref MC_GCELL*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCELL             (WM_USER + 110)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_GRID_H */
