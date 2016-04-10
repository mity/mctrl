/*
 * Copyright (c) 2015-2016 Martin Mitas
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

#ifndef WD_BACKEND_DWRITE_H
#define WD_BACKEND_DWRITE_H

#include "misc.h"
#include "dummy/dwrite.h"


extern dummy_IDWriteFactory* dwrite_factory;


int dwrite_init(void);
void dwrite_fini(void);

void dwrite_default_user_locale(WCHAR buffer[LOCALE_NAME_MAX_LENGTH]);

dummy_IDWriteTextLayout* dwrite_create_text_layout(dummy_IDWriteTextFormat* tf,
            const WD_RECT* rect, const WCHAR* str, int len, DWORD flags);


#endif  /* WD_BACKEND_DWRITE_H */

