/*
 * Copyright (c) 2008-2012 Martin Mitas
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

#ifndef MC_MISC_H
#define MC_MISC_H

#include <stdio.h>
#include <stddef.h>
#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

#include "compat.h"
#include "debug.h"
#include "resource.h"
#include "version.h"



/***************************
 *** Miscelaneous macros ***
 ***************************/

/* Mininum and maximum et al. */
#define MC_MIN(a,b)            ((a) < (b) ? (a) : (b))
#define MC_MAX(a,b)            ((a) > (b) ? (a) : (b))
#define MC_ABS(a)              ((a) >= 0 ? (a) : -(a))
#define MC_SIGN(a)             ((a) > 0 ? +1 : ((a) < 0 ? -1 : 0))

/* Specifier for variable-sized array (last member in structures) */
#if defined __STDC__ && __STDC_VERSION__ >= 199901L
    #define MC_VARARRAY_SIZE     /* empty */
#elif defined _MSC_VER && _MSC_VER >= 1500
    #define MC_VARARRAY_SIZE     /* empty */
#elif defined __GNUC__
    #define MC_VARARRAY_SIZE     0
#else
    #define MC_VARARRAY_SIZE     1
#endif

/* Get count of records in an array */
#define MC_ARRAY_SIZE(array)   (sizeof((array)) / sizeof((array)[0]))

/* Offset of struct member */
#if defined offsetof
    #define MC_OFFSETOF(type, member)   offsetof(type, member)
#elif defined __GNUC__  &&  __GNUC__ >= 4
    #define MC_OFFSETOF(type, member)   __builtin_offsetof(type, member)
#else
    #define MC_OFFSETOF(type, member)   ((size_t) &((type*)0)->member)
#endif

/* Pointer to container structure */
#define MC_CONTAINEROF(ptr, type, member)                                \
            ((type*)((BYTE*)(ptr) - MC_OFFSETOF(type, member)))

/* Rectangle macros */
#define MC_WIDTH(rect)                 ((rect)->right - (rect)->left)
#define MC_HEIGHT(rect)                ((rect)->bottom - (rect)->top)
#define MC_CONTAINS(rect, pt)                                            \
        ((rect)->left <= (pt)->x  &&  (pt)->x < (rect)->right  &&        \
         (rect)->top <= (pt)->y  &&  (pt)->y < (rect)->bottom)
#define MC_RECT_IS_EMPTY(rect)                                           \
        ((rect)->left >= (rect)->right  ||  (rect)->top >= (rect)->bottom)
#define MC_SET_RECT(rect, x0, y0, x1, y1)                                \
        do { (rect)->left = x0; (rect)->top = y0;                        \
             (rect)->right = x1; (rect)->bottom = y1; } while(0)
#define MC_COPY_RECT(rect, rect2)                                        \
        MC_SET_RECT((rect), (rect2)->left, (rect2)->top,                 \
                            (rect2)->right, (rect2)->bottom)
#define MC_OFFSET_RECT(rect, dx, dy)                                     \
        do { (rect)->left += dx; (rect)->top += dx;                      \
             (rect)->right += dx; (rect)->bottom += dx; } while(0)
#define MC_INFLATE_RECT(rect, dx, dy)                                    \
        do { (rect)->left -= dx; (rect)->top -= dx;                      \
             (rect)->right += dx; (rect)->bottom += dx; } while(0)


/* Inlined memcpy(), memmove() et al.
 * Prefer these for memory blocks known a priori to be small. */
#define MC_MEMCPY(a, b, s)                                               \
    do {                                                                 \
        BYTE* __a = (BYTE*)(a);                                          \
        BYTE* __b = (BYTE*)(b);                                          \
        size_t __s = (size_t)(s);                                        \
        while(__s-- > 0)  *__a++ = *__b++;                               \
    } while(0)
#define MC_MEMMOVE(a, b, s)                                              \
    do {                                                                 \
        BYTE* __a = (BYTE*)(a);                                          \
        BYTE* __b = (BYTE*)(b);                                          \
        size_t __s = (size_t)(s);                                        \
        if(__a < __b) {                                                  \
            while(__s-- > 0)  *__a++ = *__b++;                           \
        } else {                                                         \
            __a += __s; __b += __s;                                      \
            while(__s-- > 0)  *(--__a) = *(--__b);                       \
        }                                                                \
    } while(0)
#define MC_MEMSWAP(a, b, s)                                              \
    do {                                                                 \
        BYTE* __a = (BYTE*)(a);                                          \
        BYTE* __b = (BYTE*)(b);                                          \
        size_t __s = (size_t)(s);                                        \
        while(__s-- > 0) {                                               \
            BYTE __tmp = *__a;                                           \
            *__a++ = *__b;                                               \
            *__b++ = __tmp;                                              \
        }                                                                \
    } while(0)

/* Macros telling the compiler that the condition is likely or unlikely to
 * be true. (If supported) it allows better optimization. Use only sparingly
 * in important loops where it really matters. Programmers are often bad in
 * this kind of prediction. */
#if defined __GNUC__  &&  __GNUC__ >= 3
    #define MC_LIKELY(condition)       __builtin_expect(!!(condition), !0)
    #define MC_UNLIKELY(condition)     __builtin_expect((condition), 0)
#else
    #define MC_LIKELY(condition)       (condition)
    #define MC_UNLIKELY(condition)     (condition)
#endif

/* Macro for wrapping error conditions. */
#define MC_ERR(condition)              MC_UNLIKELY(condition)


/***************
 *** Globals ***
 ***************/

extern HINSTANCE mc_instance;

/* Checking OS version (compare with normal operators: ==, <, <= etc.) */
#define MC_WIN_VER(platform, major, minor)                               \
    (((platform) << 16) | ((major) << 8) | ((minor) << 0))

#define MC_WIN_95          MC_WIN_VER(1, 4, 0)
#define MC_WIN_98          MC_WIN_VER(1, 4, 10)
#define MC_WIN_ME          MC_WIN_VER(1, 4, 90)
#define MC_WIN_NT4         MC_WIN_VER(2, 4, 0)
#define MC_WIN_2000        MC_WIN_VER(2, 5, 0)
#define MC_WIN_XP          MC_WIN_VER(2, 5, 1)
#define MC_WIN_S2003       MC_WIN_VER(2, 5, 2)
#define MC_WIN_VISTA       MC_WIN_VER(2, 6, 0)
#define MC_WIN_S2008       MC_WIN_VER(2, 6, 0)
#define MC_WIN_7           MC_WIN_VER(2, 6, 1)
#define MC_WIN_S2008R2     MC_WIN_VER(2, 6, 1)
#define MC_WIN_8           MC_WIN_VER(2, 6, 2)

extern DWORD mc_win_version;

/* Checking version of COMCTRL32.DLL */
#define MC_DLL_VER(major, minor)                                         \
    (((major) << 16) | ((minor) << 0))

extern DWORD mc_comctl32_version;

/* Image list of glyphs used throughout mCtrl */
#define MC_BMP_GLYPH_W               9   /* glyph image size */
#define MC_BMP_GLYPH_H               9

#define MC_BMP_GLYPH_CLOSE           0   /* indexes of particular glyphs */
#define MC_BMP_GLYPH_MORE_OPTIONS    1
#define MC_BMP_GLYPH_CHEVRON_L       2
#define MC_BMP_GLYPH_CHEVRON_R       3
#define MC_BMP_GLYPH_EXPANDED        4
#define MC_BMP_GLYPH_COLLAPSED       5

extern HIMAGELIST mc_bmp_glyphs;



/**************************
 *** String conversions ***
 **************************/

/* String type identifier. */
#define MC_STRA             1  /* Multibyte (ANSI) string */
#define MC_STRW             2  /* Unicode (UTF-16LE) string */
#ifdef UNICODE
    #define MC_STRT    MC_STRW
#else
    #define MC_STRT    MC_STRA
#endif


/* Copies zero-terminated string from_str to a newly allocated buffer.
 * A conversion can be applied during the copying depending on the type
 * of the from_str and the requested new string. Returns NULL on failure.
 *
 * Caller is then responsible to free the returned buffer.
 */
#define mc_str(from_str, from_type, to_type)                             \
                    mc_str_n(from_str, from_type, -1, to_type, NULL)

/* Copies string of specified length (which can contain '\0' characters)
 * to a newly allocated buffer. A conversion can be applied during the copying
 * depending on the type of the from_str and the requested new string. Returns
 * NULL on failure.
 */
void* mc_str_n(const void* from_str, int from_type, int from_len,
               int to_type, int* ptr_to_len);

/* Copies zero-terminated sring from_str to the buffer to_str. A conversion
 * can be applied during the copying depending on teh type of the from_str
 * and the requested new string. Only up to max_len-1 characters is copied.
 * The resulted string is always zero-terminated.
 */
void mc_str_inbuf(const void* from_str, int from_type,
                  void* to_str, int to_type, int to_str_bufsize);



/*************************
 *** Utility functions ***
 *************************/

/* Convenient wrapper of InitCommonControls/InitCommonControlsEx. */
void mc_init_common_controls(DWORD icc);

/* Detect icon size */
void mc_icon_size(HICON icon, SIZE* size);

/* Determine approximate font on-screen dimension in pixels (height and avg.
 * char width). Used to auto-adjust item size in complex controls. */
void mc_font_size(HFONT font, SIZE* size);

/* Send simple (i.e. using only NMHDR) notification */
LRESULT mc_send_notify(HWND parent, HWND win, UINT code);

/* Easy-to-use clipping functions */
HRGN mc_clip_get(HDC dc);
void mc_clip_set(HDC dc, LONG left, LONG top, LONG right, LONG bottom);
void mc_clip_reset(HDC dc, HRGN old_clip);

/* Convert wheel messages into line count */
int mc_wheel_scroll(HWND win, BOOL is_vertical, int wheel_delta);
#define mc_wheel_reset()   mc_wheel_scroll(NULL, 0, 0)


/**********************
 *** Initialization ***
 **********************/

int mc_init(void);
void mc_fini(void);


#endif  /* MC_MISC_H  */
