/*
 * Copyright (c) 2015 Martin Mitas
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

#include "xcom.h"


/* Who is responsible for COM initialization? Note it is "almost" constant
 * during process lifetime. It only changes once from UNKNOWN to either APP
 * or MCTRL. */
#define XCOM_MODE_UNKNOWN       0
#define XCOM_MODE_APP           1
#define XCOM_MODE_MCTRL         2

static volatile int xcom_mode = XCOM_MODE_UNKNOWN;
static mc_mutex_t xcom_mutex;


void*
xcom_init_create(const CLSID* clsid, DWORD context, const IID* iid)
{
    void* obj;
    HRESULT hr;

redo:
    switch(xcom_mode) {
        case XCOM_MODE_APP:
            /* The application already initialized COM so we do not manage it
             * at all and just reuse app's apartment. */
            hr = CoCreateInstance(clsid, NULL, context, iid, &obj);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("xcom_create_init: CoCreateInstance(1) failed.");
                return NULL;
            }
            return obj;

        case XCOM_MODE_MCTRL:
            /* We are responsible to initialize COM whenever we want to use
             * it. According to Raymond Chen, we should use single-threaded
             * apartment (COINIT_APARTMENTTHREADED):
             * http://blogs.msdn.com/b/oldnewthing/archive/2008/04/24/8420242.aspx
             */
            hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("xcom_create_init: CoInitializeEx() failed.");
                return NULL;
            }
            hr = CoCreateInstance(clsid, NULL, context, iid, &obj);
            if(MC_ERR(FAILED(hr))) {
                MC_TRACE_HR("xcom_create_init: CoCreateInstance(2) failed.");
                CoUninitialize();
                return NULL;
            }
            return obj;

        case XCOM_MODE_UNKNOWN:
            mc_mutex_lock(&xcom_mutex);
            if(xcom_mode != XCOM_MODE_UNKNOWN) {   /* Resolve a race. */
                mc_mutex_unlock(&xcom_mutex);
                goto redo;
            }
            hr = CoCreateInstance(clsid, NULL, context, iid, &obj);
            if(SUCCEEDED(hr)) {
                xcom_mode = XCOM_MODE_APP;
            } else if(hr == CO_E_NOTINITIALIZED) {
                xcom_mode = XCOM_MODE_MCTRL;
            } else {
                MC_TRACE_HR("xcom_create_init: CoCreateInstance(3) failed.");
                obj = NULL;
            }
            mc_mutex_unlock(&xcom_mutex);
            if(hr == CO_E_NOTINITIALIZED)
                goto redo;
            return obj;

        default:
            MC_UNREACHABLE;
            return NULL;
    }
}

void
xcom_uninit(void)
{
    if(xcom_mode == XCOM_MODE_MCTRL)
        CoUninitialize();
}

void
xcom_dllmain_init(void)
{
    mc_mutex_init(&xcom_mutex);
}

void
xcom_dllmain_fini(void)
{
    mc_mutex_fini(&xcom_mutex);
}
