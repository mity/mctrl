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
 * fact the control embeds Internet Explorer in it, so it can display a
 * plethory of multimedia files, use javascript etc. The easiest way how to
 * show some HTML is to create the HTML control and send a message
 * @ref MC_HM_GOTOURL which takes target URL as its argument to the control.
 *
 * The control accepts any protocol understood by the Internet Explorer,
 * for example:
 * - http://www.example.org
 * - file://C:/page.html
 * - res://some_dll/id_of_resource
 * - its:helpFile.chm::page.htm
 *
 * @section html_proto_res Resource Protocol
 *
 * The @c res: protocol is especially useful. It allows you to embed some
 * resources like HTML pages, cascading stylesheets (CSS), images (PNG, JPG etc.),
 * javascripts into binary of your application or any DLL it uses.
 *
 * The resource must be of type @c RT_HTML. Then you can link to that resource
 * with url in format "res://modname/res_id" where @c modname is name of the
 * binary module (usually filename of your program or any DLL it loads) and
 * @c res_id is ID of the resource in the resource script (RC). It can be both
 * string or number identifier.
 *
 * For example if you have a HTML file named @c some_page.html and add the
 * following line into your resource script
 * @code
 * my_html_id HTML some_page.html
 * @endcode
 * which is used to build a @c MYLIBRARY.DLL used by your application then
 * your application can simply send the message @c MC_HM_GOTOURL with URL
 * @c "res://mylibrary.dll/my_html_id".
 *
 * @section html_proto_app Application Protocol
 *
 * The control also implements a simple application protocol@app: which is
 * intended for integration of HTML contents into your application logic.
 *
 * Whenever user clicks on a link with URL starting with the @c app:
 * the control sends notification @ref MC_HN_APPLINK to its parent which is
 * supposed to react programatically. The control itself does not interpret
 * application link URLs in any way.
 *
 * @section html_gotchas Gotchas
 *
 * - The HTML control implementation is based on OLE and COM technologies.
 *   During creation of the control @c OleInitialize() is called. I.e.
 *   the OLE subsystem is initialized for every thread which creates
 *   the HTML control. @c OleUninitialize() is similarly called when the
 *   control is destroyed. If you want to send some messages to the control
 *   from another thread then where it has been created, you have to
 *   initialize OLE subsystem in that thread manually.
 *
 * - Remember that the control is a wrapper of embedded MS Internet Explorer
 *   so exact behavior depends on the version of MS IE installed.
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
 * @name Control Styles
 */
/*@{*/
/** @brief Disables context menu */
#define MC_HS_NOCONTEXTMENU   (0x00000001L)
/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Displays a contents specified by the given URL (unicode variant).
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c const @c WCHAR*) Pointer to URL string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_HM_GOTOURLW        (WM_USER + 10)

/**
 * @brief Displays a contents specified by the given URL (ANSI variant).
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

/**
 * @brief Structure used for some HTML control notifications (unicode variant).
 * @sa @ref MC_HN_APPLINK @ref MC_HN_DOCUMENTCOMPLETE
 */
typedef struct MC_NMHTMLURLW_tag {
    /** @brief Common notification structure header. */
    NMHDR hdr;
    /** @brief String representation of @c app: protocol URL. */
    LPCWSTR pszUrl;
} MC_NMHTMLURLW;

/**
 * @brief Structure used for some HTML control notifications (ANSI variant).
 * @sa @ref MC_HN_APPLINK @ref MC_HN_DOCUMENTCOMPLETE
 */
typedef struct MC_NMHTMLURLA_tag {
    /** @brief Common notification structure header. */
    NMHDR hdr;
    /** @brief String representation of @c app: protocol URL. */
    LPCSTR pszUrl;
} MC_NMHTMLURLA;


#ifdef UNICODE
    /** @brief Unicode-resolution alias. */
    #define MC_NMHTMLURL         MC_NMHTMLURLW
#else
    #define MC_NMHTMLURL         MC_NMHTMLURLA
#endif



/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Fired when the browser should navigates to to URL with application protocol.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * details about the URL.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_APPLINK            ((0U-2000U) + 0x0001)

/**
 * @brief Fired when loading of a document is complete.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@ref MC_NMHTMLURL*) Pointer to a structure specifying
 * details about the URL.
 * @return Application should return zero if it processes the notification.
 */
#define MC_HN_DOCUMENTCOMPLETE   ((0U-2000U) + 0x0002)

/*@}*/



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_HTML_H */
