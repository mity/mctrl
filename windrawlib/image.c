/*
 * WinDrawLib
 * Copyright (c) 2015-2016 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "misc.h"
#include "backend-d2d.h"
#include "backend-wic.h"
#include "backend-gdix.h"
#include "lock.h"
#include "memstream.h"


WD_HIMAGE
wdCreateImageFromHBITMAP(HBITMAP hBmp)
{
    if(d2d_enabled()) {
        IWICBitmap* bitmap;
        IWICBitmapSource* converted_bitmap;
        HRESULT hr;

        if(wic_factory == NULL) {
            WD_TRACE("wdCreateImageFromHBITMAP: Image API disabled.");
            return NULL;
        }

        hr = IWICImagingFactory_CreateBitmapFromHBITMAP(wic_factory, hBmp,
                NULL, WICBitmapIgnoreAlpha, &bitmap);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdCreateImageFromHBITMAP: "
                        "IWICImagingFactory::CreateBitmapFromHBITMAP() failed.");
            return NULL;
        }

        converted_bitmap = wic_convert_bitmap((IWICBitmapSource*) bitmap);
        if(converted_bitmap == NULL)
            WD_TRACE("wdCreateImageFromHBITMAP: wic_convert_bitmap() failed.");

        IWICBitmap_Release(bitmap);

        return (WD_HIMAGE) converted_bitmap;
    } else {
        dummy_GpBitmap* b;
        int status;

        status = gdix_vtable->fn_CreateBitmapFromHBITMAP(hBmp, NULL, &b);
        if(status != 0) {
            WD_TRACE("wdCreateImageFromHBITMAP: "
                     "GdipCreateBitmapFromHBITMAP() failed. [%d]", status);
            return NULL;
        }

        return (WD_HIMAGE) b;
    }
}

WD_HIMAGE
wdLoadImageFromFile(const WCHAR* pszPath)
{
    if(d2d_enabled()) {
        IWICBitmapDecoder* decoder;
        IWICBitmapFrameDecode* bitmap;
        IWICBitmapSource* converted_bitmap = NULL;
        HRESULT hr;

        if(wic_factory == NULL) {
            WD_TRACE("wdLoadImageFromFile: Image API disabled.");
            return NULL;
        }

        hr = IWICImagingFactory_CreateDecoderFromFilename(wic_factory, pszPath,
                NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdLoadImageFromFile: "
                        "IWICImagingFactory::CreateDecoderFromFilename() failed.");
            goto err_CreateDecoderFromFilename;
        }

        hr = IWICBitmapDecoder_GetFrame(decoder, 0, &bitmap);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdLoadImageFromFile: "
                        "IWICBitmapDecoder::GetFrame() failed.");
            goto err_GetFrame;
        }

        converted_bitmap = wic_convert_bitmap((IWICBitmapSource*) bitmap);
        if(converted_bitmap == NULL)
            WD_TRACE("wdLoadImageFromFile: wic_convert_bitmap() failed.");

        IWICBitmapFrameDecode_Release(bitmap);
err_GetFrame:
        IWICBitmapDecoder_Release(decoder);
err_CreateDecoderFromFilename:
        return (WD_HIMAGE) converted_bitmap;
    } else {
        dummy_GpImage* img;
        int status;

        status = gdix_vtable->fn_LoadImageFromFile(pszPath, &img);
        if(status != 0) {
            WD_TRACE("wdLoadImageFromFile: "
                     "GdipLoadImageFromFile() failed. [%d]", status);
            return NULL;
        }

        return (WD_HIMAGE) img;
    }
}

WD_HIMAGE
wdLoadImageFromIStream(IStream* pStream)
{
    if(d2d_enabled()) {
        IWICBitmapDecoder* decoder;
        IWICBitmapFrameDecode* bitmap;
        IWICBitmapSource* converted_bitmap = NULL;
        HRESULT hr;

        if(wic_factory == NULL) {
            WD_TRACE("wdLoadImageFromIStream: Image API disabled.");
            return NULL;
        }

        hr = IWICImagingFactory_CreateDecoderFromStream(wic_factory, pStream,
                NULL, WICDecodeMetadataCacheOnLoad, &decoder);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdLoadImageFromIStream: "
                        "IWICImagingFactory::CreateDecoderFromFilename() failed.");
            goto err_CreateDecoderFromFilename;
        }

        hr = IWICBitmapDecoder_GetFrame(decoder, 0, &bitmap);
        if(FAILED(hr)) {
            WD_TRACE_HR("wdLoadImageFromIStream: "
                        "IWICBitmapDecoder::GetFrame() failed.");
            goto err_GetFrame;
        }

        converted_bitmap = wic_convert_bitmap((IWICBitmapSource*) bitmap);
        if(converted_bitmap == NULL)
            WD_TRACE("wdLoadImageFromIStream: wic_convert_bitmap() failed.");

        IWICBitmapFrameDecode_Release(bitmap);
err_GetFrame:
        IWICBitmapDecoder_Release(decoder);
err_CreateDecoderFromFilename:
        return (WD_HIMAGE) converted_bitmap;
    } else {
        dummy_GpImage* img;
        int status;

        status = gdix_vtable->fn_LoadImageFromStream(pStream, &img);
        if(status != 0) {
            WD_TRACE("wdLoadImageFromIStream: "
                     "GdipLoadImageFromFile() failed. [%d]", status);
            return NULL;
        }

        return (WD_HIMAGE) img;
    }
}

WD_HIMAGE
wdLoadImageFromResource(HINSTANCE hInstance, const WCHAR* pszResType,
                        const WCHAR* pszResName)
{
    IStream* stream;
    WD_HIMAGE img;

    stream = memstream_create_from_resource(hInstance, pszResType, pszResName);
    if(stream == NULL) {
        WD_TRACE("wdLoadImageFromResource: "
                 "memstream_create_from_resource() failed.");
        return NULL;
    }

    img = wdLoadImageFromIStream(stream);
    if(img == NULL)
        WD_TRACE("wdLoadImageFromResource: wdLoadImageFromIStream() failed.");

    IStream_Release(stream);
    return img;
}

void
wdDestroyImage(WD_HIMAGE hImage)
{
    if(d2d_enabled()) {
        IWICBitmapSource_Release((IWICBitmapSource*) hImage);
    } else {
        gdix_vtable->fn_DisposeImage((dummy_GpImage*) hImage);
    }
}

void
wdGetImageSize(WD_HIMAGE hImage, UINT* puWidth, UINT* puHeight)
{
    if(d2d_enabled()) {
        UINT w, h;

        IWICBitmapSource_GetSize((IWICBitmapSource*) hImage, &w, &h);
        if(puWidth != NULL)
            *puWidth = w;
        if(puHeight != NULL)
            *puHeight = h;
    } else {
        if(puWidth != NULL)
            gdix_vtable->fn_GetImageWidth((dummy_GpImage*) hImage, puWidth);
        if(puHeight != NULL)
            gdix_vtable->fn_GetImageHeight((dummy_GpImage*) hImage, puHeight);
    }
}

