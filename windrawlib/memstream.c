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

#include "memstream.h"


typedef struct memstream_tag memstream_t;
struct memstream_tag {
    const BYTE* buffer;
    ULONG pos;
    ULONG size;
    LONG refs;

    IStream stream;  /* COM interface */
};


#define MEMSTREAM_FROM_IFACE(stream_iface)                                    \
    WD_CONTAINEROF(stream_iface, memstream_t, stream)


static HRESULT STDMETHODCALLTYPE
memstream_QueryInterface(IStream* self, REFIID riid, void** obj)
{
    if(IsEqualGUID(riid, &IID_IUnknown)  ||
       IsEqualGUID(riid, &IID_IDispatch)  ||
       IsEqualGUID(riid, &IID_ISequentialStream)  ||
       IsEqualGUID(riid, &IID_IStream))
    {
        memstream_t* s = MEMSTREAM_FROM_IFACE(self);
        InterlockedIncrement(&s->refs);
        *obj = s;
        return S_OK;
    } else {
        *obj = NULL;
        return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE
memstream_AddRef(IStream* self)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    return InterlockedIncrement(&s->refs);
}

static ULONG STDMETHODCALLTYPE
memstream_Release(IStream* self)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    ULONG refs;

    refs = InterlockedDecrement(&s->refs);
    if(refs == 0)
        free(s);
    return refs;
}

static HRESULT STDMETHODCALLTYPE
memstream_Read(IStream* self, void* buf, ULONG n, ULONG* n_read)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);

    if(s->pos >= s->size) {
        n = 0;
        if(n_read != NULL)
            *n_read = 0;
        return S_FALSE;
    }

    if(n > s->size - s->pos)
        n = s->size - s->pos;
    memcpy(buf, s->buffer + s->pos, n);
    s->pos += n;

    if(n_read != NULL)
        *n_read = n;

    return (s->pos < s->size ? S_OK : S_FALSE);
}

static HRESULT STDMETHODCALLTYPE
memstream_Write(IStream* self, const void* buf, ULONG n, ULONG* n_written)
{
    /* We are read-only stream. */
    if(n_written != NULL)
        *n_written = 0;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
memstream_Seek(IStream* self, LARGE_INTEGER delta, DWORD origin,
                    ULARGE_INTEGER* new_pos)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    LARGE_INTEGER pos;

    switch(origin) {
        case STREAM_SEEK_SET:  pos.QuadPart = delta.QuadPart; break;
        case STREAM_SEEK_CUR:  pos.QuadPart = s->pos + delta.QuadPart; break;
        case STREAM_SEEK_END:  pos.QuadPart = s->size + delta.QuadPart; break;
        default:               return STG_E_INVALIDPARAMETER;
    }

    if(pos.QuadPart < 0)
        return STG_E_INVALIDFUNCTION;

    /* We trim the high part here. */
    s->pos = pos.u.LowPart;
    if(new_pos != NULL)
        new_pos->QuadPart = s->pos;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
memstream_SetSize(IStream* self, ULARGE_INTEGER new_size)
{
    /* We are read-only stream. */
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
memstream_CopyTo(IStream* self, IStream* other, ULARGE_INTEGER n,
                      ULARGE_INTEGER* n_read, ULARGE_INTEGER* n_written)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    HRESULT hr;
    ULONG written;

    if(s->pos + n.QuadPart >= s->size)
        n.QuadPart = (s->pos < s->size ? s->size - s->pos : 0);

    hr = IStream_Write(other, s->buffer + s->pos, (ULONG) n.QuadPart, &written);
    s->pos += written;
    if(n_read != NULL)
        n_read->QuadPart = written;
    if(n_written != NULL)
        n_written->QuadPart = written;
    return hr;
}

static HRESULT STDMETHODCALLTYPE
memstream_Commit(IStream* self, DWORD flags)
{
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
memstream_Revert(IStream* self)
{
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
memstream_LockRegion(IStream* self, ULARGE_INTEGER offset,
                          ULARGE_INTEGER n, DWORD type)
{
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
memstream_UnlockRegion(IStream* self, ULARGE_INTEGER offset,
                            ULARGE_INTEGER n, DWORD type)
{
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
memstream_Stat(IStream* self, STATSTG* stat, DWORD flag)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);

    memset(stat, 0, sizeof(STATSTG));
    stat->type = STGTY_STREAM;
    stat->cbSize.QuadPart = s->size;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
memstream_Clone(IStream* self, IStream** other)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    IStream* o;
    memstream_t* so;

    o = memstream_create(s->buffer, s->size);
    if(o == NULL) {
        WD_TRACE("memstream_Clone: memstream_create() failed.");
        return STG_E_INSUFFICIENTMEMORY;
    }
    so = MEMSTREAM_FROM_IFACE(o);
    so->pos = s->pos;
    *other = o;
    return S_OK;
}


static IStreamVtbl memstream_vtable = {
    memstream_QueryInterface,
    memstream_AddRef,
    memstream_Release,
    memstream_Read,
    memstream_Write,
    memstream_Seek,
    memstream_SetSize,
    memstream_CopyTo,
    memstream_Commit,
    memstream_Revert,
    memstream_LockRegion,
    memstream_UnlockRegion,
    memstream_Stat,
    memstream_Clone
};


IStream*
memstream_create(const BYTE* buffer, ULONG size)
{
    memstream_t* s;

    s = (memstream_t*) malloc(sizeof(memstream_t));
    if(s == NULL) {
        WD_TRACE("memstream_create: malloc() failed.");
        return NULL;
    }

    s->buffer = buffer;
    s->pos = 0;
    s->size = size;
    s->refs = 1;
    s->stream.lpVtbl = &memstream_vtable;

    return &s->stream;
}

IStream*
memstream_create_from_resource(HINSTANCE instance,
            const WCHAR* res_type, const WCHAR* res_name)
{
    HRSRC res;
    DWORD res_size;
    HGLOBAL res_global;
    void* res_data;
    IStream* stream;

    /* We rely on the fact that UnlockResource() and FreeResource() do nothing:
     *  -- MSDN docs for LockResource() says no unlocking is needed.
     *  -- MSDN docs for FreeResource() says it just returns FALSE on 32/64-bit
     *     Windows.
     *
     * See also http://blogs.msdn.com/b/oldnewthing/archive/2011/03/07/10137456.aspx
     *
     * It may look a bit ugly, but it simplifies things a lot as the stream
     * does not need to do any bookkeeping for the resource.
     */

    res = FindResourceW(instance, res_name, res_type);
    if(res == NULL) {
        WD_TRACE_ERR("memstream_create_from_resource: FindResource() failed.");
        return NULL;
    }

    res_size = SizeofResource(instance, res);
    res_global = LoadResource(instance, res);
    if(res_global == NULL) {
        WD_TRACE_ERR("memstream_create_from_resource: LoadResource() failed");
        return NULL;
    }

    res_data = LockResource(res_global);
    if(res_data == NULL) {
        WD_TRACE_ERR("memstream_create_from_resource: LockResource() failed");
        return NULL;
    }

    stream = memstream_create(res_data, res_size);
    if(stream == NULL) {
        WD_TRACE("memstream_create_from_resource: memstream_create() failed.");
        return NULL;
    }

    return stream;
}

