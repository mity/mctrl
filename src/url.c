/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2019-2020 Martin Mitas
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

#include "url.h"


#define DECODE_IS_XDIGIT(ch)    ((_T('0') <= (ch) && (ch) <= _T('9'))  ||     \
                                 (_T('a') <= (ch) && (ch) <= _T('f'))  ||     \
                                 (_T('A') <= (ch) && (ch) <= _T('F')))

#define DECODE_XDIGIT_VAL(ch)                                                 \
            ((_T('0') <= (ch) && (ch) <= _T('9')) ? (ch) - _T('0') :          \
             (_T('a') <= (ch) && (ch) <= _T('f')) ? 10 + (ch) - _T('a') :     \
                                                    10 + (ch) - _T('A'))

void
url_decode(TCHAR* str)
{
    TCHAR* in = str;
    TCHAR* out;

    /* Fast path for any prefix without any percents. */
    while(*in != _T('\0')  &&  *in != _T('%'))
        in++;
    out = in;

    /* Slow path if we encounter any percent(s). */
    while(*in != _T('\0')) {
        if(*in == _T('%')  &&  DECODE_IS_XDIGIT(*(in+1))  &&  DECODE_IS_XDIGIT(*(in+2))) {
            *out = (DECODE_XDIGIT_VAL(*(in+1)) << 4) | DECODE_XDIGIT_VAL(*(in+2));
            in += 3;
            out++;
        } else {
            *in = *out;
            in++;
            out++;
        }
    }
}
