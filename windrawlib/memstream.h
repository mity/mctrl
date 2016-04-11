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

#ifndef WD_MEMSTREAM_H
#define WD_MEMSTREAM_H

#include "misc.h"


/* The caller is responsible to ensure the data (or the resource) are valid
 * and immutable for the lifetime of the stream. The stream does not copy the
 * data and reads them directly from the given source.
 *
 * The caller should release the stream as a standard COM object, i.e. via
 * method IStream::Release().
 */

IStream* memstream_create(const BYTE* buffer, ULONG size);

IStream* memstream_create_from_resource(HINSTANCE instance,
                        const WCHAR* res_type, const WCHAR* res_name);


#endif  /* WD_MEMSTREAM_H */
