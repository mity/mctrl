/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2019-2020 Martin Mitas
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
 * Markdown view control is a control which is able to display Markdown files
 * which can be stored on a file system or embedded in the executable as a
 * resource loadable via @code LoadResource() API.
 *
 * @note The control is very new and at the moment the control has many
 * limitations. It should have more features added in the future.
 *
 * This control has been created as a much light-wighted variant for the HTML
 * control and the primary motivation is displaying richer text for showing
 * license, readme or simple help.
 *
 *
 * @section mdview_markdown_dialect Markdown Dialect
 *
 * The control uses MD4C Markdown parser (https://github.com/mity/md4c) under
 * the hood. The parser is fully compliant to CommonMark specification
 * (https://spec.commonmark.org/0.29/). Additionally supports some extensions.
 *
 * Therefore refer to the specification and MD4C documentation for exact
 * description of the Markdown syntax supported.
 *
 * However note that some Markdown features are not yet implemented in the
 * control:
 *
 * Not yet supported (planned to be added):
 *  - Images
 *  - Task Lists
 *  - Tables
 *
 * Also note that raw HTML blocks and spans are intentionally disabled and
 * won't be supported.
 *
 *
 * @section mdview_links Handling Links
 *
 * Currently the control attempts to follow the links with addresses which end
 * with '.md' or '.markdown'. Other links are opened via @code ShellExecute()
 * so, depending on the link address, they are opened by some default
 * application as defined by system configuration.
 *
 * Therefore typical links with the http: or https: schemes are opened in the
 * default web browser and links with the mailto: scheme in the default e-mail
 * client.
 *
 * (We plan some better control and API to make this more flexible.)
 *
 *
 * @section mdview_std_msgs Standard Messages
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
