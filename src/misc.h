/*
 * Copyright (c) 2008-2013 Martin Mitas
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

#ifndef _USE_MATH_DEFINES
    /* This is needed for <math.h> from MSVC to define constant macros like
     * M_PI for example. */
    #define _USE_MATH_DEFINES
#endif

#include <stdio.h>
#include <stddef.h>
#include <tchar.h>
#include <math.h>
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <wingdi.h>

/*#include <windowsx.h>*/
/* This header is polluting namespace of macros too much. We are interested
 * only in macros GET_[XY]_LPARAM. */
#ifndef GET_X_LPARAM
    #define GET_X_LPARAM(lp)   ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
    #define GET_Y_LPARAM(lp)   ((int)(short)HIWORD(lp))
#endif

#include "compat.h"
#include "debug.h"
#include "resource.h"
#include "version.h"

#include "mCtrl/_defs.h"
#include "mCtrl/_common.h"


/****************************
 *** Miscellaneous Macros ***
 ****************************/

/* Whether we are unicode build */
#ifdef UNICODE
    #define MC_IS_UNICODE      TRUE
#else
    #define MC_IS_UNICODE      FALSE
#endif

/* Minimum and maximum et al. */
#define MC_MIN(a,b)            ((a) < (b) ? (a) : (b))
#define MC_MAX(a,b)            ((a) > (b) ? (a) : (b))
#define MC_MIN3(a,b,c)         MC_MIN(MC_MIN((a), (b)), (c))
#define MC_MAX3(a,b,c)         MC_MAX(MC_MAX((a), (b)), (c))
#define MC_ABS(a)              ((a) >= 0 ? (a) : -(a))
#define MC_SIGN(a)             ((a) > 0 ? +1 : ((a) < 0 ? -1 : 0))

/* Get count of records in an array */
#define MC_ARRAY_SIZE(array)   (sizeof((array)) / sizeof((array)[0]))


/* Offset of struct member */
#if defined offsetof
    #define MC_OFFSETOF(type, member)   offsetof(type, member)
#elif defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 40000
    #define MC_OFFSETOF(type, member)   __builtin_offsetof(type, member)
#else
    #define MC_OFFSETOF(type, member)   ((size_t) &((type*)0)->member)
#endif

/* Pointer to container structure */
#define MC_CONTAINEROF(ptr, type, member)                                \
            ((type*)((BYTE*)(ptr) - MC_OFFSETOF(type, member)))


/* Macros telling the compiler that the condition is likely or unlikely to
 * be true. (If supported) it allows better optimization. Use only sparingly
 * in important loops where it really matters. Programmers are often bad in
 * this kind of prediction. */
#if defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 30000
    #define MC_LIKELY(condition)       __builtin_expect(!!(condition), !0)
    #define MC_UNLIKELY(condition)     __builtin_expect(!!(condition), 0)
#else
    #define MC_LIKELY(condition)       (condition)
    #define MC_UNLIKELY(condition)     (condition)
#endif

/* Macro for wrapping error conditions. */
#define MC_ERR(condition)              MC_UNLIKELY(condition)


/* Send and PostMessage() wrappers to save some typing and casting. */
#define MC_SEND(win, msg, wp, lp)                                         \
            SendMessage((win), (msg), (WPARAM)(wp), (LPARAM)(lp))
#define MC_POST(win, msg, wp, lp)                                         \
            PostMessage((win), (msg), (WPARAM)(wp), (LPARAM)(lp))


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


/*************************************
 *** Memory Manipulation Utilities ***
 *************************************/

/* memcpy/memmove can be slow because of 'call' instruction for very small
 * blocks of memory. Hence if we know the size is small already in compile
 * time, we may inline it instead.
 *
 * (gcc has a capability of inlining of these built-in.)
 */

#ifndef MC_COMPILER_GCC
    static inline void
    mc_inlined_memcpy(void* addr0, const void* addr1, size_t n)
    {
        BYTE* iter0 = (BYTE*) addr0;
        const BYTE* iter1 = (const BYTE*) addr1;

        while(n-- > 0)
            *iter0++ = *iter1++;
    }
#else
    #define mc_inlined_memcpy memcpy
#endif

#ifndef MC_COMPILER_GCC
    static inline void
    mc_inlined_memmove(void* addr0, const void* addr1, size_t n)
    {
        BYTE* iter0 = (BYTE*) addr0;
        const BYTE* iter1 = (const BYTE*) addr1;

        if(iter0 < iter1) {
            while(n-- > 0)
                *iter0++ = *iter1++;
        } else {
            iter0 += n;
            iter1 += n;
            while(n-- > 0)
                *(--iter0) = *(--iter1);
        }
    }
#else
    #define mc_inlined_memmove memmove
#endif

/* Swapping two memory blocks. They must not overlay. */
static inline void
mc_inlined_memswap(void* addr0, void* addr1, size_t n)
{
    BYTE* iter0 = (BYTE*) addr0;
    BYTE* iter1 = (BYTE*) addr1;
    BYTE tmp;

    while(n-- > 0) {
        tmp = *iter0;
        *iter0++ = *iter1;
        *iter1++ = tmp;
    }
}

#define mc_memswap    mc_inlined_memswap


/************************
 *** String Utilities ***
 ************************/

/* String type identifier. */

typedef enum mc_str_type_tag mc_str_type_t;
enum mc_str_type_tag {
    MC_STRA = 1,   /* Multibyte (ANSI) string */
    MC_STRW = 2,   /* Unicode (UTF-16LE) string */
#ifdef UNICODE
    MC_STRT = MC_STRW
#else
    MC_STRT = MC_STRA
#endif
};


/* Loading strings from resources */

TCHAR* mc_str_load(UINT id);


/* Copying/converting strings into provided buffer */

void mc_str_inbuf_A2A(const char* from_str, char* to_str, int to_str_bufsize);
void mc_str_inbuf_W2W(const WCHAR* from_str, WCHAR* to_str, int to_str_bufsize);
void mc_str_inbuf_A2W(const char* from_str, WCHAR* to_str, int to_str_bufsize);
void mc_str_inbuf_W2A(const WCHAR* from_str, char* to_str, int to_str_bufsize);

static inline void
mc_str_inbuf(const void* from_str, mc_str_type_t from_type,
             void* to_str, mc_str_type_t to_type, int to_str_bufsize)
{
    if(from_type == to_type) {
        if(from_type == MC_STRA)
            mc_str_inbuf_A2A(from_str, to_str, to_str_bufsize);
        else
            mc_str_inbuf_W2W(from_str, to_str, to_str_bufsize);
    } else {
        if(from_type == MC_STRA)
            mc_str_inbuf_A2W(from_str, to_str, to_str_bufsize);
        else
            mc_str_inbuf_W2A(from_str, to_str, to_str_bufsize);
    }
}


/* Copying/converting strings into malloc'ed buffer */

char* mc_str_n_A2A(const char* from_str, int from_len, int* ptr_to_len);
WCHAR* mc_str_n_W2W(const WCHAR* from_str, int from_len, int* ptr_to_len);
WCHAR* mc_str_n_A2W(const char* from_str, int from_len, int* ptr_to_len);
char* mc_str_n_W2A(const WCHAR* from_str, int from_len, int* ptr_to_len);

static inline void*
mc_str_n(const void* from_str, mc_str_type_t from_type, int from_len,
         mc_str_type_t to_type, int* ptr_to_len)
{
    if(from_type == to_type) {
        if(from_type == MC_STRA)
            return mc_str_n_A2A(from_str, from_len, ptr_to_len);
        else
            return mc_str_n_W2W(from_str, from_len, ptr_to_len);
    } else {
        if(from_type == MC_STRA)
            return mc_str_n_A2W(from_str, from_len, ptr_to_len);
        else
            return mc_str_n_W2A(from_str, from_len, ptr_to_len);
    }
}

static inline char* mc_str_A2A(const char* from_str)
    { return mc_str_n_A2A(from_str, -1, NULL); }
static inline WCHAR* mc_str_W2W(const WCHAR* from_str)
    { return mc_str_n_W2W(from_str, -1, NULL); }
static inline WCHAR* mc_str_A2W(const char* from_str)
    { return mc_str_n_A2W(from_str, -1, NULL); }
static inline char* mc_str_W2A(const WCHAR* from_str)
    { return mc_str_n_W2A(from_str, -1, NULL); }

static inline void*
mc_str(const void* from_str, mc_str_type_t from_type, mc_str_type_t to_type)
{
    if(from_type == to_type) {
        if(from_type == MC_STRA)
            return mc_str_A2A(from_str);
        else
            return mc_str_W2W(from_str);
    } else {
        if(from_type == MC_STRA)
            return mc_str_A2W(from_str);
        else
            return mc_str_W2A(from_str);
    }
}


/*********************************
 *** Atomic Reference Counting ***
 *********************************/

#if defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 40100
    typedef int32_t mc_ref_t;
#else
    typedef LONG mc_ref_t;
#endif

static inline mc_ref_t
mc_ref(mc_ref_t* i)
{
#if defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 40700
    return __atomic_add_fetch(i, 1, __ATOMIC_RELAXED);
#elif defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 40100
    return __sync_add_and_fetch(i, 1);
#elif defined MC_COMPILER_MSVC
    return _InterlockedIncrement(i);
#else
    return InterlockedIncrement(i);
#endif
}

static inline mc_ref_t
mc_unref(mc_ref_t* i)
{
#if defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 40700
    /* See http://stackoverflow.com/questions/10268737/c11-atomics-and-intrusive-shared-pointer-reference-count */
    mc_ref_t ref = __atomic_sub_fetch(i, 1, __ATOMIC_RELEASE);
    if(ref == 0)
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return ref;
#elif defined MC_COMPILER_GCC  &&  MC_COMPILER_GCC >= 40100
    return __sync_sub_and_fetch(i, 1);
#elif defined MC_COMPILER_MSVC
    return _InterlockedDecrement(i);
#else
    return InterlockedDecrement(i);
#endif
}


/**********************
 *** Rect Utilities ***
 **********************/

/* These are so trivial that inlining these is probably always better then
 * calling Win32API functions like InflateRect() etc. */

static inline LONG mc_rect_width(const RECT* r)
    { return (r->right - r->left); }
static inline LONG mc_rect_height(const RECT* r)
    { return (r->bottom - r->top); }

/* These are too common, so lets save typing. */
#define mc_width   mc_rect_width
#define mc_height  mc_rect_height

static inline BOOL mc_rect_is_empty(const RECT* r)
    { return (r->left >= r->right  ||  r->top >= r->bottom); }

static inline void mc_rect_set(RECT* r, LONG x0, LONG y0, LONG x1, LONG y1)
    { r->left = x0; r->top = y0; r->right = x1; r->bottom = y1; }
static inline void mc_rect_copy(RECT* r0, const RECT* r1)
    { mc_rect_set(r0, r1->left, r1->top, r1->right, r1->bottom); }

static inline void mc_rect_offset(RECT* r, LONG dx, LONG dy)
    { r->left += dx; r->top += dy; r->right += dx; r->bottom += dy; }
static inline void mc_rect_inflate(RECT* r, LONG dx, LONG dy)
    { r->left -= dx; r->top -= dy; r->right += dx; r->bottom += dy; }

static inline BOOL mc_rect_contains_xy(const RECT* r, LONG x, LONG y)
    { return (r->left <= x  &&  x < r->right  &&  r->top <= y  &&  y < r->bottom); }
static inline BOOL mc_rect_contains_pt(const RECT* r, const POINT* pt)
    { return mc_rect_contains_xy(r, pt->x, pt->y); }
static inline BOOL mc_rect_contains_pos(const RECT* r, DWORD pos)
    { return mc_rect_contains_xy(r, GET_X_LPARAM(pos), GET_Y_LPARAM(pos)); }
static inline BOOL mc_rect_contains_rect(const RECT* r0, const RECT* r1)
    { return (r0->left <= r1->left  &&  r1->right <= r0->right  &&
              r0->top <= r1->top  &&  r1->bottom <= r0->bottom); }
static inline BOOL mc_rect_overlaps_rect(const RECT* r0, const RECT* r1)
    { return (r0->left < r1->right  &&  r0->top < r1->bottom  &&
              r0->right > r1->left  &&  r0->bottom > r1->top); }


/**************************
 *** Clipping Utilities ***
 **************************/

static inline HRGN
mc_clip_get(HDC dc)
{
    HRGN old_clip;

    old_clip = CreateRectRgn(0, 0, 0, 0);
    if(GetClipRgn(dc, old_clip) != 1) {
        DeleteObject(old_clip);
        return NULL;
    }

    return old_clip;
}

static inline void
mc_clip_set(HDC dc, LONG left, LONG top, LONG right, LONG bottom)
{
    HRGN clip;
    POINT pt[2] = { {left, top}, {right, bottom} };

    LPtoDP(dc, pt, 2);
    clip = CreateRectRgn(pt[0].x, pt[0].y, pt[1].x, pt[1].y);
    if(MC_ERR(clip == NULL)) {
        MC_TRACE("mc_clip_set: CreateRectRgn() failed.");
        return;
    }

    SelectClipRgn(dc, clip);
    DeleteObject(clip);
}

static inline void
mc_clip_reset(HDC dc, HRGN old_clip)
{
    SelectClipRgn(dc, old_clip);
    if(old_clip != NULL)
        DeleteObject(old_clip);
}


/*****************************
 *** Mouse Wheel Utilities ***
 *****************************/

int mc_wheel_scroll(HWND win, BOOL is_vertical, int wheel_delta, int lines_per_page);

static inline void
mc_wheel_reset(void)
{
    mc_wheel_scroll(NULL, 0, 0, 0);
}


/********************************
 *** Double-buffered Painting ***
 ********************************/

/* Note: To use optimized way (with bitmap caches), the thread should be
 * initialized with mcBufferedPaintInit(). */

void mc_doublebuffer(void* control, PAINTSTRUCT* ps,
             void (*func_paint)(void* /*control_data*/, HDC /*dc*/,
                                RECT* /*dirty_rect*/, BOOL /*erase*/));


/**************************
 *** Assorted Utilities ***
 **************************/

/* Convenient wrapper of InitCommonControls/InitCommonControlsEx. */
void mc_init_common_controls(DWORD icc);

/* Detect icon size */
void mc_icon_size(HICON icon, SIZE* size);
static inline UINT mc_icon_width(HICON icon)
    { SIZE s; mc_icon_size(icon, &s); return s.cx; }
static inline UINT mc_icon_height(HICON icon)
    { SIZE s; mc_icon_size(icon, &s); return s.cy; }

/* Determine approximate font on-screen dimension in pixels (height and avg.
 * char width). Used to auto-adjust item size in complex controls. */
void mc_font_size(HFONT font, SIZE* size);

void mc_string_size(const TCHAR* str, HFONT font, SIZE* size);
static inline UINT mc_string_width(const TCHAR* str, HFONT font)
    { SIZE s; mc_string_size(str, font, &s); return s.cx; }
static inline UINT mc_string_height(const TCHAR* str, HFONT font)
    { SIZE s; mc_string_size(str, font, &s); return s.cy; }

/* Converting pixels <--> DLUs */
int mc_pixels_from_dlus(HFONT font, int dlus, BOOL vert);
int mc_dlus_from_pixels(HFONT font, int pixels, BOOL vert);

/* Send simple (i.e. using only NMHDR) notification */
static inline LRESULT
mc_send_notify(HWND parent, HWND win, UINT code)
{
    NMHDR hdr;

    hdr.hwndFrom = win;
    hdr.idFrom = GetWindowLong(win, GWL_ID);
    hdr.code = code;

    return MC_SEND(parent, WM_NOTIFY, hdr.idFrom, &hdr);
}

/* TrackMouseEvent() convenient wrapper */
static inline void
mc_track_mouse(HWND win, DWORD flags)
{
    TRACKMOUSEEVENT tme;

    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = flags;
    tme.hwndTrack = win;
    tme.dwHoverTime = HOVER_DEFAULT;

    TrackMouseEvent(&tme);
}

/****************
 *** Tooltips ***
 ****************/
 
/* Creates a tooltip window that is optionally a tracked tooltip */
HWND mc_tooltip_create(HWND parent, HWND notify, BOOL track);

/* Activates a tracking tooltip */
void mc_tooltip_track_activate(HWND parent, HWND tooltip, BOOL show);

/* Activates a stationary tooltip */
void mc_tooltip_activate(HWND parent, HWND tooltip, BOOL show);

/* Sets the position of our tracking tooltip */
void mc_tooltip_set_track_pos(HWND parent, HWND tooltip, int x, int y);

/* Update the text of a tooltip */
void mc_tooltip_set_text(HWND parent, HWND tooltip, const TCHAR* str);

#endif  /* MC_MISC_H  */
