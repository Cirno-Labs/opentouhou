#include "th02/patterns.h"

#include <string.h>

static size_t M_PlaneSize(const PATTERN *const pattern)
{
    return (size_t)(pattern->size >> 8) * (pattern->size & 255);
}

static void M_Free(PATTERN_BANK *const bank, const uint16_t slot)
{
    PATTERN *const pattern = &bank->entries[slot];
    if (pattern->data != nullptr) {
        Allocator_Free(&bank->allocator, pattern->data, pattern->storage_size);
    }
    memset(pattern, 0, sizeof(*pattern));
}

static RESULT M_Dimensions(const int32_t width, const int32_t height)
{
    FAIL_IF(
        width <= 0 || width > 2040 || width % 8 != 0 || height <= 0
            || height > 255 || width / 8 * height > 65535 / 5,
        "Invalid pattern dimensions");
    return OK;
}

void PatternBank_Init(
    PATTERN_BANK *const bank, const ALLOCATOR *const allocator)
{
    memset(bank, 0, sizeof(*bank));
    bank->allocator = *allocator;
}

void PatternBank_Reset(PATTERN_BANK *const bank)
{
    for (uint16_t slot = 0; slot < PATTERN_LIMIT; slot++) {
        M_Free(bank, slot);
    }
    bank->next = 0;
}

RESULT PatternBank_Put(
    PATTERN_BANK *const bank, const uint16_t slot, const int32_t width,
    const int32_t height, const uint8_t *const pixels, const uint8_t key)
{
    FAIL_IF(slot >= PATTERN_LIMIT || key > 15, "Invalid pattern slot or key");
    MUST(M_Dimensions(width, height));
    FAIL_IF(pixels == nullptr, "Missing pattern pixels");
    const size_t pixel_count = (size_t)width * height;
    for (size_t i = 0; i < pixel_count; i++) {
        FAIL_IF(pixels[i] > 15, "Invalid pattern pixel index");
    }
    const size_t plane_size = pixel_count / 8;
    uint8_t *const data = Allocator_AllocZ(&bank->allocator, plane_size * 5);
    for (size_t i = 0; i < pixel_count; i++) {
        const uint8_t color = pixels[i];
        if (color == key) {
            continue;
        }
        const uint8_t mask = 0x80u >> (i % 8);
        data[i / 8] |= mask;
        for (size_t plane = 0; plane < 4; plane++) {
            if ((color & (1u << plane)) != 0) {
                data[(plane + 1) * plane_size + i / 8] |= mask;
            }
        }
    }
    M_Free(bank, slot);
    bank->entries[slot] = (PATTERN) {
        .size = (uint16_t)((width / 8 << 8) | height),
        .data = data,
        .storage_size = plane_size * 5,
    };
    if (slot >= bank->next) {
        bank->next = slot + 1;
    }
    return OK;
}

RESULT PatternBank_AppendBfnt(
    PATTERN_BANK *const bank, const BFNT *const sprites)
{
    FAIL_IF(
        sprites->bits_per_pixel != 4 || sprites->count <= 0
            || sprites->count > PATTERN_LIMIT || sprites->pixels == nullptr,
        "Invalid color BFNT patterns");
    MUST(M_Dimensions(sprites->width, sprites->height));
    const size_t image_size = (size_t)sprites->width * sprites->height;
    for (int32_t image = 0; image < sprites->count; image++) {
        MUST(PatternBank_Put(
            bank, bank->next, sprites->width, sprites->height,
            sprites->pixels + image_size * image, 0));
        bank->entries[bank->next - 1].sheet_start = image == 0;
    }
    return OK;
}

RESULT PatternBank_ConvertTiny(PATTERN_BANK *const bank, const uint16_t slot)
{
    FAIL_IF(
        slot >= bank->next || bank->entries[slot].data == nullptr,
        "Pattern slot is not registered");
    PATTERN *const pattern = &bank->entries[slot];
    const size_t plane_size = M_PlaneSize(pattern);
    FAIL_IF(plane_size % 2 != 0, "Tiny pattern plane size must be even");
    uint8_t *const records = Mem_Alloc((plane_size + 2) * 5);
    size_t offset = 0;
    for (uint8_t color = 0; color < 16; color++) {
        bool covered = false;
        for (size_t byte = 0; byte < plane_size; byte++) {
            uint8_t mask = pattern->data[byte];
            for (size_t plane = 0; plane < 4; plane++) {
                const uint8_t bits =
                    pattern->data[(plane + 1) * plane_size + byte];
                mask &= (color & (1u << plane)) != 0 ? bits : (uint8_t)~bits;
            }
            records[offset + 2 + byte] = mask;
            covered |= mask != 0;
        }
        if (covered) {
            if (offset == (plane_size + 2) * 4) {
                Mem_Free(records);
                return FAIL("Tiny pattern has more than four colors");
            }
            records[offset] = 0x80;
            records[offset + 1] = color;
            offset += plane_size + 2;
        }
    }
    records[offset++] = 0;
    records[offset++] = 0;
    if (offset > pattern->storage_size) {
        uint8_t *const data = Allocator_Alloc(&bank->allocator, offset);
        memcpy(data, pattern->data, pattern->storage_size);
        Allocator_Free(&bank->allocator, pattern->data, pattern->storage_size);
        pattern->data = data;
        pattern->storage_size = offset;
    }
    memcpy(pattern->data, records, offset);
    Mem_Free(records);
    return OK;
}

RESULT PatternBank_SetNext(PATTERN_BANK *const bank, const uint16_t next)
{
    FAIL_IF(next > PATTERN_LIMIT, "Invalid pattern append cursor");
    bank->next = next;
    return OK;
}

RESULT PatternBank_Cancel(PATTERN_BANK *const bank, const uint16_t slot)
{
    FAIL_IF(
        slot >= bank->next || bank->entries[slot].size == 0,
        "Pattern slot is not registered");
    M_Free(bank, slot);
    if (slot + 1 == bank->next) {
        do {
            bank->next--;
        } while (bank->next != 0
                 && bank->entries[bank->next - 1].data == nullptr);
    }
    return OK;
}

void PatternBank_Clean(
    PATTERN_BANK *const bank, const uint16_t first, const uint16_t last)
{
    const uint16_t end = last < bank->next ? last + 1 : bank->next;
    for (uint16_t slot = first; slot < end; slot++) {
        Result_Ignore(PatternBank_Cancel(bank, slot));
    }
}
