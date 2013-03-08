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

#include "propview.h"
#include "propset.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define PROPVIEW_DEBUG     1*/

#ifdef PROPVIEW_DEBUG
    #define PROPVIEW_TRACE         MC_TRACE
#else
    #define PROPVIEW_TRACE(...)    do { } while(0)
#endif


#define PADDING_H     3
#define PADDING_V     2


static const TCHAR propview_wc[] = MC_WC_PROPVIEW;    /* window class name */


typedef struct propview_tag propview_t;
struct propview_tag {
    HWND win;
    HWND notify_win;
    HFONT font;
    propset_t* propset;
    UINT style            : 30;
    UINT no_redraw        : 1;
    UINT dirty_scrollbars : 1;
    WORD row_height;
    WORD label_width;
    WORD scroll_y;
};


static void
propview_do_vscroll(propview_t* pv, SCROLLINFO* si, int scroll_y)
{
    if(scroll_y > si->nMax - (int)si->nPage + 1)
        scroll_y = si->nMax - (int)si->nPage + 1;
    if(scroll_y < si->nMin)
        scroll_y = si->nMin;

    if(scroll_y == pv->scroll_y)
        return;

    SetScrollPos(pv->win, SB_VERT, scroll_y, TRUE);
    if(!pv->no_redraw)
        ScrollWindowEx(pv->win, 0, (pv->scroll_y - scroll_y) * pv->row_height,
                       NULL, NULL, NULL, NULL, SW_ERASE | SW_INVALIDATE);
    pv->scroll_y = scroll_y;
}

static void
propview_vscroll_rel(propview_t* pv, int row_delta)
{
    SCROLLINFO si;

    PROPVIEW_TRACE("propview_vscroll_rel(%p, %d)", pv, (int)row_delta);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(pv->win, SB_VERT, &si);
    propview_do_vscroll(pv, &si, pv->scroll_y + row_delta);
}

static void
propview_vscroll(propview_t* pv, WORD opcode)
{
    SCROLLINFO si;
    int scroll_y = pv->scroll_y;

    PROPVIEW_TRACE("propview_scroll(%p, %d)", pv, (int)opcode);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;
    GetScrollInfo(pv->win, SB_VERT, &si);
    switch(opcode) {
        case SB_BOTTOM:          scroll_y = si.nMax; break;
        case SB_LINEUP:          scroll_y -= 1; break;
        case SB_LINEDOWN:        scroll_y += 1; break;
        case SB_PAGEUP:          scroll_y -= si.nPage; break;
        case SB_PAGEDOWN:        scroll_y += si.nPage; break;
        case SB_THUMBPOSITION:   scroll_y = si.nPos; break;
        case SB_THUMBTRACK:      scroll_y = si.nTrackPos; break;
        case SB_TOP:             scroll_y = 0; break;
    }
    propview_do_vscroll(pv, &si, scroll_y);
}

static void
propview_mouse_wheel(propview_t* pv, int wheel_delta)
{
    SCROLLINFO si;
    int line_delta;

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    GetScrollInfo(pv->win, SB_VERT, &si);

    line_delta = mc_wheel_scroll(pv->win, TRUE, wheel_delta, si.nPage);
    if(line_delta != 0)
        propview_vscroll_rel(pv, line_delta);
}

static void
propview_setup_scrollbars(propview_t* pv)
{
    RECT rect;
    SCROLLINFO si;
    int scroll_y;

    PROPVIEW_TRACE("propview_setup_scrollbars(%p)", pv);

    if(pv->no_redraw) {
        pv->dirty_scrollbars = 1;
        return;
    }

    GetClientRect(pv->win, &rect);

    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = dsa_size(&pv->propset->items) - 1;
    si.nPage = MC_MAX(1, mc_height(&rect) / pv->row_height);

    scroll_y = SetScrollInfo(pv->win, SB_VERT, &si, TRUE);
    propview_do_vscroll(pv, &si, scroll_y);
    pv->dirty_scrollbars = 0;
}

static void
propview_refresh(void* view, void* refresh_data)
{
    propview_t* pv = (propview_t*) view;
    propset_refresh_data_t* data = (propset_refresh_data_t*) refresh_data;

    if(pv->no_redraw)
        return;

    if(data == NULL  ||  data->size_delta != 0)
        propview_setup_scrollbars(pv);

    if(data != NULL) {
        // TODO -- invalidate rect(s) for affected items only instead of everything
        //return;
    }

    InvalidateRect(pv->win, NULL, TRUE);
}

static void
propview_paint(propview_t* pv, HDC dc)
{
    RECT rect;
    RECT label_rect;
    RECT value_rect;
    WORD i, n;
    propset_item_t* item;
    int old_dc_state;
    HPEN pen;

    n = propset_size(pv->propset);
    GetClientRect(pv->win, &rect);

    old_dc_state = SaveDC(dc);
    pen = CreatePen(PS_SOLID, 0, RGB(223,223,223));
    SelectObject(dc, pen);
    SelectObject(dc, pv->font ? pv->font : GetStockObject(SYSTEM_FONT));
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));

    /* Paint the vertical grid line */
    MoveToEx(dc, pv->label_width, 0, NULL);
    LineTo(dc, pv->label_width, (n - pv->scroll_y) * pv->row_height);

    for(i = pv->scroll_y; i < n; i++) {
        item = propset_item(pv->propset, i);

        /* Paint horizontal grid line */
        MoveToEx(dc, 0, (i - pv->scroll_y + 1) * pv->row_height - 1, NULL);
        LineTo(dc, rect.right, (i - pv->scroll_y + 1) * pv->row_height - 1);

        /* Paint label */
        label_rect.left = PADDING_H;
        label_rect.top = (i - pv->scroll_y) * pv->row_height;
        label_rect.right = pv->label_width - 1 - label_rect.left,
        label_rect.bottom = label_rect.top + pv->row_height - 1;
        DrawText(dc, item->text, -1, &label_rect,
                 DT_SINGLELINE | DT_END_ELLIPSIS | DT_LEFT | DT_VCENTER);

        /* Paint value */
        if(item->value != NULL) {
            int dc_state;

            mc_rect_set(&value_rect, pv->label_width + PADDING_H, label_rect.top,
                                     rect.right - PADDING_H, label_rect.bottom);

            dc_state = SaveDC(dc);
            value_paint(item->value, dc, &value_rect, 0);
            RestoreDC(dc, dc_state);
        }

        /* Check if out of visible area */
        if(label_rect.bottom > rect.bottom)
            break;
    }

    DeleteObject(pen);
    RestoreDC(dc, old_dc_state);
}

static int
propview_set_propset(propview_t* pv, propset_t* propset)
{
    PROPVIEW_TRACE("propview_set_propset(%p, %p)", pv, propset);

    if(propset != NULL  &&  propset == pv->propset)
        return 0;

    if(propset != NULL) {
        propset_ref(propset);
    } else if(!(pv->style & MC_PVS_NOPROPSETCREATE)) {
        DWORD flags = ((pv->style & MC_PVS_SORTITEMS) ? MC_PSF_SORTITEMS : 0);
        propset = propset_create(flags);
        if(MC_ERR(propset == NULL)) {
            MC_TRACE("propview_set_propset: propset_create() failed.");
            mc_send_notify(pv->notify_win, pv->win, NM_OUTOFMEMORY);
            return -1;
        }
    }

    if(propset != NULL) {
        if(MC_ERR(propset_install_view(propset, pv, propview_refresh) != 0)) {
            MC_TRACE("propview_set_propset: propset_install_view() failed.");
            propset_unref(propset);
            mc_send_notify(pv->notify_win, pv->win, NM_OUTOFMEMORY);
            return -1;
        }
    }

    if(pv->propset != NULL) {
        propset_uninstall_view(pv->propset, pv);
        propset_unref(pv->propset);
    }

    pv->propset = propset;

    propview_setup_scrollbars(pv);
    if(!pv->no_redraw)
        InvalidateRect(pv->win, NULL, TRUE);
    return 0;
}

static void
propview_style_changed(propview_t* pv, int style_type, STYLESTRUCT* ss)
{
    if(style_type == GWL_STYLE) {
        pv->style = ss->styleNew;

        if(!(pv->style & MC_PVS_NOPROPSETCREATE)  &&  pv->propset == NULL) {
            if(MC_ERR(propview_set_propset(pv, NULL) != 0)) {
                MC_TRACE("propview_style_changed: propview_set_propset() failed.");
                mc_send_notify(pv->notify_win, pv->win, NM_OUTOFMEMORY);
            }
        }
    }

    propview_setup_scrollbars(pv);
    if(!pv->no_redraw)
        RedrawWindow(pv->win, NULL, NULL,
                     RDW_INVALIDATE | RDW_FRAME | RDW_ERASE | RDW_ALLCHILDREN);
}

static propview_t*
propview_nccreate(HWND win, CREATESTRUCT* cs)
{
    propview_t* pv;
    SIZE size;

    PROPVIEW_TRACE("propview_nccreate(%p, %p)", win, cs);

    pv = (propview_t*) malloc(sizeof(propview_t));
    if(MC_ERR(pv == NULL)) {
        MC_TRACE("propview_nccreate: malloc() failed.");
        return NULL;
    }

    mc_font_size(NULL, &size);

    memset(pv, 0, sizeof(propview_t));
    pv->win = win;
    pv->notify_win = cs->hwndParent;
    pv->style = cs->style;
    pv->row_height = size.cy + 2 * PADDING_V + 1;  /* +1 for grid line */
    pv->label_width = 10 * size.cx;

    return pv;
}


static int
propview_create(propview_t* pv)
{
    PROPVIEW_TRACE("propview_create(%p)", pv);

    if(MC_ERR(propview_set_propset(pv, NULL) != 0)) {
        MC_TRACE("propview_create: propview_set_propset() failed.");
        return -1;
    }

    return 0;
}

static void
propview_destroy(propview_t* pv)
{
    PROPVIEW_TRACE("propview_destroy(%p)", pv);

    if(pv->propset != NULL) {
        propset_uninstall_view(pv->propset, pv);
        propset_unref(pv->propset);
        pv->propset = NULL;
    }
}

static inline void
propview_ncdestroy(propview_t* pv)
{
    free(pv);
}


/************************
 *** Window procedure ***
 ************************/

static LRESULT CALLBACK
propview_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    propview_t* pv = (propview_t*) GetWindowLongPtr(win, 0);

    switch(msg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(win, &ps);
            if(!pv->no_redraw)
                propview_paint(pv, ps.hdc);
            EndPaint(win, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
            propview_paint(pv, (HDC) wp);
            return 0;

        case MC_PVM_GETPROPSET:
            return (LRESULT) pv->propset;

        case MC_PVM_SETPROPSET:
            return (propview_set_propset(pv, (propset_t*)lp) == 0);

        case MC_PVM_INSERTITEMW:
            return mcPropSet_InsertItemW(pv->propset, (MC_PROPSETITEMW*)lp);

        case MC_PVM_INSERTITEMA:
            return mcPropSet_InsertItemA(pv->propset, (MC_PROPSETITEMA*)lp);

        case MC_PVM_SETITEMW:
            return mcPropSet_SetItemW(pv->propset, (MC_PROPSETITEMW*)lp);

        case MC_PVM_SETITEMA:
            return mcPropSet_SetItemA(pv->propset, (MC_PROPSETITEMA*)lp);

        case MC_PVM_GETITEMW:
            return mcPropSet_GetItemW(pv->propset, (MC_PROPSETITEMW*)lp);

        case MC_PVM_GETITEMA:
            return mcPropSet_GetItemA(pv->propset, (MC_PROPSETITEMA*)lp);

        case MC_PVM_DELETEITEM:
            return mcPropSet_DeleteItem(pv->propset, wp);

        case MC_PVM_DELETEALLITEMS:
            return mcPropSet_DeleteAllItems(pv->propset);

        case MC_PVM_GETITEMCOUNT:
            return mcPropSet_GetItemCount(pv->propset);

        case WM_VSCROLL:
            propview_vscroll(pv, LOWORD(wp));
            return 0;

        case WM_MOUSEWHEEL:
            propview_mouse_wheel(pv, (int)(SHORT)HIWORD(wp));
            return 0;

        case WM_KILLFOCUS:
            mc_wheel_reset();
            break;

        case WM_SIZE:
            propview_setup_scrollbars(pv);
            return 0;

        case WM_SETFONT:
        {
            SIZE size;
            pv->font = (HFONT) wp;
            mc_font_size(pv->font, &size);
            pv->row_height = size.cy + 2 * PADDING_V + 1;
            if((BOOL) lp  &&  !pv->no_redraw)
                InvalidateRect(win, NULL, TRUE);
            return 0;
        }

        case WM_GETFONT:
            return (LRESULT) pv->font;

        case WM_SETREDRAW:
            pv->no_redraw = !wp;
            if(!pv->no_redraw  &&  pv->dirty_scrollbars)
                propview_setup_scrollbars(pv);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;

        case WM_STYLECHANGED:
            propview_style_changed(pv, wp, (STYLESTRUCT*)lp);
            return 0;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = pv->notify_win;
            pv->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LPARAM) old;
        }

        case WM_NCCREATE:
            pv = propview_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(pv == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)pv);
            return TRUE;

        case WM_CREATE:
            return (propview_create(pv) == 0 ? 0 : -1);

        case WM_DESTROY:
            propview_destroy(pv);
            return 0;

        case WM_NCDESTROY:
            if(pv)
                propview_ncdestroy(pv);
            return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}

int
propview_init_module(void)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = propview_proc;
    wc.cbWndExtra = sizeof(propview_t*);
    wc.hInstance = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = propview_wc;
    if(MC_ERR(RegisterClass(&wc) == 0)) {
        MC_TRACE_ERR("propview_init_module: RegisterClass() failed");
        return -1;
    }

    return 0;
}

void
propview_fini_module(void)
{
    UnregisterClass(propview_wc, NULL);
}
