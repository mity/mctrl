/*
 * Copyright (c) 2019 Martin Mitas
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

#include "xwic.h"
#include "xcom.h"
#include "c-reusables/win32/memstream.h"


static IWICImagingFactory* xwic_factory;


/* This corresponds to the pixel format we use in xd2d.c.
 * (see https://docs.microsoft.com/en-us/windows/desktop/direct2d/supported-pixel-formats-and-alpha-modes) */
static const GUID* xwic_pixel_format = &GUID_WICPixelFormat32bppPBGRA;


static IWICBitmapSource*
xwic_convert(IWICBitmapSource* b)
{
    GUID pixel_format;
    IWICFormatConverter* converter;
    HRESULT hr;

    hr = IWICBitmapSource_GetPixelFormat(b, &pixel_format);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_convert: IWICBitmapSource::GetPixelFormat() failed.");
        goto err_GetPixelFormat;
    }

    if(IsEqualGUID(&pixel_format, xwic_pixel_format))
        return b;   /* No conversion needed. */

    hr = IWICImagingFactory_CreateFormatConverter(xwic_factory, &converter);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_convert: IWICImagingFactory::CreateFormatConverter() failed.");
        goto err_CreateFormatConverter;
    }

    hr = IWICFormatConverter_Initialize(converter, b, xwic_pixel_format,
                WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_convert: IWICFormatConverter::Initialize() failed.");
        goto err_Initialize;
    }

    /* Success. */
    IWICBitmap_Release(b);
    return (IWICBitmapSource*) converter;

    /* Error unrolling. */
err_Initialize:
    IWICFormatConverter_Release(converter);
err_CreateFormatConverter:
err_GetPixelFormat:
    IWICBitmap_Release(b);
    return NULL;
}


IWICBitmapSource*
xwic_from_HICON(HICON icon)
{
    IWICBitmap* b;
    HRESULT hr;

    hr = IWICImagingFactory_CreateBitmapFromHICON(xwic_factory, icon, &b);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_HICON: IWICImagingFactory::CreateBitmapFromHICON() failed.");
        return NULL;
    }

    return xwic_convert((IWICBitmapSource*) b);
}

IWICBitmapSource*
xwic_from_HBITMAP(HBITMAP bmp, WICBitmapAlphaChannelOption alpha_mode)
{
    IWICBitmap* b;
    HRESULT hr;

    hr = IWICImagingFactory_CreateBitmapFromHBITMAP(xwic_factory, bmp, NULL, alpha_mode, &b);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_HBITMAP: IWICImagingFactory::CreateBitmapFromHBITMAP() failed.");
        return NULL;
    }

    return xwic_convert((IWICBitmapSource*) b);
}

IWICBitmapSource*
xwic_from_file(const TCHAR* path)
{
    IWICBitmapDecoder* decoder;
    IWICBitmapFrameDecode* b;
    HRESULT hr;

    hr = IWICImagingFactory_CreateDecoderFromFilename(xwic_factory, path,
                NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_file: IWICImagingFactory::CreateDecoderFromFilename() failed.");
        return NULL;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &b);
    IWICBitmapDecoder_Release(decoder);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_file: IWICBitmapDecoder::GetFrame() failed.");
        return NULL;
    }

    return xwic_convert((IWICBitmapSource*) b);
}

static IWICBitmapSource*
xwic_from_IStream(IStream* in)
{
    IWICBitmapDecoder* decoder;
    IWICBitmapFrameDecode* b;
    HRESULT hr;

    hr = IWICImagingFactory_CreateDecoderFromStream(xwic_factory, in,
                NULL, WICDecodeMetadataCacheOnLoad, &decoder);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_IStream: IWICImagingFactory::CreateDecoderFromStream() failed.");
        return NULL;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &b);
    IWICBitmapDecoder_Release(decoder);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_IStream: IWICBitmapDecoder::GetFrame() failed.");
        return NULL;
    }

    return xwic_convert((IWICBitmapSource*) b);
}

IWICBitmapSource*
xwic_from_resource(HINSTANCE instance, const TCHAR* res_type, const TCHAR* res_name)
{
    IStream* stream;
    IWICBitmapSource* b;
    HRESULT hr;

    hr = memstream_create_from_resource(instance, res_type, res_name, &stream);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("xwic_from_resource: memstream_create_from_resource() failed.");
        return NULL;
    }

    b = xwic_from_IStream(stream);
    if(MC_ERR(b == NULL))
        MC_TRACE("xwic_from_resource: xwic_from_IStream() failed.");

    IStream_Release(stream);
    return b;
}

int
xwic_init_module(void)
{
    xwic_factory = (IWICImagingFactory*) xcom_init_create(&CLSID_WICImagingFactory,
                CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory);
    if(MC_ERR(xwic_factory == NULL)) {
        MC_TRACE("xwic_init_module: xcom_init_create(IID_IWICImagingFactory) failed.");
        return -1;
    }

    return 0;
}

void
xwic_fini_module(void)
{
    IWICImagingFactory_Release(xwic_factory);
}
