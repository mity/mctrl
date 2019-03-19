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

#include "viewlist.h"


int
view_list_install_view(view_list_t* vlist, void* view, view_refresh_t refresh)
{
    view_node_t* node;

    node = dsa_insert_raw(&vlist->dsa, dsa_size(&vlist->dsa));
    if(MC_ERR(node == NULL)) {
        MC_TRACE("view_install: dsa_insert_raw() failed.");
        return -1;
    }

    node->view = view;
    node->refresh = refresh;
    return 0;
}

void
view_list_uninstall_view(view_list_t* vlist, void* view)
{
    int i, n;

    n = dsa_size(&vlist->dsa);
    for(i = 0; i < n; i++) {
        view_node_t* node = DSA_ITEM(&vlist->dsa, i, view_node_t);
        if(view == node->view) {
            dsa_remove(&vlist->dsa, i, NULL);
            return;
        }
    }
}
