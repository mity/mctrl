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

#ifndef MC_COMPAT_H
#define MC_COMPAT_H


/* This file is here to hide differences between various build environments
 * and toolchains. */


/*********************************
 *** MSVC Compatibility hacks  ***
 *********************************/

#ifdef _MSC_VER
	/* Disable warning C4996 ("This function or variable may be unsafe.") */
	#pragma warning( disable : 4996 )

	/* MSVC does not understand inline when building as pure C (not C++) */
	#ifndef __cplusplus
		#define inline
	#endif

	/* MS platform SDK #defines [GS]etWindowLongPtr as plain [GS]etWindowLong for 
	 * x86 builds, without any casting, hence causing lots of compiler 
	 * warnings C4312. Lets workaround it. */
	#ifndef _WIN64
		#ifdef GetWindowLongPtrA
			#undef GetWindowLongPtrA
			#define GetWindowLongPtrA(win,ix)  (intptr_t)GetWindowLongA(win,ix)
		#endif
		#ifdef GetWindowLongPtrW
			#undef GetWindowLongPtrW
			#define GetWindowLongPtrW(win,ix)  (intptr_t)GetWindowLongW(win,ix)
		#endif
		#ifdef SetWindowLongPtrA
			#undef SetWindowLongPtrA
			#define SetWindowLongPtrA(win,ix,val)  SetWindowLongA(win,ix,(LONG)val)
		#endif
		#ifdef SetWindowLongPtrW
			#undef SetWindowLongPtrW
			#define SetWindowLongPtrW(win,ix,val)  SetWindowLongW(win,ix,(LONG)val)
		#endif
	#endif
#endif


/******************************************
 *** Missing constants in mingw headers ***
 ******************************************/
 
/* Some of them should be constants from enumeration, other are just #defines.
 * However as we cannot use preprocessor to detect if enums or their members
 * are missing, we always #define it here. */

#ifndef BST_HOT
    #define BST_HOT      0x0200
#endif

#ifndef DT_HIDEPREFIX
    #define DT_HIDEPREFIX 0x00100000
#endif

#ifndef UISF_HIDEFOCUS
    #define UISF_HIDEFOCUS  0x1
#endif

#ifndef UISF_HIDEACCEL
    #define UISF_HIDEACCEL  0x2
#endif

#ifndef UIS_SET
    #define UIS_SET 1
#endif

#ifndef UIS_CLEAR
    #define UIS_CLEAR 2
#endif

#ifndef UIS_INITIALIZE
    #define UIS_INITIALIZE 3
#endif


/********************************************
 *** Hack for broken COM headers in mingw ***
 ********************************************/

/* There is a lot of COM interfaces missing in the w32api package (as of 
 * version 3.13) of mingw project. Hence we use copy of those headers 
 * from mingw-w64 project which seems to be more complete.
 * 
 * These are placed in the com/ subdirectory. The below are hacks
 * hiding incompatibilities between headeres mingw-w64 and mingw.
 */

#if defined __GNUC__  &&  !defined SHANDLE_PTR
    #define SHANDLE_PTR HANDLE_PTR
#endif


#endif  /* MC_COMPAT_H */
