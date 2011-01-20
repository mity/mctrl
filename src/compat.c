/*
 * Copyright (c) 2011 Martin Mitas
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

#include <windows.h>
#include <errno.h>
#include "compat.h"


/* compat_wcstoi64() and compat_wcstoui64() were adapted from 
 * MSVCRT__wcstoi64_l() and MSVCRT__wcstoui64_l respectivally,
 * both found in wine-1.3.11/dlls/msvcrt/wcs.c */


#define MSVCRT_CHECK_PMT(cond)    (cond)

#define tolowerW(c)    ((wchar_t)(intptr_t)CharLowerW((LPWSTR)(intptr_t)(int)(c)))
#define isspaceW       iswspace
#define isdigitW       iswdigit


int64_t
compat_wcstoi64(const wchar_t *nptr, wchar_t **endptr, int base)
{
    BOOL negative = FALSE;
    int64_t ret = 0;

    if (!MSVCRT_CHECK_PMT(nptr != NULL) || !MSVCRT_CHECK_PMT(base == 0 || base >= 2) ||
        !MSVCRT_CHECK_PMT(base <= 36)) {
        errno = EINVAL;
        return 0;
    }

    while(isspaceW(*nptr)) nptr++;

    if(*nptr == '-') {
        negative = TRUE;
        nptr++;
    } else if(*nptr == '+')
        nptr++;

    if((base==0 || base==16) && *nptr=='0' && tolowerW(*(nptr+1))=='x') {
        base = 16;
        nptr += 2;
    }

    if(base == 0) {
        if(*nptr=='0')
            base = 8;
        else
            base = 10;
    }

    while(*nptr) {
        char cur = tolowerW(*nptr);
        int v;

        if(isdigitW(cur)) {
            if(cur >= '0'+base)
                break;
            v = cur-'0';
        } else {
            if(cur<'a' || cur>='a'+base-10)
                break;
            v = cur-'a'+10;
        }

        if(negative)
            v = -v;

        nptr++;

        if(!negative && (ret>INT64_MAX/base || ret*base>INT64_MAX-v)) {
            ret = INT64_MAX;
            errno = ERANGE;
        } else if(negative && (ret<INT64_MIN/base || ret*base<INT64_MIN-v)) {
            ret = INT64_MIN;
            errno = ERANGE;
        } else
            ret = ret*base + v;
    }

    if(endptr)
        *endptr = (wchar_t*)nptr;

    return ret;
}

uint64_t
compat_wcstoui64(const wchar_t *nptr, wchar_t **endptr, int base)
{
    BOOL negative = FALSE;
    uint64_t ret = 0;

    if (!MSVCRT_CHECK_PMT(nptr != NULL) || !MSVCRT_CHECK_PMT(base == 0 || base >= 2) ||
        !MSVCRT_CHECK_PMT(base <= 36)) {
        errno = EINVAL;
        return 0;
    }

    while(isspaceW(*nptr)) nptr++;

    if(*nptr == '-') {
        negative = TRUE;
        nptr++;
    } else if(*nptr == '+')
        nptr++;

    if((base==0 || base==16) && *nptr=='0' && tolowerW(*(nptr+1))=='x') {
        base = 16;
        nptr += 2;
    }

    if(base == 0) {
        if(*nptr=='0')
            base = 8;
        else
            base = 10;
    }

    while(*nptr) {
        char cur = tolowerW(*nptr);
        int v;

        if(isdigit(cur)) {
            if(cur >= '0'+base)
                break;
            v = *nptr-'0';
        } else {
            if(cur<'a' || cur>='a'+base-10)
                break;
            v = cur-'a'+10;
        }

        nptr++;

        if(ret>UINT64_MAX/base || ret*base>UINT64_MAX-v) {
            ret = UINT64_MAX;
            errno = ERANGE;
        } else
            ret = ret*base + v;
    }

    if(endptr)
        *endptr = (wchar_t*)nptr;

    return negative ? -ret : ret;
}



