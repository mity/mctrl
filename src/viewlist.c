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

    /* View can be installed only once in the list */
#ifdef DEBUG
    for(node = vlist->head; node != NULL; node = node->next)
        MC_ASSERT(node->view != view);
#endif

    node = (view_node_t*) malloc(sizeof(view_node_t));
    if(MC_ERR(node == NULL)) {
        MC_TRACE("view_install: malloc() failed.");
        return -1;
    }

    node->view = view;
    node->refresh = refresh;
    node->next = vlist->head;
    vlist->head = node;
    return 0;
}

void
view_list_uninstall_view(view_list_t* vlist, void* view)
{
    view_node_t* node = vlist->head;
    view_node_t* prev = NULL;

    while(node->view != view) {
        MC_ASSERT(node != NULL);
        prev = node;
        node = node->next;
    }

    if(prev)
        prev->next = node->next;
    else
        vlist->head = node->next;
    free(node);
}

