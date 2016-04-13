/*
 * Copyright (c) 2016 Martin Mitas
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

#include "misc.h"
#include "backend-d2d.h"
#include "backend-dwrite.h"
#include "backend-wic.h"
#include "backend-gdix.h"
#include "lock.h"


void (*wd_fn_lock)(void) = NULL;
void (*wd_fn_unlock)(void) = NULL;

static DWORD wd_preinit_flags = 0;


void
wdPreInitialize(void (*fnLock)(void), void (*fnUnlock)(void), DWORD dwFlags)
{
    wd_fn_lock = fnLock;
    wd_fn_unlock = fnUnlock;
    wd_preinit_flags = dwFlags;
}


static int
wd_init_core_api(void)
{
    if(!(wd_preinit_flags & WD_DISABLE_D2D)) {
        if(d2d_init() == 0)
            return 0;
    }

    if(!(wd_preinit_flags & WD_DISABLE_GDIPLUS)) {
        if(gdix_init() == 0)
            return 0;
    }

    return -1;
}

static void
wd_fini_core_api(void)
{
    if(d2d_enabled())
        d2d_fini();
    else
        gdix_fini();
}

static int
wd_init_image_api(void)
{
    if(d2d_enabled()) {
        return wic_init();
    } else {
        /* noop */
        return 0;
    }
}

static void
wd_fini_image_api(void)
{
    if(d2d_enabled()) {
        wic_fini();
    } else {
        /* noop */
    }
}

static int
wd_init_string_api(void)
{
    if(d2d_enabled()) {
        return dwrite_init();
    } else {
        /* noop */
        return 0;
    }
}

static void
wd_fini_string_api(void)
{
    if(d2d_enabled()) {
        dwrite_fini();
    } else {
        /* noop */
    }
}


static const struct {
    int (*fn_init)(void);
    void (*fn_fini)(void);
} wd_modules[] = {
    { wd_init_core_api, wd_fini_core_api },
    { wd_init_image_api, wd_fini_image_api },
    { wd_init_string_api, wd_fini_string_api }
};

#define WD_MOD_COUNT    (sizeof(wd_modules) / sizeof(wd_modules[0]))

#define WD_MOD_COREAPI          0
#define WD_MOD_IMAGEAPI         1
#define WD_MOD_STRINGAPI        2

static UINT wd_init_counter[WD_MOD_COUNT] = { 0 };


BOOL
wdInitialize(DWORD dwFlags)
{
    BOOL want_init[WD_MOD_COUNT];
    int i;

    want_init[WD_MOD_COREAPI] = TRUE;
    want_init[WD_MOD_IMAGEAPI] = (dwFlags & WD_INIT_IMAGEAPI);
    want_init[WD_MOD_STRINGAPI] = (dwFlags & WD_INIT_STRINGAPI);

    wd_lock();

    for(i = 0; i < WD_MOD_COUNT; i++) {
        if(!want_init[i])
            continue;

        wd_init_counter[i]++;
        if(wd_init_counter[i] > 0) {
            if(wd_modules[i].fn_init() != 0)
                goto fail;
        }
    }

    wd_unlock();
    return TRUE;

fail:
    /* Undo initializations from successful iterations. */
    while(--i >= 0) {
        if(want_init[i]) {
            wd_init_counter[i]--;
            if(wd_init_counter[i] == 0)
                wd_modules[i].fn_fini();
        }
    }

    wd_unlock();
    return FALSE;
}

void
wdTerminate(DWORD dwFlags)
{
    BOOL want_fini[WD_MOD_COUNT];
    int i;

    want_fini[WD_MOD_COREAPI] = TRUE;
    want_fini[WD_MOD_IMAGEAPI] = (dwFlags & WD_INIT_IMAGEAPI);
    want_fini[WD_MOD_STRINGAPI] = (dwFlags & WD_INIT_STRINGAPI);

    wd_lock();

    for(i = WD_MOD_COUNT-1; i >= 0; i--) {
        if(!want_fini[i])
            continue;

        wd_init_counter[i]--;
        if(wd_init_counter[i] == 0)
            wd_modules[i].fn_fini();
    }

    /* If core module counter has dropped to zero, caller likely forgot to
     * terminate some optional module (i.e. mismatching flags for wdTerminate()
     * somewhere. So lets kill all those modules forcefully now anyway even
     * though well behaving applications should never do that...
     */
    if(wd_init_counter[WD_MOD_COREAPI] == 0) {
        for(i = WD_MOD_COUNT-1; i >= 0; i--) {
            if(wd_init_counter[i] > 0) {
                WD_TRACE("wdTerminate: Forcefully terminating module %d.", i);
                wd_modules[i].fn_fini();
                wd_init_counter[i] = 0;
            }
        }
    }

    wd_unlock();
}
