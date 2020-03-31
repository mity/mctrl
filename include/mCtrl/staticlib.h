/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2020 Martin Mitas
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

#ifndef MCTRL_STATICLIB_H
#define MCTRL_STATICLIB_H

#include <mCtrl/_defs.h>
#include <mCtrl/_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Helper header when using mCtrl as a static library.
 *
 * @attention This is a new and experimental feature. It may be subject to any
 * change, even a removal, if it turns out as too problematic.
 *
 * @note Currently, building of the static library is by default disabled and
 * the pre-built binary packages do not contain the static libraries. If you
 * want to use the static lib, you have to build mCtrl from sources and enable
 * the feature by specifying @c -DCMAKE_BUILD_STATIC on cmake command line.
 *
 * @note This header is not included by the all-in-one @c mCtrl.h. Include it
 * manually if and only if you link with mCtrl as a static library.
 * This helper provides prototypes of global initialization and termination
 * (which is otherwise implemented by @c DllMain() when linking with DLL.)
 *
 *
 * @section sec_static_init Static Library Initialization
 *
 * When linking with the static library, application has to call explicitly
 * function @ref mcInitialize() before any other per-module initialization
 * is performed, and @ref mcTerminate() after mCtrl is not used anymore.
 *
 * These functions perform some initialization or clean-up which normally takes
 * place when @c DllMain() of @c MCTRL.DLL is called.
 *
 * Unlike other per-module initialization functions, there is no reference
 * counting and both the functions should be called exactly once.
 *
 * Application may call these functions from the context of its @c DllMain()
 * (when linking the static mCtrl lib into a DLL target).
 *
 *
 * @section sec_static_resources Static Library and Resources.
 *
 * Some mCtrl controls require some resources like e.g. cursors or bitmaps
 * used when painting the control.
 *
 * Normally, these resources are embedded in @c MCTRL.DLL, and mCtrl code
 * loads them normally via @c LoadResource() or other appropriate Win32 API
 * function.
 *
 * However, this is not possible when using mCtrl as a static library. If the
 * application uses any mCtrl control or functionality which requires loading
 * any such resource, the application developer must make sure the required
 * resource is available in the @c .EXE or @c .DLL module which links with the
 * mCtrl static library.
 *
 * The static library allows to specify a custom resource ID base so that
 * application developer can place all the resources into a range of his choice
 * where it does not collide with with resource IDs of the application itself.
 * This ID base has then be passed as the 2nd argument of @ref mcInitialize().
 *
 * In other words, if @c MCTRL.DLL uses some particular resource with ID 100
 * then, in the case of the static lib, mCtrl code will attempt to load the
 * resource with ID equal to <tt>(100 + iResourceIdBase)</tt> where @c
 * iResourceIdBase is the custom resource ID base.
 *
 * Note application does not need to provide all resources available in @c
 * MCTRL.DLL, only a subset required by the functionality the application uses.
 *
 * @attention Please remember we cannot provide any compatibility guarantees
 * for the resources. New resources may be added or some resources can be
 * modified or removed in the future versions of mCtrl. When using mCtrl as a
 * static lib, application developer has to check whether such a change has
 * happened whenever migrating to a new mCtrl version.
 */


/**
 * Perform global initialization (when mCtrl is linked in as a static library).
 *
 * @param hInstance Handle of the .EXE or .DLL module the static library is
 * linked into.
 * @param iResourceIdBase See @ref sec_static_resources
 * @return @c TRUE if the initialization is successful, @c FALSE otherwise.
 */
BOOL MCTRL_API mcInitialize(HINSTANCE hInstance, int iResourceIdBase);

/**
 * Perform global clean-up (when mCtrl is linked in as a static library).
 */
void MCTRL_API mcTerminate(void);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_STATICLIB_H */
