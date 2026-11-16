#include "th01/pellets.h"

#include "th01/sprite.h"

static int16_t M_Speed(
    const PELLET_SHARED *const shared, const int32_t rank, int16_t speed)
{
    speed = Math_Wrap16(
        speed
        + (rank == 0       ? 0
               : rank == 1 ? 6
               : rank == 2 ? 12
                           : 18));
    speed = Math_Wrap16(speed + Math_Wrap16(shared->speed * speed) / 40);
    return speed < 16 ? 16 : speed;
}

static void M_Init(
    PELLET *const p, PELLETS *const pellets, const int16_t left,
    const int16_t top, const int16_t group)
{
    p->decay_frame = 0;
    p->left = Math_SubpixelFromPixel(left);
    p->top = Math_SubpixelFromPixel(top);
    p->cloud_left = left;
    p->cloud_top = top;
    if (pellets->spawn_with_cloud) {
        p->cloud_frame = 1;
    } else {
        p->moving = true;
    }
    p->from_group = group;
    p->previous_left = -1;
    p->age = 0;
    pellets->alive_count_excluding_clouds_after_reset =
        Math_Wrap16(pellets->alive_count_excluding_clouds_after_reset + 1);
}

static MATH_VECTOR M_Aim(
    const PELLET *const p, const PLAYER *const player, const int16_t speed)
{
    return Math_VectorBetween(
        (MATH_VECTOR) { Math_SubpixelToPixel(p->left),
                        Math_SubpixelToPixel(p->top) },
        (MATH_VECTOR) { Math_Wrap16(player->left + 8), 384 }, speed);
}

static void M_Restore(
    VIDEO *const video, const PELLET *const p, const bool current)
{
    Sprite_Restore16(
        video, Math_SubpixelToPixel(current ? p->left : p->previous_left),
        Math_SubpixelToPixel(current ? p->top : p->previous_top), 16, 8);
}

static void M_Motion(
    PELLET *const p, const PLAYER *const player, uint32_t *const rng)
{
    switch (p->motion_type) {
    case PELLET_GRAVITY:
        p->velocity.y = Math_Wrap16(p->velocity.y + p->speed);
        break;
    case PELLET_SLING_AIMED:
        if (p->sling_direction == 0) {
            p->sling_direction = (int16_t)((Math_Random(rng) & 1) + 1);
        }
        p->velocity = Math_Vector(p->speed, p->angle);
        if (p->sling_direction != 1) {
            const int16_t x = p->velocity.x;
            p->velocity.x = p->velocity.y;
            p->velocity.y = x;
        }
        p->angle = Math_Wrap16(p->angle + 8);
        if (p->angle > 256) {
            p->velocity = M_Aim(p, player, p->speed);
            p->from_group = PELLET_GROUP_SINGLE_AIMED;
            p->angle = 0;
        }
        break;
    case PELLET_BOUNCE_FROM_TOP_THEN_GRAVITY:
        if (p->left <= 0 || Math_SubpixelToPixel(p->left) >= 632 * 16
            || p->top <= 70 * 16) {
            p->velocity.x = Math_Wrap16(-p->velocity.x);
            p->velocity.y = 0;
            p->motion_type = PELLET_GRAVITY;
            if (p->left <= 0) {
                p->left = 16;
            }
            if (p->left >= 632 * 16) {
                p->left = 631 * 16;
            }
            if (p->top <= 64 * 16) {
                p->top = 65 * 16;
            }
        }
        break;
    case PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR:
        if (Math_SubpixelToPixel(p->top) <= 70) {
            p->velocity.x = 0;
            p->velocity.y = p->speed;
            p->motion_type = PELLET_REGULAR;
            if (Math_SubpixelToPixel(p->top) <= 64) {
                p->top = 65 * 16;
            }
        }
        break;
    case PELLET_SPIN:
        p->left = Math_PolarX(p->spin_center.x, 256, p->angle);
        p->top = Math_PolarY(p->spin_center.y, 256, p->angle);
        p->spin_center.x = Math_Wrap16(p->spin_center.x + p->spin_velocity.x);
        p->spin_center.y = Math_Wrap16(p->spin_center.y + p->spin_velocity.y);
        p->velocity = (MATH_VECTOR) {};
        p->angle = Math_Wrap16(p->angle + 4);
        break;
    case PELLET_CHASE: {
        const MATH_VECTOR target = M_Aim(p, player, p->speed);
        if (p->top < 320 * 16) {
            p->velocity.x = Math_Wrap16(
                p->velocity.x + (p->velocity.x < target.x)
                - (p->velocity.x > target.x));
            p->velocity.y = Math_Wrap16(
                p->velocity.y + (p->velocity.y < target.y)
                - (p->velocity.y > target.y));
        }
        break;
    }
    default:
        break;
    }
}

static bool M_Overlap(
    const int16_t left, const int16_t top, const int16_t other_left,
    const int16_t other_top, const int16_t width, const int16_t height)
{
    const int16_t dx = Math_Wrap16(left - other_left);
    const int16_t dy = Math_Wrap16(top - other_top);
    return dx < width && dx > -8 && dy < height && dy > -8;
}

static bool M_Visible(PELLET *const p, const PELLET_CONTEXT *const context)
{
    const int16_t left = Math_SubpixelToPixel(p->left);
    const int16_t top = Math_SubpixelToPixel(p->top);
    for (int32_t i = 0; i < 3; i++) {
        const SHOT *const shot = &context->shots->slots[i];
        if ((shot->moving
             && M_Overlap(left, top, shot->left, shot->top, 16, 16))
            || M_Overlap(
                Math_Wrap16(left + 8), top, shot->left, shot->top, 16, 16)) {
            return false;
        }
    }
    if (p->decay_frame != 0) {
        return true;
    }
    if (M_Overlap(left, top, context->orb->left, context->orb->top, 32, 32)
        || M_Overlap(
            Math_Wrap16(left + 8), top, context->orb->left, context->orb->top,
            32, 32)) {
        p->velocity.x = (int16_t)(p->velocity.x / 2 - (p->velocity.x % 2 < 0));
        p->velocity.y = (int16_t)(p->velocity.y / 2 - (p->velocity.y % 2 < 0));
        p->decay_frame = 1;
        context->shared->destroy_score_delta =
            (uint16_t)(context->shared->destroy_score_delta + 10);
        return true;
    }
    if (context->player->deflecting
        && M_Overlap(
            left, top, Math_Wrap16(context->player->left - 8), 352, 48, 48)) {
        p->velocity =
            Math_Vector(128, left <= context->player->left + 12 ? 128 : 0);
        if (p->from_group == 0) {
            p->motion_type = PELLET_REGULAR;
        }
        return false;
    }
    return true;
}

static void M_Clouds(
    PELLETS *const pellets, const PELLET_CONTEXT *const context)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (p->cloud_frame == 0) {
            continue;
        }
        p->cloud_frame = Math_Wrap16(p->cloud_frame + 1);
        const int16_t frame = p->cloud_frame;
        if (frame == 2 || frame == 5 || frame == 6 || frame == 9) {
            PelletAssets_Cloud(
                context->video, context->assets, p->cloud_left, p->cloud_top,
                frame >= 6 ? 1 : 0, frame == 5 || frame == 9);
        }
        if (frame == 9) {
            p->cloud_frame = 0;
            p->moving = true;
        }
    }
}

static void M_Remove(PELLETS *const pellets, PELLET *const p)
{
    p->moving = false;
    p->decay_frame = 0;
    pellets->alive_count_excluding_clouds_after_reset =
        Math_Wrap16(pellets->alive_count_excluding_clouds_after_reset - 1);
}

void Pellets_AddGroup(
    PELLETS *const pellets, PELLET_SHARED *const shared,
    const PLAYER *const player, uint32_t *const rng, const int32_t rank,
    const int16_t left, const int16_t top, const PELLET_GROUP group,
    int16_t speed)
{
    if (pellets->alive_count_excluding_clouds_after_reset > PELLETS_COUNT
        || left >= 640 || left < -8 || top < 56 || top > 400
        || (group != 1 && (group < 12 || group > 30))) {
        return;
    }
    speed = M_Speed(shared, rank, speed);
    int32_t group_i = 0;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (p->moving || p->cloud_frame != 0) {
            continue;
        }
        M_Init(p, pellets, left, top, (int16_t)group);
        int32_t count = 1;
        if (group == PELLET_GROUP_SINGLE) {
            p->velocity = (MATH_VECTOR) { 0, speed };
        } else if (group == PELLET_GROUP_SINGLE_AIMED) {
            p->velocity = M_Aim(p, player, speed);
        } else {
            int32_t angle;
            bool aimed = group >= PELLET_GROUP_TWO_WIDE_AIMED;
            if (group == PELLET_GROUP_RANDOM_NARROW_AIMED) {
                angle = (Math_Random(rng) & 15) - 7;
            } else if (group == PELLET_GROUP_RANDOM_WIDE) {
                angle = (Math_Random(rng) & 63) + 32;
                aimed = false;
            } else {
                const int32_t spread = ((int32_t)group - 13) % 8;
                count = 2 + spread / 2;
                const bool wide = (spread & 1) == 0;
                const int32_t step = !wide ? 4
                    : count == 2           ? 12
                    : count == 3           ? 9
                                           : 8;
                if (count == 2) {
                    angle = group_i == 0 ? step : -step;
                } else if (count == 3 || count == 5) {
                    angle = group_i == 0 ? 0
                                         : ((group_i + 1) / 2) * step
                            * ((group_i & 1) != 0 ? 1 : -1);
                } else {
                    angle = ((group_i / 2) * 2 + 1) * step
                        * ((group_i & 1) != 0 ? -1 : 1);
                }
                if (!aimed) {
                    angle += 64;
                }
            }
            if (aimed) {
                angle += Math_Angle(
                    Math_Wrap16(384 - top),
                    Math_Wrap16(player->left + 12 - left));
            }
            p->velocity = Math_Vector(speed, (uint8_t)angle);
        }
        if (++group_i == count) {
            return;
        }
    }
}

void Pellets_AddSingle(
    PELLETS *const pellets, const PELLET_SHARED *const shared,
    const int32_t rank, const int16_t left, const int16_t top,
    const uint8_t angle, const int16_t speed_base, const PELLET_MOTION motion,
    const int16_t speed_for_motion, const int16_t spin_center_x,
    const int16_t spin_center_y)
{
    if (pellets->alive_count_excluding_clouds_after_reset > PELLETS_COUNT) {
        return;
    }
    const int16_t speed = M_Speed(shared, rank, speed_base);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (p->moving || p->cloud_frame != 0) {
            continue;
        }
        M_Init(p, pellets, left, top, 0);
        p->motion_type = (uint8_t)motion;
        p->spin_center =
            (MATH_VECTOR) { Math_SubpixelFromPixel(spin_center_x),
                            Math_SubpixelFromPixel(spin_center_y) };
        if (motion == PELLET_SPIN) {
            p->spin_velocity = Math_Vector(speed_for_motion, angle);
            p->angle = Math_Angle(
                Math_Wrap16(p->top - p->spin_center.y),
                Math_Wrap16(p->left - p->spin_center.x));
        }
        p->speed = speed_for_motion;
        p->velocity = Math_Vector(speed, angle);
        return;
    }
}

void Pellets_UpdateAndRender(
    PELLETS *const pellets, const PELLET_CONTEXT *const context)
{
    pellets->interlace_field = !pellets->interlace_field;
    M_Clouds(pellets, context);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (!p->moving) {
            continue;
        }
        if ((!pellets->interlace || pellets->interlace_field == ((i & 1) != 0))
            && !p->not_rendered && p->previous_left != -1) {
            M_Restore(context->video, p, false);
        }
        if (p->from_group == 0 && p->motion_type != 0) {
            M_Motion(p, context->player, context->rng);
        }
        if (p->velocity.y > 128) {
            p->velocity.y = 128;
        }
        if (p->velocity.x > 128) {
            p->velocity.x = 128;
        }
        p->top = Math_Wrap16(p->top + p->velocity.y);
        p->left = Math_Wrap16(p->left + p->velocity.x);
        if (p->decay_frame == 0
            && Shots_HitTestPellet(
                context->shots, Math_SubpixelToPixel(p->left),
                Math_SubpixelToPixel(p->top), context->shots_output)) {
            M_Remove(pellets, p);
            M_Restore(context->video, p, false);
        }
        if (p->top >= 400 * 16 || p->top < 64 * 16 || p->left >= 632 * 16
            || p->left <= 0) {
            M_Remove(pellets, p);
            M_Restore(context->video, p, false);
        }
    }
    Shots_UpdateAndRender(
        context->shots, context->orb, context->rng, context->shots_output);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (!p->moving) {
            continue;
        }
        if (!pellets->interlace || pellets->interlace_field == ((i & 1) != 0)) {
            if (M_Visible(p, context)) {
                p->not_rendered = false;
                if (p->decay_frame < 20) {
                    PelletAssets_Put(
                        context->video, context->assets,
                        Math_SubpixelToPixel(p->left),
                        Math_SubpixelToPixel(p->top),
                        p->decay_frame == 0        ? 0
                            : p->decay_frame <= 10 ? 1
                                                   : 2);
                }
            } else {
                p->not_rendered = true;
            }
            p->previous_left = p->left;
            p->previous_top = p->top;
        }
        p->age = Math_Wrap16(p->age + 1);
        if (p->decay_frame != 0) {
            p->decay_frame = Math_Wrap16(p->decay_frame + 1);
            if (p->decay_frame > 21) {
                M_Remove(pellets, p);
            }
        } else if (
            !context->player->invincible
            && Math_SubpixelToPixel(p->left) >= context->player->left + 4
            && Math_SubpixelToPixel(p->left) <= context->player->left + 20
            && Math_SubpixelToPixel(p->top)
                >= 368 + context->player->sliding * 8
            && Math_SubpixelToPixel(p->top) < 392) {
            context->player->is_hit = true;
            if (!p->not_rendered) {
                M_Restore(context->video, p, false);
            }
            M_Remove(pellets, p);
        }
    }
}

void Pellets_Decay(PELLETS *const pellets, PELLET_SHARED *const shared)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (p->moving && p->decay_frame == 0) {
            p->velocity.x = (int16_t)(p->velocity.x * 2 / 3);
            p->velocity.y = (int16_t)(p->velocity.y * 2 / 3);
            p->decay_frame = 1;
            shared->destroy_score_delta =
                (uint16_t)(shared->destroy_score_delta + 10);
        }
    }
}

void Pellets_ResetMoving(PELLETS *const pellets)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (p->moving) {
            p->moving = false;
            p->decay_frame = 0;
            p->cloud_frame = 0;
        }
    }
    pellets->alive_count_excluding_clouds_after_reset = 0;
}

void Pellets_EraseAndResetMoving(PELLETS *const pellets, VIDEO *const video)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &pellets->slots[i];
        if (p->moving) {
            if (!p->not_rendered) {
                M_Restore(video, p, true);
            }
            p->decay_frame = 0;
            p->moving = false;
            p->cloud_frame = 0;
        }
    }
    pellets->alive_count_excluding_clouds_after_reset = 0;
}

void Pellets_SpeedLower(
    PELLET_SHARED *const shared, const int16_t max, const int16_t delta)
{
    shared->speed =
        shared->speed > max ? max : Math_Wrap16(shared->speed + delta);
    if (shared->speed < -15) {
        shared->speed = -15;
    }
}

void Pellets_SpeedRaise(PELLET_SHARED *const shared, const int16_t delta)
{
    shared->speed = Math_Wrap16(shared->speed + delta);
    if (shared->speed > 20) {
        shared->speed = 20;
    }
}
