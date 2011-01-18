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

#ifndef MC_THEME_H
#define MC_THEME_H

#include "misc.h"
#include <tmschema.h>


/* Fake HTHEME definition as we don't want #include <uxtheme.h> */
#define HTHEME HANDLE


/* Preprocessor magic to declare extern variables of pointers to 
 * UXTHEME.DLL functions. Each is declared as theme_XXXX where XXXX
 * is the name of the UXTHEME function, e.g. theme_OpenThemeData. 
 *
 * Note that theme_OpenThemeData() is always valid after theme_init().
 * If UXTHEME.DLL is not available, or the application does not use it,
 * we set the pointer to our internal dummy function which then just always 
 * return NULL.
 *
 * Thus the control implementations can call it anytime and then only decide 
 * depending whether it has a valid theme handle or NULL. When NULL, no other
 * UXTHEME function can be called!
 */
#define THEME_FN(rettype, name, params)             \
    extern rettype (WINAPI* theme_##name)params;
#include "theme_fn.h"
#undef THEME_FN


int theme_init(void);
void theme_fini(void);
    
    
#endif  /* MC_THEME_H */
