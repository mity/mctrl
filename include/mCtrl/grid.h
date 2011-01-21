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
 * @sa MC_GM_SETCELL MC_GM_GETCELL
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
 * @anchor MC_GGF_xxxx
 * @name MC_GGEOMETRY::fMask Bits
 * @memberof MC_GGEOMETRY
 */
/*@{*/
/** @brief @ref MC_GGEOMETRY::wColumnHeaderHeight is valid. */
#define MC_GGF_COLUMNHEADERHEIGHT     (1 << 0)
/** @brief @ref MC_GGEOMETRY::wRowHeaderWidth is valid. */
#define MC_GGF_ROWHEADERWIDTH         (1 << 1)
/** @brief @ref MC_GGEOMETRY::wColumnWidth is valid. */
#define MC_GGF_COLUMNWIDTH            (1 << 2)
/** @brief @ref MC_GGEOMETRY::wRowHeight is valid. */
#define MC_GGF_ROWHEIGHT              (1 << 3)
/** @brief @ref MC_GGEOMETRY::wPaddingHorz is valid. */
#define MC_GGF_PADDINGHORZ            (1 << 4)
/** @brief @ref MC_GGEOMETRY::wPaddingVert is valid. */
#define MC_GGF_PADDINGVERT            (1 << 5)
/*@}*/

/**
 * @brief Structure describing inner geometry of the grid.
 * @sa MC_GM_SETGEOMETRY MC_GM_GETGEOMETRY
 */
typedef struct MC_GGEOMETRY_tag {
    /** @brief Bitmask specifying what other members are valid. */
    DWORD fMask;
    /** @brief Height of column header cells. */
    WORD wColumnHeaderHeight;
    /** @brief Width of row header cells. */
    WORD wRowHeaderWidth;
    /** @brief Width of regular contents cells. */
    WORD wColumnWidth;
    /** @brief Height of regular contents cells. */
    WORD wRowHeight;
    /** @brief Horizontal padding in cells. */
    WORD wPaddingHorz;
    /** @brief Vertical padding in cells. */
    WORD wPaddingVert;
} MC_GGEOMETRY;

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
 * @brief Sets a table cell.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam[in] (@ref MC_GCELL*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCELL             (WM_USER + 110)

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
 * @brief Sets geometry of the grid.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam[in] (@ref MC_GGEOMETRY*) Pointer to structure describing
 * the geometry. Only fields specified by the member @c fMask are set.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETGEOMETRY         (WM_USER + 112)

/**
 * @brief Sets geometry of the grid.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam[in,out] (@ref MC_GGEOMETRY*) Pointer to structure describing
 * the geometry. Only fields specified by the member @c fMask are retrieved.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETGEOMETRY         (WM_USER + 113)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

#ifdef UNICODE
    /** @brief Unicode-resolution alias. @sa MC_WC_GRIDW MC_WC_GRIDA */
    #define MC_WC_GRID          MC_WC_GRIDW
#else
    #define MC_WC_GRID          MC_WC_GRIDA
#endif

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_GRID_H */
