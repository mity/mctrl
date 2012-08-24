/*
 * Copyright (c) 2011 Martin Mitas
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

#ifndef MC_PROPSET_H
#define MC_PROPSET_H

#include "mCtrl/propset.h"

#include "misc.h"
#include "dsa.h"
#include "value.h"
#include "viewlist.h"


typedef struct propset_tag propset_t;
struct propset_tag {
    LONG refs;
    dsa_t items;
    DWORD flags;
    view_list_t vlist;
};


typedef struct propset_item_tag propset_item_t;
struct propset_item_tag {
    TCHAR* text;
    value_type_t* type;
    value_t value;
    LPARAM lp;
    DWORD flags;
};



propset_t* propset_create(DWORD flags);
void propset_destroy(propset_t* propset);


static inline void
propset_ref(propset_t* propset)
{
    InterlockedIncrement(&propset->refs);
}

static inline void
propset_unref(propset_t* propset)
{
    if(InterlockedDecrement(&propset->refs) == 0)
        propset_destroy(propset);
}


static inline WORD
propset_size(propset_t* propset)
{
    return dsa_size(&propset->items);
}

static inline propset_item_t*
propset_item(propset_t* propset, WORD index)
{
    return (propset_item_t*) dsa_item_(&propset->items, index, sizeof(propset_item_t));
}



/* Passed optionally to the refresh callback. */
typedef struct propset_refresh_data_tag propset_refresh_data_t;
struct propset_refresh_data_tag {
    WORD index;
    SHORT size_delta;  /* +1 on insert, 0 on set, -1 on remove */
};

static inline int
propset_install_view(propset_t* propset, void* view, view_refresh_t refresh)
{
    return view_list_install_view(&propset->vlist, view, refresh);
}

static inline void
propset_uninstall_view(propset_t* propset, void* view)
{
    view_list_uninstall_view(&propset->vlist, view);
}

static inline void
propset_refresh_views(propset_t* propset, void* data)
{
    view_list_refresh(&propset->vlist, data);
}


#endif  /* MC_PROPSET_H */
