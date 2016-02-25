/*
 * Copyright (c) 2010-2016 Martin Mitas
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

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>
#include <mCtrl/table.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Grid control (@c MC_WC_GRID).
 *
 * The grid control provides user interface for presentation of a lot of data
 * in, as the name of the control suggests, a grid.
 *
 *
 * @section grid_model Data Model
 *
 * By default, the control uses @ref MC_HTABLE data model to manage the data
 * displayed by the control. Then, actually, all messages manipulating with
 * data hold by the control just call corresponding function manipulating with
 * the underlying @ref MC_HTABLE.
 *
 * By default, the control creates an empty table during its creation, of size
 * 0 x 0. So, usually, one of the first messages sent to the control by any
 * application is @ref MC_GM_RESIZE.
 *
 * Alternatively, you can use the style @ref MC_GS_NOTABLECREATE. In that case
 * the control does not create any table during its creation, and you must
 * associate an existing table with the control with the message
 * @ref MC_GM_SETTABLE. Until you do so, all messages attempting
 * to modify the underlying table will just fail.
 *
 * @ref MC_GM_SETTABLE together with @ref MC_GM_GETTABLE allows attaching one
 * table to multiple controls.
 *
 * Messages which do not manipulate with the table determine how the table
 * is presented and these are tied to the control. I.e. if any table is
 * attached to multiple control, each of the controls can present the table
 * in other way (e.g. have another dimensions for each cell etc.).
 *
 *
 * @section grid_virtual Virtual Grid
 *
 * The grid can defer management of the data to the parent window instead of
 * using the @ref MC_HTABLE. To do this, application has to apply the style
 * @ref MC_GS_OWNERDATA.
 *
 * The control then uninstalls any table currently associated with it (if any),
 * and whenever painting a cell, it asks its parent for the data it needs.
 * The control uses the notification @ref MC_GN_GETDISPINFO to retrieve the data.
 *
 * The virtual grid is especially useful to present data calculated on the fly
 * or retrieved from some large database, so that only the required amount
 * of the data is calculated and retrieved, which can result in saving a lot of
 * memory and CPU cycles.
 *
 * The control actually only remembers dimensions of the virtual table as set
 * by @ref MC_GM_RESIZE.
 *
 * If the retrieval of the data is expensive operation, the application may
 * want to implement some caching scheme. For this purpose the control sends
 * the notification @ref MC_GN_ODCACHEHINT, which informs the application about
 * the region of cells it may ask most frequently. Note however the control is
 * free to ask for any cell, not just the one included in the range as specified
 * by the latest @ref MC_GN_ODCACHEHINT.
 *
 * Also note that (if the control has the styles @ref MC_GS_COLUMNHEADERNORMAL
 * and/or @ref MC_GS_ROWHEADERNORMAL) the grid may ask for the header cells,
 * although the headers are never explicitly included in @ref MC_GN_ODCACHEHINT.
 *
 * To force repainting of one or more items, when the underlying data change,
 * the application is supposed to use the message @ref MC_GM_REDRAWCELLS.
 *
 * Please remember that when the style @ref MC_GS_OWNERDATA is used, some
 * control messages and styles behave differently:
 *
 * - Message @ref MC_GM_RESIZE does not resize a table associated with the control
 *   but only informs the control how large the virtual table is (and application
 *   then has to be ready to provide data for any cell within the dimensions).
 *
 * - Messages @ref MC_GM_SETCELL, @ref MC_GM_GETCELL, @ref MC_GM_CLEAR,
 *   @ref MC_GM_SETTABLE do nothing: They all just fail (they just return
 *   @c FALSE).
 *
 * - Style @ref MC_GS_NOTABLECREATE has no effect (the @ref MC_GS_OWNERDATA
 *   actually does what @ref MC_GS_NOTABLECREATE and much more).
 *
 *
 * @section grid_header Column and Row Headers
 *
 * The application can manipulate also with headers of columns and rows. For
 * many purposes, the control treats them as any ordinary cells, but of course
 * there are also differences.
 *
 * The header cells have to be addressed by special index @ref MC_TABLE_HEADER.
 * Column header uses index of the column of interest and row index @ref
 * MC_TABLE_HEADER and, similarly, row header uses column header @ref
 * MC_TABLE_HEADER and the row index.
 *
 * For example, to get or set contents of header cells, the application can
 * use exactly the same messages as for ordinary cells, i.e. messages @ref
 * MC_GM_GETCELL and @ref MC_GM_SETCELL respectively, where one of the
 * indexes is set to @ref MC_TABLE_HEADER.
 *
 *
 * @section grid_selection Selection
 *
 * The grid control supports four distinct selection modes. These modes
 * determine how the cells may or may not be selected:
 *
 * - With style @c MC_GS_NOSEL, selection is disabled altogether and no cell
 *   can be selected. This is default.
 *
 * - With style @c MC_GS_SINGLESEL, only single cell may be selected at any
 *   time.
 *
 * - With style @c MC_GS_RECTSEL, only a set of cells which form a block of
 *   rectangular shape may be selected.
 *
 * - With style @c MC_GS_COMPLEXSEL, any set of cells, even discontinuous one,
 *   may be selected.
 *
 * The mode influences how the control reacts on mouse and keyboard events
 * with respect to the selection, and also how the control processes various
 * selection related messages and notifications.
 *
 * Note that the selection may also be changed with keyboard only if
 * the style @c ref MC_GS_FOCUSEDCELL is also used.
 *
 *
 * @section grid_cell_focus Focused Cell
 *
 * With the style @ref MC_GS_FOCUSEDCELL, the control supports a cell focus.
 * Then, exactly one cell has a status of the focused cell.
 *
 * Certain aspects of control behavior change when the style is enabled:
 *
 * - The focused cell has a focus rectangle painted around it (if the control
 *   has focus).
 *
 * - Navigation keys (like arrow keys, page up/down, home/end etc.) move the
 *   focus from a given cell to another one, as appropriate to the given key.
 *   (When the style is not used the keys translate directly to a scrolling
 *   action.)
 *
 * - Depending on the selection mode, the style @c MC_GS_FOCUSEDCELL enables
 *   changing the current selection with keyboard.
 *
 * Application can set and get currently focused cell with messages
 * @ref MC_GM_SETFOCUSEDCELL and @ref MC_GM_GETFOCUSEDCELL, and be notified
 * about its change of focused cell via the notification @ref
 * MC_GN_FOCUSEDCELLCHANGING and @ref MC_GN_FOCUSEDCELLCHANGED.
 *
 *
 * @section grid_label_edit Cell Label Editing
 *
 * When the style @ref MC_GS_EDITLABELS is enabled, user can edit cell labels.
 * The editing is started when user clicks on the focused cell, when user
 * presses @c ENTER, or when the application explicitly sends the message
 * @ref MC_GM_EDITLABEL.
 *
 * When the label editing starts, application gets the notification
 * @ref MC_GN_BEGINLABELEDIT which allows it to suppress the editing mode,
 * or to customize the embedded edit window. The code handling the notification
 * may use the message @ref MC_GM_GETEDITCONTROL to retrieve handle of the edit
 * control.
 *
 * Likewise, when the edit mode ends, the application gets the notification
 * @ref MC_GN_ENDLABELEDIT, which allows the application to accept or reject
 * the new text for the label.
 *
 * If it is accepted by returning non-zero from the notification, the control
 * saves the new text label within the table or it tells the parent to remember
 * it via @ref MC_GN_SETDISPINFO if the parent maintains the table or cell
 * contents (this happens when the control uses the style @c MC_GS_OWNERDATA
 * or when the cell uses the magic value @ref MC_LPSTR_TEXTCALLBACK).
 *
 *
 * @section grid_std_msgs Standard Messages
 *
 * These standard messages are handled by @c MC_WC_GRID control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 * - @c CCM_GETUNICODEFORMAT
 * - @c CCM_SETNOTIFYWINDOW
 * - @c CCM_SETUNICODEFORMAT
 * - @c CCM_SETWINDOWTHEME
 *
 * These standard notifications are sent by the control:
 * - @c NM_CLICK
 * - @c NM_CUSTOMDRAW (see @ref MC_NMGCUSTOMDRAW)
 * - @c NM_DBLCLK
 * - @c NM_KILLFOCUS
 * - @c NM_OUTOFMEMORY
 * - @c NM_RCLICK
 * - @c NM_RDBLCLK
 * - @c NM_RELEASEDCAPTURE
 * - @c NM_SETFOCUS
 * - @c WM_CONTEXTMENU
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcGrid_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcGrid_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_GRIDW            L"mCtrl.grid"
/** Window class name (ANSI variant). */
#define MC_WC_GRIDA             "mCtrl.grid"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/
/** @brief Do not automatically create empty table. */
#define MC_GS_NOTABLECREATE          0x0001

/** @brief Do not paint grid lines. */
#define MC_GS_NOGRIDLINES            0x0002

/** @brief Use double buffering. */
#define MC_GS_DOUBLEBUFFER           0x0004

/** @brief Enable the virtual grid mode.
 *  @details See @ref grid_virtual for more info. */
#define MC_GS_OWNERDATA              0x0008

/** @brief Enable changing column width by dragging header divider. */
#define MC_GS_RESIZABLECOLUMNS       0x0010
/** @brief Enable changing row height by dragging row header divider. */
#define MC_GS_RESIZABLEROWS          0x0020

/** @brief Enables cell focus. */
#define MC_GS_FOCUSEDCELL            0x0040

/** @brief Enables label editing support. */
#define MC_GS_EDITLABELS             0x0080

/** @brief Control does not allow cell selection. */
#define MC_GS_NOSEL                  0x0000
/** @brief Control allows to select only one cell. */
#define MC_GS_SINGLESEL              0x0100
/** @brief Control allows to select only a rectangular area of cells. */
#define MC_GS_RECTSEL                0x0200
/** @brief Control allows to select any arbitrary set of cells. */
#define MC_GS_COMPLEXSEL             0x0300

/** @brief Paint selection even when the control does not have focus. */
#define MC_GS_SHOWSELALWAYS          0x0400

/** @brief The contents of column headers is used. This is default. */
#define MC_GS_COLUMNHEADERNORMAL     0x0000
/** @brief Columns have numerical headers (i.e. "1", "2", "3" etc.) */
#define MC_GS_COLUMNHEADERNUMBERED   0x1000
/** @brief Columns have alphabetical headers (i.e. "A", "B", "C" etc.) */
#define MC_GS_COLUMNHEADERALPHABETIC 0x2000
/** @brief Columns have no header. */
#define MC_GS_COLUMNHEADERNONE       0x3000
/** @brief Bit mask specifying the column header mode. */
#define MC_GS_COLUMNHEADERMASK      (MC_GS_COLUMNHEADERNORMAL |               \
                                     MC_GS_COLUMNHEADERNUMBERED |             \
                                     MC_GS_COLUMNHEADERALPHABETIC |           \
                                     MC_GS_COLUMNHEADERNONE)

/** @brief The contents of row headers is used. This is default. */
#define MC_GS_ROWHEADERNORMAL        0x0000
/** @brief Rows have numerical headers (i.e. "1", "2", "3" etc.) */
#define MC_GS_ROWHEADERNUMBERED      0x4000
/** @brief Rows have alphabetical headers (i.e. "A", "B", "C" etc.) */
#define MC_GS_ROWHEADERALPHABETIC    0x8000
/** @brief Rows have no header. This is default. */
#define MC_GS_ROWHEADERNONE          0xC000
/** @brief Bit mask specifying the row header mode. */
#define MC_GS_ROWHEADERMASK         (MC_GS_ROWHEADERNORMAL |                  \
                                     MC_GS_ROWHEADERNUMBERED |                \
                                     MC_GS_ROWHEADERALPHABETIC |              \
                                     MC_GS_ROWHEADERNONE)

/*@}*/


/**
 * @name MC_GGEOMETRY::fMask Bits
 * @anchor MC_GGF_xxxx
 */
/*@{*/

/** @brief Set if @ref MC_GGEOMETRY::wColumnHeaderHeight is valid. */
#define MC_GGF_COLUMNHEADERHEIGHT     (1 << 0)
/** @brief Set if @ref MC_GGEOMETRY::wRowHeaderWidth is valid. */
#define MC_GGF_ROWHEADERWIDTH         (1 << 1)
/** @brief Set if @ref MC_GGEOMETRY::wDefColumnWidth is valid. */
#define MC_GGF_DEFCOLUMNWIDTH         (1 << 2)
/** @brief Set if @ref MC_GGEOMETRY::wDefRowHeight is valid. */
#define MC_GGF_DEFROWHEIGHT           (1 << 3)
/** @brief Set if @ref MC_GGEOMETRY::wPaddingHorz is valid. */
#define MC_GGF_PADDINGHORZ            (1 << 4)
/** @brief Set if @ref MC_GGEOMETRY::wPaddingVert is valid. */
#define MC_GGF_PADDINGVERT            (1 << 5)

/*@}*/


/**
 * @name MC_GHITTESTINFO::flags Bits
 * @anchor MC_GHT_xxxx
 */
/*@{*/

/** @brief In the client area, but does not hit any cell. */
#define MC_GHT_NOWHERE              (1 << 0)
/** @brief On a column header cell. */
#define MC_GHT_ONCOLUMNHEADER       (1 << 1)
/** @brief On a row header cell. */
#define MC_GHT_ONROWHEADER          (1 << 2)
/** @brief On any header cell. */
#define MC_GHT_ONHEADER             (MC_GHT_ONCOLUMNHEADER | MC_GHT_ONROWHEADER)
/** @brief On normal cell. */
#define MC_GHT_ONNORMALCELL         (1 << 3)
/** @brief On cell. */
#define MC_GHT_ONCELL               (MC_GHT_ONHEADER | MC_GHT_ONNORMALCELL)
/** @brief Not supported, reserved for future use. */
#define MC_GHT_ONCOLUMNDIVIDER      (1 << 4)
/** @brief Not supported, reserved for future use. */
#define MC_GHT_ONROWDIVIDER         (1 << 5)
/** @brief Not supported, reserved for future use. */
#define MC_GHT_ONCOLUMNDIVOPEN      (1 << 6)
/** @brief Not supported, reserved for future use. */
#define MC_GHT_ONROWDIVOPEN         (1 << 7)
/** @brief Above the client area. */
#define MC_GHT_ABOVE                (1 << 8)
/** @brief Below the client area. */
#define MC_GHT_BELOW                (1 << 9)
/** @brief To right of the client area. */
#define MC_GHT_TORIGHT              (1 << 10)
/** @brief To left of the client area. */
#define MC_GHT_TOLEFT               (1 << 11)

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief A miscellaneous structure determining a rectangular area  in the grid.
 *
 * Note that by convention the top left corner (@c wColumnFrom, @c wRowFrom)
 * is inclusive and the right bottom corner (@c wColumnTo, @c wRowTo) is
 * exclusive.
 */
typedef struct MC_GRECT_tag {
    /** The left column coordinate. */
    WORD wColumnFrom;
    /** The top row coordinate. */
    WORD wRowFrom;
    /** The right column coordinate. */
    WORD wColumnTo;
    /** The bottom row coordinate. */
    WORD wRowTo;
} MC_GRECT;

/**
 * @brief Structure describing inner geometry of the grid.
 * @sa MC_GM_SETGEOMETRY MC_GM_GETGEOMETRY
 */
typedef struct MC_GGEOMETRY_tag {
    /** Bitmask specifying which other members are valid. See @ref MC_GGF_xxxx. */
    DWORD fMask;
    /** Height of column header cells. */
    WORD wColumnHeaderHeight;
    /** Width of row header cells. */
    WORD wRowHeaderWidth;
    /** Default width of regular contents cells. */
    WORD wDefColumnWidth;
    /** Default height of regular contents cells. */
    WORD wDefRowHeight;
    /** Horizontal padding in cells. */
    WORD wPaddingHorz;
    /** Vertical padding in cells. */
    WORD wPaddingVert;
} MC_GGEOMETRY;

/**
 * @brief Structure for message @ref MC_GM_HITTEST.
 */
typedef struct MC_GHITTESTINFO_tag {
    /** Client coordinate of the point to test. */
    POINT pt;
    /** Flag receiving detail about result of the test. See @ref MC_GHT_xxxx */
    UINT flags;
    /** Column index of the cell that occupies the point. */
    WORD wColumn;
    /** Row index of the cell that occupies the point. */
    WORD wRow;
} MC_GHITTESTINFO;

/**
 * @brief Structure describing a selection.
 *
 * In general, the selection is described as a set of rectangular areas. All
 * cells in any of those areas is then considered selected.
 *
 * The control guarantees that on output (when using @ref MC_GM_GETSELECTION
 * or handling any selection-related notification), the rectangles do not
 * overlap each other.
 *
 * I.e. the following code snippet can be used to enumerate all the cells
 * within the selection @c sel:
 * @code
 * UINT uIndex;
 * WORD wCol, wRow;
 *
 * for(uIndex = 0; uIndex < sel.uDataCount; uIndex++) {
 *     MC_GRECT* pRc = &sel.rcData[uIndex];
 *     for(wRow = pRc->wRowFrom; wRow < pRc->wRowTo; wRow++) {
 *         for(wCol = pRc->wColumnFrom; wCol < pRc->wColumnTo; wCol++) {
 *             // ... process the cell [wCol, wRow]
 *         }
 *     }
 * }
 * @endcode
 *
 * @sa MC_GM_SETSELECTION MC_GM_GETSELECTION
 * @sa MC_GN_SELECTIONCHANGING MC_GN_SELECTIONCHANGED
 */
typedef struct MC_GSELECTION_tag {
    /** Extents rectangle of the selection. This member is ignored on input. */
    MC_GRECT rcExtents;
    /** Count of rectangles in the @c rcData array. Zero if no cell is
     *  selected. The count may be larger then 1 only if style @ref
     *  MC_GS_COMPLEXSEL is used. */
    UINT uDataCount;
    /** Pointer to array (of @c uCount members) of rectangles describing the
     *  selection. */
    MC_GRECT* rcData;
} MC_GSELECTION;

/**
 * @brief Structure used by notification @ref MC_GN_ODCACHEHINT.
 */
typedef struct MC_NMGCACHEHINT_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** First column of the region to be cached. */
    WORD wColumnFrom;
    /** First row of the region to be cached. */
    WORD wRowFrom;
    /** Last column of the region to be cached. */
    WORD wColumnTo;
    /** Last row of the region to be cached. */
    WORD wRowTo;
} MC_NMGCACHEHINT;

/**
 * @brief Structure used by the standard notification @c NM_CUSTOMDRAW.
 */
typedef struct MC_NMGCUSTOMDRAW_tag {
    /** Standard custom-draw structure. Note that for draw stages related to
      * a cell (item), i.e. when @c nmcd.dwDrawStage has set the flag
      * @c CDDS_ITEM, <tt>LOWORD(nmcd.dwItemSpec)</tt> specifies column index
      * and <tt>HIWORD(nmcd.dwItemSpec)</tt> specifies row index. */
    MC_NMCUSTOMDRAW nmcd;
    /** Item text color. */
    COLORREF clrText;
    /** Item background color. */
    COLORREF clrTextBk;
} MC_NMGCUSTOMDRAW;

/**
 * @brief Structure used by notifications @ref MC_GN_GETDISPINFO and
 * @ref MC_GN_SETDISPINFO (Unicode variant).
 */
typedef struct MC_NMGDISPINFOW_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Column index. */
    WORD wColumn;
    /** Row index. */
    WORD wRow;
    /** Structure describing the contents of the cell. */
    MC_TABLECELLW cell;
} MC_NMGDISPINFOW;

/**
 * @brief Structure used by notifications @ref MC_GN_GETDISPINFO and
 * @ref MC_GN_SETDISPINFO (ANSI variant).
 */
typedef struct MC_NMGDISPINFOA_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Column index. */
    WORD wColumn;
    /** Row index. */
    WORD wRow;
    /** Structure describing the contents of the cell. */
    MC_TABLECELLA cell;
} MC_NMGDISPINFOA;

/**
 * @brief Structure used by notifications related to resizing of column and
 * headers.
 *
 * @sa MC_GN_COLUMNWIDTHCHANGING MC_GN_COLUMNWIDTHCHANGED
 * @sa MC_GN_ROWHEIGHTCHANGING MC_GN_ROWHEIGHTCHANGED
 */
typedef struct MC_NMGCOLROWSIZECHANGE_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Column index (for @ref MC_GN_COLUMNWIDTHCHANGING and
     *  @ref MC_GN_COLUMNWIDTHCHANGED), or row index
     *  (for @ref MC_GN_ROWHEIGHTCHANGING and @ref MC_GN_ROWHEIGHTCHANGED). */
    WORD wColumnOrRow;
    /** Column width (for @ref MC_GN_COLUMNWIDTHCHANGING and
     *  @ref MC_GN_COLUMNWIDTHCHANGED), or row height
     *  (for @ref MC_GN_ROWHEIGHTCHANGING and @ref MC_GN_ROWHEIGHTCHANGED). */
    WORD wWidthOrHeight;
} MC_NMGCOLROWSIZECHANGE;

/**
 * @brief Structure used by notifications related to focused cell.
 *
 * @sa MC_GN_FOCUSEDCELLCHANGING MC_GN_FOCUSEDCELLCHANGED
 */
typedef struct MC_NMGFOCUSEDCELLCHANGE_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Column of old focused cell. */
    WORD wOldColumn;
    /** Row of old focused cell. */
    WORD wOldRow;
    /** Column of new focused cell. */
    WORD wNewColumn;
    /** Row of new focused cell. */
    WORD wNewRow;
} MC_NMGFOCUSEDCELLCHANGE;

/**
 * @brief  Structure used by notifications related to selection change.
 *
 * @sa MC_GN_SELECTIONCHANGING MC_GN_SELECTIONCHANGED
 */
typedef struct MC_NMGSELECTIONCHANGE_tag {
    /** Common notification structure header. */
    NMHDR hdr;
    /** Old selection description. */
    MC_GSELECTION oldSelection;
    /** New selection description. */
    MC_GSELECTION newSelection;
} MC_NMGSELECTIONCHANGE;

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Gets handle of table attached to the control or @c NULL if none is
 * attached.
 *
 * Note that calling the message does not change reference counter of the
 * returned table. If you want to preserve the handle, you should call
 * @ref mcTable_AddRef() on it and then @ref mcTable_Release() when you no
 * longer need it.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@ref MC_HTABLE) Handle of the table, or @c NULL.
 */
#define MC_GM_GETTABLE            (MC_GM_FIRST + 0)

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
 * @param[in] lParam (@ref MC_HTABLE) Handle of the table, or @c NULL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETTABLE            (MC_GM_FIRST + 1)

/**
 * @brief Gets count of columns in table attached to the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c WORD) Returns count of table columns.
 */
#define MC_GM_GETCOLUMNCOUNT      (MC_GM_FIRST + 2)

/**
 * @brief Gets count of rows in table attached to the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c WORD) Returns count of table rows.
 */
#define MC_GM_GETROWCOUNT         (MC_GM_FIRST + 3)

/**
 * @brief Resizes table attached to the control.
 *
 * @param[in] wParam (@c DWORD) The low-order word specifies count of columns,
 * high-order word specifies count of rows.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_RESIZE              (MC_GM_FIRST + 4)

/**
 * @brief Clears the table.
 *
 * @param[in] wParam Specification of the cells to be cleared. When set to zero,
 * all table contents (including header cells) is cleared. When non-zero, the
 * value is interpreted as a bit-mask of cells to clear:
 * Set bit @c 0x1 to clear all ordinary cells, @c 0x2 to clear column
 * headers and bit @c 0x4 to clear row cells.
 * @param lParam Reserved, set to zero.
 * @return Not defined, do not rely on return value.
 */
#define MC_GM_CLEAR               (MC_GM_FIRST + 5)

/**
 * @brief Sets a table cell (Unicode variant).
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[in] lParam (@ref MC_TABLECELLW*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCELLW            (MC_GM_FIRST + 6)

/**
 * @brief Sets a table cell (ANSI variant).
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[in] lParam (@ref MC_TABLECELLA*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCELLA            (MC_GM_FIRST + 7)

/**
 * @brief Gets a table cell (Unicode variant).
 *
 * Before calling this function, the member @c MC_TABLECELL::fMask must specify
 * what attributes of the cell to retrieve.
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[out] lParam (@ref MC_TABLECELLW*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETCELLW            (MC_GM_FIRST + 8)

/**
 * @brief Gets a table cell (ANSI variant).
 *
 * Before calling this function, the member @c MC_TABLECELL::fMask must specify
 * what attributes of the cell to retrieve.
 *
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row.
 * @param[out] lParam (@ref MC_TABLECELLA*) Pointer to structure describing
 * the cell.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETCELLA            (MC_GM_FIRST + 9)

/**
 * @brief Sets geometry of the grid.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_GGEOMETRY*) Pointer to structure describing
 * the geometry. Only fields specified by the member @c fMask are set.
 * If @c lParam is @c NULL, the geometry is reset to a default values.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETGEOMETRY         (MC_GM_FIRST + 10)

/**
 * @brief Sets geometry of the grid.
 *
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_GGEOMETRY*) Pointer to structure describing
 * the geometry. Only fields specified by the member @c fMask are retrieved.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_GETGEOMETRY         (MC_GM_FIRST + 11)

/**
 * @brief Requests to redraw region of cells.
 *
 * Note the message just invalidates the region of cells, and the control is
 * not actually repainted until it receives a @c WM_PAINT message.
 *
 * @param[in] wParam Specification of top left cell in the region to be
 * (re)painted. Low word specifies its column, high word specifies its row.
 * @param[in] lParam Specification of bottom right cell in the region to
 * be (re)painted. Low word specifies its column, high word specifies its row.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 *
 * For example, to repaint a single cell, use can use code similar to this:
 * @code
 * SendMessage(hwndGrid, MC_GM_REDRAWCELLS, MAKEWPARAM(wCol, wRow), MAKELPARAM(wCol, wRow));
 * @endcode
 */
#define MC_GM_REDRAWCELLS         (MC_GM_FIRST + 12)

/**
 * @brief Set width of specified column.
 *
 * To reset width of the column to the default value (as specified with
 * @ref MC_GGEOMETRY::wDefColumnWidth), set the column width to @c 0xFFFF.
 *
 * Note this message can set only width of an ordinary grid column.
 * To change the width of row headers, use @ref MC_GM_SETGEOMETRY.
 *
 * @param[in] wParam (@c WORD) Index of the column.
 * @param[in] lParam (@c DWORD) Set low word to the desired width in pixels,
 * high word to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETCOLUMNWIDTH      (MC_GM_FIRST + 13)

/**
 * @brief Get width of specified column.
 *
 * @param[in] wParam (@c WORD) Index of the column.
 * @param lParam Reserved, set to zero.
 * @return (@c LRESULT) If the message fails, the return value is @c -1.
 * On success, low word is the width in pixels, high word is reserved for
 * future and it is currently always set to zero.
 */
#define MC_GM_GETCOLUMNWIDTH      (MC_GM_FIRST + 14)

/**
 * @brief Set height of specified row.
 *
 * To reset height of the row to the default value (as specified with
 * @ref MC_GGEOMETRY::wDefRowHeight), set the row height to @c 0xFFFF.
 *
 * Note this message can set only height of an ordinary grid row.
 * To change the height of column headers, use @ref MC_GM_SETGEOMETRY.
 *
 * @param[in] wParam (@c WORD) Index of the row.
 * @param[in] lParam (@c DWORD) Set low word to the desired height in pixels,
 * high word to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_GM_SETROWHEIGHT        (MC_GM_FIRST + 15)

/**
 * @brief Get height of specified row.
 *
 * @param[in] wParam (@c WORD) Index of the row.
 * @param lParam Reserved, set to zero.
 * @return (@c LRESULT) If the message fails, the return value is @c -1.
 * On success, low word is the height in pixels, high word is reserved for
 * future and it is currently always set to zero.
 */
#define MC_GM_GETROWHEIGHT        (MC_GM_FIRST + 16)

/**
 * @brief Tests which cell (and its part) is placed on specified position.
 *
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_GHITTESTINFO*) Pointer to a hit test
 * structure. Set @ref MC_GHITTESTINFO::pt on input.
 * @return (@c DWORD) Cell column and row specification, if any, or -1
 * otherwise. If not -1, in low word of the value, the column index is
 * specified and, in high word of the value, its row index is specified.
 */
#define MC_GM_HITTEST             (MC_GM_FIRST + 17)

/**
 * @brief Get cell rectangle.
 * @param[in] wParam (@c DWORD) Column and row of the cell. Low word specifies
 * column index and high word specifies row index.
 * @param[out] lParam (@c RECT*) Pointer to rectangle.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_GM_GETCELLRECT         (MC_GM_FIRST + 18)

/**
 * @brief Ensure the cell is visible.
 * @details If not visible, the control scrolls to make the cell visible.
 * @param[in] wParam (@c DWORD) Column and row of the cell. Low word specifies
 * column index and high word specifies row index.
 * @param[in] lParam (@c BOOL) A value specifying whether entire cell should be
 * visible. If @c TRUE, no scrolling happens when the cell is at least
 * partially visible.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_GM_ENSUREVISIBLE       (MC_GM_FIRST + 19)

/**
 * @brief Set cell which has focus.
 * @param[in] wParam (@c DWORD) Low word specifies column, high word specifies
 * row. Note that cursor can move only to ordinary cells.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_GM_SETFOCUSEDCELL      (MC_GM_FIRST + 20)

/**
 * @brief Get cell which has focus.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c DWORD) Low word specifies column, high word specifies row.
 */
#define MC_GM_GETFOCUSEDCELL      (MC_GM_FIRST + 21)

/**
 * @brief Set selection.
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@ref MC_GSELECTION*) Pointer to structure describing the
 * selection, or @c NULL to reset selection.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_GM_SETSELECTION        (MC_GM_FIRST + 22)

/**
 * @brief Get selection.
 *
 * When sending this message application has set some members of @c
 * MC_GSELECTION, as provided via @c lParam. There are three modes operations.
 *
 * - Application sets @c lParam to @c NULL. Then no actual data are retrieved
 *   and the message only returns the count of rectangles the selection is
 *   composed of.
 *
 * - Application provides a buffer where control fills the selection data.
 *   The application has to set @c MC_GSELECTION::rcData to point to the
 *   buffer, and @c MC_GSELECTION::uDataCount to maximal count of rectangles
 *   the buffer can hold. On output, the @c MC_GSELECTION::uDataCount is set
 *   to the actual count of rectangles copied into the buffer.
 *
 * - Application does not provide any buffer. In this case the application
 *   has to set @c MC_GSELECTION::uDataCount to @c (UINT)-1. Instead of copying,
 *   the control sets @c MC_GSELECTION::rcData to point into its internal
 *   buffer. In this mode application must not free or modify the contents of
 *   the internal buffer, and also it must not retain the pointer for later
 *   use as it may become invalid any time.
 *
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@ref MC_GSELECTION*) Pointer to structure to be
 * filled by the control, or @c NULL. Application has initialize some members
 * of the structure as described above.
 * @return (@c UINT) Count of rectangles required for @c MC_GSELECTION::rcData
 * on success. Zero means the selection is empty.
 */
#define MC_GM_GETSELECTION        (MC_GM_FIRST + 23)

/**
 * @brief Set selection.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c HWND) Handle of edit control on success, @c NULL otherwise.
 */
#define MC_GM_GETEDITCONTROL      (MC_GM_FIRST + 24)

/**
 * @brief Begins in-place editing of the specified cell.
 * @details This message implicitly sets focus to the given cell.
 * @param[in] wParam (@c DWORD) Column and row of the cell. Low word specifies
 * column index and high word specifies row index.
 * @param lParam Reserved, set to zero.
 * @return (@c HWND) Handle of edit control on success, @c NULL otherwise.
 */
#define MC_GM_EDITLABEL           (MC_GM_FIRST + 25)

/**
 * @brief Ends a cell editing operation, if any is in progress.
 * @details If any edit operation is in progress, the message causes a
 * notification @ref MC_GN_ENDLABELEDIT to be sent.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return None.
 */
#define MC_GM_CANCELEDITLABEL     (MC_GM_FIRST + 26)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Notification providing a hint for data caching strategy for grids
 * with virtual table.
 *
 * The notification is sent only when the control has the style @ref
 * MC_GS_OWNERDATA. The notification informs the application about region of
 * cells it is likely to ask for (via @ref MC_GN_GETDISPINFO).
 *
 * If the retrieval of assorted data by the application is slow (e.g. because
 * it requires remote access to a database system), the application should
 * cache locally the data for cells in the rectangular cell region as specified
 * by the @c MC_NMGCACHEHINT.
 *
 * The structure never specify header cells, however if the control has style
 * @ref MC_GS_COLUMNHEADERNORMAL and/or @ref MC_GS_ROWHEADERNORMAL, it should
 * also cache data needed for column/row headers corresponding for the cells
 * in the region.
 *
 * For example, if the grid uses the styles @ref MC_GS_COLUMNHEADERNORMAL and
 * @ref MC_GS_ROWHEADERNORMAL, the following data are recommended to be cached
 * within the application:
 *
 * - All ordinary cells in the rectangle with left top cell
 *   [@c columnFrom, @c rowFrom] and right bottom cell [@c columnTo, @c rowTo].
 * - All column header cells in the range @c columnFrom ... @c columnFrom.
 * - All row header cells in the range @c rowFrom ... @c rowTo.
 *
 * In general, the specified cell region roughly corresponds to cells currently
 * visible in the control's window client area.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCACHEHINT*) Pointer to @ref MC_NMGCACHEHINT
 * structure.
 * @return None.
 */
#define MC_GN_ODCACHEHINT         (MC_GN_FIRST + 0)

/**
 * @brief Fired when control needs to tell parent to update some cell data
 * it manages (Unicode variant).
 *
 * When sending the notification, the control sets @c MC_NMGDISPINFO::wColumn
 * and @c MC_NMGDISPINFO::wRow to identify the cell. The control also sets
 * @c MC_NMTLDISPINFO::item::lParam (however if the style @ref MC_GS_OWNERDATA
 * is in effect, it is always set to zero).
 *
 * The control specifies what members in @c MC_NMGDISPINFO::cell the application
 * should remember with the @c MC_NMGDISPINFO::cell::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return None.
 */
#define MC_GN_SETDISPINFOW        (MC_GN_FIRST + 1)

/**
 * @brief Fired when control needs to tell parent to update some cell data
 * it manages (ANSI variant).
 *
 * When sending the notification, the control sets @c MC_NMGDISPINFO::wColumn
 * and @c MC_NMGDISPINFO::wRow to identify the cell. The control also sets
 * @c MC_NMTLDISPINFO::item::lParam (however if the style @ref MC_GS_OWNERDATA
 * is in effect, it is always set to zero).
 *
 * The control specifies what members in @c MC_NMGDISPINFO::cell the application
 * should remember with the @c MC_NMGDISPINFO::cell::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return None.
 */
#define MC_GN_SETDISPINFOA        (MC_GN_FIRST + 2)

/**
 * @brief Fired when control needs to retrieve some cell data, the parent holds
 * (Unicode variant).
 *
 * This may happen when @c MC_TABLECELL::pszText was set to the magical value
 * @ref MC_LPSTR_TEXTCALLBACK, or when the control has the style @ref
 * MC_GS_OWNERDATA.
 *
 * When sending the notification, the control sets @c MC_NMGDISPINFO::wColumn
 * and @c MC_NMGDISPINFO::wRow to identify the cell. The control also sets
 * @c MC_NMTLDISPINFO::item::lParam (however if the style @ref MC_GS_OWNERDATA
 * is in effect, it is always set to zero).
 *
 * The control specifies what members in @c MC_NMGDISPINFO::cell the application
 * should fill with the @c MC_NMGDISPINFO::cell::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return None.
 */
#define MC_GN_GETDISPINFOW        (MC_GN_FIRST + 3)

/**
 * @brief Fired when control needs to retrieve some cell data, the parent holds
 * (ANSI variant).
 *
 * This may happen when @c MC_TABLECELL::pszText was set to the magical value
 * @ref MC_LPSTR_TEXTCALLBACK, or when the control has the style @ref
 * MC_GS_OWNERDATA.
 *
 * When sending the notification, the control sets @c MC_NMGDISPINFO::wColumn
 * and @c MC_NMGDISPINFO::wRow to identify the cell. The control also sets
 * @c MC_NMTLDISPINFO::item::lParam (however if the style @ref MC_GS_OWNERDATA
 * is in effect, it is always set to zero).
 *
 * The control specifies what members in @c MC_NMGDISPINFO::cell the application
 * should fill with the @c MC_NMGDISPINFO::cell::fMask.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return None.
 */
#define MC_GN_GETDISPINFOA        (MC_GN_FIRST + 4)

/**
 * @brief Fired when user has begun dragging a column header divider in the
 * control.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return @c TRUE to prevent the column width change, @c FALSE to allow it.
 */
#define MC_GN_BEGINCOLUMNTRACK    (MC_GN_FIRST + 5)

/**
 * @brief Fired when user has finished dragging a column header divider in the
 * control.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_GN_ENDCOLUMNTRACK      (MC_GN_FIRST + 6)

/**
 * @brief Fired when user has begun dragging a row header divider in the
 * control.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return @c TRUE to prevent the row height change, @c FALSE to allow it.
 */
#define MC_GN_BEGINROWTRACK       (MC_GN_FIRST + 7)

/**
 * @brief Fired when user has finished dragging a row header divider in the
 * control.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_GN_ENDROWTRACK         (MC_GN_FIRST + 8)

/**
 * @brief Fired when column width is about to change.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return @c TRUE to prevent the column width change, @c FALSE to allow it.
 */
#define MC_GN_COLUMNWIDTHCHANGING (MC_GN_FIRST + 9)

/**
 * @brief Fired after column width has been changed.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_GN_COLUMNWIDTHCHANGED  (MC_GN_FIRST + 10)

/**
 * @brief Fired when column width is about to change.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return @c TRUE to prevent the row height change, @c FALSE to allow it.
 */
#define MC_GN_ROWHEIGHTCHANGING   (MC_GN_FIRST + 11)

/**
 * @brief Fired after row height has been changed.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGCOLROWSIZECHANGE*) Pointer to
 * @ref MC_NMGCOLROWSIZECHANGE structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_GN_ROWHEIGHTCHANGED    (MC_GN_FIRST + 12)

/**
 * @brief Fired when focused cell is about to change.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGFOCUSEDCELLCHANGE*) Pointer to @ref
 * MC_NMGFOCUSEDCELLCHANGE structure.
 * @return @c TRUE to prevent the row height change, @c FALSE to allow it.
 */
#define MC_GN_FOCUSEDCELLCHANGING (MC_GN_FIRST + 13)

/**
 * @brief Fired after focused cell has been changed.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGFOCUSEDCELLCHANGE*) Pointer to @ref
 * MC_NMGFOCUSEDCELLCHANGE structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_GN_FOCUSEDCELLCHANGED  (MC_GN_FIRST + 14)

/**
 * @brief Fired when selection is about to change.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGSELECTIONCHANGE*) Pointer to @ref
 * MC_NMGSELECTIONCHANGE structure.
 * @return @c TRUE to prevent the selection change, @c FALSE to allow it.
 */
#define MC_GN_SELECTIONCHANGING   (MC_GN_FIRST + 15)

/**
 * @brief Fired after selection has been changed.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMGSELECTIONCHANGE*) Pointer to @ref
 * MC_NMGSELECTIONCHANGE structure.
 * @return Application should return zero if it processes the notification.
 */
#define MC_GN_SELECTIONCHANGED    (MC_GN_FIRST + 16)

/**
 * @brief Fired when control is about to start label editing (Unicode variant).
 *
 * The cell to be edited is specified with @c MC_NMGDISPINFO::wCol and @c wRow.
 *
 * When this notification is sent the edit control for editing the label already
 * exists (but is hidden). Application may ask for it with the message
 * @ref MC_GM_GETEDITCONTROL and customize it.
 *
 * This may only happen if the control has the style @ref MC_GS_EDITLABELS.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return @c TRUE to prevent the label editing, @c FALSE to allow it.
 */
#define MC_GN_BEGINLABELEDITW     (MC_GN_FIRST + 17)

/**
 * @brief Fired when control is about to start label editing (ANSI variant).
 *
 * The cell to be edited is specified with @c MC_NMGDISPINFO::wCol and @c wRow.
 *
 * When this notification is sent the edit control for editing the label already
 * exists (but is hidden). Application may ask for it with the message
 * @ref MC_GM_GETEDITCONTROL and customize it.
 *
 * This may only happen if the control has the style @ref MC_GS_EDITLABELS.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return @c TRUE to prevent the label editing, @c FALSE to allow it.
 */
#define MC_GN_BEGINLABELEDITA     (MC_GN_FIRST + 18)

/**
 * @brief Fired when control is about to end label editing (Unicode variant).
 *
 * The control sets @c MC_NMGDISPINFO provided via @c LPARAM to describe change
 * of the label. @c MC_NMGDISPINFO::wCol and @c wRow determine the cell.
 * @c MC_NMGDISPINFO::cell::pszText is set to the new label or to @c NULL if
 * the label editing is being canceled.
 *
 * This may only happen if the control has the style @ref MC_GS_EDITLABELS.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return If @c MC_NMGDISPINFO::cell::pszText is not @c NULL, return @c TRUE
 * to allow change of the label, @c FALSE to suppress it. If the @c pszText
 * is @c NULL, the return value is ignored and no label change happens.
 */
#define MC_GN_ENDLABELEDITW       (MC_GN_FIRST + 19)

/**
 * @brief Fired when control is about to end label editing (ANSI variant).
 *
 * The control sets @c MC_NMGDISPINFO provided via @c LPARAM to describe change
 * of the label. @c MC_NMGDISPINFO::wCol and @c wRow determine the cell.
 * @c MC_NMGDISPINFO::cell::pszText is set to the new label or to @c NULL if
 * the label editing is being canceled.
 *
 * This may only happen if the control has the style @ref MC_GS_EDITLABELS.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMGDISPINFO*) Pointer to @ref MC_NMGDISPINFO
 * structure.
 * @return If @c MC_NMGDISPINFO::cell::pszText is not @c NULL, return @c TRUE
 * to allow change of the label, @c FALSE to suppress it. If the @c pszText
 * is @c NULL, the return value is ignored and no label change happens.
 */
#define MC_GN_ENDLABELEDITA       (MC_GN_FIRST + 20)


/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_GRIDW MC_WC_GRIDA */
#define MC_WC_GRID              MCTRL_NAME_AW(MC_WC_GRID)
/** Unicode-resolution alias. @sa MC_NMGDISPINFOW MC_NMGDISPINFOA */
#define MC_NMGDISPINFO          MCTRL_NAME_AW(MC_NMGDISPINFO)
/** Unicode-resolution alias. @sa MC_GM_SETCELLW MC_GM_SETCELLA */
#define MC_GM_SETCELL           MCTRL_NAME_AW(MC_GM_SETCELL)
/** Unicode-resolution alias. @sa MC_GM_GETCELLW MC_GM_GETCELLA */
#define MC_GM_GETCELL           MCTRL_NAME_AW(MC_GM_GETCELL)
/** Unicode-resolution alias. @sa MC_GN_SETDISPINFOW MC_GN_SETDISPINFOA */
#define MC_GN_SETDISPINFO       MCTRL_NAME_AW(MC_GN_SETDISPINFO)
/** Unicode-resolution alias. @sa MC_GN_GETDISPINFOW MC_GN_GETDISPINFOA */
#define MC_GN_GETDISPINFO       MCTRL_NAME_AW(MC_GN_GETDISPINFO)
/** Unicode-resolution alias. @sa MC_GN_BEGINLABELEDITW MC_GN_BEGINLABELEDITA */
#define MC_GN_BEGINLABELEDIT    MCTRL_NAME_AW(MC_GN_BEGINLABELEDIT)
/** Unicode-resolution alias. @sa MC_GN_ENDLABELEDITW MC_GN_ENDLABELEDITA */
#define MC_GN_ENDLABELEDIT      MCTRL_NAME_AW(MC_GN_ENDLABELEDIT)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_GRID_H */
