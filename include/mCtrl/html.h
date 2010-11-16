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

#ifndef MCTRL_HTML_H
#define MCTRL_HTML_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief HTML control (@c MC_WC_HTML).
 *
 * As the control name suggests, the control displays a HTML documents. In 
 * fact the control embeds Internet Explorer in it, so it can display much 
 * more, inclusing images, directory contents, Microsoft help files etc.
 *
 * Currently there is only one message, @ref MC_HM_GOTOURL which controls what 
 * page is displayed in the control. That's all. ;-)
 *
 * Examples of URLs accepted by the @c MC_HM_GOTOURL message:
 * - http://www.example.org
 * - file://C:/page.html
 *
 * In future the control should be extended to support displaying of 
 * in memory HTML documents, documents stored in application resource files,
 * and to send some notification messages when some interesting stuff happens
 * (e.g. when user clicks on a link, or when the control finishes loading
 * of the HTML page).
 *
 * However it will never be intended to do everything, so do not plan 
 * developing of full-featured web browser on top of the control ;-)
 *
 * The HTML control implementation is based on OLE and COM technologies.
 * During creation of the control @c OleInitialize() is called. I.e.
 * the OLE subsystem is inirialized for every thread which creates 
 * the HTML control. @c OleUninitialize() is similarly called when the 
 * control is destroyed.
 *
 * @attention
 * If you want to send some messages to the control from another thread then
 * where it has been created, you have to initialize OLE subsystem in that
 * thread manually.
 */


/**
 * @brief Registers window class of the HTML control.
 * @return @c TRUE on success, @c FALSE on failure.
 * @sa @ref sec_init
 */
BOOL MCTRL_API mcHtml_Initialize(void);

/**
 * @brief Unregisters window class of the HTML control.
 *
 * @sa @ref sec_init
 */
void MCTRL_API mcHtml_Terminate(void);


/**
 * @name Window Class
 */
/*@{*/
/** @brief Window class name (unicode variant). */
#define MC_WC_HTMLW            L"mCtrl.html"
/** @brief Window class name (ANSI variant). */
#define MC_WC_HTMLA             "mCtrl.html"
#ifdef UNICODE
    /** @brief Unicode-resolution alias. */
    #define MC_WC_HTML          MC_WC_HTMLW
#else
    #define MC_WC_HTML          MC_WC_HTMLA
#endif
/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * Displays a contents specified by the given URL (unicode variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) Pointer to URL string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_GOTOURLW        (WM_USER + 10)

/**
 * Displays a contents specified by the given URL (ANSI variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c char*) Pointer to URL string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_GOTOURLA        (WM_USER + 11)


#ifdef UNICODE
    /** @brief Unicode-resolution alias. */
    #define MC_HM_GOTOURL     MC_HM_GOTOURLW
#else
    #define MC_HM_GOTOURL     MC_HM_GOTOURLA
#endif

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_HTML_H */
