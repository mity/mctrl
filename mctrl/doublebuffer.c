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

#include "doublebuffer.h"


void
doublebuffer_init(void)
{
    mcBufferedPaintInit();
}

void
doublebuffer_fini(void)
{
    mcBufferedPaintUnInit();
}

HDC
doublebuffer_open(doublebuffer_t* dblbuf, HDC dc, const RECT* rect)
{
    BP_PAINTPARAMS params = { sizeof(BP_PAINTPARAMS), BPPF_NOCLIP, NULL, NULL };
    HDC dc_buffered;

    dblbuf->uxtheme_buf = mcBeginBufferedPaint(dc, rect, BPBF_TOPDOWNDIB, &params, &dc_buffered);
    if(MC_ERR(dblbuf->uxtheme_buf == NULL)) {
        MC_TRACE("doublebuffer_open: mcBeginBufferedPaint() failed.");
        /* We shall painting directly, without the double-buffering. */
        return dc;
    }

    return dc_buffered;
}

void
doublebuffer_close(doublebuffer_t* dblbuf, BOOL blit)
{
    if(dblbuf != NULL)
        mcEndBufferedPaint(dblbuf->uxtheme_buf, blit);
}
