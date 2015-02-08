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

#include "memstream.h"


/* Uncomment this to have more verbose traces from this module. */
/*#define MEMSTREAM_DEBUG     1*/

#ifdef MEMSTREAM_DEBUG
    #define MEMSTREAM_TRACE       MC_TRACE
#else
    #define MEMSTREAM_TRACE(...)  do {} while(0)
#endif



typedef struct memstream_tag memstream_t;
struct memstream_tag {
    const BYTE* buffer;
    ULONG pos;
    ULONG size;
    mc_ref_t refs;

    IStream stream;  /* COM interface */
};


#define MEMSTREAM_FROM_IFACE(stream_iface)                                    \
    MC_CONTAINEROF(stream_iface, memstream_t, stream)


static HRESULT STDMETHODCALLTYPE
memstream_QueryInterface(IStream* self, REFIID riid, void** obj)
{
    if(IsEqualGUID(riid, &IID_IUnknown)  ||
       IsEqualGUID(riid, &IID_IDispatch)  ||
       IsEqualGUID(riid, &IID_ISequentialStream)  ||
       IsEqualGUID(riid, &IID_IStream))
    {
        memstream_t* s = MEMSTREAM_FROM_IFACE(self);
        mc_ref(&s->refs);
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
    MEMSTREAM_TRACE("memstream_AddRef(%d -> %d)", (int) s->refs, (int) s->refs+1);
    return mc_ref(&s->refs);
}

static ULONG STDMETHODCALLTYPE
memstream_Release(IStream* self)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    ULONG refs;

    MEMSTREAM_TRACE("memstream_Release(%d -> %d)", (int) s->refs, (int) s->refs-1);
    refs = mc_unref(&s->refs);
    if(refs == 0) {
        MEMSTREAM_TRACE("memstream_Release: Freeing the stream object.");
        free(s);
    }
    return refs;
}

static HRESULT STDMETHODCALLTYPE
memstream_Read(IStream* self, void* buf, ULONG n, ULONG* n_read)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);

    MEMSTREAM_TRACE("memstream_Read(%lu)", n);

    if(MC_ERR(s->pos >= s->size)) {
        n = 0;
        if(n_read != NULL)
            *n_read = 0;
        return STG_E_INVALIDFUNCTION;
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
    MEMSTREAM_TRACE("memstream_Write: Stub.");
    if(n_written != NULL)
        *n_written = 0;
    return STG_E_CANTSAVE;
}

static HRESULT STDMETHODCALLTYPE
memstream_Seek(IStream* self, LARGE_INTEGER delta, DWORD origin,
                    ULARGE_INTEGER* new_pos)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);
    LARGE_INTEGER pos;
    HRESULT hres = S_OK;

    MEMSTREAM_TRACE("memstream_Seek(%lu, %lu)", delta, origin);

    switch(origin) {
        case STREAM_SEEK_SET:  pos.QuadPart = delta.QuadPart; break;
        case STREAM_SEEK_CUR:  pos.QuadPart = s->pos + delta.QuadPart; break;
        case STREAM_SEEK_END:  pos.QuadPart = s->size + delta.QuadPart; break;
        default:               hres = STG_E_SEEKERROR;
    }

    if(pos.QuadPart < 0)
        hres = STG_E_INVALIDFUNCTION;

    s->pos = pos.QuadPart;
    if(new_pos != NULL)
        new_pos->QuadPart = pos.QuadPart;
    return hres;
}

static HRESULT STDMETHODCALLTYPE
memstream_SetSize(IStream* self, ULARGE_INTEGER new_size)
{
    MEMSTREAM_TRACE("memstream_SetSize: Stub.");
    return STG_E_INVALIDFUNCTION;
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

    hr = IStream_Write(other, s->buffer + s->pos, n.QuadPart, &written);
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
    MEMSTREAM_TRACE("memstream_Commit: Stub.");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
memstream_Revert(IStream* self)
{
    MEMSTREAM_TRACE("memstream_Revert: Stub.");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
memstream_LockRegion(IStream* self, ULARGE_INTEGER offset,
                          ULARGE_INTEGER n, DWORD type)
{
    MEMSTREAM_TRACE("memstream_LockRegion: Stub.");
    return STG_E_INVALIDFUNCTION;
}

static HRESULT STDMETHODCALLTYPE
memstream_UnlockRegion(IStream* self, ULARGE_INTEGER offset,
                            ULARGE_INTEGER n, DWORD type)
{
    MEMSTREAM_TRACE("memstream_UnlockRegion: Stub.");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE
memstream_Stat(IStream* self, STATSTG* stat, DWORD flag)
{
    memstream_t* s = MEMSTREAM_FROM_IFACE(self);

    MEMSTREAM_TRACE("memstream_Stat: Stub.");

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

    MEMSTREAM_TRACE("memstream_Clone");

    o = memstream_create(s->buffer, s->size);
    if(MC_ERR(o == NULL)) {
        MC_TRACE("memstream_Clone: memstream_create() failed.");
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
    if(MC_ERR(s == NULL)) {
        MC_TRACE("memstream_create: malloc() failed.");
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
            const TCHAR* res_type, const TCHAR* res_name)
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
     * It may look a bit ugly, but it simplifies things a lot. Otherwise our
     * IStream implementation would have to "own" the resource and free it in
     * its destructor. That would complicate especially the IStream::Clone() as
     * the image resource would have to be shared by multiple IStreams objects.
     */

    res = FindResource(instance, res_name, res_type);
    if(MC_ERR(res == NULL)) {
        MC_TRACE_ERR("memstream_create_from_resource: FindResource() failed.");
        return NULL;
    }

    res_size = SizeofResource(instance, res);
    res_global = LoadResource(instance, res);
    if(MC_ERR(res_global == NULL)) {
        MC_TRACE_ERR("memstream_create_from_resource: LoadResource() failed");
        return NULL;
    }

    res_data = LockResource(res_global);
    if(MC_ERR(res_data == NULL)) {
        MC_TRACE_ERR("memstream_create_from_resource: LockResource() failed");
        return NULL;
    }

    stream = memstream_create(res_data, res_size);
    if(MC_ERR(stream == NULL)) {
        MC_TRACE("memstream_create_from_resource: memstream_create() failed.");
        return NULL;
    }

    return stream;
}

