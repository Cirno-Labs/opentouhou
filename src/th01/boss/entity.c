#include "th01/boss/entity.h"
#include "th01/sprite.h"
#include "core/base/assert.h"
#include <string.h>

#include "pc98/zun/math16.h"

typedef struct {
    const BOS *entities[BOSS_ENTITY_SLOTS];
    const BOS *animations[BOSS_ANIMATION_SLOTS];
} BOSS_SPRITES;

typedef enum {
    BOSS_PUT,
    BOSS_ERASE,
    BOSS_RESTORE_PUT,
    BOSS_ANIMATION_PUT,
} BOSS_PUT_KIND;

static BOSS_SPRITES m_Sprites;

static RESULT M_Validate(const BOS *const sprites)
{
    FAIL_IF(sprites == nullptr, "Missing boss sprites");
    FAIL_IF(
        sprites->pixels == nullptr || sprites->coverage == nullptr,
        "Missing boss sprite storage");
    FAIL_IF(
        sprites->width < 16 || sprites->width > 2032 || sprites->width % 16 != 0
            || sprites->height < 1 || sprites->height > 255,
        "Invalid boss sprite dimensions");
    FAIL_IF(
        sprites->count < 1 || sprites->count > 8,
        "Invalid boss sprite image count");
    return OK;
}

static BOSS_METADATA M_Metadata(const BOS *const sprites, const uint8_t slot)
{
    return (BOSS_METADATA) { .width_bytes = (int16_t)(sprites->width / 8),
                             .height = (int16_t)sprites->height,
                             .count = (int16_t)sprites->count,
                             .slot = slot };
}

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
    const int32_t page, const int32_t plane, const uint32_t offset)
{
    uint32_t byte;
    int32_t target;
    if (!M_Location(plane, offset, &byte, &target)) {
        return 0;
    }
    uint8_t value = 0;
    for (int32_t bit = 0; bit < 8; bit++) {
        if ((g_Video.pages[page][byte * 8 + bit] & (1u << target)) != 0) {
            value |= 0x80u >> bit;
        }
    }
    return value;
}

static void M_WriteByte(
    const int32_t page, const int32_t plane, const uint32_t offset,
    const uint8_t value)
{
    uint32_t byte;
    int32_t target;
    if (!M_Location(plane, offset, &byte, &target)) {
        return;
    }
    const uint8_t mask = (uint8_t)(1u << target);
    for (int32_t bit = 0; bit < 8; bit++) {
        uint8_t *const pixel = &g_Video.pages[page][byte * 8 + bit];
        *pixel = (uint8_t)((*pixel & ~mask)
                           | ((value & (0x80u >> bit)) != 0 ? mask : 0));
    }
}

static uint16_t M_ReadWord(
    const int32_t page, const int32_t plane, const uint16_t offset)
{
    return (uint16_t)(M_ReadByte(page, plane, offset)
                      | (uint16_t)M_ReadByte(page, plane, (uint32_t)offset + 1)
                          << 8);
}

static void M_WriteWord(
    const int32_t page, const int32_t plane, const uint16_t offset,
    const uint16_t value)
{
    M_WriteByte(page, plane, offset, (uint8_t)value);
    M_WriteByte(page, plane, (uint32_t)offset + 1, (uint8_t)(value >> 8));
}

static void M_EraseByte(
    const int32_t page, const uint32_t offset, const uint8_t mask)
{
    const uint32_t byte = offset & 0x7fffu;
    if (byte < 32000) {
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask & (0x80u >> bit)) != 0) {
                g_Video.pages[page][byte * 8 + bit] = 0;
            }
        }
    }
}

static void M_EraseWord(
    const int32_t page, const uint16_t offset, const uint16_t mask)
{
    M_EraseByte(page, offset, (uint8_t)mask);
    M_EraseByte(page, (uint32_t)offset + 1, (uint8_t)(mask >> 8));
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
    const int16_t left, const int16_t top, const int16_t image,
    const BOSS_PUT_KIND kind)
{
    if (M_Get(metadata, sprites, image) == nullptr) {
        return;
    }
    int16_t row_offset = Math16_Wrap(left / 8 + top * 80);
    int32_t source = 0;
    for (int32_t y = 0; y < metadata->height; y++) {
        int16_t offset = row_offset;
        const int32_t intended = row_offset / 80 + (left < 0 ? 1 : 0);
        for (int32_t word = 0; word < metadata->width_bytes / 2; word++) {
            const bool plain = kind == BOSS_PUT || kind == BOSS_ANIMATION_PUT;
            if (offset / 80 == intended
                && (plain
                    || (offset >= 0
                        && Math16_Wrap(offset + 1) / 80 == intended))) {
                const uint16_t coverage =
                    M_SourceWord(sprites, image, source, -1);
                const uint16_t address = (uint16_t)offset;
                if (kind == BOSS_RESTORE_PUT) {
                    uint16_t background[4];
                    for (int32_t plane = 0; plane < 4; plane++) {
                        background[plane] =
                            M_ReadWord(1, plane, address) & ~coverage;
                    }
                    Video_SelectDrawingPage(0);
                    for (int32_t plane = 0; plane < 4; plane++) {
                        M_WriteWord(
                            0, plane, address,
                            (uint16_t)(background[plane]
                                       | M_SourceWord(
                                           sprites, image, source, plane)));
                    }
                } else if (kind == BOSS_ERASE) {
                    Video_SelectDrawingPage(0);
                    M_EraseWord(0, address, coverage);
                    for (int32_t plane = 0; plane < 4; plane++) {
                        M_WriteWord(
                            0, plane, address,
                            (uint16_t)(M_ReadWord(0, plane, address)
                                       | (M_ReadWord(1, plane, address)
                                          & coverage)));
                    }
                } else if (coverage != 0 || kind == BOSS_ANIMATION_PUT) {
                    M_EraseWord(g_Video.drawing_page, address, coverage);
                    for (int32_t plane = 0; plane < 4; plane++) {
                        M_WriteWord(
                            g_Video.drawing_page, plane, address,
                            (uint16_t)(M_ReadWord(
                                           g_Video.drawing_page, plane, address)
                                       | M_SourceWord(
                                           sprites, image, source, plane)));
                    }
                }
            }
            offset = Math16_Wrap(offset + 2);
            source++;
        }
        row_offset = Math16_Wrap(row_offset + 80);
        if (row_offset >= 32000) {
            break;
        }
    }
    if (kind == BOSS_ERASE) {
        Video_SelectDrawingPage(0);
    }
}

static uint16_t M_ShiftSigned(const uint16_t word, const int32_t shift)
{
    const int32_t value = Math16_Wrap(word);
    const int32_t divisor = 1 << shift;
    const int32_t right = value / divisor - (value % divisor < 0);
    return (uint16_t)((uint32_t)right + ((uint32_t)word << (16 - shift)));
}

static void M_Line(
    const BOSS_ENTITY *const entity, const int16_t left, const int16_t top,
    const int16_t image, const int16_t row, const bool restore)
{
    ASSERT(row >= 0 && row < entity->sprites.height);
    ASSERT(entity->sprites.slot < BOSS_ENTITY_SLOTS);
    const BOS *const sprites = m_Sprites.entities[entity->sprites.slot];
    if (M_Get(&entity->sprites, sprites, image) == nullptr) {
        return;
    }
    const int32_t shift = (uint16_t)left & 7;
    const int16_t row_offset =
        Math16_Wrap(left / 8 - (left % 8 < 0) + top * 80);
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
                    background[plane] = M_ReadWord(1, plane, address) & rotated;
                }
                Video_SelectDrawingPage(0);
                for (int32_t plane = 0; plane < 4; plane++) {
                    const uint16_t foreground =
                        M_SourceWord(sprites, image, source, plane);
                    M_WriteWord(
                        0, plane, address,
                        (uint16_t)(background[plane]
                                   | (shift == 0
                                          ? foreground
                                          : M_ShiftSigned(foreground, shift))));
                }
            } else {
                for (int32_t byte = 0; byte < 2; byte++) {
                    const uint8_t mask = (uint8_t)(coverage >> (byte * 8));
                    M_EraseByte(
                        g_Video.drawing_page, (uint32_t)address + byte,
                        (uint8_t)(mask >> shift));
                    if (shift != 0) {
                        M_EraseByte(
                            g_Video.drawing_page, (uint32_t)address + byte + 1,
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
                                g_Video.drawing_page, plane, at,
                                M_ReadByte(g_Video.drawing_page, plane, at)
                                    | dots);
                        }
                    }
                }
            }
        }
        offset = Math16_Wrap(offset + 2);
    }
}

void BossEntity_Init(void)
{
    memset(&m_Sprites, 0, sizeof(m_Sprites));
}

RESULT BossEntity_Bind(
    BOSS_ENTITY *const entity, const uint8_t slot, const BOS *const sprites)
{
    FAIL_IF(slot >= BOSS_ENTITY_SLOTS, "Invalid boss entity slot");
    MUST(M_Validate(sprites));
    m_Sprites.entities[slot] = sprites;
    entity->sprites = M_Metadata(sprites, slot);
    return OK;
}

RESULT BossAnimation_Bind(
    BOSS_ANIMATION *const animation, const uint8_t slot,
    const BOS *const sprites)
{
    FAIL_IF(slot >= BOSS_ANIMATION_SLOTS, "Invalid boss animation slot");
    MUST(M_Validate(sprites));
    m_Sprites.animations[slot] = sprites;
    animation->sprites = M_Metadata(sprites, slot);
    return OK;
}

void BossEntity_CopyMetadata(
    BOSS_ENTITY *const entity, const BOSS_ENTITY *const source)
{
    entity->sprites = source->sprites;
}

void BossEntity_SetPosition(
    BOSS_ENTITY *const entity, const int16_t left, const int16_t top,
    const BOSS_BOUNDS clamp)
{
    entity->left = left;
    entity->top = top;
    entity->move_clamp = clamp;
}

void BossEntity_SetOrbHitbox(
    BOSS_ENTITY *const entity, const BOSS_BOUNDS bounds)
{
    entity->orb_hitbox = (BOSS_BOUNDS) {
        .left = Math16_Wrap(bounds.left + ORB_HITBOX_MAX_X - ORB_HITBOX_MIN_X),
        .right =
            Math16_Wrap(bounds.right - ORB_HITBOX_MAX_X + ORB_HITBOX_MIN_X),
        .top = Math16_Wrap(bounds.top + ORB_HITBOX_MAX_Y - ORB_HITBOX_MIN_Y),
        .bottom =
            Math16_Wrap(bounds.bottom - ORB_HITBOX_MAX_Y + ORB_HITBOX_MIN_Y),
    };
}

bool BossEntity_HitTestOrb(const BOSS_ENTITY *const entity)
{
    return !entity->orb_inactive
        && Math16_Wrap(
               entity->orb_hitbox.left + entity->left - ORB_HITBOX_MAX_X)
        <= g_Orb->previous_left
        && Math16_Wrap(
               entity->orb_hitbox.right + entity->left - ORB_HITBOX_MIN_X)
        >= g_Orb->previous_left
        && Math16_Wrap(entity->top + entity->orb_hitbox.top - ORB_HITBOX_MAX_Y)
        <= g_Orb->previous_top
        && Math16_Wrap(
               entity->top + entity->orb_hitbox.bottom - ORB_HITBOX_MIN_Y)
        >= g_Orb->previous_top;
}

void BossEntity_Move(
    BOSS_ENTITY *const entity, const int16_t delta_x, const int16_t delta_y)
{
    entity->previous_left = entity->left;
    entity->previous_top = entity->top;
    entity->left = Math16_Wrap(entity->left + delta_x);
    if (entity->move_clamp.left > entity->left) {
        entity->left = entity->move_clamp.left;
    }
    if (entity->move_clamp.right < entity->left) {
        entity->left = entity->move_clamp.right;
    }
    entity->top = Math16_Wrap(entity->top + delta_y);
    if (entity->top < entity->move_clamp.top) {
        entity->top = entity->move_clamp.top;
    }
    if (entity->top > entity->move_clamp.bottom) {
        entity->top = entity->move_clamp.bottom;
    }
}

void BossEntity_Put(
    const BOSS_ENTITY *const e, const int16_t x, const int16_t y,
    const int16_t image)
{
    ASSERT(e->sprites.slot < BOSS_ENTITY_SLOTS);
    M_Full(
        &e->sprites, m_Sprites.entities[e->sprites.slot], x, y, image,
        BOSS_PUT);
}

void BossEntity_Erase(
    const BOSS_ENTITY *const e, const int16_t x, const int16_t y,
    const int16_t image)
{
    ASSERT(e->sprites.slot < BOSS_ENTITY_SLOTS);
    M_Full(
        &e->sprites, m_Sprites.entities[e->sprites.slot], x, y, image,
        BOSS_ERASE);
}

void BossEntity_RestorePut(
    const BOSS_ENTITY *const e, const int16_t x, const int16_t y,
    const int16_t image)
{
    ASSERT(e->sprites.slot < BOSS_ENTITY_SLOTS);
    M_Full(
        &e->sprites, m_Sprites.entities[e->sprites.slot], x, y, image,
        BOSS_RESTORE_PUT);
}

void BossEntity_PutLine(
    const BOSS_ENTITY *const e, const int16_t x, const int16_t y,
    const int16_t image, const int16_t row)
{
    M_Line(e, x, y, image, row, false);
}

void BossEntity_RestorePutLine(
    const BOSS_ENTITY *const e, const int16_t x, const int16_t y,
    const int16_t image, const int16_t row)
{
    M_Line(e, x, y, image, row, true);
}

void BossEntity_WavePut(
    const BOSS_ENTITY *const e, const int16_t x, const int16_t y,
    const int16_t image, const int16_t wavelength, const int16_t amplitude,
    const int16_t phase)
{
    ASSERT(wavelength != 0);
    int16_t angle = phase;
    for (int32_t row = 0; row < e->sprites.height; row++) {
        const int16_t left = Math16_Wrap(x + Math16_Wave(amplitude, angle));
        angle = Math16_Wrap(angle + 256 / wavelength);
        BossEntity_PutLine(e, left, Math16_Wrap(y + row), image, (int16_t)row);
    }
}

void BossAnimation_Put(const BOSS_ANIMATION *const a)
{
    ASSERT(a->sprites.slot < BOSS_ANIMATION_SLOTS);
    M_Full(
        &a->sprites, m_Sprites.animations[a->sprites.slot], a->left, a->top,
        a->image, BOSS_ANIMATION_PUT);
}

void BossEntity_EraseRect(const BOSS_ENTITY *const e)
{
    Sprite_Restore16(
        e->left, e->top, (int16_t)(e->sprites.width_bytes * 8),
        e->sprites.height);
}

void BossEntity_LockedMovePut(
    BOSS_ENTITY *const e, const int16_t dx, const int16_t dy,
    const int16_t frames, const bool restore)
{
    if (e->lock_frame == 0) {
        BossEntity_Move(e, dx, dy);
        if (restore) {
            const int16_t left = dx > 0
                ? (int16_t)(e->previous_left / 8 * 8)
                : Math16_Wrap(e->left / 8 * 8 + e->sprites.width_bytes * 8);
            Sprite_Restore16(left, e->previous_top, 8, e->sprites.height);
            const int16_t top = e->top > e->previous_top
                ? e->previous_top
                : Math16_Wrap(e->top + e->sprites.height);
            const int16_t difference = Math16_Wrap(e->top - e->previous_top);
            Sprite_Restore16(
                e->previous_left, top, (int16_t)(e->sprites.width_bytes * 8),
                Math16_Wrap(difference < 0 ? -difference : difference));
            BossEntity_RestorePut(e, e->left, e->top, e->image);
        } else {
            BossEntity_Put(e, e->left, e->top, e->image);
        }
        e->lock_frame = 1;
    } else if (e->lock_frame >= frames) {
        e->lock_frame = 0;
    } else {
        e->lock_frame = Math16_Wrap(e->lock_frame + 1);
    }
}
