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

#include "propset.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define PROPSET_DEBUG     1*/

#ifdef PROPSET_DEBUG
    #define PROPSET_TRACE         MC_TRACE
#else
    #define PROPSET_TRACE(...)    do { } while(0)
#endif



#define PROPSET_SUPPORTED_FLAGS        ((DWORD)(MC_PSF_SORTITEMS))
#define PROPSET_SUPPORTED_ITEM_MASK                                           \
        ((DWORD)(MC_PSIMF_TEXT | MC_PSIMF_VALUE | MC_PSIMF_PARAM | MC_PSIMF_FLAGS))
#define PROPSET_SUPPORTED_ITEM_FLAGS   ((DWORD)(0))  // TODO


static void
propset_item_dtor(dsa_t* dsa, void* dsa_item)
{
    propset_item_t* item = (propset_item_t*) dsa_item;
    if(item->text != NULL)
        free(item->text);
    if(item->value != NULL)
        value_destroy(item->value);
}

static int
propset_item_cmp(dsa_t* dsa, const void* dsa_item1, const void* dsa_item2)
{
    const propset_item_t* item1 = (propset_item_t*) dsa_item1;
    const propset_item_t* item2 = (propset_item_t*) dsa_item2;

    return _tcsicmp((item1->text != NULL ? item1->text : _T("")),
                    (item2->text != NULL ? item2->text : _T("")));
}

propset_t*
propset_create(DWORD flags)
{
    propset_t* propset;

    PROPSET_TRACE("propset_create(0x%x)", flags);

    propset = (propset_t*) malloc(sizeof(propset_t));
    if(MC_ERR(propset == NULL)) {
        MC_TRACE("propset_create: malloc() failed.");
        return NULL;
    }

    propset->refs = 1;
    dsa_init(&propset->items, sizeof(propset_item_t));
    propset->flags = flags;
    view_list_init(&propset->vlist);
    return propset;
}

void
propset_destroy(propset_t* propset)
{
    PROPSET_TRACE("propset_destroy(%p)", propset);

    MC_ASSERT(propset->refs == 0);
    MC_ASSERT(VIEW_LIST_IS_EMPTY(&propset->vlist));

    dsa_fini(&propset->items, propset_item_dtor);
    free(propset);
}

static int
propset_apply(propset_item_t* item, MC_PROPSETITEM* pi, BOOL unicode)
{
    if(MC_ERR(pi->fMask & ~PROPSET_SUPPORTED_ITEM_MASK)) {
        MC_TRACE("propset_apply: Unsupported MC_PROPSETITEM::fMask.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(MC_ERR((pi->fMask & MC_PSIMF_FLAGS)  &&  (pi->dwFlags & ~PROPSET_SUPPORTED_ITEM_FLAGS))) {
        MC_TRACE("propset_apply: Unsupported MC_PROPSETITEM::dwFlags.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(pi->fMask & MC_PSIMF_TEXT) {
        TCHAR* text;

        text = mc_str(pi->pszText, (unicode ? MC_STRW : MC_STRA), MC_STRT);
        if(MC_ERR(pi->pszText != NULL && text == NULL)) {
            MC_TRACE("propset_apply: mc_str() failed.");
            return -1;
        }

        if(item->text != NULL)
            free(item->text);
        item->text = text;
    }

    if(pi->fMask & MC_PSIMF_VALUE) {
        if(item->value != NULL)
            value_destroy(item->value);
        item->value = (value_t*) pi->hValue;
    }

    if(pi->fMask & MC_PSIMF_PARAM)
        item->lp = pi->lParam;

    if(pi->fMask & MC_PSIMF_FLAGS)
        item->flags = pi->dwFlags;

    return 0;
}

static int
propset_insert(propset_t* propset, MC_PROPSETITEM* pi, BOOL unicode)
{
    WORD index;
    propset_item_t item = {0};

    PROPSET_TRACE("propset_insert(%p, %p, %d)", propset, pi, unicode);

    if(MC_ERR(propset == NULL)) {
        MC_TRACE("propset_insert: invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    if(MC_ERR(propset_apply(&item, pi, unicode) != 0)) {
        MC_TRACE("propset_insert: propset_apply() failed.");
        return -1;
    }

    index = MC_MAX(0, MC_MIN(pi->iItem, propset_size(propset)));
    return dsa_insert_smart(&propset->items, index, &item,
                (propset->flags & MC_PSF_SORTITEMS) ? propset_item_cmp : NULL);
}

static int
propset_set(propset_t* propset, MC_PROPSETITEM* pi, BOOL unicode)
{
    propset_item_t* item;
    int index = pi->iItem;

    PROPSET_TRACE("propset_set(%p, %p, %d)", propset, pi, unicode);

    if(MC_ERR(propset == NULL)) {
        MC_TRACE("propset_set: Invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    if(MC_ERR(index < 0 || index >= propset_size(propset))) {
        MC_TRACE("propset_set: iItem out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    item = propset_item(propset, index);
    if(MC_ERR(propset_apply(item, pi, unicode) != 0)) {
        MC_TRACE("propset_set: propset_apply() failed.");
        return -1;
    }

    /* When sorted, we may need to move the item to new location if its
     * name has changed */
    if((propset->flags & MC_PSF_SORTITEMS)  &&  (pi->fMask & MC_PSIMF_TEXT))
        index = dsa_move_sorted(&propset->items, index, propset_item_cmp);

    return index;
}

static int
propset_get(propset_t* propset, MC_PROPSETITEM* pi, BOOL unicode)
{
    propset_item_t* item;

    PROPSET_TRACE("propset_get(%p, %p, %d)", propset, pi, unicode);

    if(MC_ERR(propset == NULL)) {
        MC_TRACE("propset_get: Invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    if(MC_ERR(pi->fMask & ~PROPSET_SUPPORTED_ITEM_MASK)) {
        MC_TRACE("propset_get: Unsupported MC_PROPSETITEM::fMask.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if(MC_ERR(pi->iItem < 0 || pi->iItem >= propset_size(propset))) {
        MC_TRACE("propset_get: iItem out of range.");
        return -1;
    }

    item = propset_item(propset, pi->iItem);

    if(pi->fMask & MC_PSIMF_TEXT) {
        mc_str_inbuf(item->text, MC_STRT, pi->pszText,
                     (unicode ? MC_STRW : MC_STRA), pi->cchTextMax);
    }

    if(pi->fMask & MC_PSIMF_VALUE)
        pi->hValue = (MC_HVALUE) item->value;

    if(pi->fMask & MC_PSIMF_PARAM)
        pi->lParam = item->lp;

    if(pi->fMask & MC_PSIMF_FLAGS)
        pi->dwFlags = item->flags;

    return 0;
}


/**************************
 *** Exported functions ***
 **************************/

MC_HPROPSET MCTRL_API
mcPropSet_Create(DWORD dwFlags)
{
    propset_t* propset;

    PROPSET_TRACE("mcPropSet_Create(0x%x)", dwFlags);

    if(MC_ERR((dwFlags & ~PROPSET_SUPPORTED_FLAGS) != 0)) {
        MC_TRACE("mcPropSet_Create: invalid dwFlags.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    propset = propset_create(dwFlags);
    if(MC_ERR(propset == NULL)) {
        MC_TRACE("mcPropSet_Create: propset_create() failed.");
        return NULL;
    }

    return (MC_HPROPSET) propset;
}

BOOL MCTRL_API
mcPropSet_AddRef(MC_HPROPSET hPropSet)
{
    if(MC_ERR(hPropSet == NULL)) {
        MC_TRACE("mcPropSet_AddRef: invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    propset_ref((propset_t*) hPropSet);
    return TRUE;
}

BOOL MCTRL_API
mcPropSet_Release(MC_HPROPSET hPropSet)
{
    if(MC_ERR(hPropSet == NULL)) {
        MC_TRACE("mcPropSet_Release: invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    propset_unref((propset_t*) hPropSet);
    return TRUE;
}

int MCTRL_API
mcPropSet_GetItemCount(MC_HPROPSET hPropSet)
{
    if(MC_ERR(hPropSet == NULL)) {
        MC_TRACE("mcPropSet_Size: invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    return propset_size((propset_t*) hPropSet);
}

int MCTRL_API
mcPropSet_InsertItemW(MC_HPROPSET hPropSet, MC_PROPSETITEMW* pItem)
{
    return (propset_insert((propset_t*)hPropSet, (MC_PROPSETITEM*)pItem, TRUE) == 0);
}

int MCTRL_API
mcPropSet_InsertItemA(MC_HPROPSET hPropSet, MC_PROPSETITEMA* pItem)
{
    return (propset_insert((propset_t*)hPropSet, (MC_PROPSETITEM*)pItem, FALSE) == 0);
}

BOOL MCTRL_API
mcPropSet_GetItemW(MC_HPROPSET hPropSet, MC_PROPSETITEMW* pItem)
{
    return (propset_get((propset_t*)hPropSet, (MC_PROPSETITEM*)pItem, TRUE) == 0);
}

BOOL MCTRL_API
mcPropSet_GetItemA(MC_HPROPSET hPropSet, MC_PROPSETITEMA* pItem)
{
    return (propset_get((propset_t*)hPropSet, (MC_PROPSETITEM*)pItem, FALSE) == 0);
}

int MCTRL_API
mcPropSet_SetItemW(MC_HPROPSET hPropSet, MC_PROPSETITEMW* pItem)
{
    return propset_set((propset_t*)hPropSet, (MC_PROPSETITEM*)pItem, TRUE);
}

int MCTRL_API
mcPropSet_SetItemA(MC_HPROPSET hPropSet, MC_PROPSETITEMA* pItem)
{
    return propset_set((propset_t*)hPropSet, (MC_PROPSETITEM*)pItem, FALSE);
}

BOOL MCTRL_API
mcPropSet_DeleteItem(MC_HPROPSET hPropSet, int iItem)
{
    propset_t* propset = (propset_t*) hPropSet;
    propset_refresh_data_t refresh_data;

    PROPSET_TRACE("mcPropSet_DeleteItem(%p, %d)", propset, iItem);

    if(MC_ERR(propset == NULL)) {
        MC_TRACE("mcPropSet_DeleteItem: invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if(MC_ERR(iItem < 0  ||  iItem >= propset_size(propset))) {
        MC_TRACE("mcPropSet_DeleteItem: index out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    dsa_remove(&propset->items, iItem, propset_item_dtor);

    refresh_data.index = iItem;
    refresh_data.size_delta = -1;
    propset_refresh_views(propset, &refresh_data);

    return TRUE;
}

BOOL MCTRL_API
mcPropSet_DeleteAllItems(MC_HPROPSET hPropSet)
{
    propset_t* propset = (propset_t*) hPropSet;

    PROPSET_TRACE("mcPropSet_Clear(%p)", propset);

    if(MC_ERR(hPropSet == NULL)) {
        MC_TRACE("mcPropSet_Clear: invalid handle.");
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    dsa_clear(&propset->items, propset_item_dtor);
    propset_refresh_views(propset, NULL);
    return TRUE;
}
