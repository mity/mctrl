/*
 * Copyright (c) 2010-2015 Martin Mitas
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
#include "viewlist.h"


typedef struct table_cell_tag table_cell_t;
struct table_cell_tag {
    TCHAR* text;
    LPARAM lp;
    DWORD flags;
};



typedef struct table_tag table_t;
struct table_tag {
    mc_ref_t refs;
    WORD col_count;
    WORD row_count;
    table_cell_t* restrict cols;
    table_cell_t* restrict rows;
    table_cell_t* restrict cells;

    view_list_t vlist;
};



typedef struct table_region_tag table_region_t;
struct table_region_tag {
    WORD col0;  /* [col0,row0] inclusive */
    WORD row0;
    WORD col1;  /* [col1,row1] exclusive */
    WORD row1;
};



table_t* table_create(WORD col_count, WORD row_count);
void table_destroy(table_t* table);

int table_resize(table_t* table, WORD col_count, WORD row_count);

static inline table_cell_t* table_cell(table_t* table, WORD col, WORD row)
    { return &table->cells[row * table->col_count + col]; }

static inline void table_ref(table_t* table)
    { mc_ref(&table->refs); }
static inline void table_unref(table_t* table)
    { if(mc_unref(&table->refs) == 0) table_destroy(table); }

int table_set_cell_data(table_t* table, WORD col, WORD row, MC_TABLECELL* cell_data, BOOL unicode);
int table_get_cell_data(table_t* table, WORD col, WORD row, MC_TABLECELL* cell_data, BOOL unicode);



/* Structure passed into view_refresh_t callback.
 * It specifies what has changed. */
typedef struct table_refresh_detail_tag table_refresh_detail_t;
struct table_refresh_detail_tag {
    enum {
        TABLE_CELL_CHANGED,      /* params: col, row */
        TABLE_REGION_CHANGED,    /* params: col0, row0, col1, row1 (and never mixes header and ordinary cells) */
        TABLE_COLCOUNT_CHANGED,  /* params: old_count, new_count, col_pos */
        TABLE_ROWCOUNT_CHANGED   /* params: old_count, new_count, row_pos */
    } event;

    int param[4];
};


static inline int
table_install_view(table_t* table, void* view, view_refresh_t refresh)
    { return view_list_install_view(&table->vlist, view, refresh); }
static inline void
table_uninstall_view(table_t* table, void* view)
    { view_list_uninstall_view(&table->vlist, view); }


#endif  /* MC_TABLE_H */
