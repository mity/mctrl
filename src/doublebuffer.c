/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2015-2020 Martin Mitas
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
    BufferedPaintInit();
}

void
doublebuffer_fini(void)
{
    BufferedPaintUnInit();
}

void
doublebuffer(void* control, PAINTSTRUCT* ps, doublebuffer_callback_t callback)
{
    BP_PAINTPARAMS params = { sizeof(BP_PAINTPARAMS), BPPF_NOCLIP, NULL, NULL };
    HPAINTBUFFER paint_buffer;
    HDC dc;

    paint_buffer = BeginBufferedPaint(ps->hdc, &ps->rcPaint, BPBF_TOPDOWNDIB, &params, &dc);
    if(paint_buffer != NULL) {
        callback(control, dc, &ps->rcPaint, TRUE);
        EndBufferedPaint(paint_buffer, TRUE);
    } else {
        MC_TRACE("doublebuffer: BeginBufferedPaint() failed.");
        /* We shall painting directly, without the double-buffering. */
        callback(control, ps->hdc, &ps->rcPaint, ps->fErase);
    }
}

