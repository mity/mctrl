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

#include "anim.h"


static inline int
anim_time_cmp(DWORD t1, DWORD t2)
{
    return (int32_t) t1 - (int32_t) t2;
}


anim_t* anim_start_ex(HWND win, DWORD duration, DWORD freq,
                      void* extra_bytes, size_t extra_size)
{
    anim_t* anim;

    anim = (anim_t*) malloc(sizeof(anim_t) + extra_size);
    if(MC_ERR(anim == NULL)) {
        MC_TRACE("anim_start_ex: malloc() failed.");
        return NULL;
    }

    anim->win = win;
    if(extra_size > 0)
        memcpy(anim+1, extra_bytes, extra_size);

    anim->time_start = GetTickCount();
    anim->time_prev_frame = anim->time_start;
    anim->time_curr_frame = anim->time_start;
    anim->time_end = anim->time_start + duration;

    if(MC_ERR(SetTimer(win, (UINT_PTR) anim, 1000 / freq, NULL) == 0)) {
        MC_TRACE_ERR("anim_start_ex: SetTimer() failed.");
        free(anim);
        return NULL;
    }

    return anim;
}

BOOL
anim_step(anim_t* anim)
{
    BOOL check_end = (anim->time_start != anim->time_end);
    DWORD now;

    now = GetTickCount();
    if(check_end  &&  anim_time_cmp(now, anim->time_end) > 0)
        now = anim->time_end;

    anim->time_prev_frame = anim->time_curr_frame;
    anim->time_curr_frame = now;

    if(check_end)
        return (anim_time_cmp(now, anim->time_end) < 0);
    else
        return TRUE;
}

BOOL
anim_is_done(anim_t* anim)
{
    return (anim_time_cmp(anim->time_curr_frame, anim->time_end) >= 0);
}

void
anim_stop(anim_t* anim)
{
    KillTimer(anim->win, (UINT_PTR) anim);
    free(anim);
}
