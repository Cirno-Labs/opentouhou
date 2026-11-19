// C, so that d3d8.h declares the vtables as structs and the compiler
// checks every replacement against the method it replaces.

#include "direct3d.h"

#include "iat.h"

#include <string.h>

typedef IDirect3D8 *(WINAPI *M_CREATE_FN)(UINT sdk_version);

// Holds the hooks; the hooked methods take no context. The vtables are
// Wine's, shared by every object of their interface.
static struct {
    M_CREATE_FN create;
    void (*present)(IDirect3DDevice8 *device);
    bool draw;
    HRESULT(STDMETHODCALLTYPE *create_device)(
        IDirect3D8 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *,
        IDirect3DDevice8 **);
    HRESULT(STDMETHODCALLTYPE *real_present)(
        IDirect3DDevice8 *, const RECT *, const RECT *, HWND, const RGNDATA *);
    HRESULT(STDMETHODCALLTYPE *real_clear)(
        IDirect3DDevice8 *, DWORD, const D3DRECT *, DWORD, D3DCOLOR, float,
        DWORD);
    HRESULT(STDMETHODCALLTYPE *real_draw_primitive)(
        IDirect3DDevice8 *, D3DPRIMITIVETYPE, UINT, UINT);
    HRESULT(STDMETHODCALLTYPE *real_draw_indexed_primitive)(
        IDirect3DDevice8 *, D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT);
    HRESULT(STDMETHODCALLTYPE *real_draw_primitive_up)(
        IDirect3DDevice8 *, D3DPRIMITIVETYPE, UINT, const void *, UINT);
    HRESULT(STDMETHODCALLTYPE *real_draw_indexed_primitive_up)(
        IDirect3DDevice8 *, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void *,
        D3DFORMAT, const void *, UINT);
} m_Direct3d;

static HRESULT STDMETHODCALLTYPE M_Present(
    IDirect3DDevice8 *const device, const RECT *const src,
    const RECT *const dst, const HWND window, const RGNDATA *const dirty)
{
    m_Direct3d.present(device);
    return m_Direct3d.draw
        ? m_Direct3d.real_present(device, src, dst, window, dirty)
        : D3D_OK;
}

static HRESULT STDMETHODCALLTYPE M_Clear(
    IDirect3DDevice8 *device, DWORD count, const D3DRECT *rects, DWORD flags,
    D3DCOLOR color, float z, DWORD stencil)
{
    return m_Direct3d.draw
        ? m_Direct3d.real_clear(device, count, rects, flags, color, z, stencil)
        : D3D_OK;
}

static HRESULT STDMETHODCALLTYPE M_DrawPrimitive(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    return m_Direct3d.draw
        ? m_Direct3d.real_draw_primitive(device, type, start, count)
        : D3D_OK;
}

static HRESULT STDMETHODCALLTYPE M_DrawIndexedPrimitive(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT min_index,
    UINT vertices, UINT start, UINT count)
{
    return m_Direct3d.draw
        ? m_Direct3d.real_draw_indexed_primitive(
              device, type, min_index, vertices, start, count)
        : D3D_OK;
}

static HRESULT STDMETHODCALLTYPE M_DrawPrimitiveUP(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT count,
    const void *data, UINT stride)
{
    return m_Direct3d.draw
        ? m_Direct3d.real_draw_primitive_up(device, type, count, data, stride)
        : D3D_OK;
}

static HRESULT STDMETHODCALLTYPE M_DrawIndexedPrimitiveUP(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT min_index,
    UINT vertices, UINT count, const void *indices, D3DFORMAT format,
    const void *data, UINT stride)
{
    return m_Direct3d.draw ? m_Direct3d.real_draw_indexed_primitive_up(
                                 device, type, min_index, vertices, count,
                                 indices, format, data, stride)
                           : D3D_OK;
}

static bool M_Unprotect(void *const vtbl, const SIZE_T size, DWORD *const old)
{
    return VirtualProtect(vtbl, size, PAGE_READWRITE, old);
}

static HRESULT STDMETHODCALLTYPE M_CreateDevice(
    IDirect3D8 *const d3d, const UINT adapter, const D3DDEVTYPE type,
    const HWND window, const DWORD flags, D3DPRESENT_PARAMETERS *const params,
    IDirect3DDevice8 **const device)
{
    const HRESULT result = m_Direct3d.create_device(
        d3d, adapter, type, window, flags, params, device);
    if (FAILED(result) || *device == nullptr) {
        return result;
    }
    IDirect3DDevice8Vtbl *const vtbl =
        (IDirect3DDevice8Vtbl *)(*device)->lpVtbl;
    DWORD old;
    if (vtbl->Present != M_Present && M_Unprotect(vtbl, sizeof(*vtbl), &old)) {
        m_Direct3d.real_present = vtbl->Present;
        m_Direct3d.real_clear = vtbl->Clear;
        m_Direct3d.real_draw_primitive = vtbl->DrawPrimitive;
        m_Direct3d.real_draw_indexed_primitive = vtbl->DrawIndexedPrimitive;
        m_Direct3d.real_draw_primitive_up = vtbl->DrawPrimitiveUP;
        m_Direct3d.real_draw_indexed_primitive_up =
            vtbl->DrawIndexedPrimitiveUP;
        vtbl->Present = M_Present;
        vtbl->Clear = M_Clear;
        vtbl->DrawPrimitive = M_DrawPrimitive;
        vtbl->DrawIndexedPrimitive = M_DrawIndexedPrimitive;
        vtbl->DrawPrimitiveUP = M_DrawPrimitiveUP;
        vtbl->DrawIndexedPrimitiveUP = M_DrawIndexedPrimitiveUP;
        VirtualProtect(vtbl, sizeof(*vtbl), old, &old);
    }
    return result;
}

static IDirect3D8 *WINAPI M_Direct3DCreate8(const UINT sdk_version)
{
    IDirect3D8 *const d3d = m_Direct3d.create(sdk_version);
    if (d3d == nullptr) {
        return nullptr;
    }
    IDirect3D8Vtbl *const vtbl = (IDirect3D8Vtbl *)d3d->lpVtbl;
    DWORD old;
    if (vtbl->CreateDevice != M_CreateDevice
        && M_Unprotect(vtbl, sizeof(*vtbl), &old)) {
        m_Direct3d.create_device = vtbl->CreateDevice;
        vtbl->CreateDevice = M_CreateDevice;
        VirtualProtect(vtbl, sizeof(*vtbl), old, &old);
    }
    return d3d;
}

void Direct3d_SetDrawing(const bool draw)
{
    m_Direct3d.draw = draw;
}

bool Direct3d_Install(
    const HMODULE module, void (*const present)(IDirect3DDevice8 *device))
{
    m_Direct3d.present = present;
    m_Direct3d.draw = true;
    uintptr_t original;
    if (!Iat_Patch(
            module, "d3d8.dll", "Direct3DCreate8", (uintptr_t)M_Direct3DCreate8,
            &original)) {
        return false;
    }
    m_Direct3d.create = (M_CREATE_FN)original;
    return true;
}

static void M_ToRgba(
    const D3DFORMAT format, const uint8_t *const row, uint8_t *const out,
    const int32_t width)
{
    for (int32_t x = 0; x < width; x++) {
        uint8_t *const pixel = &out[x * 4];
        if (format == D3DFMT_R5G6B5) {
            uint16_t value;
            memcpy(&value, &row[x * 2], sizeof(value));
            const uint8_t r = (value >> 11) & 0x1f;
            const uint8_t g = (value >> 5) & 0x3f;
            const uint8_t b = value & 0x1f;
            pixel[0] = (uint8_t)(r << 3 | r >> 2);
            pixel[1] = (uint8_t)(g << 2 | g >> 4);
            pixel[2] = (uint8_t)(b << 3 | b >> 2);
        } else {
            pixel[0] = row[x * 4 + 2];
            pixel[1] = row[x * 4 + 1];
            pixel[2] = row[x * 4];
        }
        pixel[3] = 255;
    }
}

static RESULT M_ReadSurface(
    IDirect3DDevice8 *const device, IDirect3DSurface8 *const back,
    IMAGE *const out)
{
    D3DSURFACE_DESC desc;
    FAIL_IF(
        FAILED(back->lpVtbl->GetDesc(back, &desc)),
        "cannot describe the back buffer");
    FAIL_IF(
        desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8
            && desc.Format != D3DFMT_R5G6B5,
        "the back buffer has format %d", (int)desc.Format);
    IDirect3DSurface8 *copy;
    FAIL_IF(
        FAILED(device->lpVtbl->CreateImageSurface(
            device, desc.Width, desc.Height, desc.Format, &copy)),
        "cannot create a surface for the back buffer");
    RESULT result = OK;
    D3DLOCKED_RECT locked;
    if (FAILED(device->lpVtbl->CopyRects(
            device, back, nullptr, 0, copy, nullptr))) {
        result = FAIL("cannot copy the back buffer");
    } else if (
        FAILED(
            copy->lpVtbl->LockRect(copy, &locked, nullptr, D3DLOCK_READONLY))) {
        result = FAIL("cannot lock the back buffer's copy");
    } else {
        *out = Image_Create(
            (int32_t)desc.Width, (int32_t)desc.Height, &g_HeapAllocator);
        for (int32_t y = 0; y < out->height; y++) {
            M_ToRgba(
                desc.Format, (const uint8_t *)locked.pBits + y * locked.Pitch,
                &out->pixels[(size_t)y * (size_t)out->width * 4], out->width);
        }
        copy->lpVtbl->UnlockRect(copy);
    }
    copy->lpVtbl->Release(copy);
    return result;
}

RESULT Direct3d_ReadBackBuffer(IDirect3DDevice8 *const device, IMAGE *const out)
{
    IDirect3DSurface8 *back;
    FAIL_IF(
        FAILED(device->lpVtbl->GetBackBuffer(
            device, 0, D3DBACKBUFFER_TYPE_MONO, &back)),
        "cannot get the back buffer");
    const RESULT result = M_ReadSurface(device, back, out);
    back->lpVtbl->Release(back);
    return result;
}
