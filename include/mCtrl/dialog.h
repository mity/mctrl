/*
 * Copyright (c) 2012 Martin Mitas
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

#ifndef MCTRL_DIALOG_H
#define MCTRL_DIALOG_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Dialog functions
 *
 * This module offers functions for creation of modal and modless dialogs
 * in ery similar manner as standard functions @c DialogBox and @c CreateDialog
 * do.
 * 
 * Therefore mCtrl functions are very similar to their @c USER32.DLL
 * counterparts, including their function name and parameteres. Actually
 * the only difference is that the mCtrl functions take an extra argument
 * @c dwFlags. When the @c dwFlags is zero, the functions behace exactly as
 * the original functions.
 *
 * When set to non-zero, the functions provide new fucntionality. Currently
 * only the flag @c MC_DF_DEFAULTFONT is supported. When set, it forces the
 * dialog to use default font, as defined by MS user interface guide lines.
 *
 * All the functions support the classic dialog templates (@c DLGTEMPLATE)
 * as well as the extended dialog tempolates (@c DLGTEMPLATEEX).
 */

/**
 * @name Dialog flags
 */
/**@{*/

/**
 * @brief Force a default font into the dialog template.
 *
 * When this flag is set, the dialog template is modified so the dialog uses
 * a default font for the particular Windows version, according to the
 * MS user itnerface guide lines.
 *
 * Depending on Windows version, it forces the template to use
 * <tt>MS Shell Dlg</tt>, <tt>MS Shell Dlg 2</tt> or <tt>Segoe UI</tt>.
 *
 * Note that when using this flag, the font specified originally in the
 * dialog template is used only as a fallback in cae of any error.
 *
 * @attention Metrics of the default fonts does differ. When using this font
 * you should test your dialog on multiple Windows version to ensure that no
 * content overflows.
 */
#define MC_DF_DEFAULTFONT              0x00000001

/**@}*/

/**
 * @brief Creates modeless dialog (unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * @brief Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * @brief Creates modeless dialog (unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogW(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags) \
                mcCreateDialogParamW((hInstance),(lpTemplateName),(hWndParent),   \
                                     (lpDialogFunc),0L,(dwFlags))

/**
 * @brief Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogA(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags) \
                mcCreateDialogParamA((hInstance),(lpTemplateName),(hWndParent),   \
                                     (lpDialogFunc),0L,(dwFlags))

/**
 * @brief Creates modeless dialog (unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogIndirectParamW(HINSTANCE hInstance,
                LPCDLGTEMPLATEW lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * @brief Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
HWND MCTRL_API mcCreateDialogIndirectParamA(HINSTANCE hInstance,
                LPCDLGTEMPLATEA lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * @brief Creates modeless dialog (unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogIndirectW(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)  \
                mcCreateDialogIndirectParamW((hInstance),(lpTemplate),(hWndParent),    \
                                             (lpDialogFunc),0L,(dwFlags))

/**
 * @brief Creates modeless dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return Handle of the created dialog or @c NULL on error.
 */
#define mcCreateDialogIndirectA(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)  \
                mcCreateDialogIndirectParamA((hInstance),(lpTemplate),(hWndParent),    \
                                             (lpDialogFunc),0L,(dwFlags))


/**
 * @brief Creates and runs modal dialog (unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * @brief Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                DWORD dwFlags);

/**
 * @brief Creates and runs modal dialog (unicode variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxW(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxParamW((hInstance),(lpTemplateName),(hWndParent),     \
                                  (lpDialogFunc),0L,(dwFlags))

/**
 * @brief Creates and runs modal dialog (ANSII variant).
 * @param hInstance
 * @param lpTemplateName
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxA(hInstance,lpTemplateName,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxParamA((hInstance),(lpTemplateName),(hWndParent),     \
                                  (lpDialogFunc),0L,(dwFlags))


/**
 * @brief Creates and runs modal dialog (unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxIndirectParamW(HINSTANCE hInstance,
                LPCDLGTEMPLATEW lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * @brief Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param lParamInit
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
INT_PTR MCTRL_API mcDialogBoxIndirectParamA(HINSTANCE hInstance,
                LPCDLGTEMPLATEA lpTemplate, HWND hWndParent,
                DLGPROC lpDialogFunc, LPARAM lParamInit, DWORD dwFlags);

/**
 * @brief Creates and runs modal dialog (unicode variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxIndirectW(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxIndirectParamW((hInstance),(lpTemplate),(hWndParent),     \
                                          (lpDialogFunc),0L,(lpDialogFunc))

/**
 * @brief Creates and runs modal dialog (ANSI variant).
 * @param hInstance
 * @param lpTemplate
 * @param hWndParent
 * @param lpDialogFunc
 * @param dwFlags Dialog flags.
 * @return The result of the dialog run as stored with @c EndDialog, or @c -1
 * if the function fails.
 */
#define mcDialogBoxIndirectA(hInstance,lpTemplate,hWndParent,lpDialogFunc,dwFlags)   \
                mcDialogBoxIndirectParamA((hInstance),(lpTemplate),(hWndParent),     \
                                          (lpDialogFunc),0L,(lpDialogFunc))


/**
 * @name Unicode Resolution
 */
/*@{*/

#ifdef UNICODE
    /** @brief Unicode-resolution alias. @sa mcCreateDialogParamW mcCreateDialogParamA */
    #define mcCreateDialogParam            mcCreateDialogParamW
    /** @brief Unicode-resolution alias. @sa mcCreateDialogW mcCreateDialogA */
    #define mcCreateDialog                 mcCreateDialogW
    /** @brief Unicode-resolution alias. @sa mcCreateDialogIndirectParamW mcCreateDialogIndirectParamA */
    #define mcCreateDialogIndirectParam    mcCreateDialogIndirectParamW
    /** @brief Unicode-resolution alias. @sa mcCreateDialogIndirectW mcCreateDialogIndirectA */
    #define mcCreateDialogIndirect         mcCreateDialogIndirectW
    /** @brief Unicode-resolution alias. @sa mcDialogBoxParamW mcDialogBoxParamA */
    #define mcDialogBoxParam               mcDialogBoxParamW
    /** @brief Unicode-resolution alias. @sa mcDialogBoxW mcDialogBoxA */
    #define mcDialogBox                    mcDialogBoxW
    /** @brief Unicode-resolution alias. @sa mcDialogBoxIndirectParamW mcDialogBoxIndirectParamA */
    #define mcDialogBoxIndirectParam       mcDialogBoxIndirectParamW
    /** @brief Unicode-resolution alias. @sa mcDialogBoxIndirectW mcDialogBoxIndirectA */
    #define mcDialogBoxIndirect            mcDialogBoxIndirectW
#else
    #define mcCreateDialogParam            mcCreateDialogParamA
    #define mcCreateDialog                 mcCreateDialogA
    #define mcCreateDialogIndirectParam    mcCreateDialogIndirectParamA
    #define mcCreateDialogIndirect         mcCreateDialogIndirectA
    #define mcDialogBoxParam               mcDialogBoxParamA
    #define mcDialogBox                    mcDialogBoxA
    #define mcDialogBoxIndirectParam       mcDialogBoxIndirectParamA
    #define mcDialogBoxIndirect            mcDialogBoxIndirectA
#endif

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_DIALOG_H */
