// C, so that d3d8.h declares the vtables as structs and the compiler
// checks every replacement against the method it replaces. Mesa's
// GALLIUM_NOOP would drop the drawing below Wine, but wined3d crashes on
// it, and wined3d's no3d renderer gives a game no device.

#include "null_d3d8.h"

#include "iat.h"

#include "core/base/log.h"

#include <d3d8.h>

#include <stdint.h>
#include <stdlib.h>

typedef IDirect3D8 *(WINAPI *DIRECT3D_CREATE8_FN)(UINT sdk_version);
typedef HRESULT(STDMETHODCALLTYPE *CREATE_DEVICE_FN)(
    IDirect3D8 *d3d, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
    D3DPRESENT_PARAMETERS *params, IDirect3DDevice8 **device);

static DIRECT3D_CREATE8_FN m_RealDirect3DCreate8;
static CREATE_DEVICE_FN m_RealCreateDevice;
// Draws, clears and presents dropped.
static volatile LONG m_Dropped;

static HRESULT M_Drop(void)
{
    InterlockedIncrement(&m_Dropped);
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE M_Present(
    IDirect3DDevice8 *device, const RECT *src, const RECT *dst, HWND window,
    const RGNDATA *dirty)
{
    return M_Drop();
}

static HRESULT STDMETHODCALLTYPE M_Clear(
    IDirect3DDevice8 *device, DWORD count, const D3DRECT *rects, DWORD flags,
    D3DCOLOR color, float z, DWORD stencil)
{
    return M_Drop();
}

static HRESULT STDMETHODCALLTYPE M_DrawPrimitive(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT start, UINT count)
{
    return M_Drop();
}

static HRESULT STDMETHODCALLTYPE M_DrawIndexedPrimitive(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT min_index,
    UINT vertices, UINT start, UINT count)
{
    return M_Drop();
}

static HRESULT STDMETHODCALLTYPE M_DrawPrimitiveUP(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT count,
    const void *data, UINT stride)
{
    return M_Drop();
}

static HRESULT STDMETHODCALLTYPE M_DrawIndexedPrimitiveUP(
    IDirect3DDevice8 *device, D3DPRIMITIVETYPE type, UINT min_index,
    UINT vertices, UINT count, const void *indices, D3DFORMAT format,
    const void *data, UINT stride)
{
    return M_Drop();
}

// The vtable is Wine's, shared by every object of the interface, and
// read-only.
static bool M_Unprotect(void *const vtbl, const SIZE_T size, DWORD *const old)
{
    return VirtualProtect(vtbl, size, PAGE_READWRITE, old);
}

static HRESULT STDMETHODCALLTYPE M_CreateDevice(
    IDirect3D8 *const d3d, const UINT adapter, const D3DDEVTYPE type,
    const HWND window, const DWORD flags, D3DPRESENT_PARAMETERS *const params,
    IDirect3DDevice8 **const device)
{
    const HRESULT result =
        m_RealCreateDevice(d3d, adapter, type, window, flags, params, device);
    if (FAILED(result) || *device == nullptr) {
        return result;
    }
    IDirect3DDevice8Vtbl *const vtbl =
        (IDirect3DDevice8Vtbl *)(*device)->lpVtbl;
    DWORD old;
    if (M_Unprotect(vtbl, sizeof(*vtbl), &old)) {
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
    IDirect3D8 *const d3d = m_RealDirect3DCreate8(sdk_version);
    if (d3d == nullptr) {
        return nullptr;
    }
    IDirect3D8Vtbl *const vtbl = (IDirect3D8Vtbl *)d3d->lpVtbl;
    DWORD old;
    if (vtbl->CreateDevice != M_CreateDevice
        && M_Unprotect(vtbl, sizeof(*vtbl), &old)) {
        m_RealCreateDevice = vtbl->CreateDevice;
        vtbl->CreateDevice = M_CreateDevice;
        VirtualProtect(vtbl, sizeof(*vtbl), old, &old);
    }
    return d3d;
}

static void M_LogDropped(void)
{
    LOG_INFO("null renderer: %ld draws dropped", m_Dropped);
}

bool NullD3d8_Install(const HMODULE module)
{
    uintptr_t original;
    if (!Iat_Patch(
            module, "d3d8.dll", "Direct3DCreate8", (uintptr_t)M_Direct3DCreate8,
            &original)) {
        return false;
    }
    m_RealDirect3DCreate8 = (DIRECT3D_CREATE8_FN)original;
    atexit(M_LogDropped);
    return true;
}
