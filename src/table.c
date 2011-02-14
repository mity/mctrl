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



#define MC_TCM_ALL    (MC_TCM_VALUE | MC_TCM_FOREGROUND | MC_TCM_BACKGROUND | MC_TCM_FLAGS)



/**********************
 *** Table contents ***
 **********************/

#define TABLE_CONTENTS_VALUES              0x01
#define TABLE_CONTENTS_TYPES               0x02
#define TABLE_CONTENTS_FOREGROUNDS         0x04
#define TABLE_CONTENTS_BACKGROUNDS         0x08
#define TABLE_CONTENTS_FLAGS               0x10

typedef struct table_contents_tag table_contents_t;
struct table_contents_tag {
    DWORD mask;
    WORD col_count;
    WORD row_count;
    value_t* values;
    union {
        value_type_t* type;    /* if homogenous */
        value_type_t** types;  /* if heterogenous */
    };
    COLORREF* foregrounds;
    COLORREF* backgrounds;
    BYTE* flags;               /* Now all public cell bits fit into single byte. In future we may need to extend this to WORD or DWORD. */
};

#define TABLE_CONTENTS_IS_HOMOGENOUS(contents)      \
     (!((contents)->mask & TABLE_CONTENTS_TYPES))


static int
table_contents_alloc(table_contents_t* contents, value_type_t* homotype,
                     WORD col_count, WORD row_count, DWORD mask)
{
    static const struct {
        DWORD mask_bit;
        size_t size;
    } SIZE_MAP[] = {
        { TABLE_CONTENTS_VALUES,      sizeof(value_t*) },
        { TABLE_CONTENTS_TYPES,       sizeof(value_type_t*) },
        { TABLE_CONTENTS_FOREGROUNDS, sizeof(COLORREF) },
        { TABLE_CONTENTS_BACKGROUNDS, sizeof(COLORREF) },
        { TABLE_CONTENTS_FLAGS,       sizeof(BYTE) }
    };
    size_t partsize[MC_ARRAY_SIZE(SIZE_MAP)];
    void** partptr[MC_ARRAY_SIZE(SIZE_MAP)];
    int i;
    size_t size = 0;
    BYTE* mem;

    MC_ASSERT(mask & TABLE_CONTENTS_VALUES);
    MC_ASSERT( ((mask & TABLE_CONTENTS_TYPES)  &&  homotype == NULL) ||
              !((mask & TABLE_CONTENTS_TYPES)  &&  homotype != NULL));

    contents->mask = mask;
    contents->col_count = col_count;
    contents->row_count = row_count;

    partptr[0] = (void**) &contents->values;
    partptr[1] = (void**) &contents->types;
    partptr[2] = (void**) &contents->foregrounds;
    partptr[3] = (void**) &contents->backgrounds;
    partptr[4] = (void**) &contents->flags;

    /* Zero-sized table is simple */
    if(col_count == 0  ||  row_count == 0) {
        for(i = 0; i < MC_ARRAY_SIZE(SIZE_MAP); i++)
            *partptr[i] = NULL;
        contents->type = homotype;
        return 0;
    }

    /* Calculate byte sizes for each needed property and their sume. */
    for(i = 0; i < MC_ARRAY_SIZE(SIZE_MAP); i++) {
        if(mask & SIZE_MAP[i].mask_bit) {
            partsize[i] = SIZE_MAP[i].size * col_count * row_count;
            size += partsize[i];
        } else {
            partsize[i] = 0;
        }
    }

    /* Allocation */
    mem = (BYTE*) malloc(size);
    if(MC_ERR(mem == NULL)) {
        MC_TRACE("table_contents_alloc: malloc() failed.");
        return -1;
    }

    /* Setup all pointers to the inside of the allocated chunk */
    for(i = 0; i < MC_ARRAY_SIZE(SIZE_MAP); i++) {
        if(partsize[i] > 0) {
            *partptr[i] = mem;
            mem += partsize[i];
        } else {
            *partptr[i] = NULL;
        }
    }

    /* If needed, setup homogenous table type */
    if(TABLE_CONTENTS_IS_HOMOGENOUS(contents)) {
        MC_ASSERT(contents->types == NULL);
        contents->type = homotype;
    }

    return 0;
}

static void
table_contents_free(table_contents_t* contents)
{
    /* Buffers for all tabke attributes share one allocated chunk
     * and ->values is the first of them. */
    free(contents->values);
}

static void
table_contents_init_region(table_contents_t* contents, table_region_t* region)
{
    WORD row;

    /* __stosd() intrinsic is intended for 32-bit */
    MC_STATIC_ASSERT(sizeof(COLORREF) == sizeof(DWORD));

    /* Case 0: empty region */
    if(region->col1 - region->col0 == 0 || region->row1 - region->row0 == 0)
        return;
        
    /* Case 1: region contains complete lines */
    if(region->col0 == 0 && region->col1 == contents->col_count) {
        DWORD cell0 = region->row0 * (DWORD)contents->col_count;
        DWORD cell_count = (region->row1 - region->row0) * (DWORD)contents->col_count;

        if(contents->mask & TABLE_CONTENTS_VALUES)
            memset(&contents->values[cell0], 0, cell_count * sizeof(value_t));
        if(contents->mask & TABLE_CONTENTS_TYPES)
            memset(&contents->types[cell0], 0, cell_count * sizeof(value_type_t*));
        if(contents->mask & TABLE_CONTENTS_FOREGROUNDS)
            memset(&contents->foregrounds[cell0], 0, cell_count * sizeof(COLORREF));
        if(contents->mask & TABLE_CONTENTS_BACKGROUNDS)
            __stosd(&contents->backgrounds[cell0], MC_CLR_NONE, cell_count);
        if(contents->mask & TABLE_CONTENTS_FLAGS)
            __stosd(&contents->backgrounds[cell0], MC_CLR_DEFAULT, cell_count);

        return;
    }

    /* Case 2: general case */
    for(row = region->row0; row < region->row1; row++) {
        DWORD cell0 = row * (DWORD)contents->col_count + region->col0;
        DWORD cell_count = region->col1 - region->col0;

        if(contents->mask & TABLE_CONTENTS_VALUES)
            memset(&contents->values[cell0], 0, cell_count * sizeof(value_t));
        if(contents->mask & TABLE_CONTENTS_TYPES)
            memset(&contents->types[cell0], 0, cell_count * sizeof(value_type_t*));
        if(contents->mask & TABLE_CONTENTS_FOREGROUNDS)
            memset(&contents->foregrounds[cell0], 0, cell_count * sizeof(COLORREF));
        if(contents->mask & TABLE_CONTENTS_BACKGROUNDS)
            __stosd(&contents->backgrounds[cell0], MC_CLR_NONE, cell_count);
        if(contents->mask & TABLE_CONTENTS_FLAGS)
            __stosd(&contents->backgrounds[cell0], MC_CLR_DEFAULT, cell_count);
    }
}

static void
table_contents_free_region(table_contents_t* contents, table_region_t* region)
{
    WORD row, col;

    for(row = region->row0; row < region->row1; row++) {
        for(col = region->col0; col < region->col1; col++) {
            DWORD index = row * (DWORD)contents->col_count + col;
            value_type_t* t;

            if(contents->values[index] == NULL)
                continue;

            if(TABLE_CONTENTS_IS_HOMOGENOUS(contents))
                t = contents->type;
            else
                t = contents->types[index];

            MC_ASSERT(t != NULL);
            t->free(contents->values[index]);
        }
    }
}

static void
table_contents_move_region(table_contents_t* contents_from, table_region_t* region_from,
                           table_contents_t* contents_to, table_region_t* region_to)
{
    WORD i;

    MC_ASSERT(contents_from->mask == contents_to->mask);

    /* Size of both regions must be the same */
    MC_ASSERT(region_from->col1 - region_from->col0 == region_to->col1 - region_to->col0);
    MC_ASSERT(region_from->row1 - region_from->row0 == region_to->row1 - region_to->row0);

    /* Case 0: zero-sized regions */
    if(region_from->col1 - region_from->col0 == 0  ||
       region_from->row1 - region_from->row0 == 0)
        return;

    /* Case 1: both regions contain complete lines */
    if(region_from->col0 == 0  &&  region_to->col0 == 0  &&
       region_from->col1 == contents_from->col_count  &&
       region_to->col1 == contents_from->col_count) {
        DWORD cellfrom0 = region_from->row0 * (DWORD)contents_from->col_count;
        DWORD cellto0 = region_to->row0 * (DWORD)contents_to->col_count;
        DWORD cell_count = (region_from->row1 - region_from->row0) * (DWORD)contents_from->col_count;

        if(contents_from->mask & TABLE_CONTENTS_VALUES)
            memcpy(&contents_to->values[cellto0], &contents_from->values[cellfrom0], cell_count * sizeof(value_t));
        if(contents_from->mask & TABLE_CONTENTS_TYPES)
            memcpy(&contents_to->types[cellto0], &contents_from->types[cellfrom0], cell_count * sizeof(value_type_t*));
        if(contents_from->mask & TABLE_CONTENTS_FOREGROUNDS)
            memcpy(&contents_to->foregrounds[cellto0], &contents_from->foregrounds[cellfrom0], cell_count * sizeof(COLORREF));
        if(contents_from->mask & TABLE_CONTENTS_BACKGROUNDS)
            memcpy(&contents_to->backgrounds[cellto0], &contents_from->backgrounds[cellfrom0], cell_count * sizeof(COLORREF));
        if(contents_from->mask & TABLE_CONTENTS_FLAGS)
            memcpy(&contents_to->flags[cellto0], &contents_from->flags[cellfrom0], cell_count * sizeof(BYTE));

        return;
    }

    /* Case 2: general case */
    for(i = 0; i < region_from->row1 - region_from->row0; i++) {
        DWORD cellfrom0 = (region_from->row0 + i) * (DWORD)contents_from->col_count + region_from->col0;
        DWORD cellto0 = (region_to->row0 + i) * (DWORD)contents_to->col_count + region_to->col0;
        DWORD cell_count = region_from->col1 - region_from->col0;

        if(contents_from->mask & TABLE_CONTENTS_VALUES)
            memcpy(&contents_to->values[cellto0], &contents_from->values[cellfrom0], cell_count * sizeof(value_t));
        if(contents_from->mask & TABLE_CONTENTS_TYPES)
            memcpy(&contents_to->types[cellto0], &contents_from->types[cellfrom0], cell_count * sizeof(value_type_t*));
        if(contents_from->mask & TABLE_CONTENTS_FOREGROUNDS)
            memcpy(&contents_to->foregrounds[cellto0], &contents_from->foregrounds[cellfrom0], cell_count * sizeof(COLORREF));
        if(contents_from->mask & TABLE_CONTENTS_BACKGROUNDS)
            memcpy(&contents_to->backgrounds[cellto0], &contents_from->backgrounds[cellfrom0], cell_count * sizeof(COLORREF));
        if(contents_from->mask & TABLE_CONTENTS_FLAGS)
            memcpy(&contents_to->flags[cellto0], &contents_from->flags[cellfrom0], cell_count * sizeof(BYTE));
    }
}


/****************************
 *** Table implementation ***
 ****************************/

struct table_tag {
    DWORD refs;
    table_contents_t contents;
    view_list_t vlist;
};


static inline void
table_refresh_views(table_t* table, table_region_t* region)
{
    view_list_refresh(&table->vlist, region);
}

table_t*
table_create(WORD col_count, WORD row_count, value_type_t* cell_type, DWORD flags)
{
    table_t* table;
    table_region_t region;
    DWORD mask = TABLE_CONTENTS_VALUES | TABLE_CONTENTS_TYPES;

    if(!(flags & MC_TF_NOCELLFOREGROUND))
        mask |= TABLE_CONTENTS_FOREGROUNDS;
    if(!(flags & MC_TF_NOCELLBACKGROUND))
        mask |= TABLE_CONTENTS_BACKGROUNDS;
    if(!(flags & MC_TF_NOCELLFLAGS))
        mask |= TABLE_CONTENTS_FLAGS;

    TABLE_TRACE("table_create(%d, %d, %p, 0x%x)",
                (int)col_count, (int)row_count, cell_type, mask);

    table = (table_t*) malloc(sizeof(table_t));
    if(MC_ERR(table == NULL)) {
        MC_TRACE("table_create: malloc() failed.");
        return NULL;
    }

    if(MC_ERR(table_contents_alloc(&table->contents, cell_type,
                                   col_count, row_count, mask) != 0)) {
        MC_TRACE("table_create: table_contents_alloc() failed.");
        free(table);
        return NULL;
    }

    region.col0 = 0;
    region.row0 = 0;
    region.col1 = col_count;
    region.row1 = row_count;
    table_contents_init_region(&table->contents, &region);

    table->refs = 1;
    view_list_init(&table->vlist);
    return table;
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
        table_region_t region;

        TABLE_TRACE("table_unref(%p): Freeing", table);
        MC_ASSERT(VIEW_LIST_IS_EMPTY(&table->vlist));

        region.col0 = 0;
        region.row0 = 0;
        region.col1 = table->contents.col_count;
        region.row1 = table->contents.row_count;
        table_contents_free_region(&table->contents, &region);

        table_contents_free(&table->contents);
        free(table);
    }
}

WORD
table_col_count(const table_t* table)
{
    return table->contents.col_count;
}

WORD
table_row_count(const table_t* table)
{
    return table->contents.row_count;
}

void
table_paint_cell(const table_t* table, WORD col, WORD row, HDC dc, RECT* rect)
{
    DWORD index = row * (DWORD)table->contents.col_count + col;
    value_t* value = table->contents.values[index];
    value_type_t* type;
    DWORD flags = 0;

    if(TABLE_CONTENTS_IS_HOMOGENOUS(&table->contents))
        type = table->contents.type;
    else
        type = table->contents.types[index];

    if(table->contents.mask & TABLE_CONTENTS_FLAGS)
        flags |= table->contents.flags[index] & 0xf;  /* alignement */

    if(type != NULL)
        type->paint(value, dc, rect, flags);
}

int
table_resize(table_t* table, WORD col_count, WORD row_count)
{
    table_contents_t contents;
    value_type_t* homotype;
    table_region_t region;
    
    if(col_count == table->contents.col_count && row_count == table->contents.row_count)
        return 0;

    homotype = TABLE_CONTENTS_IS_HOMOGENOUS(&table->contents) ? table->contents.type : NULL;

    if(MC_ERR(table_contents_alloc(&contents, homotype, col_count, row_count,
                                   table->contents.mask) != 0)) {
        MC_TRACE("table_resize: table_contents_alloc() failed.");
        return -1;
    }
    
    /* Move intersected contents */
    region.col0 = 0;
    region.row0 = 0;
    region.col1 = MC_MIN(col_count, table->contents.col_count);
    region.row1 = MC_MIN(row_count, table->contents.row_count);
    table_contents_move_region(&table->contents, &region, &contents, &region);
    
    /* Handle difference in col_count */
    region.col0 = region.col1;
    if(col_count > table->contents.col_count) {
        region.col1 = col_count;
        table_contents_init_region(&contents, &region);
    } else if(col_count < table->contents.col_count) {
        region.col1 = table->contents.col_count;
        table_contents_free_region(&contents, &region);
    }
    
    /* Handle difference in row_count */
    region.col0 = 0;
    region.row0 = region.row1;
    if(row_count > table->contents.row_count) {
        region.row1 = row_count;
        table_contents_init_region(&contents, &region);
    } else if(row_count < table->contents.row_count) {
        region.row1 = table->contents.row_count;
        table_contents_free_region(&contents, &region);
    }
    
    /* Install new contents */
    table_contents_free(&table->contents);
    memcpy(&table->contents, &contents, sizeof(table_contents_t));

    table_refresh_views(table, NULL);
    return 0;
}

void
table_clear(table_t* table)
{
    table_region_t region;
    
    region.col0 = 0;
    region.row0 = 0;
    region.col1 = table->contents.col_count;
    region.row1 = table->contents.row_count;
    table_contents_free_region(&table->contents, &region);
    table_contents_init_region(&table->contents, &region);

    table_refresh_views(table, &region);
}

void
table_get_cell(const table_t* table, WORD col, WORD row, MC_TABLECELL* cell)
{
    DWORD index = row * table->contents.col_count + col;

    if(cell->fMask & MC_TCM_VALUE) {
        if(TABLE_CONTENTS_IS_HOMOGENOUS(&table->contents))
            cell->hType = table->contents.type;
        else
            cell->hType = table->contents.types[index];
        cell->hValue = table->contents.values[index];
    }

    if(cell->fMask & MC_TCM_FOREGROUND) {
        if(table->contents.mask & TABLE_CONTENTS_FOREGROUNDS)
            cell->crForeground = table->contents.foregrounds[index];
        else
            cell->crForeground = MC_CLR_DEFAULT;
    }

    if(cell->fMask & MC_TCM_BACKGROUND) {
        if(table->contents.mask & TABLE_CONTENTS_BACKGROUNDS)
            cell->crBackground = table->contents.backgrounds[index];
        else
            cell->crBackground = MC_CLR_NONE;
    }

    if(cell->fMask & MC_TCM_FLAGS) {
        if(table->contents.mask & TABLE_CONTENTS_FLAGS)
            cell->dwFlags = table->contents.flags[index];
        else
            cell->dwFlags = 0;
    }
}

void
table_set_cell(table_t* table, WORD col, WORD row, MC_TABLECELL* cell)
{
    DWORD index = row * table->contents.col_count + col;
    table_region_t region;

    if(cell->fMask & MC_TCM_VALUE) {
        if(TABLE_CONTENTS_IS_HOMOGENOUS(&table->contents)) {
            MC_ASSERT(cell->hType == NULL  ||  cell->hType == table->contents.type);
            if(table->contents.values[index] != NULL)
                table->contents.type->free(table->contents.values[index]);
        } else {
            if(table->contents.values[index] != NULL)
                table->contents.types[index]->free(table->contents.values[index]);
            table->contents.types[index] = (value_type_t*) cell->hType;
        }
    
        table->contents.values[index] = (value_t) cell->hValue;
    }

    if(cell->fMask & MC_TCM_FOREGROUND) {
        if(table->contents.mask & TABLE_CONTENTS_FOREGROUNDS)
            table->contents.foregrounds[index] = cell->crForeground;
    }

    if(cell->fMask & MC_TCM_BACKGROUND) {
        if(table->contents.mask & TABLE_CONTENTS_BACKGROUNDS)
            table->contents.backgrounds[index] = cell->crBackground;
    }

    if(cell->fMask & MC_TCM_FLAGS) {
        if(table->contents.mask & TABLE_CONTENTS_FLAGS)
            table->contents.flags[index] = cell->dwFlags;
    }

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

MC_HTABLE MCTRL_API
mcTable_Create(WORD wColumnCount, WORD wRowCount, MC_HVALUETYPE hType, DWORD dwFlags)
{
    return (MC_HTABLE) table_create(wColumnCount, wRowCount, (value_type_t*)hType, dwFlags);
}

void MCTRL_API
mcTable_AddRef(MC_HTABLE hTable)
{
    if(hTable)
        table_ref((table_t*)hTable);
}

void MCTRL_API
mcTable_Release(MC_HTABLE hTable)
{
    if(hTable)
        table_unref((table_t*)hTable);
}

WORD MCTRL_API
mcTable_ColumnCount(MC_HTABLE hTable)
{
    return (hTable ? table_col_count((table_t*)hTable) : 0);
}

WORD MCTRL_API
mcTable_RowCount(MC_HTABLE hTable)
{
    return (hTable ? table_row_count((table_t*)hTable) : 0);
}

BOOL MCTRL_API
mcTable_Resize(MC_HTABLE hTable, WORD wColumnCount, WORD wRowCount)
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
mcTable_Clear(MC_HTABLE hTable)
{
    if(hTable)
        table_clear((table_t*)hTable);
}

BOOL MCTRL_API
mcTable_SetCell(MC_HTABLE hTable, WORD wCol, WORD wRow,
                MC_HVALUETYPE hType, MC_HVALUE hValue)
{
    MC_TABLECELL cell;

    cell.fMask = MC_TCM_VALUE;
    cell.hType = hType;
    cell.hValue = hValue;
    return mcTable_SetCellEx(hTable, wCol, wRow, &cell);
}

BOOL MCTRL_API
mcTable_GetCell(MC_HTABLE hTable, WORD wCol, WORD wRow,
                MC_HVALUETYPE* phType, MC_HVALUE* phValue)
{
    MC_TABLECELL cell;

    cell.fMask = MC_TCM_VALUE;
    if(MC_ERR(!mcTable_GetCellEx(hTable, wCol, wRow, &cell)))
        return FALSE;
    if(phType) *phType = cell.hType;
    if(phValue) *phValue = cell.hValue;
    return TRUE;
}

BOOL MCTRL_API
mcTable_SetCellEx(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELL* pCell)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_SetCell: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(wCol >= table->contents.col_count || wRow >= table->contents.row_count)) {
        MC_TRACE("mcTable_SetCell: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(pCell->fMask & ~MC_TCM_ALL)) {
        MC_TRACE("mcTable_SetCell: Unsupported pCell->fMask");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR((pCell->fMask & MC_TCM_VALUE)  &&
              TABLE_CONTENTS_IS_HOMOGENOUS(&table->contents)  &&
              pCell->hType != table->contents.type)) {
        MC_TRACE("mcTable_SetCell: Value type mismatch in homogenous table.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    table_set_cell(table, wCol, wRow, pCell);
    return TRUE;
}

BOOL MCTRL_API
mcTable_GetCellEx(MC_HTABLE hTable, WORD wCol, WORD wRow, MC_TABLECELL* pCell)
{
    table_t* table = (table_t*) hTable;

    if(MC_ERR(hTable == NULL)) {
        MC_TRACE("mcTable_GetCell: hTable == NULL");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(wCol >= table->contents.col_count || wRow >= table->contents.row_count)) {
        MC_TRACE("mcTable_GetCell: [wCol, wRow] out of range.");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(MC_ERR(pCell->fMask & ~MC_TCM_ALL)) {
        MC_TRACE("mcTable_SetCell: Unsupported pCell->fMask");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    table_get_cell(table, wCol, wRow, pCell);
    return TRUE;
}
