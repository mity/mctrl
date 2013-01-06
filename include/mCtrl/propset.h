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

#ifndef MCTRL_PROPSET_H
#define MCTRL_PROPSET_H

#include <mCtrl/defs.h>
#include <mCtrl/value.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Item set (data model for property view control)
 *
 * The property set is a container of property items. It serves as a back-end
 * for the property view control (@ref MC_WC_PROPVIEW).
 */


/**
 * @brief Opaque property set handle.
 */
typedef void* MC_HPROPSET;


/**
 * @anchor MC_PSIMF_xxxx
 * @name MC_PROPSETITEM::fMask Bits
 */
/*@{*/

/** @ref MC_PROPSETITEMW::pszText and @ref MC_PROPSETITEMW::cchTextMax, or
 *  @ref MC_PROPSETITEMA::pszText and @ref MC_PROPSETITEMA::cchTextMax are valid. */
#define MC_PSIMF_TEXT                 (0x00000001)
/** @ref MC_PROPSETITEMW::hValue, or @ref MC_PROPSETITEMA::hValue is valid. */
#define MC_PSIMF_VALUE                (0x00000002)
/** @ref MC_PROPSETITEMW::lParam or @ref MC_PROPSETITEMA::lParam is valid. */
#define MC_PSIMF_LPARAM               (0x00000004)
/** @ref MC_PROPSETITEMW::dwFlags or @ref MC_PROPSETITEMA::dwFlags is valid. */
#define MC_PSIMF_FLAGS                (0x00000008)

/*@}*/


/**
 * @anchor MC_PSIF_xxxx
 * @name Property set item flags
 */
/*@{*/

// TODO

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure describing a single property (unicode variant).
 *
 * Before using the structure you have always have to set bits of the member
 * @c fMask to indicate what structure members are valid (on input) or
 * expected (on output), and also set the member @c iItem to determine an
 * index of property in the set.
 *
 * @sa mcPropSet_InsertItemW mcPropSet_SetItemW mcPropSet_GetItemW
 */
typedef struct MC_PROPSETITEMW_tag {
    /** @brief Bitmask specifying what members are valid. See @ref MC_PSIMF_xxxx. */
    DWORD fMask;
    /** @brief Index of the property. */
    int iItem;
    /** @brief Text label of the property. */
    LPWSTR pszText;
    /** @brief Maximal number of characters in @c pszText. Used only on output. */
    int cchTextMax;
    /** @brief Handle of property value. */
    MC_HVALUE hValue;
    /** @brief User data. */
    LPARAM lParam;
    /** @brief Property flags. */
    DWORD dwFlags;
} MC_PROPSETITEMW;

/**
 * @brief Structure describing a single property (ANSI variant).
 *
 * Before using the structure you have always have to set bits of the member
 * @c fMask to indicate what structure members are valid (on input) or
 * expected (on output), and also set the member @c iItem to determine an
 * index of property in the set.
 *
 * @sa mcPropSet_InsertItemA mcPropSet_SetItemA mcPropSet_GetItemA
 */
typedef struct MC_PROPSETITEMA_tag {
    /** @brief Bitmask specifying what members are valid. See @ref MC_PSIMF_xxxx. */
    DWORD fMask;
    /** @brief Index of the property. */
    int iItem;
    /** @brief Text label of the property. */
    LPSTR pszText;
    /** @brief Maximal number of characters in @c pszText. Used only on output. */
    int cchTextMax;
    /** @brief Handle of property value type. */
    MC_HVALUE hValue;
    /** @brief User data. */
    LPARAM lParam;
    /** @brief Property flags. */
    DWORD dwFlags;
} MC_PROPSETITEMA;

/*@}*/


/**
 * @anchor MC_PSF_xxxx
 * @name Property set flags
 */
/*@{*/

/** @brief Sort items alphabetically. */
#define MC_PSF_SORTITEMS             (0x00000004L)

/*@}*/


/**
 * @name Functions
 */
/*@{*/

/**
 * @brief Create new property set.
 *
 * @param[in] dwFlags Flags of the new property set.
 * @return Handle of the property set, or @c NULL if the function fails.
 */
MC_HPROPSET MCTRL_API mcPropSet_Create(DWORD dwFlags);

/**
 * @brief Increment reference counter of the property set.
 *
 * @param[in] hPropSet The property set.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropSet_AddRef(MC_HPROPSET hPropSet);

/**
 * @brief Decrement reference counter of the property set.
 *
 * If the reference counter drops to zero, all resources allocated for
 * the property set are released.
 *
 * @param[in] hPropSet The property set.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropSet_Release(MC_HPROPSET hPropSet);

/**
 * @brief Get count of properties in the property set.
 *
 * @param[in] hPropSet The property set.
 * @return The count, or @c -1 on failure.
 */
int MCTRL_API mcPropSet_GetItemCount(MC_HPROPSET hPropSet);

/**
 * @brief Insert new item into the property set (unicode variant).
 *
 * Note the item may be inserted to different position then requested with
 * @c pItem->iItem, if the property set was created with the flag
 * @c MC_PSF_SORTITEMS.
 *
 * @param[in] hPropSet The property set.
 * @param[in] pItem The item.
 * @return Index of the inserted item, or @c -1 on failure.
 */
int MCTRL_API mcPropSet_InsertItemW(MC_HPROPSET hPropSet, MC_PROPSETITEMW* pItem);

/**
 * @brief Insert new item into the property set (ANSI variant).
 *
 * Note the item may be inserted to different position then requested with
 * @c pItem->iItem, if the property set was created with the flag
 * @c MC_PSF_SORTITEMS.
 *
 * @param[in] hPropSet The property set.
 * @param[in] pItem The item.
 * @return Index of the inserted item, or @c -1 on failure.
 */
int MCTRL_API mcPropSet_InsertItemA(MC_HPROPSET hPropSet, MC_PROPSETITEMA* pItem);

/**
 * @brief Get some attributes of an item in the property set (unicode variant).
 *
 * @param[in] hPropSet The property set.
 * @param[in,out] pItem Item structure.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropSet_GetItemW(MC_HPROPSET hPropSet, MC_PROPSETITEMW* pItem);

/**
 * @brief Get some attributes of an item in the property set (ANSI variant).
 *
 * @param[in] hPropSet The property set.
 * @param[in,out] pItem Item structure to be filled.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropSet_GetItemA(MC_HPROPSET hPropSet, MC_PROPSETITEMA* pItem);

/**
 * @brief Set some attributes of an item int the property set (unicode variant).
 *
 * Note that if the property set was created with the flag @c MC_PSF_SORTITEMS
 * then the item can be moved to a new position in the property set. You can
 * detect this by examining the return value.
 *
 * @param[in] hPropSet The property set.
 * @param[in] pItem Item structure.
 * @return Index of the item after the operation, or @c -1 on failure.
 */
int MCTRL_API mcPropSet_SetItemW(MC_HPROPSET hPropSet, MC_PROPSETITEMW* pItem);

/**
 * @brief Set some attributes of an item int the property set (ANSI variant).
 *
 * Note that if the property set was created with the flag @c MC_PSF_SORTITEMS
 * then the item can be moved to a new position in the property set. You can
 * detect this by examining the return value.
 *
 * @param[in] hPropSet The property set.
 * @param[in] pItem Item structure.
 * @return Index of the item after the operation, or @c -1 on failure.
 */
int MCTRL_API mcPropSet_SetItemA(MC_HPROPSET hPropSet, MC_PROPSETITEMA* pItem);

/**
 * @brief Delete an item from the proprty set.
 *
 * @param[in] hPropSet The property set.
 * @param[in] iItem Index of the item.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropSet_DeleteItem(MC_HPROPSET hPropSet, int iItem);

/**
 * @brief Delete all items of the proprty set.
 *
 * @param[in] hPropSet The property set.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcPropSet_DeleteAllItems(MC_HPROPSET hPropSet);

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_PROPERTYW MC_PROPERTYA */
#define MC_PROPSETITEM          MCTRL_NAME_AW(MC_PROPSETITEM)
/** Unicode-resolution alias. @sa mcPropSet_InsertItemW mcPropSet_InsertItemA */
#define mcPropSet_InsertItem    MCTRL_NAME_AW(mcPropSet_InsertItem)
/** Unicode-resolution alias. @sa mcPropSet_SetItemW mcPropSet_SetItemA */
#define mcPropSet_SetItem       MCTRL_NAME_AW(mcPropSet_SetItem)
/** Unicode-resolution alias. @sa mcPropSet_GetItemW mcPropSet_GetItemA */
#define mcPropSet_GetItem       MCTRL_NAME_AW(mcPropSet_GetItem)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_PROPSET_H */
