/*
 * Copyright (c) 2011-2013 Martin Mitas
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

#ifndef MCTRL_PROPVIEW_H
#define MCTRL_PROPVIEW_H

#include <mCtrl/defs.h>
#include <mCtrl/value.h>
#include <mCtrl/propset.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Property view control (@c MC_WC_PROPVIEW)
 *
 * Property view provides a way how to present larger set of some editable
 * properties in a relatively condensed way.
 *
 * Each property has a textual label, a current value (@ref MC_HVALUE) and
 * few other attributes. Few ways how the property can be edited by the user
 * are supported and the particular way is selected by flags of each item.
 *
 * The collection of properties are managed by a property set
 * (see @ref MC_HPROPSET) so the messages of the control manipulating with
 * the properties are just forwarded to the underlying property set.
 *
 * Normally the control creates its own property set during @c WM_CREATE.
 * You can suppress this behavior by using style @c MC_PVS_NOPROPSETCREATE.
 *
 * You can retrieve the underlying property set with @c MC_PVM_GETPROPSET
 * or change it with @c MC_PVM_SETPROPSET.
 *
 * These standard messages are handled by the control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 *
 * These standard notifications are sent by the control:
 * - @c NM_OUTOFMEMORY
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropView_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcPropView_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (unicode variant). */
#define MC_WC_PROPVIEWW        L"mCtrl.propView"
/** Window class name (ANSI variant). */
#define MC_WC_PROPVIEWA         "mCtrl.propView"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief Do not automatically create empty property set. */
#define MC_PVS_NOPROPSETCREATE       0x0001

/** @brief Sort items alphabetically.
 *
 * This applies only when the control creates new property set.
 */
#define MC_PVS_SORTITEMS             0x0002

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Gets handle of the underlying property set.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c MC_HPROPSET) Handle of the property set.
 */
#define MC_PVM_GETPROPSET         (MC_PVM_FIRST + 0)

/**
 * @brief Installs another property set into the control.
 *
 * Note the control releases a reference of the previously installed property
 * and references the newly installed property set.
 *
 * Unless the control has style @c MC_PVS_NOPROPSETCREATE, the control will
 * create and install new empty property set if @c lParam is @c NULL.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c MC_HPROPSET) Handle of the property set.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_PVM_SETPROPSET         (MC_PVM_FIRST + 1)

/**
 * @brief Inserts an item into the underlying property set (unicode variant).
 *
 * @param wParam Reserved, set to to zero.
 * @param[in] lParam (@ref MC_PROPSETITEMW) The item.
 * @return (@c int) Index of the item, or @c -1 on failure.
 */
#define MC_PVM_INSERTITEMW        (MC_PVM_FIRST + 2)

/**
 * @brief Inserts an item into the underlying property set (ANSI variant).
 *
 * @param wParam Reserved, set to to zero.
 * @param[in] lParam (@ref MC_PROPSETITEMA) The item.
 * @return (@c int) Index of the item, or @c -1 on failure.
 */
#define MC_PVM_INSERTITEMA        (MC_PVM_FIRST + 3)

/**
 * @brief Sets an item in the underlying property set (unicode variant).
 *
 * @param wParam Reserved, set to to zero.
 * @param[in] lParam (@ref MC_PROPSETITEMW) The item.
 * @return (@c int) Index of the item, or @c -1 on failure.
 */
#define MC_PVM_SETITEMW           (MC_PVM_FIRST + 4)

/**
 * @brief Sets an item in the underlying property set (ANSI variant).
 *
 * @param wParam Reserved, set to to zero.
 * @param[in] lParam (@ref MC_PROPSETITEMA) The item.
 * @return (@c int) Index of the item, or @c -1 on failure.
 */
#define MC_PVM_SETITEMA           (MC_PVM_FIRST + 5)

/**
 * @brief Gets an item from the underlying property set (unicode variant).
 *
 * Application has to set @c MC_PROPSETITEM::dwMask prior sending the message
 * to indicate what attributes of the item to retrieve. If the application uses
 * @c MC_PSIMF_TEXT, then it also has to set @c MC_PROPSETITEM::pszText to
 * point to a buffer where the text will be stored and set
 * @c MC_PROPSETITEM::cchTextMax to specify size of the buffer.
 *
 * @param wParam Reserved, set to to zero.
 * @param[out] lParam (@ref MC_PROPSETITEMW) The item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_PVM_GETITEMW           (MC_PVM_FIRST + 6)

/**
 * @brief Gets an item from the underlying property set (ANSI variant).
 *
 * Application has to set @c MC_PROPSETITEM::dwMask prior sending the message
 * to indicate what attributes of the item to retrieve. If the application uses
 * @c MC_PSIMF_TEXT, then it also has to set @c MC_PROPSETITEM::pszText to
 * point to a buffer where the text will be stored and set
 * @c MC_PROPSETITEM::cchTextMax to specify size of the buffer.
 *
 * @param wParam Reserved, set to to zero.
 * @param[out] lParam (@ref MC_PROPSETITEMA) The item.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_PVM_GETITEMA           (MC_PVM_FIRST + 7)

/**
 * @brief Delete an item from the underlying property set.
 *
 * @param[in] wParam Index of the item.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_PVM_DELETEITEM         (MC_PVM_FIRST + 8)

/**
 * @brief Delete all items from the underlying property set.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE on failure.
 */
#define MC_PVM_DELETEALLITEMS     (MC_PVM_FIRST + 9)

/**
 * @brief Gets count of items in the underlying property set.
 *
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) Count of items, or @c -1 on failure.
 */
#define MC_PVM_GETITEMCOUNT       (MC_PVM_FIRST + 10)

//#define MC_PVM_SETITEMCOUNT       (MC_PVM_FIRST + 11)
//#define MC_PVM_SETHOTITEM         (MC_PVM_FIRST + 12)
//#define MC_PVM_GETHOTITEM         (MC_PVM_FIRST + 13)
//#define MC_PVM_ENSUREVISIBLE      (MC_PVM_FIRST + 14)
//#define MC_PVM_ISITEMVISIBLE      (MC_PVM_FIRST + 15)
//#define MC_PVM_GETEDITCONTROL     (MC_PVM_FIRST + 16)
//#define MC_PVM_HITTEST            (MC_PVM_FIRST + 17)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

//#define MC_PVN_BEGINITEMEDIT      (MC_PVN_FIRST + 0)
//#define MC_PVN_ENDITEMEDIT        (MC_PVN_FIRST + 1)
//#define MC_PVN_ITEMDROPDOWN       (MC_PVN_FIRST + 2)
//#define MC_PVN_ITEMDLGEDIT        (MC_PVN_FIRST + 3)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_PROPVIEWW MC_WC_PROPVIEWA */
#define MC_WC_PROPVIEW      MCTRL_NAME_AW(MC_WC_PROPVIEW)
/** Unicode-resolution alias. @sa MC_PVITEMW MC_PVITEMA */
#define MC_PVITEM           MCTRL_NAME_AW(MC_PVITEM)
/** Unicode-resolution alias. @sa MC_PVM_INSERTITEMW MC_PVM_INSERTITEMA */
#define MC_PVM_INSERTITEM   MCTRL_NAME_AW(MC_PVM_INSERTITEM)
/** Unicode-resolution alias. @sa MC_PVM_SETITEMW MC_PVM_SETITEMA */
#define MC_PVM_SETITEM      MCTRL_NAME_AW(MC_PVM_SETITEM)
/** Unicode-resolution alias. @sa MC_PVM_GETITEMW MC_PVM_GETITEMA */
#define MC_PVM_GETITEM      MCTRL_NAME_AW(MC_PVM_GETITEM)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_PROPVIEW_H */
