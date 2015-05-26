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


static uint64_t anim_timestamp_freq = 0;


#define ANIM_CURR_VALUE(anim,i)    (anim)->var[(i)]
#define ANIM_START_VALUE(anim,i)   (anim)->var[(anim)->var_count + (i)]
#define ANIM_END_VALUE(anim,i)     (anim)->var[2 * (anim)->var_count + (i)]


static inline uint64_t
anim_timestamp(void)
{
    LARGE_INTEGER ts;
    QueryPerformanceCounter(&ts);
    return ts.QuadPart;
}


anim_t*
anim_start_ex(HWND win, UINT timer_id, UINT duration, UINT freq,
              anim_var_t* vars, UINT var_count, LPARAM lp)
{
    anim_t* anim;
    int i;

    if(MC_ERR(anim_timestamp_freq == 0)) {
        MC_TRACE("anim_start_ex: Perf. counter frequency is zero.");
        return NULL;
    }

    MC_ASSERT(duration != 0  ||  var_count == 0);

    anim = (anim_t*) malloc(sizeof(anim_t) + 3 * var_count * sizeof(float));
    if(MC_ERR(anim == NULL)) {
        MC_TRACE("anim_start_ex: malloc() failed.");
        return NULL;
    }

    if(var_count > 0) {
        for(i = 0; i < var_count; i++) {
            ANIM_START_VALUE(anim, i) = vars[i].f0;
            ANIM_END_VALUE(anim, i) = vars[i].f1;
        }

        memcpy(&ANIM_CURR_VALUE(anim, 0), &ANIM_START_VALUE(anim, 0), var_count * sizeof(float));
    }

    anim->lp = lp;
    anim->win = win;
    anim->timer_id = timer_id;
    anim->var_count = var_count;

    anim->timestamp_start = anim_timestamp();
    anim->timestamp_prev_frame = anim->timestamp_start;
    anim->timestamp_curr_frame = anim->timestamp_start;
    if(duration >= 0)
        anim->timestamp_end = anim->timestamp_start + (anim_timestamp_freq * (uint64_t)duration / 1000);
    else
        anim->timestamp_end = UINT64_MAX;

    if(MC_ERR(SetTimer(win, timer_id, 1000 / freq, NULL) == 0)) {
        MC_TRACE_ERR("anim_start_ex: SetTimer() failed.");
        free(anim);
        return NULL;
    }

    return anim;
}

BOOL
anim_step(anim_t* anim)
{
    uint64_t timestamp_now;
    int i;

    timestamp_now = anim_timestamp();

    anim->timestamp_prev_frame = anim->timestamp_curr_frame;

    if(timestamp_now < anim->timestamp_end) {
        if(anim->var_count > 0) {
            float factor = (float)(timestamp_now - anim->timestamp_start) /
                           (float)(anim->timestamp_end - anim->timestamp_start);

            for(i = 0; i < anim->var_count; i++) {
                ANIM_CURR_VALUE(anim, i) = ANIM_START_VALUE(anim, i) +
                        factor * (ANIM_END_VALUE(anim, i) - ANIM_START_VALUE(anim, i));
            }
        }

        anim->timestamp_curr_frame = timestamp_now;
        return TRUE;
    } else {
        if(anim->var_count > 0) {
            memcpy(&ANIM_CURR_VALUE(anim, 0), &ANIM_END_VALUE(anim, 0),
                   anim->var_count * sizeof(float));
        }

        anim->timestamp_curr_frame = anim->timestamp_end;
        return FALSE;
    }
}

DWORD
anim_time(anim_t* anim, BOOL since_start)
{
    uint64_t diff;

    if(since_start)
        diff = anim->timestamp_curr_frame - anim->timestamp_start;
    else
        diff = anim->timestamp_curr_frame - anim->timestamp_prev_frame;

    return 1000 * diff / anim_timestamp_freq;
}

void
anim_stop(anim_t* anim)
{
    KillTimer(anim->win, anim->timer_id);
    free(anim);
}

int
anim_init_module(void)
{
    LARGE_INTEGER perf_freq;

    /* According to MSDN, the functions QueryPerformanceFrequency() and
     * QueryPerformanceCounter() never fail on XP and newer Windows versions.
     *
     * On Win 2000, this may depend on availability of a HW support.
     * To not over-complicate the code, I assume the functions either always
     * fail or always succeed.
     *
     * If it fails, we make anim_init_module() still succeed but any call to
     * anim_start_ex() will return NULL to disable animation. (Caller should
     * fall back to instant change instead of animation.)
     */

    if(QueryPerformanceFrequency(&perf_freq)) {
        anim_timestamp_freq = perf_freq.QuadPart;
    } else {
        MC_TRACE_ERR("anim_init_module: QueryPerformanceFrequency() failed.");
        anim_timestamp_freq = 0;
    }

    return 0;
}

void
anim_fini_module(void)
{
    /* noop */
}
