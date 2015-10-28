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

#ifndef MC_ANIM_H
#define MC_ANIM_H

#include "misc.h"


/* Structure representing the animation. */
typedef struct anim_tag anim_t;
struct anim_tag {
    HWND win;
    DWORD time_start;
    DWORD time_prev_frame;
    DWORD time_curr_frame;
    DWORD time_end;
};


#define ANIM_DEFAULT_FREQUENCY         50
#define ANIM_UNLIMITED_DURATION         0

/* Get the extra data associated with the animation. */
#define ANIM_EXTRA_DATA(anim, type)     ((type*)((anim) + 1))


/* Start a new animation.
 *  -- Allocates and sets up the structure.
 *  -- If duration is ANIM_UNLIMITED_DURATION, the anim_step() never returns
 *     non-zero and the animation continues unless caller decides to stop it
 *     with anim_stop().
 *  -- Starts a timer with SetTimer(), accordingly to the desired frequency
 *     (frames per second).
 */
anim_t* anim_start_ex(HWND win, DWORD duration, DWORD freq,
                      void* extra_bytes, size_t extra_size);

static inline anim_t* anim_start(HWND win, DWORD duration, DWORD freq)
    { return anim_start_ex(win, duration, freq, NULL, 0); }

static inline UINT_PTR anim_timer_id(anim_t* anim)
    { return (UINT_PTR) anim; }

/* Performs animation step.
 *  -- Can be called anytime between anim_start() and anim_end(), but typically
 *     it is called from a WM_TIMER handler.
 *  -- Updates all animation variables according to the passed time.
 *  -- Returns TRUE if the animation did not yet reach the duration and should
 *     continue, or FALSE if it reached the time duration.
 *  -- If it returns FALSE, caller should call anim_stop() to release its
 *     resources..
 */
BOOL anim_step(anim_t* anim);

/* Gets how much time in milliseconds has passed since the previous
 * anim_step() call, or since the animation start. */
static inline DWORD anim_time(anim_t* anim)
    { return (anim->time_curr_frame - anim->time_start); }
static inline DWORD anim_frame_time(anim_t* anim)
    { return (anim->time_curr_frame - anim->time_prev_frame); }

/* Gets a current animation progress (in a range of 0.0f ... 1.0f).
 * Valid only for finite animations. */
static inline float anim_progress(anim_t* anim)
    { return (float)(anim->time_curr_frame - anim->time_start) /
             (float)(anim->time_end - anim->time_start); }

BOOL anim_is_done(anim_t* anim);

/* Stops the animation and releases all resources associated with it.
 *  -- Stops the animation timer with KillTimer().
 *  -- Frees the animation structure so the handle becomes invalid.
 */
void anim_stop(anim_t* anim);


#endif  /* MC_ANIM_H */
