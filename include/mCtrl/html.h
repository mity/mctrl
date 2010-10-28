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
 * Currently there is only one message, @c MC_HM_GOTOURL which controls what 
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
 * @attention
 * The HTML control implementation is based on OLE and COM technologies.
 * During creation of the control @c OleInitialize() is called. This 
 * initializes the OLE subsystem for that particular thread. Sending a message
 * from other thread(s) can fail.
 *
 * @attention
 * If you want to send some messages to the control from another thread then
 * where it has been created, you have to initialize OLE subsystem in that
 * thread manually.
 */


/**
 * Registers the HTML control class.
 * @return @c TRUE on success, @c FALSE on failure.
 * @see sec_initialization
 */
BOOL MCTRL_API mcHtml_Initialize(void);

/**
 * Unregisters the HTML control class.
 * @see sec_initialization
 */
void MCTRL_API mcHtml_Terminate(void);


/**
 * Window class (unicode version). 
 */ 
#define MC_WC_HTMLW            L"mCtrl.html"

/**
 * Window class (ANSI version). 
 */ 
#define MC_WC_HTMLA             "mCtrl.html"

#ifdef UNICODE
    /**
     * Window class.
     */
    #define MC_WC_HTML          MC_WC_HTMLW
#else
    #define MC_WC_HTML          MC_WC_HTMLA
#endif


/**
 * Send this message to display a specific HTML page in the control.
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) Pointer to URL string.
 * @return Zero on success, -1 on failure.
 */
#define MC_HM_GOTOURLW        (WM_USER + 10)

/**
 * Send this message to display a specific HTML page in the control.
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c char*) Pointer to URL string.
 * @return Zero on success, -1 on failure.
 */
#define MC_HM_GOTOURLA        (WM_USER + 11)


#ifdef UNICODE
    /**
     * Send this message to display a specific HTML page in the control.
     * @param wParam Reserved, set to zero.
     * @param[in] lParam (@c const @c TCHAR*) Pointer to URL string.
     * @return Zero on success, -1 on failure.
     */
    #define MC_HM_GOTOURL     MC_HM_GOTOURLW
#else
    #define MC_HM_GOTOURL     MC_HM_GOTOURLA
#endif


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_HTML_H */
