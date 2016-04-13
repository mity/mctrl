/*
 * Copyright (c) 2015-2016 Martin Mitas
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
#include "lock.h"


void
wdBitBltImage(WD_HCANVAS hCanvas, const WD_HIMAGE hImage,
               const WD_RECT* pDestRect, const WD_RECT* pSourceRect)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        IWICBitmapSource* bitmap = (IWICBitmapSource*) hImage;
        ID2D1Bitmap* b;
        HRESULT hr;

        /* Compensation for the translation in the base transformation matrix.
         * This is to fit the image precisely into the pixel grid the canvas
         * when there is no custom transformation applied.
         */
        D2D1_RECT_F dest = {
                pDestRect->x0 - d2d_base_transform._31,
                pDestRect->y0 - d2d_base_transform._32,
                pDestRect->x1 - d2d_base_transform._31,
                pDestRect->y1 - d2d_base_transform._32
        };

        hr = ID2D1RenderTarget_CreateBitmapFromWicBitmap(c->target, bitmap, NULL, &b);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdBitBltImage: "
                        "ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed.");
            return;
        }

        ID2D1RenderTarget_DrawBitmap(c->target, b, &dest, 1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, (D2D1_RECT_F*) pSourceRect);
        ID2D1Bitmap_Release(b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        dummy_GpImage* b = (dummy_GpImage*) hImage;
        float dx, dy, dw, dh;
        float sx, sy, sw, sh;

        dx = pDestRect->x0;
        dy = pDestRect->y0;
        dw = pDestRect->x1 - pDestRect->x0;
        dh = pDestRect->y1 - pDestRect->y0;

        if(pSourceRect != NULL) {
            sx = pSourceRect->x0;
            sy = pSourceRect->y0;
            sw = pSourceRect->x1 - pSourceRect->x0;
            sh = pSourceRect->y1 - pSourceRect->y0;
        } else {
            UINT w, h;

            gdix_vtable->fn_GetImageWidth(b, &w);
            gdix_vtable->fn_GetImageHeight(b, &h);

            sx = 0.0f;
            sy = 0.0f;
            sw = (float) w;
            sh = (float) h;
        }

        gdix_vtable->fn_DrawImageRectRect(c->graphics, b, dx, dy, dw, dh,
                 sx, sy, sw, sh, dummy_UnitPixel, NULL, NULL, NULL);
    }
}

void
wdBitBltCachedImage(WD_HCANVAS hCanvas, const WD_HCACHEDIMAGE hCachedImage,
                    int x, int y)
{
    if(d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        ID2D1Bitmap* b = (ID2D1Bitmap*) hCachedImage;
        D2D1_SIZE_U sz;
        D2D1_RECT_F src;
        D2D1_RECT_F dest;

        sz = ID2D1Bitmap_GetPixelSize(b);

        src.left = 0.0f;
        src.top = 0.0f;
        src.right = (float) sz.width;
        src.bottom = (float) sz.height;

        dest.left = (float) x;
        dest.top = (float) y;
        dest.right = (float) (x + sz.width);
        dest.bottom = (float) (y + sz.height);

        ID2D1RenderTarget_DrawBitmap(c->target, b, &dest, 1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        dummy_GpCachedBitmap* cb = (dummy_GpCachedBitmap*) hCachedImage;

        gdix_vtable->fn_DrawCachedBitmap(c->graphics, cb, x, y);
    }
}

void
wdBitBltHICON(WD_HCANVAS hCanvas, HICON hIcon,
              const WD_RECT* pDestRect, const WD_RECT* pSourceRect)
{
    if(d2d_enabled()) {
        IWICBitmap* bitmap;
        IWICFormatConverter* converter;
        HRESULT hr;

        hr = IWICImagingFactory_CreateBitmapFromHICON(wic_factory, hIcon, &bitmap);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdBitBltHICON: "
                        "IWICImagingFactory::CreateBitmapFromHICON() failed.");
            goto err_CreateBitmapFromHICON;
        }

        hr = IWICImagingFactory_CreateFormatConverter(wic_factory, &converter);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdBitBltHICON: "
                        "IWICImagingFactory::CreateFormatConverter() failed.");
            goto err_CreateFormatConverter;
        }

        hr = IWICFormatConverter_Initialize(converter,
                (IWICBitmapSource*) bitmap, &wic_pixel_format,
                WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdBitBltHICON: "
                        "IWICFormatConverter::Initialize() failed.");
            goto err_Initialize;
        }

        wdBitBltImage(hCanvas, (WD_HIMAGE) converter, pDestRect, pSourceRect);

err_Initialize:
        IWICFormatConverter_Release(converter);
err_CreateFormatConverter:
        IWICBitmap_Release(bitmap);
err_CreateBitmapFromHICON:
        ;  /* noop */
    } else {
        dummy_GpBitmap* b;
        int status;

        status = gdix_vtable->fn_CreateBitmapFromHICON(hIcon, &b);
        if(status != 0) {
            WD_TRACE("wdBitBltHICON: GdipCreateBitmapFromHICON() failed. "
                     "[%d]", status);
            return;
        }
        wdBitBltImage(hCanvas, (WD_HIMAGE) b, pDestRect, pSourceRect);
        gdix_vtable->fn_DisposeImage(b);
    }
}


