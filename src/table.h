/*
 * Copyright (c) 2010-2011 Martin Mitas
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

#ifndef MC_TABLE_H
#define MC_TABLE_H

#include "mCtrl/table.h"

#include "misc.h"
#include "value.h"
#include "viewlist.h"


typedef struct table_tag table_t;


/* [col0, row0] inclusive; [col1, row1] exclusive */
typedef struct table_region_tag table_region_t;
struct table_region_tag {
    WORD col0;
    WORD row0;
    WORD col1;
    WORD row1;
};


table_t* table_create(WORD col_count, WORD row_count,
                      value_type_t* cell_type, DWORD mask);

void table_ref(table_t* table);
void table_unref(table_t* table);

WORD table_col_count(const table_t* table);
WORD table_row_count(const table_t* table);

void table_paint_cell(const table_t* table, WORD col, WORD row, HDC dc, RECT* rect);

int table_resize(table_t* table, WORD col_count, WORD row_count);
void table_clear(table_t* table);

void table_get_cell(const table_t* table, WORD col, WORD row, MC_TABLECELL* cell);
void table_set_cell(table_t* table, WORD col, WORD row, MC_TABLECELL* cell);


/* table_region_t is passed to the refresh function as the detail where 
 * the change happened. On some more substantial changes (e.g. resize) it may
 * be NULL (meaning redraw everything). */
int table_install_view(table_t* table, void* view, view_refresh_t refresh);
void table_uninstall_view(table_t* table, void* view);


#endif  /* MC_TABLE_H */
