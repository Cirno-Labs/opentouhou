#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/shots.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "th01/sprite.h"

#include <string.h>

PELLETS g_Pellets;

void Pellets_Init(void)
{
    memset(&g_Pellets, 0, sizeof(g_Pellets));
}

static int16_t M_Speed(const int32_t rank, int16_t speed)
{
    speed = Math16_Wrap(
        speed
        + (rank == 0       ? 0
               : rank == 1 ? 6
               : rank == 2 ? 12
                           : 18));
    speed = Math16_Wrap(speed + Math16_Wrap(g_Pellets.speed * speed) / 40);
    return speed < 16 ? 16 : speed;
}

static void M_Init(
    PELLET *const p, const int16_t left, const int16_t top, const int16_t group)
{
    p->decay_frame = 0;
    p->left = Math16_FromPixel(left);
    p->top = Math16_FromPixel(top);
    p->cloud_left = left;
    p->cloud_top = top;
    if (g_Pellets.spawn_with_cloud) {
        p->cloud_frame = 1;
    } else {
        p->moving = true;
    }
    p->from_group = group;
    p->previous_left = -1;
    p->age = 0;
    g_Pellets.alive_count_excluding_clouds_after_reset =
        Math16_Wrap(g_Pellets.alive_count_excluding_clouds_after_reset + 1);
}

static MATH16_VECTOR M_Aim(const PELLET *const p, const int16_t speed)
{
    return Math16_Aim(
        (MATH16_VECTOR) { Math16_ToPixel(p->left), Math16_ToPixel(p->top) },
        (MATH16_VECTOR) { Math16_Wrap(g_Player->left + 8), 384 }, speed, 0);
}

static void M_Restore(const PELLET *const p, const bool current)
{
    Sprite_Restore16(
        Math16_ToPixel(current ? p->left : p->previous_left),
        Math16_ToPixel(current ? p->top : p->previous_top), 16, 8);
}

static void M_Motion(PELLET *const p)
{
    switch (p->motion_type) {
    case PELLET_GRAVITY:
        p->velocity.y = Math16_Wrap(p->velocity.y + p->speed);
        break;
    case PELLET_SLING_AIMED:
        if (p->sling_direction == 0) {
            p->sling_direction = (int16_t)((MasterRng_Next(&g_Rng) & 1) + 1);
        }
        p->velocity = Math16_Vector(p->speed, p->angle);
        if (p->sling_direction != 1) {
            const int16_t x = p->velocity.x;
            p->velocity.x = p->velocity.y;
            p->velocity.y = x;
        }
        p->angle = Math16_Wrap(p->angle + 8);
        if (p->angle > 256) {
            p->velocity = M_Aim(p, p->speed);
            p->from_group = PELLET_GROUP_SINGLE_AIMED;
            p->angle = 0;
        }
        break;
    case PELLET_BOUNCE_FROM_TOP_THEN_GRAVITY:
        if (p->left <= 0 || Math16_ToPixel(p->left) >= 632 * 16
            || p->top <= 70 * 16) {
            p->velocity.x = Math16_Wrap(-p->velocity.x);
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
        if (Math16_ToPixel(p->top) <= 70) {
            p->velocity.x = 0;
            p->velocity.y = p->speed;
            p->motion_type = PELLET_REGULAR;
            if (Math16_ToPixel(p->top) <= 64) {
                p->top = 65 * 16;
            }
        }
        break;
    case PELLET_SPIN:
        p->left = Math16_PolarX(p->spin_center.x, 256, p->angle);
        p->top = Math16_PolarY(p->spin_center.y, 256, p->angle);
        p->spin_center.x = Math16_Wrap(p->spin_center.x + p->spin_velocity.x);
        p->spin_center.y = Math16_Wrap(p->spin_center.y + p->spin_velocity.y);
        p->velocity = (MATH16_VECTOR) {};
        p->angle = Math16_Wrap(p->angle + 4);
        break;
    case PELLET_CHASE: {
        const MATH16_VECTOR target = M_Aim(p, p->speed);
        if (p->top < 320 * 16) {
            p->velocity.x = Math16_Wrap(
                p->velocity.x + (p->velocity.x < target.x)
                - (p->velocity.x > target.x));
            p->velocity.y = Math16_Wrap(
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
    const int16_t dx = Math16_Wrap(left - other_left);
    const int16_t dy = Math16_Wrap(top - other_top);
    return dx < width && dx > -8 && dy < height && dy > -8;
}

static bool M_Visible(PELLET *const p, const PELLET_ASSETS *const assets)
{
    const int16_t left = Math16_ToPixel(p->left);
    const int16_t top = Math16_ToPixel(p->top);
    for (int32_t i = 0; i < 3; i++) {
        const SHOT *const shot = &g_Shots->slots[i];
        if ((shot->moving
             && M_Overlap(left, top, shot->left, shot->top, 16, 16))
            || M_Overlap(
                Math16_Wrap(left + 8), top, shot->left, shot->top, 16, 16)) {
            return false;
        }
    }
    if (p->decay_frame != 0) {
        return true;
    }
    if (M_Overlap(left, top, g_Orb->left, g_Orb->top, 32, 32)
        || M_Overlap(
            Math16_Wrap(left + 8), top, g_Orb->left, g_Orb->top, 32, 32)) {
        p->velocity.x = (int16_t)(p->velocity.x / 2 - (p->velocity.x % 2 < 0));
        p->velocity.y = (int16_t)(p->velocity.y / 2 - (p->velocity.y % 2 < 0));
        p->decay_frame = 1;
        g_Pellets.destroy_score_delta =
            (uint16_t)(g_Pellets.destroy_score_delta + 10);
        return true;
    }
    if (g_Player->deflecting
        && M_Overlap(left, top, Math16_Wrap(g_Player->left - 8), 352, 48, 48)) {
        p->velocity = Math16_Vector(128, left <= g_Player->left + 12 ? 128 : 0);
        if (p->from_group == 0) {
            p->motion_type = PELLET_REGULAR;
        }
        return false;
    }
    return true;
}

static void M_Clouds(const PELLET_ASSETS *const assets)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (p->cloud_frame == 0) {
            continue;
        }
        p->cloud_frame = Math16_Wrap(p->cloud_frame + 1);
        const int16_t frame = p->cloud_frame;
        if (frame == 2 || frame == 5 || frame == 6 || frame == 9) {
            PelletAssets_Cloud(
                assets, p->cloud_left, p->cloud_top, frame >= 6 ? 1 : 0,
                frame == 5 || frame == 9);
        }
        if (frame == 9) {
            p->cloud_frame = 0;
            p->moving = true;
        }
    }
}

static void M_Remove(PELLET *const p)
{
    p->moving = false;
    p->decay_frame = 0;
    g_Pellets.alive_count_excluding_clouds_after_reset =
        Math16_Wrap(g_Pellets.alive_count_excluding_clouds_after_reset - 1);
}

void Pellets_AddGroup(
    const int32_t rank, const int16_t left, const int16_t top,
    const PELLET_GROUP group, int16_t speed)
{
    if (g_Pellets.alive_count_excluding_clouds_after_reset > PELLETS_COUNT
        || left >= 640 || left < -8 || top < 56 || top > 400
        || (group != 1 && (group < 12 || group > 30))) {
        return;
    }
    speed = M_Speed(rank, speed);
    int32_t group_i = 0;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (p->moving || p->cloud_frame != 0) {
            continue;
        }
        M_Init(p, left, top, (int16_t)group);
        int32_t count = 1;
        if (group == PELLET_GROUP_SINGLE) {
            p->velocity = (MATH16_VECTOR) { 0, speed };
        } else if (group == PELLET_GROUP_SINGLE_AIMED) {
            p->velocity = M_Aim(p, speed);
        } else {
            int32_t angle;
            bool aimed = group >= PELLET_GROUP_TWO_WIDE_AIMED;
            if (group == PELLET_GROUP_RANDOM_NARROW_AIMED) {
                angle = (MasterRng_Next(&g_Rng) & 15) - 7;
            } else if (group == PELLET_GROUP_RANDOM_WIDE) {
                angle = (MasterRng_Next(&g_Rng) & 63) + 32;
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
                angle += Math16_Angle(
                    Math16_Wrap(384 - top),
                    Math16_Wrap(g_Player->left + 12 - left));
            }
            p->velocity = Math16_Vector(speed, (uint8_t)angle);
        }
        if (++group_i == count) {
            return;
        }
    }
}

void Pellets_AddSingle(
    const int32_t rank, const int16_t left, const int16_t top,
    const uint8_t angle, const int16_t speed_base, const PELLET_MOTION motion,
    const int16_t speed_for_motion, const int16_t spin_center_x,
    const int16_t spin_center_y)
{
    if (g_Pellets.alive_count_excluding_clouds_after_reset > PELLETS_COUNT) {
        return;
    }
    const int16_t speed = M_Speed(rank, speed_base);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (p->moving || p->cloud_frame != 0) {
            continue;
        }
        M_Init(p, left, top, 0);
        p->motion_type = (uint8_t)motion;
        p->spin_center = (MATH16_VECTOR) { Math16_FromPixel(spin_center_x),
                                           Math16_FromPixel(spin_center_y) };
        if (motion == PELLET_SPIN) {
            p->spin_velocity = Math16_Vector(speed_for_motion, angle);
            p->angle = Math16_Angle(
                Math16_Wrap(p->top - p->spin_center.y),
                Math16_Wrap(p->left - p->spin_center.x));
        }
        p->speed = speed_for_motion;
        p->velocity = Math16_Vector(speed, angle);
        return;
    }
}

void Pellets_UpdateAndRender(
    const PELLET_ASSETS *const assets, const PTN *const stage_sprites)
{
    g_Pellets.interlace_field = !g_Pellets.interlace_field;
    M_Clouds(assets);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (!p->moving) {
            continue;
        }
        if ((!g_Pellets.interlace
             || g_Pellets.interlace_field == ((i & 1) != 0))
            && !p->not_rendered && p->previous_left != -1) {
            M_Restore(p, false);
        }
        if (p->from_group == 0 && p->motion_type != 0) {
            M_Motion(p);
        }
        if (p->velocity.y > 128) {
            p->velocity.y = 128;
        }
        if (p->velocity.x > 128) {
            p->velocity.x = 128;
        }
        p->top = Math16_Wrap(p->top + p->velocity.y);
        p->left = Math16_Wrap(p->left + p->velocity.x);
        if (p->decay_frame == 0
            && Shots_HitTestPellet(
                Math16_ToPixel(p->left), Math16_ToPixel(p->top))) {
            M_Remove(p);
            M_Restore(p, false);
        }
        if (p->top >= 400 * 16 || p->top < 64 * 16 || p->left >= 632 * 16
            || p->left <= 0) {
            M_Remove(p);
            M_Restore(p, false);
        }
    }
    Shots_UpdateAndRender(stage_sprites);
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (!p->moving) {
            continue;
        }
        if (!g_Pellets.interlace
            || g_Pellets.interlace_field == ((i & 1) != 0)) {
            if (M_Visible(p, assets)) {
                p->not_rendered = false;
                if (p->decay_frame < 20) {
                    PelletAssets_Put(
                        assets, Math16_ToPixel(p->left), Math16_ToPixel(p->top),
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
        p->age = Math16_Wrap(p->age + 1);
        if (p->decay_frame != 0) {
            p->decay_frame = Math16_Wrap(p->decay_frame + 1);
            if (p->decay_frame > 21) {
                M_Remove(p);
            }
        } else if (
            !g_Player->invincible
            && Math16_ToPixel(p->left) >= g_Player->left + 4
            && Math16_ToPixel(p->left) <= g_Player->left + 20
            && Math16_ToPixel(p->top) >= 368 + g_Player->sliding * 8
            && Math16_ToPixel(p->top) < 392) {
            Player_Hit();
            if (!p->not_rendered) {
                M_Restore(p, false);
            }
            M_Remove(p);
        }
    }
}

void Pellets_Decay(void)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (p->moving && p->decay_frame == 0) {
            p->velocity.x = (int16_t)(p->velocity.x * 2 / 3);
            p->velocity.y = (int16_t)(p->velocity.y * 2 / 3);
            p->decay_frame = 1;
            g_Pellets.destroy_score_delta =
                (uint16_t)(g_Pellets.destroy_score_delta + 10);
        }
    }
}

void Pellets_ResetMoving(void)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (p->moving) {
            p->moving = false;
            p->decay_frame = 0;
            p->cloud_frame = 0;
        }
    }
    g_Pellets.alive_count_excluding_clouds_after_reset = 0;
}

void Pellets_EraseAndResetMoving(void)
{
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        PELLET *const p = &g_Pellets.slots[i];
        if (p->moving) {
            if (!p->not_rendered) {
                M_Restore(p, true);
            }
            p->decay_frame = 0;
            p->moving = false;
            p->cloud_frame = 0;
        }
    }
    g_Pellets.alive_count_excluding_clouds_after_reset = 0;
}

void Pellets_SpeedLower(const int16_t max, const int16_t delta)
{
    g_Pellets.speed =
        g_Pellets.speed > max ? max : Math16_Wrap(g_Pellets.speed + delta);
    if (g_Pellets.speed < -15) {
        g_Pellets.speed = -15;
    }
}

void Pellets_SpeedRaise(const int16_t delta)
{
    g_Pellets.speed = Math16_Wrap(g_Pellets.speed + delta);
    if (g_Pellets.speed > 20) {
        g_Pellets.speed = 20;
    }
}

void Pellets_Interlace(void)
{
    g_Pellets.interlace = true;
}
