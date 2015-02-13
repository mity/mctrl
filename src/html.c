/*
 * Copyright (c) 2008-2014 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty off
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* CREDITS:
 *  - Big thanks belong to Jeff Glatt for his article and sample code on
 *    http://www.codeproject.com/KB/COM/cwebpage.aspx
 */

/* TODO:
 *  - Notifications for context menus so app could customize the menu
 *    or provide its own. (App may want provide different menus depending
 *    on the object clicked. E.g. normal link versus image etc.).
 */

#include "html.h"
#include "theme.h"
#include "xcom.h"

#include <exdisp.h>    /* IWebBrowser2 */
#include <exdispid.h>  /* DISPID_xxxx constants */
#include <mshtml.h>    /* IHTMLDocument3, IHTMLElement */
#include <mshtmhst.h>  /* IDocHostUIHandler */


/* Uncomment this to have more verbose traces about MC_HTML control. */
/*#define HTML_DEBUG     1*/


#ifdef HTML_DEBUG
    #define HTML_TRACE        MC_TRACE
    #define HTML_TRACE_GUID   MC_TRACE_GUID
#else
    #define HTML_TRACE(...)        do {} while(0)
    #define HTML_TRACE_GUID(...)   do {} while(0)
#endif


/* Window class name */
static const TCHAR html_wc[] = MC_WC_HTML;


/* We use 'about:blank' as a default URL */
static const WCHAR url_blank_data[] = L"\x16\x00about:blank";
static BSTR url_blank = (BSTR) &url_blank_data[2];


static TCHAR ie_prop[] = _T("mctrl.html.handle");


/* Main control structure */
typedef struct html_tag html_t;
struct html_tag {
    HWND win;
    HWND ie_win;
    HWND notify_win;
    WNDPROC ie_proc;
    DWORD style                 : 28;
    DWORD unicode_notifications :  1;
    DWORD can_back              :  1;
    DWORD can_forward           :  1;

    /* Pointer to the COM-object representing the embedded Internet Explorer */
    IOleObject* browser_obj;

    /* This structure is also COM-object with these interfaces, for wiring
     * MC_HTML control to the embedded Internet Explorer COM-object */
    IDispatch dispatch;
    IOleClientSite client_site;
    IOleInPlaceSiteEx inplace_site_ex;
    IOleInPlaceFrame inplace_frame;
    IDocHostUIHandler ui_handler;
};


/* Helper macros for retrieving the complete structure inside methods
 * of particular interfaces */
#define MC_HTML_FROM_DISPTACH(ptr_dispatch)                                   \
    MC_CONTAINEROF(ptr_dispatch, html_t, dispatch)
#define MC_HTML_FROM_CLIENT_SITE(ptr_client_site)                             \
    MC_CONTAINEROF(ptr_client_site, html_t, client_site)
#define MC_HTML_FROM_INPLACE_SITE_EX(ptr_inplace_site_ex)                     \
    MC_CONTAINEROF(ptr_inplace_site_ex, html_t, inplace_site_ex)
#define MC_HTML_FROM_INPLACE_FRAME(ptr_inplace_frame)                         \
    MC_CONTAINEROF(ptr_inplace_frame, html_t, inplace_frame)
#define MC_HTML_FROM_UI_HANDLER(ptr_ui_handler)                               \
    MC_CONTAINEROF(ptr_ui_handler, html_t, ui_handler)


static HRESULT
html_QueryInterface(html_t* html, REFIID riid, void** obj)
{
    if(IsEqualGUID(riid, &IID_IUnknown)) {
        HTML_TRACE_GUID("html_QueryInterface(IID_IUnknown)", riid);
        *obj = (void*)&html->dispatch;
    } else if(IsEqualGUID(riid, &IID_IDispatch) ||
              IsEqualGUID(riid, &DIID_DWebBrowserEvents) ||
              IsEqualGUID(riid, &DIID_DWebBrowserEvents2)) {
        HTML_TRACE_GUID("html_QueryInterface(IID_IDispatch)", riid);
        *obj = (void*)&html->dispatch;
    } else if(IsEqualGUID(riid, &IID_IOleClientSite)) {
        HTML_TRACE_GUID("html_QueryInterface(IID_IOleClientSite)", riid);
        *obj = (void*)&html->client_site;
    } else if(IsEqualGUID(riid, &IID_IOleWindow) ||
              IsEqualGUID(riid, &IID_IOleInPlaceSite) ||
              IsEqualGUID(riid, &IID_IOleInPlaceSiteEx)) {
        HTML_TRACE_GUID("html_QueryInterface(IID_IOleInPlaceSiteEx)", riid);
        *obj = (void*)&html->inplace_site_ex;
    } else if(IsEqualGUID(riid, &IID_IOleInPlaceFrame)) {
        HTML_TRACE_GUID("html_QueryInterface(IID_IOleInPlaceFrame)", riid);
        *obj = (void*)&html->inplace_frame;
    } else if(IsEqualGUID(riid, &IID_IDocHostUIHandler)) {
        HTML_TRACE_GUID("html_QueryInterface(IID_IDocHostUIHandler)", riid);
        *obj = (void*)&html->ui_handler;
    } else {
        HTML_TRACE_GUID("html_QueryInterface: unsupported GUID", riid);
        *obj = NULL;
        return E_NOINTERFACE;
    }

    return S_OK;
}


/********************************
 *** Dummy reference counting ***
 ********************************/

/* Lifetime of html_t is determined by the lifetime of host window. When the
 * host window is being destroyed, the embedded browser is destroyed too.
 * Hence we don't need to reference count and delay html_t deallocation
 * when it drops back to zero. It simplifies the cleaning code quite a lot. */

static ULONG STDMETHODCALLTYPE
dummy_AddRef(void* self)
{
    return 2;
}

static ULONG STDMETHODCALLTYPE
dummy_Release(void* self)
{
    return 1;
}

#define DUMMY_ADDREF(type)                                                    \
        ((ULONG (STDMETHODCALLTYPE*)(type*)) dummy_AddRef)

#define DUMMY_RELEASE(type)                                                   \
        ((ULONG (STDMETHODCALLTYPE*)(type*)) dummy_Release)


/********************************
 *** IDispatch implementation ***
 ********************************/

static HRESULT STDMETHODCALLTYPE
dispatch_QueryInterface(IDispatch* self, REFIID riid, void** obj)
{
    return html_QueryInterface(MC_HTML_FROM_DISPTACH(self), riid, obj);
}

static HRESULT STDMETHODCALLTYPE
dispatch_GetTypeInfoCount(IDispatch* self, UINT* count)
{
    HTML_TRACE("dispatch_GetTypeInfoCount: [S_OK]");
    *count = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
dispatch_GetTypeInfo(IDispatch* self, UINT index, LCID lcid, ITypeInfo** type_info)
{
    HTML_TRACE("dispatch_GetTypeInfo: Stub [TYPE_E_ELEMENTNOTFOUND]");
    *type_info = NULL;
    return TYPE_E_ELEMENTNOTFOUND;
}

static HRESULT STDMETHODCALLTYPE
dispatch_GetIDsOfNames(IDispatch* self, REFIID riid, LPOLESTR* names,
                       UINT names_count, LCID lcid, DISPID* disp_id)
{
    int i;

    HTML_TRACE("dispatch_GetIDsOfNames('%S'): Stub [E_NOTIMPL]",
               names_count > 0 ? names[0] : L"");

    for(i = 0; i < names_count; i++)
        disp_id[i] = DISPID_UNKNOWN;
    return DISP_E_UNKNOWNNAME;
}

/* Forward declarations */
static LRESULT html_notify_text(html_t* html, UINT code, const WCHAR* url);
static LRESULT html_notify_http_error(html_t* html, int http_status, const WCHAR* url);
static int html_goto_url(html_t* html, const void* url, BOOL unicode);


static HRESULT STDMETHODCALLTYPE
dispatch_Invoke(IDispatch* self, DISPID disp_id, REFIID riid, LCID lcid,
                WORD flags, DISPPARAMS* params, VARIANT* var_res,
                EXCEPINFO* except, UINT* arg_err)
{
    html_t* html = MC_HTML_FROM_DISPTACH(self);

    switch(disp_id) {
        case DISPID_BEFORENAVIGATE2:
        {
            BSTR url = V_BSTR(V_VARIANTREF(&params->rgvarg[5]));
            VARIANT_BOOL* cancel = V_BOOLREF(&params->rgvarg[0]);

            HTML_TRACE("dispatch_Invoke: DISPID_BEFORENAVIGATE2(%S)", url);

            if(url != NULL  &&  wcsncmp(url, L"app:", 4) == 0) {
                html_notify_text(html, MC_HN_APPLINK, url);
                *cancel = VARIANT_TRUE;
            }
            break;
        }

        case DISPID_NAVIGATECOMPLETE2:
            HTML_TRACE("dispatch_Invoke: DISPID_NAVIGATECOMPLETE2");
            /* noop */
            break;

        case DISPID_NAVIGATEERROR:
        {
            BSTR url = V_BSTR(V_VARIANTREF(&params->rgvarg[3]));
            LONG status = V_I4(V_VARIANTREF(&params->rgvarg[1]));
            VARIANT_BOOL* cancel = V_BOOLREF(&params->rgvarg[0]);
            LRESULT lres = 0;

            HTML_TRACE("dispatch_Invoke: DISPID_NAVIGATEERROR(%ld, %S)", status, url);

            /* Status can be HTTP error code or HRESULT.
             * We propagate just the former. */
            if(0 < status && status < 1000)
                lres = html_notify_http_error(html, status, url);

            *cancel = (lres != 0 ? VARIANT_TRUE : VARIANT_FALSE);
            break;
        }

        /* Unfortunately, IE does not send DISPID_DOCUMENTCOMPLETE when
         * refreshing the page (e.g. from context menu). So we workaround
         * via DISPID_PROGRESSCHANGE below. */
        case DISPID_DOCUMENTCOMPLETE:
            HTML_TRACE("dispatch_Invoke: DISPID_DOCUMENTCOMPLETE");
            /* noop */
            break;

        case DISPID_PROGRESSCHANGE:
        {
            LONG progress_max = V_I4(&params->rgvarg[0]);
            LONG progress = V_I4(&params->rgvarg[1]);
            MC_NMHTMLPROGRESS notify;
            HTML_TRACE("dispatch_Invoke: DISPID_PROGRESSCHANGE(%ld, %ld)",
                       progress, progress_max);

            /* Send also notification about the progress */
            notify.hdr.hwndFrom = html->win;
            notify.hdr.idFrom = GetDlgCtrlID(html->win);
            notify.hdr.code = MC_HN_PROGRESS;
            notify.lProgress = progress;
            notify.lProgressMax = progress_max;
            MC_SEND(html->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify);

            /* This replaces DISPID_DOCUMENTCOMPLETE above */
            if(progress < 0  ||  progress_max < 0) {
                IWebBrowser2* browser_iface;
                HRESULT hr;

                hr = IOleObject_QueryInterface(html->browser_obj,
                        &IID_IWebBrowser2, (void**)&browser_iface);
                if(MC_ERR(hr != S_OK  ||  browser_iface == NULL)) {
                    MC_TRACE("dispatch_Invoke(DISPID_PROGRESSCHANGE): "
                             "QueryInterface(IID_IWebBrowser2) failed [0x%lx]", hr);
                } else {
                    BSTR url = NULL;
                    hr = IWebBrowser_get_LocationURL(browser_iface, &url);
                    if(hr == S_OK && url != NULL) {
                        html_notify_text(html, MC_HN_DOCUMENTCOMPLETE, url);
                        SysFreeString(url);
                    }
                    IWebBrowser_Release(browser_iface);
                }
            }
            break;
        }

        case DISPID_STATUSTEXTCHANGE:
            HTML_TRACE("dispatch_Invoke: DISPID_STATUSTEXTCHANGE");
            html_notify_text(html, MC_HN_STATUSTEXT, V_BSTR(&params->rgvarg[0]));
            break;

        case DISPID_TITLECHANGE:
            HTML_TRACE("dispatch_Invoke: DISPID_TITLECHANGE");
            html_notify_text(html, MC_HN_TITLETEXT, V_BSTR(&params->rgvarg[0]));
            break;

        case DISPID_COMMANDSTATECHANGE:
        {
            LONG cmd = V_I4(&params->rgvarg[1]);
            HTML_TRACE("dispatch_Invoke: DISPID_COMMANDSTATECHANGE");

            if(cmd == CSC_NAVIGATEBACK  ||  cmd == CSC_NAVIGATEFORWARD) {
                MC_NMHTMLHISTORY notify;
                BOOL enabled = (V_BOOL(&params->rgvarg[0]) != VARIANT_FALSE);

                if(cmd == CSC_NAVIGATEBACK)
                    html->can_back = enabled;
                else
                    html->can_forward = enabled;

                notify.hdr.hwndFrom = html->win;
                notify.hdr.idFrom = GetDlgCtrlID(html->win);
                notify.hdr.code = MC_HN_HISTORY;
                notify.bCanBack = html->can_back;
                notify.bCanForward = html->can_forward;
                MC_SEND(html->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify);
            }
            break;
        }

        case DISPID_NEWWINDOW2:
        /* This is called instead of DISPID_NEWWINDOW3 on Windows XP SP2
         * and older. */
        {
            VARIANT_BOOL* cancel = V_BOOLREF(&params->rgvarg[0]);
            HTML_TRACE("dispatch_Invoke: DISPID_NEWWINDOW2");

            if(html_notify_text(html, MC_HN_NEWWINDOW, L"") == 0) {
                *cancel = VARIANT_TRUE;
                HTML_TRACE("dispatch_Invoke(DISPID_NEWWINDOW2): Canceled.");
            }
            break;
        }

        case DISPID_NEWWINDOW3:
        {
            BSTR url = V_BSTR(&params->rgvarg[0]);
            VARIANT_BOOL* cancel = V_BOOLREF(&params->rgvarg[3]);
            HTML_TRACE("dispatch_Invoke: DISPID_NEWWINDOW3");

            if(html_notify_text(html, MC_HN_NEWWINDOW, url) == 0) {
                *cancel = VARIANT_TRUE;
                HTML_TRACE("dispatch_Invoke(DISPID_NEWWINDOW3, '%S'): Canceled.", url);
            }
            break;
        }

        case DISPID_DOWNLOADBEGIN:
            HTML_TRACE("dispatch_Invoke: DISPID_DOWNLOADBEGIN");
            /* noop */
            break;

        case DISPID_DOWNLOADCOMPLETE:
            HTML_TRACE("dispatch_Invoke: DISPID_DOWNLOADCOMPLETE");
            /* noop */
            break;

        case DISPID_FILEDOWNLOAD:
            HTML_TRACE("dispatch_Invoke: DISPID_FILEDOWNLOAD");
            /* noop */
            break;

        case DISPID_SETSECURELOCKICON:
            HTML_TRACE("dispatch_Invoke: DISPID_SETSECURELOCKICON");
            /* noop */
            break;

        case DISPID_PROPERTYCHANGE:
            HTML_TRACE("dispatch_Invoke: DISPID_PROPERTYCHANGE(%S)",
                       V_BSTR(&params->rgvarg[0]));
            /* noop */
            break;

        case DISPID_WINDOWSETLEFT:
        case DISPID_WINDOWSETTOP:
        case DISPID_WINDOWSETWIDTH:
        case DISPID_WINDOWSETHEIGHT:
        case DISPID_WINDOWSETRESIZABLE:
            /* noop */
            break;

        default:
            HTML_TRACE("dispatch_Invoke: unsupported disp_id %d", disp_id);
            return DISP_E_MEMBERNOTFOUND;
    }

    return S_OK;
}

static IDispatchVtbl dispatch_vtable = {
    dispatch_QueryInterface,
    DUMMY_ADDREF(IDispatch),
    DUMMY_RELEASE(IDispatch),
    dispatch_GetTypeInfoCount,
    dispatch_GetTypeInfo,
    dispatch_GetIDsOfNames,
    dispatch_Invoke
};


/*************************************
 *** IOleClientSite implementation ***
 *************************************/

static HRESULT STDMETHODCALLTYPE
client_site_QueryInterface(IOleClientSite* self, REFIID riid, void** obj)
{
    return html_QueryInterface(MC_HTML_FROM_CLIENT_SITE(self), riid, obj);
}

static HRESULT STDMETHODCALLTYPE
client_site_SaveObject(IOleClientSite* self)
{
    HTML_TRACE("client_site_SaveObject: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
client_site_GetMoniker(IOleClientSite* self, DWORD assign, DWORD moniker_id,
                       IMoniker** moniker)
{
    HTML_TRACE("client_site_GetMoniker: Stub [E_NOTIMPL]");
    *moniker = NULL;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
client_site_GetContainer(IOleClientSite* self, LPOLECONTAINER* container)
{
    HTML_TRACE("client_site_GetContainer: Stub [E_NOINTERFACE]");
    *container = NULL;
    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE
client_site_ShowObject(IOleClientSite* self)
{
    HTML_TRACE("client_site_ShowObject: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
client_site_OnShowWindow(IOleClientSite* self, BOOL show)
{
    HTML_TRACE("client_site_OnShowWindow(%d): Stub [E_NOTIMPL]", show);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
client_site_RequestNewObjectLayout(IOleClientSite* self)
{
    HTML_TRACE("client_site_RequestNewObjectLayout: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}


static IOleClientSiteVtbl client_site_vtable = {
    client_site_QueryInterface,
    DUMMY_ADDREF(IOleClientSite),
    DUMMY_RELEASE(IOleClientSite),
    client_site_SaveObject,
    client_site_GetMoniker,
    client_site_GetContainer,
    client_site_ShowObject,
    client_site_OnShowWindow,
    client_site_RequestNewObjectLayout
};



/****************************************
 *** IOleInPlaceSiteEx implementation ***
 ****************************************/

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_QueryInterface(IOleInPlaceSiteEx* self, REFIID riid, void** obj)
{
    return html_QueryInterface(MC_HTML_FROM_INPLACE_SITE_EX(self), riid, obj);
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_GetWindow(IOleInPlaceSiteEx* self, HWND* win)
{
    HTML_TRACE("inplace_site_GetWindow");
    *win = MC_HTML_FROM_INPLACE_SITE_EX(self)->win;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_ContextSensitiveHelp(IOleInPlaceSiteEx* self, BOOL mode)
{
    HTML_TRACE("inplace_site_context_sensitive_help: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_CanInPlaceActivate(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_CanInPlaceActivate: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnInPlaceActivate(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_OnInPlaceActivate: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnUIActivate(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_OnUIActivate: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_GetWindowContext(IOleInPlaceSiteEx* self, LPOLEINPLACEFRAME* frame,
                LPOLEINPLACEUIWINDOW* doc, RECT* rect, RECT* clip_rect,
                LPOLEINPLACEFRAMEINFO frame_info)
{
    html_t* html;
    HTML_TRACE("inplace_site_GetWindowContext");

    html = MC_HTML_FROM_INPLACE_SITE_EX(self);
    *frame = &html->inplace_frame;
    IOleInPlaceFrame_AddRef(*frame);
    *doc = NULL;
    frame_info->fMDIApp = FALSE;
    frame_info->hwndFrame = GetAncestor(html->win, GA_ROOT);

    frame_info->haccel = NULL;
    frame_info->cAccelEntries = 0;
    GetClientRect(html->win, rect);
    GetClientRect(html->win, clip_rect);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_Scroll(IOleInPlaceSiteEx* self, SIZE extent)
{
    HTML_TRACE("inplace_site_Scroll: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnUIDeactivate(IOleInPlaceSiteEx* self, BOOL undoable)
{
    HTML_TRACE("inplace_site_OnUIDeactivate: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnInPlaceDeactivate(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_OnInPlaceDeactivate: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_DiscardUndoState(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_DiscardUndoState: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_DeactivateAndUndo(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_DeactivateAndUndo: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnPosRectChange(IOleInPlaceSiteEx* self, const RECT* rect)
{
    IOleInPlaceObject* inplace;
    HRESULT hr;
    html_t* html;

    HTML_TRACE("inplace_site_OnPosRectChange");

    html = MC_HTML_FROM_INPLACE_SITE_EX(self);
    hr = IOleObject_QueryInterface(html->browser_obj,
                        &IID_IOleInPlaceObject, (void**)&inplace);
    if(MC_ERR(hr != S_OK  ||  inplace == NULL)) {
        MC_TRACE("inplace_site_ex_OnPosRectChange: "
                 "QueryInterface(IID_IOleInPlaceObject) failed [0x%lx]", hr);
        return E_UNEXPECTED;
    }

    IOleInPlaceObject_SetObjectRects(inplace, rect, rect);
    IOleInPlaceObject_Release(inplace);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnInPlaceActivateEx(IOleInPlaceSiteEx* self, BOOL* no_redraw,
                                    DWORD flags)
{
    HTML_TRACE("inplace_site_OnInPlaceActivateEx(): Stub [S_OK]");
    *no_redraw = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_OnInPlaceDeactivateEx(IOleInPlaceSiteEx* self, BOOL no_redraw)
{
    HTML_TRACE("inplace_site_OnInPlaceDeactivateEx(%d): Stub [S_OK]", no_redraw);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_site_ex_RequestUIActivate(IOleInPlaceSiteEx* self)
{
    HTML_TRACE("inplace_site_RequestUIActivate: Stub [S_OK]");
    return S_OK;
}

static IOleInPlaceSiteExVtbl inplace_site_ex_vtable = {
    inplace_site_ex_QueryInterface,
    DUMMY_ADDREF(IOleInPlaceSiteEx),
    DUMMY_RELEASE(IOleInPlaceSiteEx),
    inplace_site_ex_GetWindow,
    inplace_site_ex_ContextSensitiveHelp,
    inplace_site_ex_CanInPlaceActivate,
    inplace_site_ex_OnInPlaceActivate,
    inplace_site_ex_OnUIActivate,
    inplace_site_ex_GetWindowContext,
    inplace_site_ex_Scroll,
    inplace_site_ex_OnUIDeactivate,
    inplace_site_ex_OnInPlaceDeactivate,
    inplace_site_ex_DiscardUndoState,
    inplace_site_ex_DeactivateAndUndo,
    inplace_site_ex_OnPosRectChange,
    inplace_site_ex_OnInPlaceActivateEx,
    inplace_site_ex_OnInPlaceDeactivateEx,
    inplace_site_ex_RequestUIActivate
};


/***************************************
 *** IOleInPlaceFrame implementation ***
 ***************************************/

static HRESULT STDMETHODCALLTYPE
inplace_frame_QueryInterface(IOleInPlaceFrame* self, REFIID riid, void** obj)
{
    return html_QueryInterface(MC_HTML_FROM_INPLACE_FRAME(self), riid, obj);
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_GetWindow(IOleInPlaceFrame* self, HWND* win)
{
    HTML_TRACE("inplace_frame_GetWindow");
    *win = GetAncestor(MC_HTML_FROM_INPLACE_FRAME(self)->win, GA_ROOT);
    return(S_OK);
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_ContextSensitiveHelp(IOleInPlaceFrame* self, BOOL mode)
{
    HTML_TRACE("inplace_frame_ContextSensitiveHelp: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_GetBorder(IOleInPlaceFrame* self, RECT* rect)
{
    HTML_TRACE("inplace_frame_GetBorder: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_RequestBorderSpace(IOleInPlaceFrame* self, LPCBORDERWIDTHS widths)
{
    HTML_TRACE("inplace_frame_RequestBorderSpace: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_SetBorderSpace(IOleInPlaceFrame* self, LPCBORDERWIDTHS space)
{
    HTML_TRACE("inplace_frame_SetBorderSpace: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_SetActiveObject(IOleInPlaceFrame* self,
            IOleInPlaceActiveObject *active_obj, LPCOLESTR obj_name)
{
    HTML_TRACE("inplace_frame_SetActiveObject: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_InsertMenus(IOleInPlaceFrame* self, HMENU menu_shared,
            LPOLEMENUGROUPWIDTHS menu_widths)
{
    HTML_TRACE("inplace_frame_InsertMenus: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_SetMenu(IOleInPlaceFrame* self, HMENU menu_shared,
            HOLEMENU old_menu, HWND active_obj)
{
    HTML_TRACE("inplace_frame_SetMenu: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_RemoveMenus(IOleInPlaceFrame* self, HMENU menu_shared)
{
    HTML_TRACE("inplace_frame_RemoveMenus: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_SetStatusText(IOleInPlaceFrame* self, LPCOLESTR status_text)
{
    HTML_TRACE("inplace_frame_SetStatusText: Stub [S_OK]: '%ls'", status_text);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_EnableModeless(IOleInPlaceFrame* self, BOOL enable)
{
    HTML_TRACE("inplace_frame_EnableModeless(%d): Stub [S_OK]", enable);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
inplace_frame_TranslateAccelerator(IOleInPlaceFrame* self, MSG* msg, WORD id)
{
    HTML_TRACE("inplace_frame_TranslateAccelerator: Stub [E_NOTIMPL]");
    return E_NOTIMPL;
}


static IOleInPlaceFrameVtbl inplace_frame_vtable = {
    inplace_frame_QueryInterface,
    DUMMY_ADDREF(IOleInPlaceFrame),
    DUMMY_RELEASE(IOleInPlaceFrame),
    inplace_frame_GetWindow,
    inplace_frame_ContextSensitiveHelp,
    inplace_frame_GetBorder,
    inplace_frame_RequestBorderSpace,
    inplace_frame_SetBorderSpace,
    inplace_frame_SetActiveObject,
    inplace_frame_InsertMenus,
    inplace_frame_SetMenu,
    inplace_frame_RemoveMenus,
    inplace_frame_SetStatusText,
    inplace_frame_EnableModeless,
    inplace_frame_TranslateAccelerator
};



/****************************************
 *** IDocHostUIHandler implementation ***
 ****************************************/

static HRESULT STDMETHODCALLTYPE
ui_handler_QueryInterface(IDocHostUIHandler* self, REFIID riid, void** obj)
{
    return html_QueryInterface(MC_HTML_FROM_UI_HANDLER(self), riid, obj);
}

static HRESULT STDMETHODCALLTYPE
ui_handler_ShowContextMenu(IDocHostUIHandler* self, DWORD menu_id, POINT* pos,
                IUnknown* reserved1, IDispatch* reserved2)
{
    html_t* html = MC_HTML_FROM_UI_HANDLER(self);
    if(html->style & MC_HS_NOCONTEXTMENU)
        return S_OK;
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_GetHostInfo(IDocHostUIHandler* self, DOCHOSTUIINFO* info)
{
    if(info == NULL || info->cbSize < sizeof(DOCHOSTUIINFO)) {
        HTML_TRACE("ui_handler_GetHostInfo: info->cbSize == %lu "
                   "[E_UNEXPECTED]", info->cbSize);
        return E_UNEXPECTED;
    }

    /* Host window is responsible for outer border (the host window may use
     * WS_BORDER and/or WS_EX_CLIENTEDGE) */
    info->dwFlags |= DOCHOSTUIFLAG_NO3DOUTERBORDER;

    /* Check whether the controls on HTML page should use XP theming. */
    if(mcIsThemeActive()) {
        info->dwFlags &= ~DOCHOSTUIFLAG_NOTHEME;
        info->dwFlags |= DOCHOSTUIFLAG_THEME;
    } else {
        info->dwFlags |= DOCHOSTUIFLAG_NOTHEME;
        info->dwFlags &= ~DOCHOSTUIFLAG_THEME;
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_ShowUI(IDocHostUIHandler* self, DWORD id, IOleInPlaceActiveObject* active_obj,
                IOleCommandTarget* target, IOleInPlaceFrame* inplace_frame,
                IOleInPlaceUIWindow* doc)
{
    HTML_TRACE("ui_handler_ShowUI: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_HideUI(IDocHostUIHandler* self)
{
    HTML_TRACE("ui_handler_HideUI: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_UpdateUI(IDocHostUIHandler* self)
{
    HTML_TRACE("ui_handler_UpdateUI: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_EnableModeless(IDocHostUIHandler* self, BOOL enable)
{
    HTML_TRACE("ui_handler_EnableModeless(%d): Stub [S_OK]", enable);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_OnDocWindowActivate(IDocHostUIHandler* self, BOOL active)
{
    HTML_TRACE("ui_handler_OnDocWindowActivate(%d): Stub [S_OK]", active);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_OnFrameWindowActivate(IDocHostUIHandler* self, BOOL active)
{
    HTML_TRACE("ui_handler_OnFrameWindowActivate(%d): Stub [S_OK]", active);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_ResizeBorder(IDocHostUIHandler* self, const RECT* rect,
                IOleInPlaceUIWindow* inplace_win, BOOL is_frame_win)
{
    HTML_TRACE("ui_handler_ResizeBorder: Stub [S_OK]");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_TranslateAccelerator(IDocHostUIHandler* self, MSG* msg,
            const GUID* guid, DWORD cmd_id)
{
    HTML_TRACE("ui_handler_TranslateAccelerator: Stub [S_FALSE]");
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_GetOptionKeyPath(IDocHostUIHandler* self, LPOLESTR* key, DWORD reserved)
{
    HTML_TRACE("ui_handler_GetOptionKeyPath: Stub [E_NOTIMPL]");
    *key = NULL;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_GetDropTarget(IDocHostUIHandler* self, IDropTarget* drop_target,
            IDropTarget** p_drop_target)
{
    HTML_TRACE("ui_handler_GetDropTarget: Stub [E_NOTIMPL]");
    *p_drop_target = NULL;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_GetExternal(IDocHostUIHandler* self, IDispatch** p_dispatch)
{
    HTML_TRACE("ui_handler_GetExternal: Stub [E_NOTIMPL]");
    *p_dispatch = NULL;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_TranslateUrl(IDocHostUIHandler* self, DWORD reserved,
                        OLECHAR* url, OLECHAR** p_url)
{
    HTML_TRACE("ui_handler_TranslateUrl('%S'): Stub [S_FALSE]", url);
    *p_url = NULL;
    return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
ui_handler_FilterDataObject(IDocHostUIHandler* self, IDataObject* obj,
            IDataObject** p_obj)
{
    HTML_TRACE("ui_handler_FilterDataObject: Stub [S_FALSE]");
    *p_obj = NULL;
    return S_FALSE;
}


static IDocHostUIHandlerVtbl ui_handler_vtable = {
    ui_handler_QueryInterface,
    DUMMY_ADDREF(IDocHostUIHandler),
    DUMMY_RELEASE(IDocHostUIHandler),
    ui_handler_ShowContextMenu,
    ui_handler_GetHostInfo,
    ui_handler_ShowUI,
    ui_handler_HideUI,
    ui_handler_UpdateUI,
    ui_handler_EnableModeless,
    ui_handler_OnDocWindowActivate,
    ui_handler_OnFrameWindowActivate,
    ui_handler_ResizeBorder,
    ui_handler_TranslateAccelerator,
    ui_handler_GetOptionKeyPath,
    ui_handler_GetDropTarget,
    ui_handler_GetExternal,
    ui_handler_TranslateUrl,
    ui_handler_FilterDataObject
};



/**********************************
 *** Host window implementation ***
 **********************************/

static BSTR
html_bstr(const void* from_str, int from_type)
{
    WCHAR* str_w;
    BSTR str_b;

    if(from_str == NULL) {
        /* According to MSDN, BSTR should never be NULL. */
        from_str = L"";
        from_type = MC_STRW;
    }

    if(from_type == MC_STRW) {
        str_w = (WCHAR*) from_str;
    } else {
        char* str_a;
        MC_ASSERT(from_type == MC_STRA);
        str_a = (char*) from_str;
        str_w = (WCHAR*) mc_str(str_a, from_type, MC_STRW);
        if(MC_ERR(str_w == NULL)) {
            MC_TRACE("html_bstr: mc_str() failed.");
            return NULL;
        }
    }

    str_b = SysAllocString(str_w);
    if(MC_ERR(str_b == NULL))
        MC_TRACE("html_bstr: SysAllocString() failed.");

    if(str_w != from_str)
        free(str_w);

    return str_b;
}

static LRESULT
html_notify_http_error(html_t* html, int http_status, const WCHAR* url)
{
    MC_NMHTTPERRORW notify;
    LRESULT res;
    BOOL need_free = FALSE;

    HTML_TRACE("html_notify_http_error: status=%d url='%S'",
               http_status, url ? url : L"[null]");

    notify.hdr.hwndFrom = html->win;
    notify.hdr.idFrom = GetDlgCtrlID(html->win);
    notify.hdr.code = MC_HN_HTTPERROR;
    if(html->unicode_notifications) {
        notify.pszUrl = url;
    } else {
        notify.pszUrl = (WCHAR*) mc_str(url, MC_STRW, MC_STRA);
        need_free = TRUE;
    }
    notify.iStatus = http_status;

    res = MC_SEND(html->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify);

    if(need_free)
        free((WCHAR*)notify.pszUrl);

    return res;
}

static LRESULT
html_notify_text(html_t* html, UINT code, const WCHAR* text)
{
    /* Note we shamelessly misuse this also for URL notifications, as the
     * MC_NMHTMLURL and MC_NMHTMLTEXT are binary compatible.
     * They are separate mainly for historical reasons.
     */

    MC_NMHTMLTEXTW notify;
    LRESULT res;
    BOOL need_free = FALSE;

    HTML_TRACE("html_notify_text: code=%d str='%S'", code, text ? text : L"[null]");

    notify.hdr.hwndFrom = html->win;
    notify.hdr.idFrom = GetDlgCtrlID(html->win);
    notify.hdr.code = code;
    if(text == NULL) {
        notify.pszText = L"";
    } else if(html->unicode_notifications) {
        notify.pszText = text;
    } else {
        notify.pszText = (WCHAR*) mc_str(text, MC_STRW, MC_STRA);
        need_free = TRUE;
    }

    res = MC_SEND(html->notify_win, WM_NOTIFY, notify.hdr.idFrom, &notify);

    if(need_free)
        free((WCHAR*)notify.pszText);

    return res;
}

static void
html_notify_format(html_t* html)
{
    LRESULT lres;
    lres = MC_SEND(html->notify_win, WM_NOTIFYFORMAT, html->win, NF_QUERY);
    html->unicode_notifications = (lres == NFR_UNICODE ? 1 : 0);
    HTML_TRACE("html_notify_format: Will use %s notifications.",
               html->unicode_notifications ? "Unicode" : "ANSI");
}

static int
html_goto_url(html_t* html, const void* url, BOOL unicode)
{
    IWebBrowser2* browser_iface;
    HRESULT hr;
    VARIANT var;

    hr = IOleObject_QueryInterface(html->browser_obj,
                    &IID_IWebBrowser2, (void**)&browser_iface);
    if(MC_ERR(hr != S_OK  ||  browser_iface == NULL)) {
        MC_TRACE("html_goto_url: QueryInterface(IID_IWebBrowser2) failed [0x%lx]", hr);
        return -1;
    }

    V_VT(&var) = VT_BSTR;

    if(url != NULL  &&  ((unicode && ((WCHAR*)url)[0] != L'\0') ||
                         (!unicode && ((char*)url)[0] != '\0'))) {
        V_BSTR(&var) = html_bstr(url, (unicode ? MC_STRW : MC_STRA));
        if(MC_ERR(var.bstrVal == NULL)) {
            MC_TRACE("html_goto_url: html_bstr() failed.");
            mc_send_notify(html->notify_win, html->win, NM_OUTOFMEMORY);
            return -1;
        }
    } else {
        V_BSTR(&var) = url_blank;
    }

    IWebBrowser2_Navigate2(browser_iface, &var, NULL, NULL, NULL, NULL);
    IWebBrowser2_Release(browser_iface);

    if(V_BSTR(&var) != url_blank)
        SysFreeString(var.bstrVal);

    return 0;
}

static int
html_goto_back(html_t* html, BOOL back)
{
    IWebBrowser2* browser_iface;
    HRESULT hr;

    hr = IOleObject_QueryInterface(html->browser_obj,
                    &IID_IWebBrowser2, (void**)&browser_iface);
    if(MC_ERR(hr != S_OK  ||  browser_iface == NULL)) {
        MC_TRACE("html_goto_back: "
                 "QueryInterface(IID_IWebBrowser2) failed [0x%lx]", hr);
        return -1;
    }

    if(back)
        hr = IWebBrowser2_GoBack(browser_iface);
    else
        hr = IWebBrowser2_GoForward(browser_iface);
    IWebBrowser2_Release(browser_iface);
    return (SUCCEEDED(hr)  ?  0  :  -1);
}

static int
html_set_element_contents(html_t* html, const void* id, const void* contents,
                          BOOL unicode)
{
    BSTR bstr_id;
    BSTR bstr_contents;
    IWebBrowser2* browser_iface;
    IDispatch* dispatch_iface;
    IHTMLDocument3* doc_iface;
    IHTMLElement* elem_iface;
    HRESULT hr;
    int res = -1;

    if(MC_ERR(id == NULL  ||  (unicode && ((WCHAR*)id)[0] == L'\0')  ||
                              (!unicode && ((char*)id)[0] == '\0'))) {
        MC_TRACE("html_set_element_contents: Empty element ID.");
        SetLastError(ERROR_INVALID_PARAMETER);
        goto err_id;
    }
    bstr_id = html_bstr(id, (unicode ? MC_STRW : MC_STRA));
    if(MC_ERR(bstr_id == NULL)) {
        MC_TRACE("html_set_element_contents: html_bstr(id) failed.");
        mc_send_notify(html->notify_win, html->win, NM_OUTOFMEMORY);
        goto err_id;
    }

    bstr_contents = html_bstr(contents, (unicode ? MC_STRW : MC_STRA));
    if(MC_ERR(bstr_contents == NULL)) {
        MC_TRACE("html_set_element_contents: html_bstr(contents) failed");
        mc_send_notify(html->notify_win, html->win, NM_OUTOFMEMORY);
        goto err_contents;
    }

    hr = IOleObject_QueryInterface(html->browser_obj,
                    &IID_IWebBrowser2, (void**)&browser_iface);
    if(MC_ERR(hr != S_OK  ||  browser_iface == NULL)) {
        MC_TRACE("html_set_element_contents: "
                 "QueryInterface(IID_IWebBrowser2) failed [0x%lx]", hr);
        goto err_browser;
    }

    hr = IWebBrowser2_get_Document(browser_iface, &dispatch_iface);
    if(MC_ERR(FAILED(hr)  ||  dispatch_iface == NULL)) {
        MC_TRACE("html_set_element_contents: get_Document() failed [0x%lx]", hr);
        goto err_dispatch;
    }

    hr = IDispatch_QueryInterface(dispatch_iface,
                                  &IID_IHTMLDocument3, (void**)&doc_iface);
    if(MC_ERR(hr != S_OK  ||  doc_iface == NULL)) {
        MC_TRACE("html_set_element_contents: "
                 "QueryInterface(IID_IHTMLDocument3) failed [0x%lx]", hr);
        goto err_doc;
    }

    hr = IHTMLDocument3_getElementById(doc_iface, bstr_id, &elem_iface);
    if(MC_ERR(FAILED(hr)  ||  elem_iface == NULL)) {
        MC_TRACE("html_set_element_contents: getElementById() failed [0x%lx]", hr);
        goto err_elem;
    }

    hr = IHTMLElement_put_innerHTML(elem_iface, bstr_contents);
    if(MC_ERR(hr != S_OK)) {
        MC_TRACE("html_set_element_contents: put_innerHTML() failed [0x%lx]", hr);
        goto err_inner_html;
    }

    res = 0;

err_inner_html:
    IHTMLElement_Release(elem_iface);
err_elem:
    IHTMLDocument3_Release(doc_iface);
err_doc:
    IDispatch_Release(dispatch_iface);
err_dispatch:
    IWebBrowser2_Release(browser_iface);
err_browser:
    SysFreeString(bstr_contents);
err_contents:
    SysFreeString(bstr_id);
err_id:
    return res;
}

/*PSIPHON*/
static int
html_call_script_fn(html_t* html, const void* vArgStruct, const void* vResultBuf,
BOOL unicode)
{
    MC_HMCALLSCRIPTFN* argStruct;
    char* resultBufA;
    wchar_t* resultBufW;
    BSTR bstr_fnname;
    BSTR bstr_arguments;
    VARIANTARG va_arguments;
    IWebBrowser2* browser_iface;
    IDispatch* dispatch_iface;
    IHTMLDocument2* doc_iface;
    IDispatch* script_dispatch_iface;
    DISPID fnDispID;
    DISPPARAMS dispParams = { 0 };
    VARIANT result;
    HRESULT hr;
    int res = -1;

    argStruct = (MC_HMCALLSCRIPTFN*)vArgStruct;
    if (argStruct == NULL)
    {
        MC_TRACE("html_call_script_fn: Empty arg struct.");
        SetLastError(ERROR_INVALID_PARAMETER);
        goto err_argstruct;
    }

    if (unicode) {
        resultBufW = (wchar_t*)vResultBuf;
    }
    else {
        resultBufA = (char*)vResultBuf;
    }

    if (MC_ERR(argStruct->pszFnName == NULL || (unicode && ((WCHAR*)argStruct->pszFnName)[0] == L'\0') ||
        (!unicode && ((char*)argStruct->pszFnName)[0] == '\0'))) {
        MC_TRACE("html_call_script_fn: Empty function name.");
        SetLastError(ERROR_INVALID_PARAMETER);
        goto err_fnname;
    }
    bstr_fnname = html_bstr(argStruct->pszFnName, (unicode ? MC_STRW : MC_STRA));
    if (MC_ERR(bstr_fnname == NULL)) {
        MC_TRACE("html_call_script_fn: html_bstr(fnname) failed.");
        mc_send_notify(html->notify_win, html->win, NM_OUTOFMEMORY);
        goto err_fnname;
    }

    if (argStruct->pszArguments == NULL) {
        bstr_arguments = NULL;
        V_VT(&va_arguments) = VT_EMPTY;
    }
    else {
        bstr_arguments = html_bstr(argStruct->pszArguments, (unicode ? MC_STRW : MC_STRA));
        if (MC_ERR(bstr_arguments == NULL)) {
            MC_TRACE("html_call_script_fn: html_bstr(arguments) failed");
            mc_send_notify(html->notify_win, html->win, NM_OUTOFMEMORY);
            goto err_arguments;
        }
        V_VT(&va_arguments) = VT_BSTR;
        V_BSTR(&va_arguments) = bstr_arguments;
    }

    hr = IOleObject_QueryInterface(html->browser_obj,
        &IID_IWebBrowser2, (void**)&browser_iface);
    if (MC_ERR(hr != S_OK || browser_iface == NULL)) {
        MC_TRACE("html_call_script_fn: "
            "QueryInterface(IID_IWebBrowser2) failed [0x%lx]", hr);
        goto err_browser;
    }

    hr = IWebBrowser2_get_Document(browser_iface, &dispatch_iface);
    if (MC_ERR(FAILED(hr) || dispatch_iface == NULL)) {
        MC_TRACE("html_call_script_fn: get_Document() failed [0x%lx]", hr);
        goto err_dispatch;
    }

    hr = IDispatch_QueryInterface(dispatch_iface,
        &IID_IHTMLDocument2, (void**)&doc_iface);
    if (MC_ERR(hr != S_OK || doc_iface == NULL)) {
        MC_TRACE("html_call_script_fn: "
            "QueryInterface(IID_IHTMLDocument2) failed [0x%lx]", hr);
        goto err_doc;
    }

    hr = IHTMLDocument2_get_Script(doc_iface, &script_dispatch_iface);
    if (MC_ERR(FAILED(hr) || dispatch_iface == NULL)) {
        MC_TRACE("html_call_script_fn: get_Script() failed [0x%lx]", hr);
        goto err_script_dispatch;
    }

    hr = IDispatch_GetIDsOfNames(script_dispatch_iface,
        &IID_NULL, &bstr_fnname, 1, LOCALE_SYSTEM_DEFAULT, &fnDispID);
    if (MC_ERR(FAILED(hr) || fnDispID < 0)) {
        MC_TRACE("html_call_script_fn: GetIDsOfNames() failed [0x%lx]", hr);
        goto err_script_dispatch;
    }

    dispParams.rgvarg = &va_arguments;
    dispParams.cArgs = 1;
    V_VT(&result) = VT_EMPTY;
    hr = IDispatch_Invoke(script_dispatch_iface,
        fnDispID, &IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, 
        &dispParams, &result, NULL, NULL);
    if (MC_ERR(FAILED(hr))) {
        MC_TRACE("html_call_script_fn: IDispatch_Invoke() failed [0x%lx]", hr);
        goto err_script_invoke;
    }

    /* We only handle BSTR results. */
    if (V_VT(&result) == VT_BSTR && argStruct->iResultBufCharCount > 0) {
        if (SysStringLen(V_BSTR(&result)) >= argStruct->iResultBufCharCount) {
            MC_TRACE("html_call_script_fn: result buffer of size %d is too small for result of size %d", argStruct->iResultBufCharCount, SysStringLen(V_BSTR(&result)));
            res = ERROR_INSUFFICIENT_BUFFER;
            goto err_result_copy;
        }
        mc_str_inbuf(
            V_BSTR(&result), MC_STRW,
            unicode ? (void*)resultBufW : (void*)resultBufA,
            unicode ? MC_STRW : MC_STRA,
            argStruct->iResultBufCharCount);
    }
    else if (argStruct->iResultBufCharCount > 0)
    {
        mc_str_inbuf(
            L"\0", MC_STRW,
            unicode ? (void*)resultBufW : (void*)resultBufA,
            unicode ? MC_STRW : MC_STRA,
            1);
    }

    res = 0;

err_result_copy:
    if (V_VT(&result) == VT_BSTR) {
        SysFreeString(V_BSTR(&result));
    }
    else {
        VariantClear(&result);
    }
err_script_invoke:
    IDispatch_Release(script_dispatch_iface);
err_script_dispatch:
    IHTMLDocument3_Release(doc_iface);
err_doc:
    IDispatch_Release(dispatch_iface);
err_dispatch:
    IWebBrowser2_Release(browser_iface);
err_browser:
    SysFreeString(bstr_arguments);
err_arguments:
    SysFreeString(bstr_fnname);
err_fnname:
err_argstruct:
    return res;
}
/*/PSIPHON*/

static BOOL
html_key_msg(html_t* html, UINT msg, WPARAM wp, LPARAM lp)
{
    /*PSIPHON*/
    /* Cheap hack to disable hotkeys, like F5 (refresh) and Alt+Left (back).
       TODO: Do this better. */
    return FALSE;
    /*/PSIPHON*/

    DWORD pos;
    MSG message;
    IWebBrowser2* browser_iface;
    IOleInPlaceActiveObject* active_iface;
    HRESULT hr;
    BOOL ret = FALSE;

    pos = GetMessagePos();

    /* Setup the message structure */
    message.hwnd = html->ie_win;
    message.message = msg;
    message.wParam = wp;
    message.lParam = lp;
    message.time = GetMessageTime();
    message.pt.x = GET_X_LPARAM(pos);
    message.pt.y = GET_Y_LPARAM(pos);

    /* ->TranslateAccelerator() */
    hr = IOleObject_QueryInterface(html->browser_obj,
                    &IID_IWebBrowser2, (void**)&browser_iface);
    if(MC_ERR(hr != S_OK  ||  browser_iface == NULL)) {
        MC_TRACE("html_key_msg: "
                 "QueryInterface(IID_IWebBrowser2) failed [0x%lx]", hr);
        goto err_browser;
    }

    hr = IWebBrowser2_QueryInterface(browser_iface,
                        &IID_IOleInPlaceActiveObject, (void**)&active_iface);
    if(MC_ERR(hr != S_OK  ||  active_iface == NULL)) {
        MC_TRACE("html_key_msg: "
                 "QueryInterface(IID_IOleInPlaceActiveObject) failed [0x%lx]", hr);
        goto err_active;
    }

    hr = IOleInPlaceActiveObject_TranslateAccelerator(active_iface, &message);
    switch(hr) {
        case S_OK:    ret = TRUE; break;
        case S_FALSE: /*noop */ break;
        default:
            MC_TRACE("html_key_msg: ->TranslateAccelerator() failed [0x%lx]", hr);
            break;
    }

    /* Cleanup */
    IOleInPlaceActiveObject_Release(active_iface);
err_active:
    IWebBrowser2_Release(browser_iface);
err_browser:
    return ret;
}

static html_t*
html_nccreate(HWND win, CREATESTRUCT* cs)
{
    html_t* html = NULL;

    /* Allocate and setup the html_t structure */
    html = (html_t*) malloc(sizeof(html_t));
    if(MC_ERR(html == NULL)) {
        MC_TRACE("html_nccreate: malloc() failed.");
        return NULL;
    }
    memset(html, 0, sizeof(html_t));

    html->win = win;
    html->notify_win = cs->hwndParent;
    html->style = cs->style;
    html->dispatch.lpVtbl = &dispatch_vtable;
    html->client_site.lpVtbl = &client_site_vtable;
    html->inplace_site_ex.lpVtbl = &inplace_site_ex_vtable;
    html->inplace_frame.lpVtbl = &inplace_frame_vtable;
    html->ui_handler.lpVtbl = &ui_handler_vtable;

    /* Ask parent if it expects Unicode or ANSI notifications */
    html_notify_format(html);

    return html;
}

static int
html_create(html_t* html, CREATESTRUCT* cs)
{
    IWebBrowser2* browser_iface = NULL;
    IConnectionPointContainer* conn_point_container;
    IConnectionPoint* conn_point;
    DWORD cookie;
    RECT rect;
    HRESULT hr;

    /* Create browser object */
    html->browser_obj = (IOleObject*) xcom_init_create(&CLSID_WebBrowser,
                                    CLSCTX_INPROC, &IID_IOleObject);
    if(MC_ERR(html->browser_obj == NULL)) {
        MC_TRACE("html_create: xcom_init_create(CLSID_WebBrowser) failed.");
        return -1;
    }

    /* Embed the browser object into our host window */
    hr = IOleObject_SetClientSite(html->browser_obj, &html->client_site);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("html_create: IOleObject::SetClientSite() failed [0x%lx]", hr);
        return -1;
    }
    GetClientRect(html->win, &rect);
    hr = IOleObject_DoVerb(html->browser_obj, OLEIVERB_INPLACEACTIVATE,
                            NULL, &html->client_site, 0, html->win, &rect);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("html_create: IOleObject::DoVerb(OLEIVERB_INPLACEACTIVATE) "
                 "failed [0x%lx]", hr);
        return -1;
    }

    /* Send events of DIID_DWebBrowserEvents2 to our IDispatch */
    hr = IOleObject_QueryInterface(html->browser_obj,
                &IID_IConnectionPointContainer, (void**)&conn_point_container);
    if(MC_ERR(hr != S_OK  ||  conn_point_container == NULL)) {
        MC_TRACE("html_create: QueryInterface(IID_IConnectionPointContainer) failed "
                 "[0x%lx]", hr);
        return -1;
    }
    hr = IConnectionPointContainer_FindConnectionPoint(conn_point_container,
                &DIID_DWebBrowserEvents2, &conn_point);
    IConnectionPointContainer_Release(conn_point_container);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE("html_create: FindConnectionPoint(DIID_DWebBrowserEvents2) failed "
                 "[0x%lx]", hr);
        return -1;
    }
    IConnectionPoint_Advise(conn_point, (IUnknown*)&html->client_site, &cookie);
    IConnectionPoint_Release(conn_point);

    /* Set browser position and size according to the host window */
    hr = IOleObject_QueryInterface(html->browser_obj,
                &IID_IWebBrowser2, (void**)&browser_iface);
    if(MC_ERR(hr != S_OK  ||  browser_iface == NULL)) {
        MC_TRACE("html_create: QueryInterface(IID_IWebBrowser2) failed "
                 "[0x%lx]", hr);
        return -1;
    }
    IWebBrowser2_put_Left(browser_iface, 0);
    IWebBrowser2_put_Top(browser_iface, 0);
#if 0  /* these are set in WM_SIZE handler */
    IWebBrowser2_put_Width(browser_iface, MC_WIDTH(&rect));
    IWebBrowser2_put_Height(browser_iface, MC_HEIGHT(&rect));
#endif
    IWebBrowser2_Release(browser_iface);

    /* Goto specified URL if any */
    if(cs->lpszName != NULL && cs->lpszName[0] != _T('\0'))
        html_goto_url(html, cs->lpszName, MC_IS_UNICODE);

    return 0;
}

static void
html_destroy(html_t* html)
{
    /* Unsubclass IE window */
    if(html->ie_win != NULL) {
        SetWindowLongPtr(html->ie_win, GWLP_WNDPROC, (LONG_PTR)html->ie_proc);
        RemoveProp(html->ie_win, ie_prop);
        html->ie_win = NULL;
    }

    if(html->browser_obj != NULL) {
        IOleObject_Close(html->browser_obj, OLECLOSE_NOSAVE);
        IOleObject_Release(html->browser_obj);
        html->browser_obj = NULL;
        xcom_uninit();
    }
}

static inline void
html_ncdestroy(html_t* html)
{
    free(html);
}


static LRESULT CALLBACK
html_ie_subclass_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    html_t* html;
    LRESULT ret;

    html = (html_t*) GetProp(win, ie_prop);

    if(WM_KEYFIRST <= msg && msg <= WM_KEYLAST) {
        if(html_key_msg(html, msg, wp, lp))
            return 0;
    }

    if(msg == WM_GETDLGCODE)
        return DLGC_WANTALLKEYS;

    ret = CallWindowProc(html->ie_proc, win, msg, wp, lp);

    if(msg == WM_DESTROY) {
        SetWindowLongPtr(win, GWLP_WNDPROC, (LONG_PTR)html->ie_proc);
        RemoveProp(win, ie_prop);
        html->ie_win = NULL;
    }

    return ret;
}

static HWND
html_find_ie_window(HWND win)
{
    static const TCHAR ie_wc[] = _T("Internet Explorer_Server");
    HWND w;

    w = FindWindowEx(win, NULL, ie_wc, NULL);
    if(w != NULL)
        return w;

    win = GetWindow(win, GW_CHILD);
    while(win != NULL) {
        w = html_find_ie_window(win);
        if(w != NULL)
            return w;

        win = GetWindow(win, GW_HWNDNEXT);
    }

    return NULL;
}


static LRESULT CALLBACK
html_proc(HWND win, UINT msg, WPARAM wp, LPARAM lp)
{
    html_t* html = (html_t*) GetWindowLongPtr(win, 0);

    if(html != NULL  &&  html->ie_win == NULL) {
        /* Let's try to subclass IE window. This is very dirty hack,
         * which allows us to forward keyboard messages properly to
         * IOleInPlaceActiveObject::TranslateAccelerator().
         *
         * Normally this should be done from main app. loop but we do not
         * have it under control in the DLL. */
        html->ie_win = html_find_ie_window(win);
        if(html->ie_win != NULL) {
            HTML_TRACE("html_proc: Subclassing MSIE.");
            html->ie_proc = (WNDPROC) SetWindowLongPtr(html->ie_win,
                            GWLP_WNDPROC, (LONG_PTR) html_ie_subclass_proc);
            SetProp(html->ie_win, ie_prop, (HANDLE) html);

            if(GetFocus() == win) {
                SetFocus(html->ie_win);
                MC_SEND(html->ie_win, WM_LBUTTONDOWN, 0, 0);
                MC_SEND(html->ie_win, WM_LBUTTONUP, 0, 0);
            }
        }
    }

    switch(msg) {
        case MC_HM_GOTOURLW:
        case MC_HM_GOTOURLA:
        {
            int res = html_goto_url(html, (const void*)lp, (msg == MC_HM_GOTOURLW));
            return (res == 0 ? TRUE : FALSE);
        }

        case MC_HM_SETTAGCONTENTSW:
        case MC_HM_SETTAGCONTENTSA:
        {
            int res = html_set_element_contents(html, (void*)wp, (void*)lp,
                                                (msg == MC_HM_SETTAGCONTENTSW));
            return (res == 0 ? TRUE : FALSE);
        }

        /*PSIPHON*/
        case MC_HM_CALLSCRIPTFNW:
        case MC_HM_CALLSCRIPTFNA:
        {
            int res = html_call_script_fn(html, (void*)wp, (void*)lp,
                (msg == MC_HM_CALLSCRIPTFNW));
            return res;
        }
        /*/PSIPHON*/

        case MC_HM_GOBACK:
        {
            int res = html_goto_back(html, wp);
            return (res == 0 ? TRUE : FALSE);
        }

        case MC_HM_CANBACK:
            return ((wp ? html->can_back : html->can_forward) ? TRUE : FALSE);

        case WM_SIZE:
        {
            IWebBrowser2* browser_iface;
            HRESULT hr;

            hr = IOleObject_QueryInterface(html->browser_obj,
                        &IID_IWebBrowser2, (void**)&browser_iface);
            if(hr == S_OK  &&  browser_iface != NULL) {
                IWebBrowser2_put_Width(browser_iface, LOWORD(lp));
                IWebBrowser2_put_Height(browser_iface, HIWORD(lp));
                IWebBrowser2_Release(browser_iface);
            }
            return 0;
        }

        case WM_STYLECHANGED:
            if(wp == GWL_STYLE) {
                STYLESTRUCT* ss = (STYLESTRUCT*) lp;
                html->style = ss->styleNew;
                RedrawWindow(win, NULL, NULL,
                             RDW_INVALIDATE | RDW_FRAME | RDW_ERASE | RDW_ALLCHILDREN);
            }
            break;

        case WM_NOTIFYFORMAT:
            switch(lp) {
                case NF_REQUERY:
                    html_notify_format(html);
                    return (html->unicode_notifications ? NFR_UNICODE : NFR_ANSI);
                case NF_QUERY:
                    return (MC_IS_UNICODE ? NFR_UNICODE : NFR_ANSI);
            }
            break;

        case CCM_SETUNICODEFORMAT:
        {
            BOOL tmp = html->unicode_notifications;
            html->unicode_notifications = (wp != 0);
            return tmp;
        }

        case CCM_GETUNICODEFORMAT:
            return html->unicode_notifications;

        case CCM_SETNOTIFYWINDOW:
        {
            HWND old = html->notify_win;
            html->notify_win = (wp ? (HWND) wp : GetAncestor(win, GA_PARENT));
            return (LPARAM) old;
        }

        case WM_SETFOCUS:
            if(html->ie_win) {
                SetFocus(html->ie_win);
                MC_SEND(html->ie_win, WM_LBUTTONDOWN, 0, 0);
                MC_SEND(html->ie_win, WM_LBUTTONUP, 0, 0);
            }
            return 0;

        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;

        case WM_SETTEXT:
            return FALSE;

        case WM_GETTEXT:
            if(wp > 0)
                ((TCHAR*)lp)[0] = _T('\0');
            return 0;

        case WM_GETTEXTLENGTH:
            return 0;

        case WM_NCCREATE:
            html = html_nccreate(win, (CREATESTRUCT*)lp);
            if(MC_ERR(html == NULL))
                return FALSE;
            SetWindowLongPtr(win, 0, (LONG_PTR)html);
            return TRUE;

        case WM_CREATE:
            return (html_create(html, (CREATESTRUCT*)lp) == 0 ? 0 : -1);

        case WM_DESTROY:
            html_destroy(html);
            return 0;

        case WM_NCDESTROY:
            if(html)
                html_ncdestroy(html);
            return 0;
    }

    /* Forward keystrokes to the IE */
    if(WM_KEYFIRST <= msg  &&  msg <= WM_KEYLAST) {
        if(html->ie_win)
            MC_SEND(html->ie_win, msg, wp, lp);
        return 0;
    }

    return DefWindowProc(win, msg, wp, lp);
}


int
html_init_module(void)
{
    WNDCLASS wc = { 0 };

    /* Register window class */
    mc_init_common_controls(ICC_STANDARD_CLASSES);
    wc.style = CS_GLOBALCLASS | CS_PARENTDC;
    wc.lpfnWndProc = html_proc;
    wc.cbWndExtra = sizeof(html_t*);
    wc.lpszClassName = html_wc;
    if(MC_ERR(!RegisterClass(&wc))) {
        MC_TRACE_ERR("html_init_module: RegisterClass() failed");
        return -1;
    }

    /* Success */
    return 0;
}

void
html_fini_module(void)
{
    UnregisterClass(html_wc, NULL);
}

