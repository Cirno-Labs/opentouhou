// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/cp932.h"

#include "core/base/memory.h"

#include "cp932_table.h"

#include <stdlib.h>
#include <string.h>

static bool M_IsLeadByte(const uint8_t byte)
{
    return (byte >= 0x81 && byte <= 0x9F) || (byte >= 0xE0 && byte <= 0xFC);
}

static uint32_t M_DecodeDoubleByte(const uint8_t lead, const uint8_t trail)
{
    if (trail < CP932_TRAIL_FIRST || trail > CP932_TRAIL_LAST) {
        return 0;
    }
    const int row = lead <= 0x9F ? lead - 0x81 : lead - 0xE0 + 0x1F;
    return g_Cp932DoubleByte[row][trail - CP932_TRAIL_FIRST];
}

static char *M_PutUtf8(char *const output, const uint32_t code_point)
{
    uint8_t *const out = (uint8_t *)output;
    if (code_point < 0x80) {
        out[0] = (uint8_t)code_point;
        return output + 1;
    }
    if (code_point < 0x800) {
        out[0] = (uint8_t)(0xC0 | (code_point >> 6));
        out[1] = (uint8_t)(0x80 | (code_point & 0x3F));
        return output + 2;
    }
    out[0] = (uint8_t)(0xE0 | (code_point >> 12));
    out[1] = (uint8_t)(0x80 | ((code_point >> 6) & 0x3F));
    out[2] = (uint8_t)(0x80 | (code_point & 0x3F));
    return output + 3;
}

// Decodes the UTF-8 sequence at the start of bytes and returns its length,
// or 0 if it is not valid: truncated, overlong, a surrogate, or beyond
// U+10FFFF.
static size_t M_GetUtf8(
    const uint8_t *const bytes, const size_t size, uint32_t *const code_point)
{
    const uint8_t first = bytes[0];
    size_t length;
    uint32_t minimum;
    uint32_t value;
    if (first < 0x80) {
        *code_point = first;
        return 1;
    } else if ((first & 0xE0) == 0xC0) {
        length = 2;
        minimum = 0x80;
        value = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        minimum = 0x800;
        value = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        minimum = 0x10000;
        value = first & 0x07;
    } else {
        return 0;
    }

    if (length > size) {
        return 0;
    }
    for (size_t i = 1; i < length; i++) {
        if ((bytes[i] & 0xC0) != 0x80) {
            return 0;
        }
        value = (value << 6) | (bytes[i] & 0x3F);
    }
    if (value < minimum || value > 0x10FFFF
        || (value >= 0xD800 && value <= 0xDFFF)) {
        return 0;
    }
    *code_point = value;
    return length;
}

static int M_ComparePair(const void *const key, const void *const element)
{
    const uint32_t code_point = *(const uint32_t *)key;
    const CP932_PAIR *const pair = element;
    return (code_point > pair->code_point) - (code_point < pair->code_point);
}

static RESULT M_FailAt(
    const uint8_t *const bytes, const size_t offset, const char *const encoding)
{
    if (bytes[offset] == 0) {
        return FAIL("NUL byte at offset %zu", offset);
    }
    return FAIL(
        "Invalid %s byte 0x%02X at offset %zu", encoding, bytes[offset],
        offset);
}

RESULT Cp932_Decode(
    const char *const bytes, const size_t size, char **const utf8)
{
    *utf8 = nullptr;
    const uint8_t *const in = (const uint8_t *)bytes;
    char *const result = Mem_AllocArr(size + 1, 3);
    char *out = result;

    size_t i = 0;
    while (i < size) {
        uint32_t code_point;
        size_t length = 1;
        if (M_IsLeadByte(in[i])) {
            code_point =
                i + 1 < size ? M_DecodeDoubleByte(in[i], in[i + 1]) : 0;
            length = 2;
        } else {
            code_point = in[i] == 0 ? 0 : g_Cp932SingleByte[in[i]];
        }
        if (code_point == 0) {
            Mem_Free(result);
            return M_FailAt(in, i, "CP932");
        }
        out = M_PutUtf8(out, code_point);
        i += length;
    }

    *out = '\0';
    *utf8 = Mem_Realloc(result, (size_t)(out - result) + 1);
    return OK;
}

RESULT Cp932_Encode(
    const char *const utf8, const size_t size, char **const bytes)
{
    *bytes = nullptr;
    const uint8_t *const in = (const uint8_t *)utf8;
    char *const result = Mem_Alloc(size + 1);
    uint8_t *out = (uint8_t *)result;

    size_t i = 0;
    while (i < size) {
        uint32_t code_point;
        const size_t length = M_GetUtf8(in + i, size - i, &code_point);
        if (length == 0 || code_point == 0) {
            Mem_Free(result);
            return M_FailAt(in, i, "UTF-8");
        }

        if (code_point < 0x80) {
            *out++ = (uint8_t)code_point;
        } else {
            const CP932_PAIR *const pair = bsearch(
                &code_point, g_Cp932Encode, (size_t)g_Cp932EncodeCount,
                sizeof(CP932_PAIR), M_ComparePair);
            if (pair == nullptr) {
                Mem_Free(result);
                return FAIL(
                    "U+%04X at offset %zu has no CP932 form",
                    (unsigned)code_point, i);
            }
            if (pair->cp932 >= 0x100) {
                *out++ = (uint8_t)(pair->cp932 >> 8);
            }
            *out++ = (uint8_t)(pair->cp932 & 0xFF);
        }
        i += length;
    }

    *out = '\0';
    *bytes = Mem_Realloc(result, (size_t)(out - (uint8_t *)result) + 1);
    return OK;
}

RESULT Cp932_DecodeText(
    const char *const bytes, const size_t size, char **const utf8)
{
    const uint8_t *const in = (const uint8_t *)bytes;
    size_t i = 0;
    while (i < size) {
        uint32_t code_point;
        const size_t length = M_GetUtf8(in + i, size - i, &code_point);
        if (length == 0 || code_point == 0) {
            return Cp932_Decode(bytes, size, utf8);
        }
        i += length;
    }
    *utf8 = Mem_Alloc(size + 1);
    memcpy(*utf8, bytes, size);
    (*utf8)[size] = '\0';
    return OK;
}
