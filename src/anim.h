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
    uint64_t timestamp_start;
    uint64_t timestamp_prev_frame;
    uint64_t timestamp_curr_frame;
    uint64_t timestamp_end;
    LPARAM lp;
    HWND win;
    UINT timer_id;
    UINT var_count;
    float var[MC_ARRAY_FLEXIBLE_SIZE_SPEC];   /* (3 * var_count) */
};


/* Range specifying a linear development of a variable during the animation.
 * (Supported only for "finite" animations, i.e. those which have non-zero
 * duration set in anim_start_ex()).
 */
typedef struct anim_var_tag anim_var_t;
struct anim_var_tag {
    float f0;    /* Initial value (at the start of animation) */
    float f1;    /* The final value (at the end of variable) */
};


/* Start a new animation.
 *  -- Allocates and sets up the structure.
 *  -- Initializes all animation variables.
 *  -- If duration is zero, the anim_step() never returns non-zero and
 *     the animation continues unless caller decides to stop it with anim_stop().
 *  -- Starts a timer with SetTimer(), accordingly to the desired frequency
 *     (frames per second).
 */
anim_t* anim_start_ex(HWND win, UINT timer_id, UINT duration, UINT freq,
                      anim_var_t* vars, UINT var_count, LPARAM lp);

static inline anim_t* anim_start(HWND win, UINT timer_id, UINT duration, UINT freq, LPARAM lp)
    { return anim_start_ex(win, timer_id, duration, freq, NULL, 0, lp); }

static inline LPARAM anim_lparam(anim_t* anim)
    { return anim->lp; }

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
DWORD anim_time(anim_t* anim, BOOL since_start);

/* Gets animation variable.
 *  -- Index specifies the respective variable as specified in anim_start_ex().
 *  -- Asks about animation variable as computed in the last anim_step() call.
 */
static inline float anim_var_value(anim_t* anim, UINT var_index)
    { MC_ASSERT(var_index < anim->var_count); return anim->var[var_index]; }

/* Gets a current animation progress (in a range of 0.0f ... 1.0f).
 * Valid only for finite animations. */
static inline float anim_progress(anim_t* anim)
    { return (float)(anim->timestamp_curr_frame - anim->timestamp_start) /
             (float)(anim->timestamp_end - anim->timestamp_start); }

static inline BOOL anim_is_done(anim_t* anim)
    { return (anim->timestamp_curr_frame >= anim->timestamp_end); }

/* Stops the animation and releases all resources associated with it.
 *  -- Stops the animation timer with KillTimer().
 *  -- Frees the animation structure so the handle becomes invalid.
 */
void anim_stop(anim_t* anim);


#endif  /* MC_ANIM_H */
