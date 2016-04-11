/*
 * Copyright (c) 2016 Martin Mitas
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

#ifndef WD_LOCK_H
#define WD_LOCK_H

#include "misc.h"


extern CRITICAL_SECTION* wd_critical_section;


static inline void
wd_lock(void)
{
    if(wd_critical_section != NULL)
        EnterCriticalSection(wd_critical_section);
}

static inline void
wd_unlock(void)
{
    if(wd_critical_section != NULL)
        LeaveCriticalSection(wd_critical_section);
}


#endif  /* WD_LOCK_H */
