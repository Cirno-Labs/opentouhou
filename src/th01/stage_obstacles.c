#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/stage.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

static void M_Put(
    const PTN *const sprites, const STAGE_OBSTACLE *const obstacle,
    const int32_t image)
{
    Sprite_PtnPut(sprites, image, -1, obstacle->left, obstacle->top);
}

static void M_PutBoth(
    const PTN *const sprites, const STAGE_OBSTACLE *const obstacle,
    const int32_t image)
{
    Video_SelectDrawingPage(1);
    M_Put(sprites, obstacle, image);
    Video_SelectDrawingPage(0);
    M_Put(sprites, obstacle, image);
}

static void M_Turret(
    const PTN *const sprites, STAGE_OBSTACLE *const obstacle, const bool reset)
{
    if ((SETTINGS_DIFFICULTY)g_Session.settings.difficulty
        != SETTINGS_DIFFICULTY_EASY) {
        obstacle->frame = Math16_Wrap(obstacle->frame + 1);
    }
    const int16_t interval =
        obstacle->type < STAGE_DATA_TURRET_QUICK_1_AIMED ? 200 : 100;
    if (obstacle->frame >= interval) {
        if (obstacle->turret_flag == 0) {
            M_PutBoth(sprites, obstacle, 24);
            obstacle->turret_flag = 1;
        } else if (obstacle->turret_flag == 1) {
            if (obstacle->frame % 10 == 9) {
                obstacle->turret_flag = 2;
            }
        } else if (obstacle->turret_flag == 2) {
            static const STAGE_PELLET_GROUP groups[] = {
                STAGE_PELLET_1_AIMED,
                STAGE_PELLET_1_RANDOM_NARROW_AIMED,
                STAGE_PELLET_2_SPREAD_WIDE_AIMED,
                STAGE_PELLET_3_SPREAD_WIDE_AIMED,
                STAGE_PELLET_4_SPREAD_WIDE_AIMED,
                STAGE_PELLET_5_SPREAD_WIDE_AIMED,
            };
            Pellets_AddGroup(
                g_Session.settings.difficulty, obstacle->left + 12,
                obstacle->top + 12,
                (PELLET_GROUP)(groups[(obstacle->type - 2) % 6]), 56);
            M_Put(sprites, obstacle, 24);
            obstacle->turret_flag = 3;
        } else {
            obstacle->turret_flag++;
            if (obstacle->turret_flag >= 10) {
                M_PutBoth(sprites, obstacle, 23);
                obstacle->turret_flag = 0;
                obstacle->frame = 0;
            }
        }
    }
    if (reset) {
        obstacle->frame = 0;
        for (int32_t i = 0; i < g_StageObjects->obstacle_count; i++) {
            g_StageObjects->obstacles[i].turret_flag = 0;
        }
    }
}

static void M_Portal(const PTN *const sprites, const int32_t slot)
{
    const STAGE_OBJECTS *const objects = g_StageObjects;
    STAGE_OBSTACLE *const obstacle = &objects->obstacles[slot];
    const ORB *const orb = g_Orb;
    if (objects->portal_active && objects->entered_portal != slot) {
        return;
    }
    if (obstacle->frame != 0) {
        obstacle->frame = Math16_Wrap(obstacle->frame + 1);
    }
    if (obstacle->frame == 0) {
        Orb_EnterPortal();
        obstacle->frame = 1;
        StageObjects_EnterPortal(slot);
        Sprite_Restore16(orb->previous_left, orb->previous_top, 32, 32);
        M_Put(sprites, obstacle, 30);
    } else if (obstacle->frame == 10) {
        Sprite_Restore16(obstacle->left, obstacle->top, 32, 32);
        M_Put(sprites, obstacle, 31);
    } else if (obstacle->frame == 20) {
        Sprite_Restore16(obstacle->left, obstacle->top, 32, 32);
        M_Put(sprites, obstacle, 29);
        int32_t destination = 0;
        int32_t completed_loops = 0;
        for (;;) {
            if (objects->obstacles[destination].type == STAGE_DATA_PORTAL
                && MasterRng_Next(&g_Rng) % 4 == 0 && destination != slot) {
                break;
            }
            destination++;
            if (completed_loops > 15) {
                destination = slot;
                break;
            } else if (destination >= objects->obstacle_count) {
                destination = 0;
                completed_loops++;
            }
        }
        StageObjects_SetPortalDestination(
            objects->obstacles[destination].left,
            objects->obstacles[destination].top);
        Sprite_Restore16(
            objects->destination_left, objects->destination_top, 32, 32);
        Sprite_PtnPut(
            sprites, 31, -1, objects->destination_left,
            objects->destination_top);
    } else if (obstacle->frame == 30 || obstacle->frame == 40) {
        Sprite_Restore16(
            objects->destination_left, objects->destination_top, 32, 32);
        Sprite_PtnPut(
            sprites, obstacle->frame == 30 ? 30 : 29, -1,
            objects->destination_left, objects->destination_top);
        if (obstacle->frame == 40) {
            Orb_SetVelocityX(MasterRng_Next(&g_Rng) % 5);
            const int16_t force = (int16_t)(MasterRng_Next(&g_Rng) % 19) - 9;
            Orb_NewForce(ORB_FORCE_IMMEDIATE, force);
            Orb_Teleport(objects->destination_left, objects->destination_top);
        }
    } else if (obstacle->frame == 60) {
        obstacle->frame = 0;
        StageObjects_ClosePortal();
    }
}

static void M_ResetPortals(const PTN *const sprites)
{
    const STAGE_OBJECTS *const objects = g_StageObjects;
    StageObjects_ResetPortals();
    if (g_Orb->in_portal) {
        Sprite_PtnPut(
            sprites, 29, -1, objects->destination_left,
            objects->destination_top);
        Orb_LeavePortal();
    }
}

void Stage_UpdateObstacles(const PTN *const sprites, const bool reset)
{
    const STAGE_OBJECTS *const objects = g_StageObjects;
    const ORB *const orb = g_Orb;
    if (reset) {
        StageObjects_SetVerticalBarActive(false);
    }
    for (int32_t i = 0; i < objects->obstacle_count; i++) {
        STAGE_OBSTACLE *const obstacle = &objects->obstacles[i];
        const int32_t dx = orb->left - obstacle->left;
        const int32_t dy = orb->top - obstacle->top;
        const int32_t ax = dx < 0 ? -dx : dx;
        const int32_t ay = dy < 0 ? -dy : dy;
        if (obstacle->type == STAGE_DATA_BUMPER) {
            if (ax < 24 && ay < 24 && obstacle->frame == 0) {
                obstacle->frame++;
                Orb_SetTop((int16_t)(obstacle->top + (dy <= 0 ? -24 : 24)));
                if (orb->velocity_x == ORB_X_RIGHT_4 && dx < 0) {
                    Orb_SetVelocityX(ORB_X_LEFT_4);
                } else if (orb->velocity_x == ORB_X_LEFT_4 && dx > 0) {
                    Orb_SetVelocityX(ORB_X_RIGHT_4);
                }
                Orb_NewForce(ORB_FORCE_SURFACE, 1.5);
            }
            if (obstacle->frame != 0) {
                obstacle->frame = Math16_Wrap(obstacle->frame + 1);
                if (obstacle->frame >= 8) {
                    obstacle->frame = 0;
                }
            }
            if (reset) {
                obstacle->frame = 0;
            }
            continue;
        }
        if (obstacle->type >= STAGE_DATA_TURRET_SLOW_1_AIMED
            && obstacle->type <= STAGE_DATA_TURRET_QUICK_5_SPREAD_WIDE_AIMED) {
            M_Turret(sprites, obstacle, reset);
            continue;
        }
        if (obstacle->type == STAGE_DATA_PORTAL) {
            if ((ax < 24 && ay < 24) || obstacle->frame != 0) {
                M_Portal(sprites, i);
            }
            if (reset) {
                obstacle->frame = 0;
                M_ResetPortals(sprites);
            }
            continue;
        }
        bool collision = false;
        switch (obstacle->type) {
        case STAGE_DATA_BAR_TOP:
            collision = ax < 32 && dy < 0 && dy >= -32;
            break;
        case STAGE_DATA_BAR_BOTTOM:
            collision = ax < 32 && dy <= 32 && dy > 0;
            break;
        case STAGE_DATA_BAR_LEFT:
            collision = ay < 32 && -dx <= 32 && -dx >= 0;
            break;
        case STAGE_DATA_BAR_RIGHT:
            collision = ay < 32 && -dx <= 0 && -dx >= -32;
            break;
        default:
            continue;
        }
        const bool vertical = obstacle->type == STAGE_DATA_BAR_LEFT
            || obstacle->type == STAGE_DATA_BAR_RIGHT;
        if (collision && obstacle->frame == 0
            && (!vertical || !objects->vertical_bar_active)) {
            obstacle->frame++;
            if (vertical) {
                StageObjects_SetVerticalBarActive(true);
            }
            if (!vertical || orb->velocity_x == ORB_X_STILL) {
                Orb_ReflectY();
            } else {
                Orb_ReflectX();
            }
        }
        if (obstacle->frame != 0) {
            obstacle->frame = Math16_Wrap(obstacle->frame + 1);
            if (obstacle->frame >= 8) {
                obstacle->frame = 0;
                StageObjects_SetVerticalBarActive(false);
            }
            if (reset) {
                obstacle->frame = 0;
            }
        }
    }
}
