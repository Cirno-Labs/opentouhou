#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/text/font_registry.h"
#include "core/text/private.h"

#include <windows.h>

// GetFontData takes table tags in little-endian order.
#define M_TAG_TTCF 0x66637474

// Resolves family through GDI, which knows the localized names that the
// registry's list of installed fonts lacks.
RESULT FontRegistry_AddSystemFont(const char *const family)
{
    FAIL_IF(family == nullptr, "Missing font family");
    wchar_t face[LF_FACESIZE];
    FAIL_IF(
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, family, -1, face, LF_FACESIZE)
            == 0,
        "The font family %s cannot be converted for Windows", family);
    HFONT const font = CreateFontW(
        0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
    FAIL_IF(font == nullptr, "Windows cannot create the font %s", family);
    HDC const dc = CreateCompatibleDC(nullptr);
    if (dc == nullptr) {
        DeleteObject(font);
        return FAIL("Windows cannot create a device context for fonts");
    }
    HGDIOBJ const previous = SelectObject(dc, font);
    DWORD table = M_TAG_TTCF;
    DWORD size = GetFontData(dc, table, 0, nullptr, 0);
    if (size == GDI_ERROR) {
        table = 0;
        size = GetFontData(dc, table, 0, nullptr, 0);
    }
    uint8_t *data = nullptr;
    if (size != GDI_ERROR && size != 0) {
        data = Mem_Alloc(size);
        if (GetFontData(dc, table, 0, data, size) != size) {
            Mem_Free(data);
            data = nullptr;
        }
    }
    SelectObject(dc, previous);
    DeleteDC(dc);
    DeleteObject(font);
    FAIL_IF(data == nullptr, "Windows has no font data for %s", family);
    char *const source = Str_Format("GDI's font for %s", family);
    const RESULT result =
        FontRegistry_AddSystemData(source, data, size, family);
    Mem_Free(source);
    return result;
}
