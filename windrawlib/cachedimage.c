/*
 * Copyright (c) 2016 Martin Mitas
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

#include "misc.h"
#include "backend-d2d.h"
#include "backend-wic.h"
#include "backend-gdix.h"


WD_HCACHEDIMAGE
wdCreateCachedImage(WD_HCANVAS hCanvas, WD_HIMAGE hImage)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Bitmap* b;
        HRESULT hr;

        hr = ID2D1RenderTarget_CreateBitmapFromWicBitmap(c->target,
                (IWICBitmapSource*) hImage, NULL, &b);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdCreateCachedImage: "
                        "ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed.");
            return NULL;
        }

        return (WD_HCACHEDIMAGE) b;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        dummy_GpCachedBitmap* cb;
        int status;

        status = gdix_vtable->fn_CreateCachedBitmap((dummy_GpImage*) hImage,
                c->graphics, &cb);
        if(status != 0) {
            WD_TRACE("wdCreateCachedImage: "
                     "GdipCreateCachedBitmap() failed. [%d]", status);
            return NULL;
        }

        return (WD_HCACHEDIMAGE) cb;
    }
}

void
wdDestroyCachedImage(WD_HCACHEDIMAGE hCachedImage)
{
    if(d2d_enabled()) {
        ID2D1Bitmap_Release((ID2D1Bitmap*) hCachedImage);
    } else {
        gdix_vtable->fn_DeleteCachedBitmap((dummy_GpCachedBitmap*) hCachedImage);
    }
}
