#include "th01/boss_render.h"

#include "core/base/assert.h"
#include "th01/math.h"
#include "th01/sprite.h"

typedef enum {
    BOSS_PUT,
    BOSS_ERASE,
    BOSS_RESTORE_PUT,
    BOSS_ANIMATION_PUT,
} BOSS_PUT_KIND;

static bool M_Location(
    const int32_t plane, const uint32_t offset, uint32_t *const byte,
    int32_t *const target)
{
    static const uint32_t bases[4] = { 0xa8000, 0xb0000, 0xb8000, 0xe0000 };
    const uint32_t address = bases[plane] + offset;
    for (int32_t i = 0; i < 4; i++) {
        if (address >= bases[i] && address - bases[i] < 32000) {
            *byte = address - bases[i];
            *target = i;
            return true;
        }
    }
    return false;
}

static uint8_t M_ReadByte(
    const VIDEO *const video, const int32_t page, const int32_t plane,
    const uint32_t offset)
{
    uint32_t byte;
    int32_t target;
    if (!M_Location(plane, offset, &byte, &target)) {
        return 0;
    }
    uint8_t value = 0;
    for (int32_t bit = 0; bit < 8; bit++) {
        if ((video->pages[page][byte * 8 + bit] & (1u << target)) != 0) {
            value |= 0x80u >> bit;
        }
    }
    return value;
}

static void M_WriteByte(
    VIDEO *const video, const int32_t page, const int32_t plane,
    const uint32_t offset, const uint8_t value)
{
    uint32_t byte;
    int32_t target;
    if (!M_Location(plane, offset, &byte, &target)) {
        return;
    }
    const uint8_t mask = (uint8_t)(1u << target);
    for (int32_t bit = 0; bit < 8; bit++) {
        uint8_t *const pixel = &video->pages[page][byte * 8 + bit];
        *pixel = (uint8_t)((*pixel & ~mask)
                           | ((value & (0x80u >> bit)) != 0 ? mask : 0));
    }
}

static uint16_t M_ReadWord(
    const VIDEO *const video, const int32_t page, const int32_t plane,
    const uint16_t offset)
{
    return (uint16_t)(M_ReadByte(video, page, plane, offset)
                      | (uint16_t)M_ReadByte(
                            video, page, plane, (uint32_t)offset + 1)
                          << 8);
}

static void M_WriteWord(
    VIDEO *const video, const int32_t page, const int32_t plane,
    const uint16_t offset, const uint16_t value)
{
    M_WriteByte(video, page, plane, offset, (uint8_t)value);
    M_WriteByte(
        video, page, plane, (uint32_t)offset + 1, (uint8_t)(value >> 8));
}

static void M_EraseByte(
    VIDEO *const video, const int32_t page, const uint32_t offset,
    const uint8_t mask)
{
    const uint32_t byte = offset & 0x7fffu;
    if (byte < 32000) {
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask & (0x80u >> bit)) != 0) {
                video->pages[page][byte * 8 + bit] = 0;
            }
        }
    }
}

static void M_EraseWord(
    VIDEO *const video, const int32_t page, const uint16_t offset,
    const uint16_t mask)
{
    M_EraseByte(video, page, offset, (uint8_t)mask);
    M_EraseByte(video, page, (uint32_t)offset + 1, (uint8_t)(mask >> 8));
}

static const BOS *M_Get(
    const BOSS_METADATA *const metadata, const BOS *const sprites,
    const int16_t image)
{
    if (sprites == nullptr || image < 0 || image >= metadata->count
        || image >= sprites->count || metadata->width_bytes <= 0
        || metadata->width_bytes % 2 != 0 || metadata->height <= 0
        || (int32_t)metadata->width_bytes * 8 * metadata->height
            > sprites->width * sprites->height) {
        return nullptr;
    }
    return sprites;
}

static uint16_t M_SourceWord(
    const BOS *const sprites, const int16_t image, const int32_t word,
    const int32_t plane)
{
    const size_t start =
        (size_t)image * sprites->width * sprites->height + word * 16;
    uint16_t value = 0;
    for (int32_t bit = 0; bit < 16; bit++) {
        const bool set = plane < 0
            ? sprites->coverage[start + bit] != 0
            : (sprites->pixels[start + bit] & (1u << plane)) != 0;
        if (set) {
            value |= (uint16_t)(0x80u >> (bit % 8)) << (bit / 8 * 8);
        }
    }
    return value;
}

static void M_Full(
    const BOSS_METADATA *const metadata, const BOS *const sprites,
    VIDEO *const video, const int16_t left, const int16_t top,
    const int16_t image, const BOSS_PUT_KIND kind)
{
    if (M_Get(metadata, sprites, image) == nullptr) {
        return;
    }
    int16_t row_offset = Math_Wrap16(left / 8 + top * 80);
    int32_t source = 0;
    for (int32_t y = 0; y < metadata->height; y++) {
        int16_t offset = row_offset;
        const int32_t intended = row_offset / 80 + (left < 0 ? 1 : 0);
        for (int32_t word = 0; word < metadata->width_bytes / 2; word++) {
            const bool plain = kind == BOSS_PUT || kind == BOSS_ANIMATION_PUT;
            if (offset / 80 == intended
                && (plain
                    || (offset >= 0
                        && Math_Wrap16(offset + 1) / 80 == intended))) {
                const uint16_t coverage =
                    M_SourceWord(sprites, image, source, -1);
                const uint16_t address = (uint16_t)offset;
                if (kind == BOSS_RESTORE_PUT) {
                    uint16_t background[4];
                    for (int32_t plane = 0; plane < 4; plane++) {
                        background[plane] =
                            M_ReadWord(video, 1, plane, address) & ~coverage;
                    }
                    Video_SelectDrawingPage(video, 0);
                    for (int32_t plane = 0; plane < 4; plane++) {
                        M_WriteWord(
                            video, 0, plane, address,
                            (uint16_t)(background[plane]
                                       | M_SourceWord(
                                           sprites, image, source, plane)));
                    }
                } else if (kind == BOSS_ERASE) {
                    Video_SelectDrawingPage(video, 0);
                    M_EraseWord(video, 0, address, coverage);
                    for (int32_t plane = 0; plane < 4; plane++) {
                        M_WriteWord(
                            video, 0, plane, address,
                            (uint16_t)(M_ReadWord(video, 0, plane, address)
                                       | (M_ReadWord(video, 1, plane, address)
                                          & coverage)));
                    }
                } else if (coverage != 0 || kind == BOSS_ANIMATION_PUT) {
                    M_EraseWord(video, video->drawing_page, address, coverage);
                    for (int32_t plane = 0; plane < 4; plane++) {
                        M_WriteWord(
                            video, video->drawing_page, plane, address,
                            (uint16_t)(M_ReadWord(
                                           video, video->drawing_page, plane,
                                           address)
                                       | M_SourceWord(
                                           sprites, image, source, plane)));
                    }
                }
            }
            offset = Math_Wrap16(offset + 2);
            source++;
        }
        row_offset = Math_Wrap16(row_offset + 80);
        if (row_offset >= 32000) {
            break;
        }
    }
    if (kind == BOSS_ERASE) {
        Video_SelectDrawingPage(video, 0);
    }
}

static uint16_t M_ShiftSigned(const uint16_t word, const int32_t shift)
{
    const int32_t value = Math_Wrap16(word);
    const int32_t divisor = 1 << shift;
    const int32_t right = value / divisor - (value % divisor < 0);
    return (uint16_t)((uint32_t)right + ((uint32_t)word << (16 - shift)));
}

static void M_Line(
    const BOSS_ENTITY *const entity, const BOSS_SPRITES *const bank,
    VIDEO *const video, const int16_t left, const int16_t top,
    const int16_t image, const int16_t row, const bool restore)
{
    ASSERT(row >= 0 && row < entity->sprites.height);
    ASSERT(entity->sprites.slot < BOSS_ENTITY_SLOTS);
    const BOS *const sprites = bank->entities[entity->sprites.slot];
    if (M_Get(&entity->sprites, sprites, image) == nullptr) {
        return;
    }
    const int32_t shift = (uint16_t)left & 7;
    const int16_t row_offset =
        Math_Wrap16(left / 8 - (left % 8 < 0) + top * 80);
    const int32_t intended = row_offset / 80 + (left < 0 ? 1 : 0);
    int16_t offset = row_offset;
    for (int32_t word = 0; word < entity->sprites.width_bytes / 2; word++) {
        const int32_t source = entity->sprites.width_bytes / 2 * row + word;
        if (offset / 80 == intended) {
            const uint16_t coverage = M_SourceWord(sprites, image, source, -1);
            const uint16_t address = (uint16_t)offset;
            if (restore) {
                const uint16_t alpha = (uint16_t)~coverage;
                const uint16_t rotated = shift == 0
                    ? alpha
                    : (uint16_t)(((uint32_t)alpha >> shift)
                                 | ((uint32_t)alpha << (16 - shift)));
                uint16_t background[4];
                for (int32_t plane = 0; plane < 4; plane++) {
                    background[plane] =
                        M_ReadWord(video, 1, plane, address) & rotated;
                }
                Video_SelectDrawingPage(video, 0);
                for (int32_t plane = 0; plane < 4; plane++) {
                    const uint16_t foreground =
                        M_SourceWord(sprites, image, source, plane);
                    M_WriteWord(
                        video, 0, plane, address,
                        (uint16_t)(background[plane]
                                   | (shift == 0
                                          ? foreground
                                          : M_ShiftSigned(foreground, shift))));
                }
            } else {
                for (int32_t byte = 0; byte < 2; byte++) {
                    const uint8_t mask = (uint8_t)(coverage >> (byte * 8));
                    M_EraseByte(
                        video, video->drawing_page, (uint32_t)address + byte,
                        (uint8_t)(mask >> shift));
                    if (shift != 0) {
                        M_EraseByte(
                            video, video->drawing_page,
                            (uint32_t)address + byte + 1,
                            (uint8_t)((uint32_t)mask << (8 - shift)));
                    }
                    for (int32_t part = 0; part < (shift == 0 ? 1 : 2);
                         part++) {
                        for (int32_t plane = 0; plane < 4; plane++) {
                            const uint8_t value =
                                (uint8_t)(M_SourceWord(
                                              sprites, image, source, plane)
                                          >> (byte * 8));
                            const uint32_t at = (uint32_t)address + byte + part;
                            const uint8_t dots = part == 0
                                ? value >> shift
                                : (uint8_t)((uint32_t)value << (8 - shift));
                            M_WriteByte(
                                video, video->drawing_page, plane, at,
                                M_ReadByte(
                                    video, video->drawing_page, plane, at)
                                    | dots);
                        }
                    }
                }
            }
        }
        offset = Math_Wrap16(offset + 2);
    }
}

void BossEntity_Put(
    const BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t x, const int16_t y, const int16_t image)
{
    ASSERT(e->sprites.slot < BOSS_ENTITY_SLOTS);
    M_Full(&e->sprites, b->entities[e->sprites.slot], v, x, y, image, BOSS_PUT);
}

void BossEntity_Erase(
    const BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t x, const int16_t y, const int16_t image)
{
    ASSERT(e->sprites.slot < BOSS_ENTITY_SLOTS);
    M_Full(
        &e->sprites, b->entities[e->sprites.slot], v, x, y, image, BOSS_ERASE);
}

void BossEntity_RestorePut(
    const BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t x, const int16_t y, const int16_t image)
{
    ASSERT(e->sprites.slot < BOSS_ENTITY_SLOTS);
    M_Full(
        &e->sprites, b->entities[e->sprites.slot], v, x, y, image,
        BOSS_RESTORE_PUT);
}

void BossEntity_PutLine(
    const BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t x, const int16_t y, const int16_t image, const int16_t row)
{
    M_Line(e, b, v, x, y, image, row, false);
}

void BossEntity_RestorePutLine(
    const BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t x, const int16_t y, const int16_t image, const int16_t row)
{
    M_Line(e, b, v, x, y, image, row, true);
}

void BossEntity_WavePut(
    const BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t x, const int16_t y, const int16_t image,
    const int16_t wavelength, const int16_t amplitude, const int16_t phase)
{
    ASSERT(wavelength != 0);
    int16_t angle = phase;
    for (int32_t row = 0; row < e->sprites.height; row++) {
        const int16_t left = Math_Wrap16(x + Math_WaveX(amplitude, angle));
        angle = Math_Wrap16(angle + 256 / wavelength);
        BossEntity_PutLine(
            e, b, v, left, Math_Wrap16(y + row), image, (int16_t)row);
    }
}

void BossAnimation_Put(
    const BOSS_ANIMATION *const a, const BOSS_SPRITES *const b, VIDEO *const v)
{
    ASSERT(a->sprites.slot < BOSS_ANIMATION_SLOTS);
    M_Full(
        &a->sprites, b->animations[a->sprites.slot], v, a->left, a->top,
        a->image, BOSS_ANIMATION_PUT);
}

void BossEntity_EraseRect(const BOSS_ENTITY *const e, VIDEO *const v)
{
    Sprite_Restore16(
        v, e->left, e->top, (int16_t)(e->sprites.width_bytes * 8),
        e->sprites.height);
}

void BossEntity_LockedMovePut(
    BOSS_ENTITY *const e, const BOSS_SPRITES *const b, VIDEO *const v,
    const int16_t dx, const int16_t dy, const int16_t frames,
    const bool restore)
{
    if (e->lock_frame == 0) {
        BossEntity_Move(e, dx, dy);
        if (restore) {
            const int16_t left = dx > 0
                ? (int16_t)(e->previous_left / 8 * 8)
                : Math_Wrap16(e->left / 8 * 8 + e->sprites.width_bytes * 8);
            Sprite_Restore16(v, left, e->previous_top, 8, e->sprites.height);
            const int16_t top = e->top > e->previous_top
                ? e->previous_top
                : Math_Wrap16(e->top + e->sprites.height);
            const int16_t difference = Math_Wrap16(e->top - e->previous_top);
            Sprite_Restore16(
                v, e->previous_left, top, (int16_t)(e->sprites.width_bytes * 8),
                Math_Wrap16(difference < 0 ? -difference : difference));
            BossEntity_RestorePut(e, b, v, e->left, e->top, e->image);
        } else {
            BossEntity_Put(e, b, v, e->left, e->top, e->image);
        }
        e->lock_frame = 1;
    } else if (e->lock_frame >= frames) {
        e->lock_frame = 0;
    } else {
        e->lock_frame = Math_Wrap16(e->lock_frame + 1);
    }
}
