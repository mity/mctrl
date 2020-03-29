/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2008-2020 Martin Mitas
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

#ifndef MCTRL_BUTTON_H
#define MCTRL_BUTTON_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Enhanced button control (@c MC_WC_BUTTON).
 *
 * @c MC_WC_BUTTON control is subclass of standard @c BUTTON class.
 * It had been designed to overcome some compatibility limitations between
 * button implementation in various versions of @c COMCTL32.DLL.
 *
 * However note, that since support for Windows versions older then Vista has
 * been removed, its benefits are rather small. Actually it only provides
 * (limited support) of split buttons (styles @c BS_SPLITBUTTON and
 * @c BS_DEFSPLITBUTTON) if and only if the application links with
 * @c COMCTL32.DLL version 5.
 *
 * If application links with @c COMCTL32.DLL version 6, our class behaves
 * exactly the same way as the vanilla control and there is no reason to use
 * the control provided by @c MCTRL.DLL.
 *
 * @attention Remember that @c MC_WC_BUTTON implements only subset of styles
 * and messages for split buttons offered by @c COMCTL32.DLL version 6.0.
 *
 *
 * @section sec_button_split Split Buttons
 *
 * Split button is a push button divided to two parts. The main part behaves
 * as the normal push buttons and the other part (called drop-down) opens some
 * options closely related with the function of the main button part.
 * In a typical use-case the drop-down launches a popup menu.
 *
 * @image html button_split.png
 *
 * To make a split button, just specify style @ref MC_BS_SPLITBUTTON or
 * @ref MC_BS_DEFSPLITBUTTON when you create the control.
 *
 * To handle clicks on the main part of the button, handle @c WM_COMMAND
 * as for any other normal push buttons.
 *
 * To handle clicks on the drop-down part of the button, handle message
 * @c WM_NOTIFY. If the message originates from @ref MC_BS_SPLITBUTTON control,
 * recast @c LPARAM to @ref MC_NMBCDROPDOWN* and check if
 * <tt>MC_NMBCDROPDOWN::hdr.code</tt> is @ref MC_BST_DROPDOWNPUSHED.
 *
 * Note that all the mCtrl split button styles and messages supported by
 * @c MCTRL.DLL (those defined in this header with the prefix @c "MC_") have
 * values equal to their standard counterparts as defined by Microsoft SDK
 * in @c <commctrl.h> so they can be used interchangeably. The advantage of
 * those mCtrl ones is that they are defined always when you include
 * @c <mCtrl/button.h>, while the standard identifiers in @c <commctrl.h>
 * are defined only if <tt>_WIN32_WINNT >= 0x0600</tt>.
 *
 * For more information, you can refer to documentation of split buttons on
 * MSDN (but still keep in mind that only some of the features are supported
 * in mCtrl).
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcButton_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcButton_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_BUTTONW          L"mCtrl.button"
/** Window class name (ANSI variant). */
#define MC_WC_BUTTONA           "mCtrl.button"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/**
 * @brief Style of a split button.
 *
 * It is equivalent to standard @c BS_SPLITBUTTON.
 * It is provided because the standard @c BS_SPLITBUTTON is defined only if
 * <tt>_WIN32_WINNT >= 0x0600</tt>.
 */
#define MC_BS_SPLITBUTTON         0x000C

/**
 * @brief Style of a default split button.
 *
 * It is equivalent to standard @c BS_DEFSPLITBUTTON.
 * It is provided because the standard @c BS_SPLITBUTTON is defined only if
 * <tt>_WIN32_WINNT >= 0x0600</tt>.
 */
#define MC_BS_DEFSPLITBUTTON      0x000D

/*@}*/


/**
 * @name Control States
 */
/*@{*/

/**
 * @brief State of the split button when drop-down button is pressed.
 *
 * It is a possible value returned by standard @c BM_GETSTATE message.
 * It is equivalent to standard @c BST_DROPDOWNPUSHED.
 * It is provided because the standard @c BST_DROPDOWNPUSHED is defined only if
 * <tt>_WIN32_WINNT >= 0x0600</tt>.
 */
#define MC_BST_DROPDOWNPUSHED    0x0400

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure for notification @ref MC_BCN_DROPDOWN.
 *
 * It is equivalent to standard @c NMBCDROPDOWN.
 * It is provided because the standard @c NMBCDROPDOWN is defined only if
 * <tt>_WIN32_WINNT >= 0x0600</tt>.
 */
typedef struct MC_NMBCDROPDOWN_tag {
    /** Standard notification structure header. */
    NMHDR hdr;
    /** Client rectangle of the drop-down button. */
    RECT rcButton;
} MC_NMBCDROPDOWN;

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Notification fired when user clicks on the drop-down button.
 *
 * It is equivalent to standard @c BCN_DROPDOWN. It is passed via
 * the @c WM_NOTIFY message.
 * It is provided because the standard @c BCN_DROPDOWN is defined only if
 * <tt>_WIN32_WINNT >= 0x0600</tt>.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMBCDROPDOWN*) Data associated with
 * the notification.
 * @return Application should return zero if it processes the notification.
 * @sa MC_NMBCDROPDOWN
 */
#define MC_BCN_DROPDOWN          ((0U-1250U) + 0x0002)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_MDITABW MC_WC_MDITABA */
#define MC_WC_BUTTON        MCTRL_NAME_AW(MC_WC_BUTTON)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_BUTTON_H */
