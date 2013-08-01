/*
 * Copyright (c) 2008-2011 Martin Mitas
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

#ifndef MCTRL_DEFS_H
#define MCTRL_DEFS_H

#include <windows.h>
#include <commctrl.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Common definitions
 *
 * This helper header file provides some macro definitions shared by other
 * public mCtrl headers. They all include this header so you shouldn't need
 * to include this header file directly.
 */


/**
 * Helper macro specifying the calling convention of functions exported
 * by @c MCTRL.DLL. You should not use it directly.
 */
#define MCTRL_API                   __stdcall


#ifdef UNICODE
     /** Helper macro for unicode resolution. You should not use it directly. */
     #define MCTRL_NAME_AW(name)    name##W
#else
     #define MCTRL_NAME_AW(name)    name##A
#endif


/**
 * @name Common Constants
 */
/*@{*/

/**
 * @brief No color.
 * @details This is defined to have the same  value and meaning as the constant
 * @c CLR_NONE from @c <commctrl.h> and applications can use these two macros
 * interchangeably.
 */
#define MC_CLR_NONE         CLR_NONE
/**
 * @brief Default color.
 * @details This is defined to have the same value and meaning as the constant
 * @c CLR_DEFAULT from @c <commctrl.h> and applications can use these two
 * macros interchangeably.
 */
#define MC_CLR_DEFAULT      CLR_DEFAULT

/**
 * @brief Index of no image in an image list.
 * @details This is defined to have the same value and meaning as the constant
 * @c I_IMAGENONE from @c <commctrl.h> and applications can use these two
 * macros interchangeably.
 */
#define MC_I_IMAGENONE      I_IMAGENONE

/**
 * @brief Index of no group.
 * @details This is defined to have the same value and meaning as the constant
 * @c I_GROUPIDNONE from @c <commctrl.h> and applications can use these two
 * macros interchangeably.
 */
#define MC_I_GROUPIDNONE    I_GROUPIDNONE

/*@}*/


/**
 * @name Control Specific Message Ranges
 */
/*@{*/

#define MC_EXM_FIRST        (WM_USER+0x4000 + 0)
#define MC_EXM_LAST         (WM_USER+0x4000 + 49)

#define MC_GM_FIRST         (WM_USER+0x4000 + 50)
#define MC_GM_LAST          (WM_USER+0x4000 + 199)

#define MC_HM_FIRST         (WM_USER+0x4000 + 200)
#define MC_HM_LAST          (WM_USER+0x4000 + 299)

#define MC_MTM_FIRST        (WM_USER+0x4000 + 300)
#define MC_MTM_LAST         (WM_USER+0x4000 + 399)

#define MC_MBM_FIRST        (WM_USER+0x4000 + 400)
#define MC_MBM_LAST         (WM_USER+0x4000 + 499)

#define MC_PVM_FIRST        (WM_USER+0x4000 + 500)
#define MC_PVM_LAST         (WM_USER+0x4000 + 599)

#define MC_CHM_FIRST        (WM_USER+0x4000 + 600)
#define MC_CHM_LAST         (WM_USER+0x4000 + 699)

#define MC_TLM_FIRST        (WM_USER+0x4000 + 700)
#define MC_TLM_LAST         (WM_USER+0x4000 + 799)

/*@}*/


/**
 * @name Control Specific Notification Ranges
 */
/*@{*/

#define MC_EXN_FIRST        (0x40000000 + 0)
#define MC_EXN_LAST         (0x40000000 + 49)

#define MC_GN_FIRST         (0x40000000 + 100)
#define MC_GN_LAST          (0x40000000 + 199)

#define MC_HN_FIRST         (0x40000000 + 200)
#define MC_HN_LAST          (0x40000000 + 299)

#define MC_MTN_FIRST        (0x40000000 + 300)
#define MC_MTN_LAST         (0x40000000 + 349)

#define MC_PVN_FIRST        (0x40000000 + 400)
#define MC_PVN_LAST         (0x40000000 + 499)

#define MC_CHN_FIRST        (0x40000000 + 500)
#define MC_CHN_LAST         (0x40000000 + 599)

#define MC_TLN_FIRST        (0x40000000 + 600)
#define MC_TLN_LAST         (0x40000000 + 699)

/*@}*/



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_DEFS_H */
