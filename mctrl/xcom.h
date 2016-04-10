/*
 * Copyright (c) 2015 Martin Mitas
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

#ifndef MC_XCOM_H
#define MC_XCOM_H

#include "misc.h"


/* The purpose of this is to not force COM-unaware applications to initialize
 * COM just for MCTRL.DLL, yet to not stand in a way if the application does so.
 * (See http://blogs.msdn.com/b/larryosterman/archive/2004/05/12/130541.aspx)
 *
 * The function ensures COM is initialized (if not it calls CoInitialize()
 * or some of its friends), and then it just calls CoCreateInstance().
 */
void* xcom_init_create(const CLSID* clsid, DWORD context, const IID* iid);

/* Calls CoUninitialize() if it was initialized in previous xcom_create_init().
 * Otherwise noop. */
void xcom_uninit(void);


#endif  /* MC_XCOM_H */
