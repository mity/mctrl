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

#include "table.h"


/* Uncomment this to have more verbous traces from this module. */
/*#define TABLE_DEBUG     1*/

#ifdef TABLE_DEBUG
    #define TABLE_TRACE         MC_TRACE
#else
    #define TABLE_TRACE(...)    do { } while(0)
#endif



/********************
 *** Helper stuff ***
 ********************/

typedef struct table_contents_tag table_contents_t;
struct table_contents_tag {
    WORD col_count;
    WORD row_count;
    value_type_t** types;
    value_t* values;
};


static int
table_alloc_contents(table_contents_t* contents, value_type_t* homogenous_type)
{
    UINT cell_count = contents->col_count * contents->row_count;
    
    if(cell_count == 0) {
        contents->values = NULL;
        contents->types = NULL;
        return 0;
    }
    
    contents->values = (value_t*) malloc(cell_count * sizeof(value_t));
    if(MC_ERR(contents->values == NULL)) {
        MC_TRACE("table_alloc_contents: malloc() failed.");
        return -1;
    }
    
    if(homogenous_type != NULL) {
        contents->types = NULL;
        return 0;
    }
        
    contents->types = (value_type_t**) malloc(cell_count * sizeof(value_type_t*));
    if(MC_ERR(contents->types == NULL)) {
        MC_TRACE("table_alloc_contents: malloc() failed.");
        free(contents->values);
        return -1;
    }
    
    return 0;
}

static void
table_init_contents(table_contents_t* contents, table_region_t* region,
                    value_type_t* homogenous_type)
{
    WORD row;
    
    if(region->col1 - region->col0 == 0 || region->row1 - region->row0 == 0)
        return;

    if(region->col0 == 0 && region->col1 == contents->col_count) {
        /* Optimized path when complete lines */
        memset(&contents->values[region->row0 * contents->col_count], 0,
               (region->row1 - region->row0) * contents->col_count * sizeof(value_t));
        
        if(homogenous_type == NULL) {
            memset(&contents->types[region->row0 * contents->col_count], 0,
                   (region->row1 - region->row0) * contents->col_count * sizeof(value_type_t*));
        }
        return;
    }
    
    for(row = region->row0; row < region->row1; row++) {
        memset(&contents->values[row * contents->col_count + region->col0], 0,
               (region->col1 - region->col0) * sizeof(value_t));
    }
    
    if(homogenous_type == NULL) {
        for(row = region->row0; row < region->row1; row++) {
            memset(&contents->types[row * contents->col_count + region->col0], 0,
                   (region->col1 - region->col0) * sizeof(value_type_t*));
        }
    }
}

static void
table_free_contents(table_contents_t* contents, table_region_t* region,
                    value_type_t* homogenous_type)
{
    value_type_t* t;
    WORD row, col;
    
    for(row = region->row0; row < region->row1; row++) {
        for(col = region->col0; col < region->col1; col++) {
            if(homogenous_type != NULL) {
                t = homogenous_type;
            } else {
                t = contents->types[row * contents->col_count + col];
                if(t == NULL)
                    continue;
            }
            
            t->free(contents->values[row * contents->col_count + col]);
        }
    }
}

static void
table_copy_contents(table_contents_t* contents_from, table_region_t* region_from,
                    table_contents_t* contents_to, table_region_t* region_to,
                    value_type_t* homogenous_type)
{
    WORD i;

    MC_ASSERT(region_from->col1 - region_from->col0 == 
                                       region_to->col1 - region_to->col0);
    MC_ASSERT(region_from->row1 - region_from->row0 == 
                                       region_to->row1 - region_to->row0);
    
    if(region_from->col1 - region_from->col0 == 0 || region_from->row1 - region_from->row0)
        return;
    
    if(region_to->col0 == 0 && region_to->col1 == contents_to->col_count &&
       region_from->col0 == 0 && region_from->col1 == contents_from->col_count) {
        /* Optimized path when complete lines moved to complete lines */
        MC_ASSERT(contents_from->col_count == contents_to->col_count);
        
        memcpy(&contents_to->values[region_to->row0 * contents_to->col_count],
               &contents_from->values[region_from->row0 * contents_from->col_count],
               (region_from->row1 - region_from->row0) * contents_from->col_count * sizeof(value_t));

        if(homogenous_type == NULL) {
            memcpy(&contents_to->types[region_to->row0 * contents_to->col_count],
                   &contents_from->types[region_from->row0 * contents_from->col_count],
                   (region_from->row1 - region_from->row0) * contents_from->col_count * sizeof(value_type_t*));
        }
        
        return;
    }
    
    for(i = 0; i < region_from->row1 - region_from->row0; i++) {
        memcpy(&contents_to->values[(region_to->row0+i) * contents_to->col_count + region_to->col0],
               &contents_from->values[(region_from->row0+i) * contents_from->col_count + region_from->col0],
               (region_from->col1 - region_from->col0) * sizeof(value_t));

        if(homogenous_type == NULL) {
            memcpy(&contents_to->types[(region_to->row0+i) * contents_to->col_count + region_to->col0],
                   &contents_from->types[(region_from->row0+i) * contents_from->col_count + region_from->col0],
                   (region_from->col1 - region_from->col0) * sizeof(value_type_t*));
        }
    }
}


/****************************
 *** Table implementation ***
 ****************************/

struct table_tag {
    DWORD refs       : 31;
    DWORD homogenous :  1;
    WORD col_count;
    WORD row_count;
    union {
        value_type_t* type;     /* if homogenous */
        value_type_t** types;   /* if heterogenous */
    };
    value_t* values;
    view_list_t vlist;
};


static inline void
table_refresh_views(table_t* table, table_region_t* region)
{
    view_list_refresh(&table->vlist, region);
}

table_t*
table_create(WORD col_count, WORD row_count, value_type_t* cell_type)
{
    table_t* table;
    table_contents_t contents;
    table_region_t region;

    table = (table_t*) malloc(sizeof(table_t));
    if(MC_ERR(table == NULL)) {
        MC_TRACE("table_create: malloc() failed.");
        goto err_malloc;
    }

    contents.col_count = col_count;
    contents.row_count = row_count;
    if(MC_ERR(table_alloc_contents(&contents, cell_type) != 0)) {
        MC_TRACE("table_create: table_alloc_contents() failed.");
        goto err_alloc_contents;
    }
    region.col0 = 0;
    region.row0 = 0;
    region.col1 = col_count;
    region.row1 = row_count;
    table_init_contents(&contents, &region, cell_type);

    table->refs = 1;
    table->homogenous = (cell_type != NULL ? 1 : 0);
    table->col_count = col_count;
    table->row_count = row_count;
    if(table->homogenous)
        table->type = cell_type;
    else
        table->types = contents.types;
    table->values = contents.values;
    view_list_init(&table->vlist);
    return table;

err_alloc_contents:
    free(table);
err_malloc:
    return NULL;
}

void
table_ref(table_t* table)
{
    TABLE_TRACE("table_ref(%p): %u -> %u", table, table->refs, table->refs+1);
    table->refs++;
}

void
table_unref(table_t* table)
{
    TABLE_TRACE("table_unref(%p): %u -> %u", table, table->refs, table->refs-1);
    table->refs--;
    if(table->refs == 0) {
        table_contents_t contents;
        table_region_t region;

        MC_ASSERT(VIEW_LIST_IS_EMPTY(&table->vlist));
        
        contents.col_count = table->col_count;
        contents.row_count = table->row_count;
        contents.types = table->types;
        contents.values = table->values;
        region.col0 = 0;
        region.row0 = 0;
        region.col1 = table->col_count;
        region.row1 = table->row_count;
        table_free_contents(&contents, &region, 
                            (table->homogenous ? table->type : NULL));
    
        if(!table->homogenous)
            free(table->types);
        free(table->values);
        free(table);
    }
}

WORD
table_col_count(const table_t* table)
{
    return table->col_count;
}

WORD
table_row_count(const table_t* table)
{
    return table->row_count;
}

void
table_paint_cell(const table_t* table, WORD col, WORD row, HDC dc, RECT* rect)
{
    int index = row * table->col_count + col;
    value_type_t* type = (table->homogenous ? table->type : table->types[index]);
    value_t* value = table->values[index];
    
    if(type != NULL)
        type->paint(value, dc, rect);
}

int
table_resize(table_t* table, WORD col_count, WORD row_count)
{
    table_contents_t contents_from;
    table_contents_t contents_to;
    table_region_t region;
    value_type_t* cell_type = (table->homogenous ? table->type : NULL);
    
    if(col_count == table->col_count  &&  row_count == table->row_count)
        return 0;

    contents_from.col_count = table->col_count;
    contents_from.row_count = table->row_count;
    contents_from.types = table->types;
    contents_from.values = table->values;

    contents_to.col_count = col_count;
    contents_to.row_count = row_count;
    if(MC_ERR(table_alloc_contents(&contents_to, cell_type) != 0)) {
        MC_TRACE("table_resize: table_alloc_contents() failed.");
        return -1;
    }
    
    /* Copy intersection */
    region.col0 = 0;
    region.row0 = 0;
    region.col1 = MC_MIN(contents_from.col_count, contents_to.col_count);
    region.row1 = MC_MIN(contents_from.row_count, contents_to.row_count);
    table_copy_contents(&contents_from, &region, &contents_to, &region, cell_type);
    
    /* Handle difference in col_count */
    region.col0 = MC_MIN(contents_from.col_count, contents_to.col_count);
    region.col1 = MC_MAX(contents_from.col_count, contents_to.col_count);
    if(contents_to.col_count >= contents_from.col_count)
        table_init_contents(&contents_to, &region, cell_type);
    else
        table_free_contents(&contents_from, &region, cell_type);
    
    /* Handle difference in row_count */
    region.col0 = 0;
    region.row0 = region.row1;
    region.row1 = MC_MAX(contents_from.row_count, contents_to.row_count);
    if(contents_to.row_count >= contents_from.row_count)
        table_init_contents(&contents_to, &region, cell_type);
    else
        table_free_contents(&contents_from, &region, cell_type);

    /* Install new values/types */
    table->col_count = col_count;
    table->row_count = row_count;
    table->values = contents_to.values;
    if(!table->homogenous) {
        table->types = contents_to.types;
        free(contents_from.types);
    }
    free(contents_from.values);
    
    table->col_count = col_count;
    table->row_count = row_count;
    table_refresh_views(table, NULL);

    return 0;
}

void
table_clear(table_t* table)
{
    table_region_t region;
    table_contents_t contents;
    value_type_t* cell_type = (table->homogenous ? table->type : NULL);

    contents.col_count = table->col_count;
    contents.row_count = table->row_count;
    contents.values = table->values;
    contents.types = table->types;
    
    region.col0 = 0;
    region.row0 = 0;
    region.col1 = contents.col_count;
    region.row1 = contents.row_count;
    
    table_free_contents(&contents, &region, cell_type);
    table_init_contents(&contents, &region, cell_type);

    table_refresh_views(table, &region);
}

const value_t
table_get_value(const table_t* table, WORD col, WORD row, value_type_t** type)
{
    int index = row * table->col_count + col;
    
    *type = (table->homogenous ? table->type : table->types[index]);
    return table->values[index];
}

void
table_set_value(table_t* table, WORD col, WORD row, value_type_t* type, const value_t value)
{
    int index = row * table->col_count + col;
    table_region_t region;

    if(table->homogenous) {
        MC_ASSERT(type == table->type || type == NULL);
        if(table->values[index] != NULL)
            table->type->free(table->values[index]);
    } else {
        if(table->types[index] != NULL)
            table->types[index]->free(table->values[index]);
        table->types[index] = type;
    } 

    table->values[index] = value;
    
    region.col0 = col;
    region.row0 = row;
    region.col1 = col+1;
    region.row1 = row+1;
    table_refresh_views(table, &region);
}

int
table_install_view(table_t* table, void* view, view_refresh_t refresh)
{
    return view_list_install_view(&table->vlist, view, refresh);
}

void
table_uninstall_view(table_t* table, void* view)
{
    view_list_uninstall_view(&table->vlist, view);
}



/**************************
 *** Exported functions ***
 **************************/

MC_TABLE MCTRL_API
mcTable_Create(WORD wColumnCount, WORD wRowCount, MC_VALUETYPE hType, DWORD dwFlags)
{
    if(MC_ERR(dwFlags != 0)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
    return (MC_TABLE) table_create(wColumnCount, wRowCount, (value_type_t*)hType);
}

void MCTRL_API
mcTable_AddRef(MC_TABLE hTable)
{
    if(hTable)
        table_ref((table_t*)hTable);
}

void MCTRL_API
mcTable_Release(MC_TABLE hTable)
{
    if(hTable)
        table_unref((table_t*)hTable);
}

WORD MCTRL_API
mcTable_ColumnCount(MC_TABLE hTable)
{
    return (hTable ? table_col_count((table_t*)hTable) : 0);
}

WORD MCTRL_API
mcTable_RowCount(MC_TABLE hTable)
{
    return (hTable ? table_row_count((table_t*)hTable) : 0);
}

BOOL MCTRL_API
mcTable_Resize(MC_TABLE hTable, WORD wColumnCount, WORD wRowCount)
{
    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_Resize: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return (table_resize((table_t*)hTable, wColumnCount, wRowCount) == 0 ? 
            TRUE : FALSE);
}

void MCTRL_API
mcTable_Clear(MC_TABLE hTable)
{
    if(hTable)
        table_clear((table_t*)hTable);
}

BOOL MCTRL_API
mcTable_SetCell(MC_TABLE hTable, WORD wCol, WORD wRow,
                MC_VALUETYPE hType, MC_VALUE hValue)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_SetCell: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    if(MC_ERR(wCol >= table->col_count || wRow >= table->row_count)) {
        MC_TRACE("mcTable_SetCell: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    if(MC_ERR(table->homogenous  &&  hType != table->type)) {
        MC_TRACE("mcTable_SetCell: Value type mismatch in homogenous table.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    table_set_value((table_t*) hTable, wCol, wRow, (value_type_t*)hType, hValue);
    return TRUE;
}

BOOL MCTRL_API
mcTable_GetCell(MC_TABLE hTable, WORD wCol, WORD wRow, 
                MC_VALUETYPE* phType, MC_VALUE* phValue)
{
    table_t* table = (table_t*) hTable;
    value_type_t* type;
    value_t value;
    
    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_GetCell: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    if(MC_ERR(wCol >= table->col_count || wRow >= table->row_count)) {
        MC_TRACE("mcTable_GetCell: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    value = table_get_value(table, wCol, wRow, &type);
    
    if(phType != NULL)
        *phType = type;
    if(phValue)
        *phValue = value;
    return TRUE;
}
