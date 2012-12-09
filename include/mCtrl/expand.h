/*
 * Copyright (c) 2012 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MCTRL_EXPAND_H
#define MCTRL_EXPAND_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Expand control (@c MC_WC_EXPAND).
 *
 * The expand control is a utility control used to toggle size of another window
 * between &quot;collapsed&quot; and &quot;expanded&quot; states.
 *
 * In addition, when collapsing, the control automatically disables and hides
 * all child windows of the managed window which fall out of the visible area;
 * and when expaning it enables and shows all the child windows revealed by the
 * resize operation.
 *
 *
 * @section Managed Window
 *
 * By default the managed window is a parent window of the control. Use message
 * @c CCM_SETNOTIFYWINDOW to change what window the control manages.
 *
 * @note The root control also gets all the notifications from the control.
 *
 *
 * @section Expanded and Collapsed Sizes
 *
 * If both width and height the of the expanded and/or collapsed sizes is zero,
 * the control computes automatically from analyzes of position and size of
 * all child windows of the managed window (usually the parent).
 *
 * The width of the managed window is kept unchanged in this automatic regime.
 * The height of the managed window is calculated so that all child windows
 * of the managed window are visible (the expanded state) or all child windows
 * positioned below the expand control itself are hidden (the collapsed state).
 *
 * If you use style @c MC_EXS_CACHESIZES, the computed sizes are retained
 * for next use, i.e. the sizes are computed only once.
 *
 * You can set expanded and collapsed sizes explicitely with message
 * @c MC_EXM_SETCOLLAPSEDSIZE and @c MC_EXM_SETEXPANDEDSIZE respectivelly.
 *
 *
 * @section Initial State
 *
 * After creation of the control, its logical state is collapsed. Though
 * the control does not resize the dialog because it may not be already fully
 * initialized, e.g. if the expand control is not last control created in the
 * dialog.
 *
 * Therefore it is supposed that application explicitely sends the message
 * @c MC_EXM_EXPAND after the dialog and all its children are created, usually
 * as part of the @c WM_INITDIALOG handling.
 *
 *
 * These standard messages are handled by the control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 * - @c CCM_SETNOTIFYWINDOW
 *
 * These standard notifications are sent by the control:
 * - @c NM_OUTOFMEMORY
 */


/**
 * @brief Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 * @sa @ref sec_init
 */
BOOL MCTRL_API mcExpand_Initialize(void);

/**
 * @brief Unregisters window class of the control.
 *
 * @sa @ref sec_init
 */
void MCTRL_API mcExpand_Terminate(void);


/**
 * @name Window Class
 */
/*@{*/

/** @brief Window class name (unicode variant). */
#define MC_WC_EXPANDW          L"mCtrl.expand"
/** @brief Window class name (ANSI variant). */
#define MC_WC_EXPANDA           "mCtrl.expand"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/**
 * @brief Cache expanded and collapsed sizes.
 *
 * When in the automatic mode, this style allows the control to store expanded
 * and/or collapsed sizes instead of recomputing it each time its state changes.
 */
#define MC_EXS_CACHESIZES           0x0001

/**
 * @brief Expanded and collapsed sizes are interpreted for whole window.
 *
 * If this style is not set, they determine client size only.
 */
#define MC_EXS_RESIZEENTIREWINDOW   0x0002

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Specify size of parent's client area when in collapsed state.
 *
 * If you set both width and height to zero, then the size is computed
 * automatically from position of all controls in the dialog.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam[in] (@c DWORD) The size. Low-order word specifies width and
 * high-order word specifies height.
 * @return (@c DWORD) The original size.
 */
#define MC_EXM_SETCOLLAPSEDSIZE    (WM_USER + 100)

/**
 * @brief Gets size of parent's client area when in collapsed state.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c DWORD) The size. Low-order word specifies width and high-order
 * word specifies height.
 */
#define MC_EXM_GETCOLLAPSEDSIZE    (WM_USER + 101)

/**
 * @brief Specify size of parent's client area when in expanded state.
 *
 * If you set both width and height to zero, then the size is computed
 * automatically from position of all controls in the dialog.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam[in] (@c DWORD) The size. Low-order word specifies width and
 * high-order word specifies height.
 * @return (@c DWORD) The original size.
 */
 #define MC_EXM_SETEXPANDEDSIZE     (WM_USER + 102)

/**
 * @brief Gets size of parent's client area when in expanded state.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c DWORD) The size. Low-order word specifies width and high-order
 * word specifies height.
 */
#define MC_EXM_GETEXPANDEDSIZE     (WM_USER + 103)

/**
 * @brief Sets current state of the control to expanded or collapsed.
 *
 * @param wParam[in] (@c BOOL) Set to @c TRUE to expand, @c FALSE to collapse.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_EXM_EXPAND              (WM_USER + 104)

/**
 * @brief Toggles current state of the control between expanded and collapsed.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_EXM_TOGGLE              (WM_USER + 106)

/**
 * @brief Gets current state of the control.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE if expanded, @c FALSE if collapsed.
 */
#define MC_EXM_ISEXPANDED          (WM_USER + 107)

/*@}*/



/**
 * @name Unicode Resolution
 */
/*@{*/

/** @brief Unicode-resolution alias. @sa MC_WC_EXPANDW MC_WC_EXPANDA */
#define MC_WC_EXPAND           MCTRL_NAME_AW(MC_WC_EXPAND)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_EXPAND_H */
