#include "th01/boss_entity.h"

#include "pc98/zun/math16.h"

BOSS_SPRITES g_BossSprites;

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

RESULT BossEntity_Bind(
    BOSS_ENTITY *const entity, const uint8_t slot, const BOS *const sprites)
{
    FAIL_IF(slot >= BOSS_ENTITY_SLOTS, "Invalid boss entity slot");
    MUST(M_Validate(sprites));
    g_BossSprites.entities[slot] = sprites;
    entity->sprites = M_Metadata(sprites, slot);
    return OK;
}

RESULT BossAnimation_Bind(
    BOSS_ANIMATION *const animation, const uint8_t slot,
    const BOS *const sprites)
{
    FAIL_IF(slot >= BOSS_ANIMATION_SLOTS, "Invalid boss animation slot");
    MUST(M_Validate(sprites));
    g_BossSprites.animations[slot] = sprites;
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

bool BossEntity_HitTestOrb(
    const BOSS_ENTITY *const entity, const ORB *const orb)
{
    return !entity->orb_inactive
        && Math16_Wrap(
               entity->orb_hitbox.left + entity->left - ORB_HITBOX_MAX_X)
        <= orb->previous_left
        && Math16_Wrap(
               entity->orb_hitbox.right + entity->left - ORB_HITBOX_MIN_X)
        >= orb->previous_left
        && Math16_Wrap(entity->top + entity->orb_hitbox.top - ORB_HITBOX_MAX_Y)
        <= orb->previous_top
        && Math16_Wrap(
               entity->top + entity->orb_hitbox.bottom - ORB_HITBOX_MIN_Y)
        >= orb->previous_top;
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
