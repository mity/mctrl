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

#include "dialog.h"


/* Uncomment this to have more verbose traces about this module. */
/*#define DLG_DEBUG     1*/

#ifdef DLG_DEBUG
    #define DLG_TRACE       MC_TRACE
    #define DLG_DUMP        MC_DUMP
#else
    #define DLG_TRACE       MC_NOOP
    #define DLG_DUMP        MC_NOOP
#endif


/* For DLGTEMPLATE description see
 *    http://blogs.msdn.com/b/oldnewthing/archive/2004/06/21/161375.aspx
 * For DLGTEMPLATEEX description see
 *    http://blogs.msdn.com/b/oldnewthing/archive/2004/06/23/163596.aspx
 *
 * Especially note the templates are always Unicode.
 */


typedef struct dlg_font_tag dlg_font_t;
struct dlg_font_tag {
    WCHAR* face_name;
    WORD point_size;
};

static const dlg_font_t*
dlg_default_font(void)
{
    static const dlg_font_t font_shell_dlg =   { L"MS Shell Dlg", 8 };
    static const dlg_font_t font_shell_dlg_2 = { L"MS Shell Dlg 2", 8 };
    static const dlg_font_t font_segoe_ui =    { L"Segoe UI", 9 };

    if(mc_win_version >= MC_WIN_VISTA)
        return &font_segoe_ui;
    else if(mc_win_version >= MC_WIN_2000)
        return &font_shell_dlg_2;
    else
        return &font_shell_dlg;
}


#define DLG_KIND_UNICODE   0x00000001
#define DLG_KIND_MODAL     0x00000002

#define DLGTEMPLATE_SIZE_HEADER                  18
#define DLGTEMPLATE_OFFSET_STYLE                  0
#define DLGTEMPLATE_OFFSET_ITEMCOUNT              8
#define DLGTEMPLATE_SIZE_ITEMHEADER              18

#define DLGTEMPLATEEX_SIZE_HEADER                26
#define DLGTEMPLATEEX_OFFSET_VERSION              0
#define DLGTEMPLATEEX_OFFSET_SIGNATURE            2
#define DLGTEMPLATEEX_OFFSET_STYLE               12
#define DLGTEMPLATEEX_OFFSET_ITEMCOUNT           16
#define DLGTEMPLATEEX_SIZE_ITEMHEADER            24

#define GET_BYTE(ptr)       (*( BYTE*)(ptr))
#define GET_WORD(ptr)       (*( WORD*)(ptr))
#define GET_DWORD(ptr)      (*(DWORD*)(ptr))

#define PUT_BYTE(ptr, b)    do { *(( BYTE*)(ptr)) = ( BYTE)(b); } while(0)
#define PUT_WORD(ptr, w)    do { *(( WORD*)(ptr)) = ( WORD)(w); } while(0)
#define PUT_DWORD(ptr, d)   do { *((DWORD*)(ptr)) = (DWORD)(d); } while(0)

#define TO_DWORD_BOUNDARY(ptr)    ((void*)(((UINT_PTR)(ptr) + 3) & ~3))


static INT_PTR
dlg_indirect(HINSTANCE instance, const DLGTEMPLATE* templ, HWND parent,
             DLGPROC proc, LPARAM lp, DWORD flags, DWORD kind)
{
    DLGTEMPLATE* templ_tmp = (DLGTEMPLATE*) templ;
    INT_PTR res;

    if(flags & MC_DF_DEFAULTFONT) {
        const dlg_font_t* default_font;
        BYTE* data = (BYTE*) templ;
        BYTE* data_tmp;
        BYTE* tail;
        BOOL templ_extended;
        DWORD header_size;
        DWORD style;
        WORD item_count;
        DWORD font_size;
        DWORD tail_size;
        DWORD tail_offset;
        int i;

        default_font = dlg_default_font();

        DLG_TRACE("dlg_indirect: Using font %S, %dpt",
                  default_font->face_name, default_font->point_size);

        /* Check DLGTEMPLATE version */
        if(GET_WORD(data + DLGTEMPLATEEX_OFFSET_SIGNATURE) == 0xffff) {
            templ_extended = TRUE;
            if(GET_WORD(data + DLGTEMPLATEEX_OFFSET_VERSION) != 1) {
                MC_TRACE("dlg_indirect: Unknown DIALOGEX version.");
                goto fallback;
            }
        } else {
            templ_extended = FALSE;
        }

        /* Retrieve some info from the template header */
        style = GET_DWORD(data + (templ_extended ? DLGTEMPLATEEX_OFFSET_STYLE : DLGTEMPLATE_OFFSET_STYLE));

        header_size =  (templ_extended ? DLGTEMPLATEEX_SIZE_HEADER : DLGTEMPLATE_SIZE_HEADER);
        if(GET_WORD(data + header_size) == 0x00ff)
            header_size += sizeof(DWORD); /* menu (by ordinal) */
        else
            header_size += sizeof(WCHAR) * (wcslen((WCHAR*)(data + header_size)) + 1);  /* menu name */

        DLG_TRACE("dlg_indirect: Dlg class '%S'", (WCHAR*)(data + header_size));
        header_size += sizeof(WCHAR) * (wcslen((WCHAR*)(data + header_size)) + 1);  /* class name */

        DLG_TRACE("dlg_indirect: Caption '%S'", (WCHAR*)(data + header_size));
        header_size += sizeof(WCHAR) * (wcslen((WCHAR*)(data + header_size)) + 1);  /* caption text */

        item_count = GET_WORD(data + (templ_extended ? DLGTEMPLATEEX_OFFSET_ITEMCOUNT
                                                     : DLGTEMPLATE_OFFSET_ITEMCOUNT));
        DLG_TRACE("dlg_indirect: Dlg has %d items", item_count);

        if(style & (DS_SETFONT | DS_SHELLFONT)) {
            WORD font_point_size;
            WCHAR* font_face_name;

            /* Get info about font */
            font_point_size = GET_WORD(data + header_size);
            font_face_name = (WCHAR*) (data + header_size + (templ_extended ? 6 : 2));

            /* Check if by chance there is already the correct font */
            if(font_point_size == default_font->point_size  &&
               wcscmp(font_face_name, default_font->face_name) == 0) {
                goto fallback;
            }

            /* Find start of the controls data after the header */
            tail = ((BYTE*)font_face_name);
            tail += sizeof(WCHAR) * (wcslen(font_face_name) + 1);
        } else {
            tail = data + header_size;
        }
        tail = TO_DWORD_BOUNDARY(tail);
        tail_size = 0;

        /* Calculate size of the rest of the data (i.e. for controls) */
        for(i = 0; i < item_count; i++) {
            tail_size = (UINT_PTR) TO_DWORD_BOUNDARY(tail_size);
            tail_size += (templ_extended ? DLGTEMPLATEEX_SIZE_ITEMHEADER
                                         : DLGTEMPLATE_SIZE_ITEMHEADER); /* control data */

            switch(GET_WORD(tail + tail_size)) {
                case 0xffff:   /* control class special code */
                case 0x00ff:   /* control class atom */
                    tail_size += 2 * sizeof(WORD);
                    break;

                default:       /* control class name */
                    tail_size += sizeof(WCHAR) * (wcslen((WCHAR*)(tail + tail_size)) + 1);
                    break;
            }

            switch(GET_WORD(tail + tail_size)) {
                case 0xffff:   /* control text by ordinal */
                    tail_size += 2 * sizeof(WORD);
                    break;

                default:       /* control text */
                    tail_size += sizeof(WCHAR) * (wcslen((WCHAR*)(tail + tail_size)) + 1);
                    break;
            }

            tail_size += sizeof(WORD) + GET_WORD(tail + tail_size);  /* extra data */
        }

        /* Determine space for the new font */
        font_size = (templ_extended ? 2 : 6);
        font_size += sizeof(WCHAR) * (wcslen(default_font->face_name) + 1);

        /* Allocate a temporary buffer for the modified template
         * (plus few bytes for alignement) */
        templ_tmp = (DLGTEMPLATE*) _malloca(header_size + font_size + tail_size + 4);
        if(MC_ERR(templ_tmp == NULL)) {
            MC_TRACE("dlg_indirect: _malloca() failed.");
            templ_tmp = (DLGTEMPLATE*) templ;
            goto fallback;
        }
        data_tmp = (BYTE*) templ_tmp;

        /* Copy template header */
        memcpy(data_tmp, data, header_size);

        /* Ensure the DS_SHELLFONT style is set */
        style |= DS_SHELLFONT;
        PUT_DWORD(data_tmp + (templ_extended ? DLGTEMPLATEEX_OFFSET_STYLE
                                             : DLGTEMPLATE_OFFSET_STYLE), style);

        /* Set template font */
        tail_offset = header_size;
        if(templ_extended) {
            tail_offset += 6;
            PUT_WORD(data_tmp + header_size + 0, default_font->point_size);  /* size */
            PUT_WORD(data_tmp + header_size + 2, FW_DONTCARE);               /* weight */
            PUT_BYTE(data_tmp + header_size + 4, FALSE);                     /* italic */
            PUT_BYTE(data_tmp + header_size + 5, DEFAULT_CHARSET);           /* charset */
            memcpy(data_tmp + header_size + 6, default_font->face_name,
                   sizeof(WCHAR) * (wcslen(default_font->face_name) + 1));
        } else {
            tail_offset += 2;
            PUT_WORD(data_tmp + header_size + 0, default_font->point_size);  /* size */
            memcpy(data_tmp + header_size + 2, default_font->face_name,
                   sizeof(WCHAR) * (wcslen(default_font->face_name) + 1));
        }
        tail_offset += sizeof(WCHAR) * (wcslen(default_font->face_name) + 1);
        tail_offset = (UINT_PTR) TO_DWORD_BOUNDARY(tail_offset);

        /* Copy template tail */
        memcpy(data_tmp + tail_offset, tail, tail_size);

        DLG_DUMP("dlg_indirect: Dialog resource dump:",
                 templ_tmp, tail_offset + tail_size);
    }

fallback:
    switch(kind) {
        case (DLG_KIND_MODAL | DLG_KIND_UNICODE):
            res = DialogBoxIndirectParamW(instance, templ_tmp, parent, proc, lp);
            break;

        case DLG_KIND_MODAL:
            res = DialogBoxIndirectParamA(instance, templ_tmp, parent, proc, lp);
            break;

        case DLG_KIND_UNICODE:
            res = (INT_PTR) CreateDialogIndirectParamW(instance, templ_tmp, parent, proc, lp);
            break;

        case 0:
            res = (INT_PTR) CreateDialogIndirectParamA(instance, templ_tmp, parent, proc, lp);
            break;

        default:
            MC_UNREACHABLE;
    }

    if(templ_tmp != templ)
        _freea(templ_tmp);

    return res;
}

static INT_PTR
dlg_direct(HINSTANCE instance, const void* templ_name, HWND parent,
           DLGPROC proc, LPARAM lp, DWORD flags, DWORD kind)
{
    HRSRC rsrc;
    HGLOBAL glob;
    DLGTEMPLATE* templ;

    if(kind & DLG_KIND_UNICODE)
        rsrc = FindResourceW(instance, (WCHAR*)templ_name, (WCHAR*)RT_DIALOG);
    else
        rsrc = FindResourceA(instance, (char*)templ_name, (char*)RT_DIALOG);
    if(MC_ERR(rsrc == NULL)) {
        MC_TRACE_ERR("dlg_load: FindResource() failed");
        goto err;
    }

    glob = LoadResource(instance, rsrc);
    if(MC_ERR(glob == NULL)) {
        MC_TRACE_ERR("dlg_load: LoadResource() failed");
        goto err;
    }

    templ = (DLGTEMPLATE*) LockResource(glob);
    if(MC_ERR(templ == NULL)) {
        MC_TRACE_ERR("dlg_load: LockResource() failed");
        goto err;
    }

    DLG_DUMP("dlg_direct: Dialog resource dump:",
             templ, SizeofResource(instance, rsrc));

    return dlg_indirect(instance, templ, parent, proc, lp, flags, kind);

err:
    return (kind & DLG_KIND_MODAL) ? (INT_PTR) -1 : (INT_PTR) NULL;
}


HWND MCTRL_API
mcCreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName,
                     HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                     DWORD dwFlags)
{
    return (HWND) dlg_direct(hInstance, lpTemplateName, hWndParent, lpDialogFunc,
                             lParamInit, dwFlags, DLG_KIND_UNICODE);
}

HWND MCTRL_API
mcCreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                     HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                     DWORD dwFlags)
{
    return (HWND) dlg_direct(hInstance, lpTemplateName, hWndParent, lpDialogFunc,
                             lParamInit, dwFlags, 0);
}

HWND MCTRL_API
mcCreateDialogIndirectParamW(HINSTANCE hInstance, LPCDLGTEMPLATEW lpTemplate,
                             HWND hWndParent, DLGPROC lpDialogFunc,
                             LPARAM lParamInit, DWORD dwFlags)
{
    return (HWND) dlg_indirect(hInstance, lpTemplate, hWndParent, lpDialogFunc,
                               lParamInit, dwFlags, DLG_KIND_UNICODE);
}

HWND MCTRL_API
mcCreateDialogIndirectParamA(HINSTANCE hInstance, LPCDLGTEMPLATEA lpTemplate,
                             HWND hWndParent, DLGPROC lpDialogFunc,
                             LPARAM lParamInit, DWORD dwFlags)
{
    return (HWND) dlg_indirect(hInstance, lpTemplate, hWndParent, lpDialogFunc,
                               lParamInit, dwFlags, 0);
}

INT_PTR MCTRL_API
mcDialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName,
                  HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                  DWORD dwFlags)
{
    return dlg_direct(hInstance, lpTemplateName, hWndParent, lpDialogFunc,
                      lParamInit, dwFlags, DLG_KIND_MODAL | DLG_KIND_UNICODE);
}

INT_PTR MCTRL_API
mcDialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName,
                  HWND hWndParent, DLGPROC lpDialogFunc, LPARAM lParamInit,
                  DWORD dwFlags)
{
    return dlg_direct(hInstance, lpTemplateName, hWndParent, lpDialogFunc,
                      lParamInit, dwFlags, DLG_KIND_MODAL);
}

INT_PTR MCTRL_API
mcDialogBoxIndirectParamW(HINSTANCE hInstance, LPCDLGTEMPLATEW lpTemplate,
                          HWND hWndParent, DLGPROC lpDialogFunc,
                          LPARAM lParamInit, DWORD dwFlags)
{
    return dlg_indirect(hInstance, lpTemplate, hWndParent, lpDialogFunc,
                        lParamInit, dwFlags, DLG_KIND_MODAL | DLG_KIND_UNICODE);
}

INT_PTR MCTRL_API
mcDialogBoxIndirectParamA(HINSTANCE hInstance, LPCDLGTEMPLATEA lpTemplate,
                          HWND hWndParent, DLGPROC lpDialogFunc,
                          LPARAM lParamInit, DWORD dwFlags)
{
    return dlg_indirect(hInstance, lpTemplate, hWndParent, lpDialogFunc,
                        lParamInit, dwFlags, DLG_KIND_MODAL);
}

