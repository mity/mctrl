/*
 * Copyright (c) 2010-2012 Martin Mitas
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

#ifndef MC_VIEWLIST_H
#define MC_VIEWLIST_H

#include "misc.h"
#include "dsa.h"


/* The models (as in model-view-controller paradigm) must be aware of their
 * views so the views are notified to be refreshed whenever the model state
 * changes.
 *
 * The view list is just such a container for the views reused in the various
 * models.
 */


typedef void (*view_refresh_t)(void* /*view*/, void* /*detail*/);


typedef struct view_node_tag view_node_t;
struct view_node_tag {
    void* view;
    view_refresh_t refresh;
};

typedef struct view_list_tag view_list_t;
struct view_list_tag {
    dsa_t dsa;
};


static inline void
view_list_init(view_list_t* vlist)
{
    dsa_init(&vlist->dsa, sizeof(view_node_t));
}

static inline void
view_list_fini(view_list_t* vlist)
{
    MC_ASSERT(dsa_size(&vlist->dsa) == 0);
    dsa_fini(&vlist->dsa, NULL);
}

int view_list_install_view(view_list_t* vlist, void* view, view_refresh_t refresh);
void view_list_uninstall_view(view_list_t* vlist, void* view);

static inline void
view_list_refresh(view_list_t* vlist, void* detail)
{
    int i, n;
    n = dsa_size(&vlist->dsa);
    for(i = 0; i < n; i++) {
        view_node_t* node = DSA_ITEM(&vlist->dsa, i, view_node_t);
        node->refresh(node->view, detail);
    }
}


#endif  /* MC_VIEWLIST_H */
