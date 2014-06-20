/*
 * Copyright (c) 2014 Martin Mitas
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
    if(theme_BufferedPaintInit != NULL)
        theme_BufferedPaintInit();
}

void
doublebuffer_fini(void)
{
    if(theme_BufferedPaintUnInit != NULL)
        theme_BufferedPaintUnInit();
}

HDC
doublebuffer_open(doublebuffer_t* dblbuf, HDC dc, const RECT* rect)
{
    HBITMAP bmp;

    if(MC_LIKELY(theme_BeginBufferedPaint != NULL)) {
        BP_PAINTPARAMS params = { sizeof(BP_PAINTPARAMS), BPPF_NOCLIP, NULL, NULL };

        dblbuf->uxtheme_buf = theme_BeginBufferedPaint(dc, rect,
                                BPBF_TOPDOWNDIB, &params, &dblbuf->mem_dc);
        if(dblbuf->uxtheme_buf != NULL)
            return dblbuf->mem_dc;
    } else {
        dblbuf->uxtheme_buf = NULL;
    }

    /* The old good plain Win32API double-buffering */
    dblbuf->mem_dc = CreateCompatibleDC(dc);
    if(MC_ERR(dblbuf->mem_dc == NULL)) {
        MC_TRACE_ERR("doublebuffer_begin: CreateCompatibleDC() failed.");
        goto err_CreateCompatibleDC;
    }
    bmp = CreateCompatibleBitmap(dc, mc_width(rect), mc_height(rect));
    if(MC_ERR(bmp == NULL)) {
        MC_TRACE_ERR("doublebuffer_begin: CreateCompatibleBitmap() failed.");
        goto err_CreateCompatibleBitmap;
    }

    mc_rect_copy(&dblbuf->rect, rect);

    dblbuf->old_bmp = SelectObject(dblbuf->mem_dc, bmp);
    OffsetViewportOrgEx(dblbuf->mem_dc, -rect->left, -rect->top, &dblbuf->old_origin);

    return dblbuf->mem_dc;

    /* Error path: Paint directly without any double-buffering. */
err_CreateCompatibleBitmap:
    DeleteDC(dblbuf->mem_dc);
err_CreateCompatibleDC:
    return dc;
}

void
doublebuffer_close(doublebuffer_t* dblbuf, BOOL blit)
{
    if(MC_LIKELY(dblbuf->uxtheme_buf != NULL)) {
        MC_ASSERT(theme_EndBufferedPaint != NULL);
        theme_EndBufferedPaint(dblbuf->uxtheme_buf, blit);
        return;
    }

    if(dblbuf->mem_dc != NULL) {
        if(blit) {
            SetViewportOrgEx(dblbuf->mem_dc,
                    dblbuf->old_origin.x, dblbuf->old_origin.y, NULL);
            BitBlt(dblbuf->dc, dblbuf->rect.left, dblbuf->rect.left,
                    mc_width(&dblbuf->rect), mc_height(&dblbuf->rect),
                    dblbuf->mem_dc, 0, 0, SRCCOPY);
        }

        HBITMAP bmp = SelectObject(dblbuf->mem_dc, dblbuf->old_bmp);
        DeleteObject(bmp);
        DeleteDC(dblbuf->mem_dc);
    }
}
