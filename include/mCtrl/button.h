/*
 * Copyright (c) 2008-2009 Martin Mitas
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

#ifndef MCTRL_BUTTON_H
#define MCTRL_BUTTON_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Enhanced button control (@c MC_WC_BUTTON).
 *
 * @c MC_WC_BUTTON control is subclass of standard @c BUTTON class, providing
 * two features:
 * - Icon buttons. See @ref sec_icon.
 * - Split buttons. See @ref sec_split.
 *
 * In all other aspects the @c MC_WC_BUTTON control behaves as the standard 
 * @c BUTTON (strictly speaking it inherits those features by subclassing) 
 * as available on the Windows version and @c comctl32.dll version avaiable
 * on the system where your application runs. 
 * So you can use @c MC_WC_BUTTON everywhere, instead of all buttons
 * and use the standard flags when creating them, e.g. @c BS_GROUPBOX or 
 * @c BS_CHECKBOX.
 *
 *
 * @section sec_icon Icon buttons
 *
 * The standard @c BUTTON uses the old and boring look from Windows 95/98 when 
 * you use style BS_ICON and set image of the button with @c BM_SETIMAGE.
 * If you use @c MC_WC_BUTTON, the button is styled if XP styles are avaiable
 * and enabled on the system. If your application supports XP theming, you 
 * should prefer @c MC_WC_BUTTON windows class over @c BUTTON for icon buttons.
 *
 * Comparision of icon button without and with Windows XP Styles:
 * @image html button_icon.png 
 *
 * From developer's point of view, the use of icon button is absolutely the
 * same as the standard @c BUTTON control. Use standard @c BS_ICON style
 * and @c BM_SETIMAGE message.
 *
 * @attention @c MC_WC_BUTTON does @b not implement support for XP styling of 
 * buttons with style @c BS_BITMAP. Only @c BS_ICON is supported. 
 *
 * @sa MC_BS_ICON
 *
 *
 * @section sec_split Split buttons
 *
 * Split button is a push button devided to two parts. The main part behaves
 * as the normal push buttons and the other part (called dropdown) opens some
 * options closely related with the function of the button. In most use-case 
 * the dropdown launches a popup menu.
 *
 * @image html button_split.png
 *
 * Microsoft introduced split buttons in Windows Vista. Some applications
 * use them even on older Windows versions, including some Microsoft products
 * (e.g. MS Visual Studio 2005). However such applications don't use the 
 * standard control but implement their own in each application, so those are 
 * not available for 3rd party developers. And limiting an application for 
 * only Windows Vista by using new control is not acceptable for majority of 
 * developers, as well as end-users of their applications.
 *
 * Therefore mCtrl comes with its own implementation of split buttons.
 * To make a split button just specify style @c MC_BS_SPLITBUTTON or 
 * @c MC_BS_DEFSPLITBUTTON when you create the control.
 *
 * To handle clicks on the main part of the button, handle @c WM_COMMAND
 * as for any other normal push buttons.
 *
 * To handle clicks on the dropdown part of the button, handle message 
 * @c WM_NOTIFY. If the message originates from @c MC_BS_SPLITBUTTON control,
 * recast @c LPARAM to @c mc_NMBCDROPDOWN structure and check if
 * @c mc_NMBCDROPDOWN::hdr.code is @c MC_BST_DROPDOWNPUSHED.
 *
 * Note that all the styles and messages have values equal to the standard
 * one as defined by Microsoft SDK so they can be used interchangably.
 *
 * @attention that @c MC_WC_BUTTON implements only subset of styles and 
 * messages for split buttons offered on Window Vista.
 *
 * The @c MC_WC_BUTTON is implemented as a subclass of standard @c BUTTON 
 * control. If the Windows support split button in a natural way (Vista or 
 * newer), the @c MC_WC_BUTTON just simply passes all messages down to the 
 * original @c BUTTON window procedure. This guarantees good consistency of 
 * the control's look and feel with other system controls, but it also brings
 * a minor pitfall for developer: If you use standard identifiers of split 
 * button messages and styles (e.g. @c BS_DEFSPLITBUTTON instead of 
 * @c MC_BS_DEFSPLITBUTTON) then it is easy to forget about the limitation.
 * It then will work in runtime as intended on Windows Vista but not on the
 * older Windows versions.
 *
 * Therefore it's recommanded practice that if you use @c MC_WC_BUTTON, you
 * also stick with the mCtrl-defined messages and styles for split buttons 
 * (i.e. those with prefix @c MC_). This has yet another advantage: to use
 * the split button, you don't need to define C preprocessor macro 
 * @c _WIN32_WINNT to @c 0x0600 (or higher) as you would to force the 
 * Win32API headers to declare the split button related stuff.
 *
 * For more information, you can refer to documentation of split buttons on 
 * MSDN (but still keep in mind that only some of the features are 
 * reimplemented in mCtrl).
 *
 * @sa MC_BS_SPLITBUTTON MC_BS_DEFSPLITBUTTON MC_BST_DROPDOWNPUSHED 
 * MC_BCN_DROPDOWN mc_NMBCDROPDOWN
 */

/**
 * Registers the button control class.
 * @return @c TRUE on success, @c FALSE on failure.
 * @see sec_initialization
 */
BOOL MCTRL_API mcButton_Initialize(void);

/**
 * Unregisters the button control class.
 * @see sec_initialization
 */
void MCTRL_API mcButton_Terminate(void);


/**
 * Window class (unicode version). 
 */ 
#define MC_WC_BUTTONW          L"mCtrl.button"

/**
 * Window class (ANSI version). 
 */ 
#define MC_WC_BUTTONA           "mCtrl.button"

#ifdef UNICODE
    /**
     * Window class.
     */
    #define MC_WC_BUTTON        MC_WC_BUTTONW
#else
    #define MC_WC_BUTTON        MC_WC_BUTTONA
#endif


/**
 * Synonymum for BS_ICON. See @ref sec_icon.
 */
#define MC_BS_ICON               (BS_ICON)

/**
 * Style to create a split button. Binary compatible with
 * @c BS_SPLITBUTTON. See @ref sec_split.
 */
#define MC_BS_SPLITBUTTON        (0x0000000CL)

/**
 * Style to create a default split button. Binary compatible with
 * @c BS_DEFSPLITBUTTON. See @ref sec_split.
 */
#define MC_BS_DEFSPLITBUTTON     (0x0000000DL)

/**
 * State of the split button when dropdown button is pressed.
 * It is a possible value returned by standard @c BM_GETSTATE message.
 * It is equivalent to standard @c BST_DROPDOWNPUSHED.
 * See @ref sec_split.
 */
#define MC_BST_DROPDOWNPUSHED    0x0400

/**
 * Notification code fired when user clicks on the dropdown button.
 * It is equivalent to standard @c BCN_DROPDOWN. It is passed via
 * the @c WM_NOTIFY message. See @ref sec_split.
 * @param[in] wParam Id of the control sending the notification.
 * @param[in] lParam Pointer to @c mc_NMBCDROPDOWN with associated data.
 * @return You should return zero if you process the notification.
 * @sa mc_NMBCDROPDOWN
 */
#define MC_BCN_DROPDOWN          (0xfffffb20U)

/**
 * Structure associated to MC_BCN_DROPDOWN notification.
 * It is binary compatible with @c NMBCDROPDOWN. See @ref sec_split.
 * @sa MC_BCN_DROPDOWN
 */
typedef struct mc_NMBCDROPDOWN_tag {
    /** @brief Common notification structure header. */
    NMHDR hdr;
    /** @brief Client reectangle of the dropdown button. */
    RECT rcButton;
} mc_NMBCDROPDOWN;


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_BUTTON_H */
