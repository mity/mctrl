/*
 * Copyright (c) 2008-2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MCTRL_MDITAB_H
#define MCTRL_MDITAB_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief MDI tab control (@c MC_WC_MDITAB).
 *
 * This control is a replacement for standard multiple document interface 
 * (MDI), as that interface seems to be outdated, and does not reflect 
 * modern GUI requirements.
 * 
 * Instead this control provides user experience similar to the web browsers
 * with tabbing support.
 *
 * The control is very similar to the standard tab control from 
 * @c COMCTL32.DLL, both visually and from developer's point of view.
 * There two main differences: 
 * - @c MC_WC_MDITAB does not have the main bodyfor contents of the tab; 
 *   i.e. the contents of the tab is not really rendered in child window of 
 *   the @c MC_WC_MDITAB control.
 * - The contol laso provides few auxiliary buttons on it. They might be 
 *   invisible depending on the control style and internal state. There is 
 *   a button to close the current tab, to show pop-up menu of all tabs
 *   and finally buttons scrolling the tabs to left or right if there are too 
 *   many.
 *
 * Styles, messages and notifications the control supports mostly correspond 
 * to subset of messages and styles of the standard tab control. The 
 * counterparts have generally also the same names (differing only in prefix
 * of the identifiers). If you are familiar with the standard tab control, 
 * you should be able to adopt @c MC_WC_MDITAB very quickly.
 * However please note that the messages and styles are not interchangeable: 
 * The constants of styles and messages generally differ in their values.
 *
 * Altough the purpose of the control is to provide a replacement for the MDI,
 * the programmatic interfaces veru differs. If you want to replace MDI
 * with this control in an existing application, expect it will take some 
 * time.
 * - In MDI, the child MDI windows can be minimalized or restored so they
 *   would not cover whole MDI client window. @c MC_WC_MDITAB control does 
 *   not provide any replacement for this (anyway only very few users rarely 
 *   used this feature of MDI). If your application needs to allow access 
 *   to multiple documents simultaneously, you need to develop in other way
 *   with the @c MC_WC_MDITAB (e.g. to allow having multiple top level windows,
 *   each with the @c MC_WC_MDITAB to manage the documents open in each 
 *   particular window).
 * - MDI absolutely expects that the application has its submenu Window, with 
 *   all the commands like Tile horizontally or vertically, or too select 
 *   another MDI document. @c MC_WC_MDITAB control does not expect that 
 *   (still you are free to implement any menu you like ;-)
 *
 * These standard messages are handled by @c MC_WC_MDITAB control:
 * - @c WM_GETFONT
 * - @c WM_SETFONT
 * - @c WM_SETREDRAW
 */
 

/**
 * Registers the MDI tab control class.
 * @return @c TRUE on success, @c FALSE on failure.
 * @see sec_initialization
 */
BOOL MCTRL_API mcMditab_Initialize(void);

/**
 * Unregisters the MDI tab control class.
 * @see sec_initialization
 */
void MCTRL_API mcMditab_Terminate(void);


/*@{*/
/**
 * @name Window class
 */
#define MC_WC_MDITABW        L"mCtrl.mditab"  /**< Unicode window class name */
#define MC_WC_MDITABA         "mCtrl.mditab"  /**< ANSI window class name */
#ifdef UNICODE
    #define MC_WC_MDITAB     MC_WC_MDITABW    /**< Unicode-resolution alias */
#else
    #define MC_WC_MDITAB     MC_WC_MDITABA
#endif
/*@}*/

/*@{*/
/**
 * @name mc_MT_ITEM::dwMask bits
 * These specifies which members of mc_MT_ITEM structure are valid.
 */
#define MC_MTIF_TEXT         (1 << 0)  /**< mc_MT_ITEM::pszText is valid. */
#define MC_MTIF_IMAGE        (1 << 1)  /**< mc_MT_ITEM::iImage is valid. */
#define MC_MTIF_PARAM        (1 << 2)  /**< mc_MT_ITEM::lParam is valid. */
/*@}*/


/*@{*/
/**
 * Structure for manipulating with tab item (unicode version).
 * @sa mc_MT_INSERTITEM mc_MT_DELETEITEM mc_MT_SETITEM mc_MT_GETITEM
 */
typedef struct mc_MT_ITEMW_tag {
    /**
     * @brief Mask specifying which other members are valid. 
     * 
     * On input, set this member to mark which members of the stucture are 
     * valid. use any combination of @c MC_MTIF_TEXT, @c MC_MTIF_IMAGE and
     * @c MC_MTIF_PARAM.
     */
    DWORD dwMask;    
    /** @brief Text label of the tab. */
    LPWSTR pszText;  
    /** @brief Number of characters in @c psxText. Used only on output. */
    int cchTextMax;
    /** @brief Index into control image list. */
    int iImage;
    /** @brief User data. */
    LPARAM lParam;
} mc_MT_ITEMW;

/**
 * Structure for manipulating with tab item (ANSI version).
 * @sa mc_MT_INSERTITEM mc_MT_DELETEITEM mc_MT_SETITEM mc_MT_GETITEM
 */
typedef struct mc_MT_ITEMA_tag {
    /**
     * @brief Mask specifying which other members are valid. 
     * 
     * Set to combination of @c MC_MTIF_TEXT, @c MC_MTIF_IMAGE and
     * @c MC_MTIF_PARAM.
     */
    DWORD dwMask;
    
    /** @brief Text label of the tab. */
    LPSTR pszText;
    /** @brief Number of characters in @c psxText. Used only on output. */
    int cchTextMax;
    /** @brief Index into control image list. */
    int iImage;
    /** @brief User data. */
    LPARAM lParam;
} mc_MT_ITEMA;
/*@}*/


#ifdef UNICODE
    /**
     * Structure for manipulating with tab item.
     * @sa mc_MT_INSERTITEM mc_MT_DELETEITEM mc_MT_SETITEM mc_MT_GETITEM
     */
    #define mc_MT_ITEM          mc_MT_ITEMW
#else
    #define mc_MT_ITEM          mc_MT_ITEMA
#endif


/*@{*/
/**
 * @name mc_MT_HITTESTINFO::flags bits
 */
/**
 * The hit test coordinates are outside of any tabs.
 */
#define MC_MTHT_NOWHERE              (1 << 0)

/**
 * The coordinates hit the tab on its icon.
 */
#define MC_MTHT_ONITEMICON           (1 << 1)

/**
 * The coordinates hit the tab, but not its icon, neither its close button.
 */
#define MC_MTHT_ONITEMLABEL          (1 << 2)

/**
 * The coordinates hit the tab on its close button.
 */
#define MC_MTHT_ONITEMCLOSEBUTTON    (1 << 3)

/**
 * The coordinates hit the tab anywhere in its rectangle.
 */
#define MC_MTHT_ONITEM               \
    (MC_MTHT_ONITEMICON | MC_MTHT_ONITEMLABEL | MC_MTHT_ONITEMCLOSEBUTTON)
/*@}*/

/**
 * @brief Structure for @c MC_MT_HITTEST message.
 */
typedef struct mc_MT_HITTESTINFO_tag {
    /** @brief Coordinates to test. */
    POINT pt;
    /** @brief On output, set to the result of the test. */
    UINT flags;
} mc_MT_HITTESTINFO;


/**
 * @name Style constants
 */
/* @{ */
/** @brief Show close button on right side of the control. This is default. */
#define MC_MTS_CBONTOOLBAR          (0x00000000L)
/** @brief Not supported, reserved for future use. */
#define MC_MTS_CBONEACHTAB          (0x00000001L)
/** @brief Not supported, reserved for future use. */
#define MC_MTS_CBONACTIVETAB        (0x00000002L)
/** @brief Don't show close button */
#define MC_MTS_CBNONE               (0x00000003L)
/** @brief This is not valid style, its bitmask of @c MC_MTS_CBxxx styles. */
#define MC_MTS_CBMASK               (0x00000003L)

/** @brief Popup tab list button is shown always. This is default. */
#define MC_MTS_TLBALWAYS            (0x00000000L)
/** @brief Popup tab list button is shown if scrolling is triggered on. */
#define MC_MTS_TLBONSCROLL          (0x00000004L)
/** @brief Popup tab list button is never displayed. */
#define MC_MTS_TLBNEVER             (0x00000008L)
/** @brief This is not valid style, but bitmask of @c MC_NTS_TLBxxx styles. */
#define MC_MTS_TLBMASK              (0x0000000CL)

/** @brief Always shows scrolling buttons. */
#define MC_MTS_SCROLLALWAYS         (0x00000010L)

/** @brief Middle click closes a tab. */
#define MC_MTS_CLOSEONMCLICK        (0x00000020L)

/** @brief Mouse button down gains focus. */
#define MC_MTS_FOCUSONBUTTONDOWN    (0x00000040L)
/** @brief Never gains focus */
#define MC_MTS_FOCUSNEVER           (0x00000080L)
/** @brief This is not valid style, but bitmask of @c MC_NTS_FOCUSxxx styles. */
#define MC_MTS_FOCUSMASK            (0x000000C0L)
/* @} */


/**
 * Gets count of tabs.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) Count of tabs.
 */
#define MC_MTM_GETITEMCOUNT       (WM_USER + 100)

/**
 * Gets imagelist.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c HIMAGELIST) The image list, or @c NULL.
 *
 * @sa MC_MTM_SETIMAGELIST
 */
#define MC_MTM_GETIMAGELIST       (WM_USER + 101)

/**
 * Sets imagelist. The tab items can refer to the images in the list
 * with @c MC_MT_ITEM::iImage.
 * @param wParam Reserved, set to zero.
 * @param[in] lParam (@c HIMAGELIST) The imagelist.
 * @return (@c HIMAGELIST) Old image list, or @c NULL.
 *
 * @sa MC_MTM_GETIMAGELIST
 */
#define MC_MTM_SETIMAGELIST       (WM_USER + 102)

/**
 * Delete all tab items. Sends @c MC_MTN_DELETEALLITEMS notification
 * and (unless @c MC_MTN_DELETEALLITEMS prevents that) @c MC_MTN_DELETEITEM
 * for each tab.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @sa MC_MTM_DELETEITEM
 */
#define MC_MTM_DELETEALLITEMS     (WM_USER + 103)

/**
 * Inserts new tab into the tab control (unicode version). Note that the new 
 * item is @b not automatically selected.
 * @param[in] wParam (@c int) Index of the new item.
 * @param[in] lParam (@c mc_MT_ITEMW*) Pointer to detailed data of the new tab.
 * @return (@c int) index of the new tab, or @c -1 on failure.
 */
#define MC_MTM_INSERTITEMW        (WM_USER + 105)

/**
 * Inserts new tab into the tab control (ANSI version). Note that the new 
 * item is @b not automatically selected.
 * @param[in] wParam (@c int) Index of the new control.
 * @param[in] lParam (@c mc_MT_ITEMA*) Pointer to detailed data of the new tab.
 * @return (@c int) index of the new tab, or @c -1 on failure.
 */
#define MC_MTM_INSERTITEMA        (WM_USER + 106)

#ifdef UNICODE
    /**
     * Inserts new tab into the tab control.
     * @param[in] wParam (@c int) Index of the new item.
     * @param[in] lParam (@c mc_MT_ITEM*) Pointer to detailed data of the new 
     * tab.
     * @return (@c int) index of the new tab, or @c -1 on failure.
     */
    #define MC_MTM_INSERTITEM     MC_MTM_INSERTITEMW
#else
    #define MC_MTM_INSERTITEM     MC_MTM_INSERTITEMA
#endif

/**
 * Sets tab in the tab control (unicode version).
 * @param[in] wParam (@c int) Index of the item.
 * @param[in] lParam (@c mc_MT_ITEMW*) Pointer to detailed data of the tab.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MTM_SETITEMW           (WM_USER + 107)

/**
 * Sets tab in the tab control (ANSI version).
 * @param[in] wParam (@c int) Index of the item.
 * @param[in] lParam (@c mc_MT_ITEMA*) Pointer to detailed data of the tab.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MTM_SETITEMA           (WM_USER + 108)

#ifdef UNICODE
    /**
     * Sets tab in the tab control.
     * @param[in] wParam (@c int) Index of the item.
     * @param[in] lParam (@c mc_MT_ITEM*) Pointer to detailed data of the tab.
     * @return @c TRUE on success, @c FALSE otherwise.
     */
    #define MC_MTM_SETITEM        MC_MTM_SETITEMW
#else
    #define MC_MTM_SETITEM        MC_MTM_SETITEMA
#endif


/**
 * Gets tab data from the tab control (unicode version).
 * @param[in] wParam (@c int) Index of the item.
 * @param[in,out] lParam (@c mc_MT_ITEMW*) Pointer to detailed data of the 
 * tab, receiving the data according to @c mc_MT_ITEM::dwMask.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MTM_GETITEMW           (WM_USER + 109)

/**
 * Gets tab data from the tab control (ANSI version).
 * @param[in] wParam (@c int) Index of the item.
 * @param[in,out] lParam (@c mc_MT_ITEMA*) Pointer to detailed data of the 
 * tab, receiving the data according to @c mc_MT_ITEM::dwMask.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MTM_GETITEMA           (WM_USER + 110)

#ifdef UNICODE
    /**
     * Gets tab data from the tab control.
     * @param[in] wParam (@c int) Index of the item.
     * @param[in,out] lParam (@c mc_MT_ITEM*) Pointer to detailed data of the 
     * tab, receiving the data according to @c mc_MT_ITEM::dwMask.
     * @return @c TRUE on success, @c FALSE otherwise.
     */
    #define MC_MTM_GETITEM        MC_MTM_GETITEMW
#else
    #define MC_MTM_GETITEM        MC_MTM_GETITEMA
#endif

/**
 * Deletes the item. Sends @c MC_MTN_DELETEITEM notification to parent window.
 * @param[in] wParam (@c int) Index of tab to be deleted.
 * @param lParam Reserved, set to zero.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MTM_DELETEITEM         (WM_USER + 111)

/**
 * Tests which tab (and its part) is placed on specified position.
 * @param wParam Reserved, set to zero.
 * @param[in,out] lParam (@c mc_MT_HITTESTINFO*) Pointer to a hit test 
 * structure. Set @c mc_MT_HITTESTINFO::pt on input.
 * @return (@c int) Index of the hit tab, or -1.
 */
#define MC_MTM_HITTEST            (WM_USER + 112)

/**
 * Selects a tab.
 * @param[in] wParam (@c int) Index of the tab to select. 
 * @param lParam Reserved, set to zero.
 * @return (@c int) Index of previously selected tab, or @c -1.
 */
#define MC_MTM_SETCURSEL          (WM_USER + 113)

/** 
 * Gets indes of selected tab. 
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) Index of selected tab, or @c -1.
 */
#define MC_MTM_GETCURSEL          (WM_USER + 114)

/**
 * Asks to close item. It causes to send @c MC_MTN_CLOSEITEM notification
 * and depening on its return value it then can cause deleteing the item.
 * @param[in] wParam (@c int) Index of the item to be closed.
 * @param lParam Reserved, set to zero.
 * @return @c TRUE on success, @c FALSE otherwise.
 */
#define MC_MTM_CLOSEITEM          (WM_USER + 115)


/**
 * @brief Structure for messages @c MC_MTM_SETWIDTH and @c MC_MTM_GETWIDTH.
 */
typedef struct mc_MT_ITEMWIDTH_tag {
    /** @brief Default width for tabs, in pixels. */
    DWORD dwDefWidth;
    /** @brief Minimal width for tabs, in pixels. */
    DWORD dwMinWidth;
} mc_MT_ITEMWIDTH;

/**
 * Sets default and minimal width for each tab. If there is enough space,
 * all tabs have the default width. When there are too many widths, they are
 * made narrower so more tabs fit into the visible space area, but never
 * narrower then the minimal width.
 * @param wParam 
 * @param[in] lParam (@c mc_MT_ITEMWIDTH*) Pointer to a structure specifying 
 * the default and minimal widths. When @c NULL is passed, the values are 
 * reset to built-in defaults.
 * @return @c TRUE on success, @c FALSE otherwise.
 * @sa MC_MTM_GETITEMWIDTH
 */
#define MC_MTM_SETITEMWIDTH       (WM_USER + 116)

/**
 * Gets default and minimal width for each tab. 
 * @param wParam 
 * @param[out] lParam (@c mc_MT_ITEMWIDTH*) Pointer to a structure where the 
 * current widths will be set.
 * @return @c TRUE on success, @c FALSE otherwise.
 * @sa MC_MTM_SETITEMWIDTH
 */
#define MC_MTM_GETITEMWIDTH       (WM_USER + 117)

/**
 * @brief Structure for @c MC_MTN_SELCHANGE notification.
 */
typedef struct mc_NM_MT_SELCHANGE_tag {
    /** @brief Common notification structure header. */
    NMHDR hdr;  
    /** @brief Index of previously selected tab. */
    int iItemOld;  
    /** @brief Data of previously selected tab, or zero. */
    LPARAM lParamOld;  
    /** @brief Index of newly selected tab */
    int iItemNew;  
    /** @brief Data of newly selected tab, or zero. */
    LPARAM lParamNew;  
} mc_NM_MT_SELCHANGE;

/**
 * Controls sends this notification when other tab has been selected.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@c mc_NM_MT_SELCHANGE*) Pointer to a structure specifying
 * details about the selection change.
 * @return You should return zero , if you process the message.
 */
#define MC_MTN_SELCHANGE          (0xfffffddb)

/**
 * @brief Structure for @c MC_MTN_DELETEITEM notification.
 */
typedef struct mc_NM_MT_DELETEITEM_tag {
    /** @brief Common notification structure header. */
    NMHDR hdr;
    /** @brief Index of the item being deleted. */
    int iItem;
    /** @brief User data of the item being deleted. */
    LPARAM lParam;
} mc_NM_MT_DELETEITEM;

/**
 * Control sends this notification when a tab is being deleted. When called
 * the item still exists.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@c mc_NM_MT_DELETEITEM*) Pointer to a structure 
 * specifying details about the item being deleted.
 * @return You should return zero if you process the notification.
 */
#define MC_MTN_DELETEITEM         (0xfffffdd0)

/**
 * Control sends this notification when it processes @c MC_MTM_DELETEALLITEMS
 * message or when it is being destroyed. Depending on the value returned
 * from the notification, calling @c MC_MTN_DELETEITEM notifications for
 * all the items can be supressed.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@c NMHDR*)
 * @return You should return @c FALSE to receive subsequent 
 * @c MC_MTN_DELETEITEM for each item; or @c TRUE to supress sending them.
 */
#define MC_MTN_DELETEALLITEMS     (0xfffffdcf)


/**
 * @brief Structure for @c MC_MTN_CLOSEITEM notification.
 */
typedef struct mc_NM_MT_CLOSEITEM {
    /** @brief Common notification structure header. */
    NMHDR hdr;
    /** @brief Index of the item being closed. */
    int iItem;
    /** @brief User data of the control being closed. */
    LPARAM lParam;
} mc_NM_MT_CLOSEITEM;

/**
 * Control sends this notification when user requets closing a tab item.
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in] lParam (@c mc_NM_MT_CLOSEITEM*) Pointer to a structure specifying
 * details about the item being closed.
 * @return You should return @c FALSE to remove the tab (the tab is then 
 * deleted and @c MC_MTN_DELETEITEM notification is sent); or @c TRUE
 * to cancel the tab closure.
 */ 
#define MC_MTN_CLOSEITEM          (0xfffffdce)



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_MDITAB_H */
