#include "session_retail.h"

#include "capture.h"
#include "clock.h"
#include "session.h"

#include <d3d8.h>

static CAPTURE_SESSION m_Session;
static bool m_Opened;
static bool m_ClockStarted;
static bool m_CalcSeen;
static HRESULT(STDMETHODCALLTYPE *m_Present)(
    IDirect3DDevice8 *, const RECT *, const RECT *, HWND, const RGNDATA *);

static HRESULT STDMETHODCALLTYPE M_Present(
    IDirect3DDevice8 *const device, const RECT *const src,
    const RECT *const dst, const HWND window, const RGNDATA *const dirty)
{
    const HRESULT result = m_Present(device, src, dst, window, dirty);
    if (FAILED(result) || Session_Done(&m_Session)) {
        return result;
    }
    D3DDEVICE_CREATION_PARAMETERS parameters;
    Capture_Check(
        SUCCEEDED(device->lpVtbl->GetCreationParameters(device, &parameters))
            ? OK
            : FAIL("cannot query the retail window"));
    const HWND target = window != nullptr ? window : parameters.hFocusWindow;
    RECT rect;
    POINT origin = {};
    Capture_Check(
        GetClientRect(target, &rect) && ClientToScreen(target, &origin)
            ? OK
            : FAIL("cannot query the retail capture rectangle"));
    Capture_Check(Session_Presented(
        &m_Session, origin.x, origin.y, rect.right, rect.bottom));
    if (Session_Done(&m_Session)) {
        Capture_CloseCase();
        Capture_CloseGame();
    }
    return result;
}

RESULT SessionRetail_Open(
    const SCRIPT_BINDING *const bindings, const size_t count)
{
    wchar_t wide[32768];
    const DWORD font_length =
        GetEnvironmentVariableW(L"CAPTURE_FONT", wide, ARRAY_SIZE(wide));
    if (font_length != 0) {
        FAIL_IF(
            font_length >= ARRAY_SIZE(wide)
                || AddFontResourceExW(wide, FR_PRIVATE, nullptr) == 0,
            "cannot register the capture font");
    }
    const DWORD length =
        GetEnvironmentVariableW(L"CAPTURE_INPUTS", wide, ARRAY_SIZE(wide));
    FAIL_IF(
        length == 0 || length >= ARRAY_SIZE(wide), "missing CAPTURE_INPUTS");
    char path[32768];
    FAIL_IF(
        WideCharToMultiByte(
            CP_UTF8, 0, wide, -1, path, sizeof(path), nullptr, nullptr)
            == 0,
        "invalid input script path");
    MUST(Session_Open(&m_Session, path, bindings, count));
    m_Opened = true;
    return OK;
}

void SessionRetail_BeforeCalc(void *const device)
{
    if (!m_Opened) {
        return;
    }
    if (m_CalcSeen) {
        if (!m_ClockStarted) {
            Clock_UseFrames();
            m_ClockStarted = true;
        }
        Clock_AdvanceFrame();
    }
    m_CalcSeen = true;
    if (m_Present != nullptr || device == nullptr) {
        return;
    }
    IDirect3DDevice8Vtbl *const table =
        (IDirect3DDevice8Vtbl *)((IDirect3DDevice8 *)device)->lpVtbl;
    DWORD previous;
    Capture_Check(
        VirtualProtect(
            &table->Present, sizeof(table->Present), PAGE_READWRITE, &previous)
            ? OK
            : FAIL("cannot hook retail presentation"));
    m_Present = table->Present;
    table->Present = M_Present;
    DWORD ignored;
    VirtualProtect(&table->Present, sizeof(table->Present), previous, &ignored);
}

uint16_t __cdecl SessionRetail_Buttons(void)
{
    return Script_Buttons(&m_Session.script, m_Session.frame);
}
