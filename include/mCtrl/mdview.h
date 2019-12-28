/*
 * Copyright (c) 2019 Martin Mitas
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

#ifndef MCTRL_MDVIEW_H
#define MCTRL_MDVIEW_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file Markdown view control (@c MC_WC_MDVIEW)
 *
 * TODO: describe me.
 *
 *
 * @section treelist_std_msgs Standard Messages
 *
 * These standard messages are handled by the control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 * - @c CCM_SETWINDOWTHEME
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcMdView_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcMdView_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (Unicode variant). */
#define MC_WC_MDVIEWW        L"mCtrl.mdview"
/** Window class name (ANSI variant). */
#define MC_WC_MDVIEWA         "mCtrl.mdview"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/* @brief Do not justify paragraphs. */
#define MC_MDS_NOJUSTIFY        0x0001

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Loads MarkDown document from file (Unicode variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) The file path to load.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MDM_GOTOFILEW        (MC_MDM_FIRST + 0)

/**
 * @brief Loads MarkDown document from file (ANSI variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c char*) The file path to load.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MDM_GOTOFILEA        (MC_MDM_FIRST + 1)

/**
 * @brief Loads MarkDown document from the specified URL (Unicode variant).
 *
 * Note that only @c res:// and @c file:// protocols are supported.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) The URL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MDM_GOTOURLW         (MC_MDM_FIRST + 2)

/**
 * @brief Loads MarkDown document from the specified URL (ANSI variant).
 *
 * Note that only @c res:// and @c file:// protocols are supported.
 *
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c char*) The URL.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MDM_GOTOURLA         (MC_MDM_FIRST + 3)

/**
 * @brief Set encoding of MarkDown documents loaded with @c MC_MDMGOTOxxx
 * family of messages.
 * @param[in] wParam (@c UINT) Codepage to use. Initial value is @c CP_UTF8.
 * @param lParam Reserved, set to zero.
 * @return Not defined, do not rely on return value.
 */
#define MC_MDM_SETINPUTENCODING (MC_MDM_FIRST + 4)

/**
 * @brief Get encoding of MarkDown documents loaded with @c MC_MDMGOTOxxx
 * family of messages.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c UINT) The codepage. Initial value is @c CP_UTF8.
 */
#define MC_MDM_GETINPUTENCODING (MC_MDM_FIRST + 5)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_MDVIEWW MC_WC_MDVIEWA */
#define MC_WC_MDVIEW            MCTRL_NAME_AW(MC_WC_MDVIEW)
/** Unicode-resolution alias. @sa MC_MDM_GOTOFILEW MC_MDM_GOTOFILEA */
#define MC_MDM_GOTOFILE         MCTRL_NAME_AW(MC_MDM_GOTOFILE)
/** Unicode-resolution alias. @sa MC_MDM_GOTOURLW MC_MDM_GOTOURLA */
#define MC_MDM_GOTOURL          MCTRL_NAME_AW(MC_MDM_GOTOURL)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_MDVIEW_H */
