#include "th01/orb.h"
#include "th01/rng.h"

#include "th01/sprite.h"

#include "th01/finite.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

#include <string.h>

ORB g_Orb;

void Orb_Init(void)
{
    memset(&g_Orb, 0, sizeof(g_Orb));
}

static bool M_Left(const ORB_VELOCITY_X velocity)
{
    return velocity == ORB_X_LEFT_4 || velocity == ORB_X_LEFT_8;
}

static bool M_Right(const ORB_VELOCITY_X velocity)
{
    return velocity == ORB_X_RIGHT_4 || velocity == ORB_X_RIGHT_8;
}

void Orb_ResetFlight(void)
{
    g_Orb.left = g_Orb.previous_left = ORB_BOUNDS_MAX_X - 8;
    g_Orb.top = g_Orb.previous_top = ORB_BOUNDS_MAX_Y - 88;
    g_Orb.force = -8.0;
    g_Orb.force_frame = 0;
    g_Orb.velocity_x = ORB_X_LEFT_4;
}

void Orb_AdvanceForceFrame(void)
{
    g_Orb.force_frame = Math16_Wrap(g_Orb.force_frame + 1);
}

int16_t Orb_UpdateVelocityY(void)
{
    const FINITE gravity = Finite_Add(
        Finite_FromDouble(g_Orb.force),
        Finite_FromDouble(g_Orb.force_frame / 5));
    g_Orb.velocity_y = Finite_ToDouble(gravity);
    if (g_Orb.velocity_y > 16.0) {
        g_Orb.velocity_y = 16.0;
    } else if (g_Orb.velocity_y < -16.0) {
        g_Orb.velocity_y = -16.0;
    }
    return Finite_ToPixel(gravity);
}

void Orb_NewForce(const ORB_FORCE mode, const double immediate)
{
    const FINITE velocity = Finite_FromDouble(g_Orb.velocity_y);
    if (mode == ORB_FORCE_SURFACE) {
        g_Orb.force = Finite_ToDouble(
            Finite_Multiply(Finite_Negate(velocity), Finite_FromDouble(0.78)));
        if (g_Orb.velocity_x == ORB_X_STILL && g_Orb.force_frame < 17) {
            if (MasterRng_Next(&g_Rng) % 50 == 0) {
                g_Orb.velocity_x = ORB_X_LEFT_4;
            }
            if (MasterRng_Next(&g_Rng) % 50 == 1) {
                g_Orb.velocity_x = ORB_X_RIGHT_4;
            }
        }
    }
    if (mode == ORB_FORCE_TOP) {
        g_Orb.force = Finite_ToDouble(Finite_Add(
            Finite_Negate(velocity),
            Finite_Negate(Finite_FromDouble(g_Orb.force_frame / 4))));
    }
    if (mode == ORB_FORCE_SHOT) {
        g_Orb.force = Finite_ToDouble(
            Finite_Add(Finite_FromDouble(-10.0), Finite_Half(velocity)));
    }
    if (mode == ORB_FORCE_IMMEDIATE) {
        g_Orb.force = immediate;
    }
    g_Orb.force_frame = 0;
}

void Orb_ReflectX(void)
{
    switch (g_Orb.velocity_x) {
    case ORB_X_LEFT_4:
        g_Orb.velocity_x = ORB_X_RIGHT_4;
        break;
    case ORB_X_RIGHT_4:
        g_Orb.velocity_x = ORB_X_LEFT_4;
        break;
    case ORB_X_LEFT_8:
        g_Orb.velocity_x = ORB_X_RIGHT_8;
        break;
    case ORB_X_RIGHT_8:
        g_Orb.velocity_x = ORB_X_LEFT_8;
        break;
    default:
        g_Orb.velocity_x = ORB_X_STILL;
        break;
    }
}

void Orb_ReflectY(void)
{
    Orb_NewForce(ORB_FORCE_IMMEDIATE, -g_Orb.velocity_y);
}

void Orb_MoveX(const ORB_VELOCITY_X requested)
{
    switch (requested) {
    case ORB_X_LEFT_4:
        g_Orb.left = Math16_Wrap(g_Orb.left - 4);
        break;
    case ORB_X_RIGHT_4:
        g_Orb.left = Math16_Wrap(g_Orb.left + 4);
        break;
    case ORB_X_LEFT_8:
        g_Orb.left = Math16_Wrap(g_Orb.left - 8);
        break;
    case ORB_X_RIGHT_8:
        g_Orb.left = Math16_Wrap(g_Orb.left + 8);
        break;
    default:
        break;
    }
    if (g_Orb.left <= ORB_BOUNDS_MIN_X && M_Left(g_Orb.velocity_x)) {
        Orb_ReflectX();
    }
    if (g_Orb.left >= ORB_BOUNDS_MAX_X && M_Right(g_Orb.velocity_x)) {
        Orb_ReflectX();
    }
}

int16_t Orb_Image(void)
{
    return (int16_t)(g_Orb.rotation_frame / ORB_FRAMES_PER_IMAGE);
}

void Orb_AdvanceMotion(int16_t *const card_combo)
{
    if (g_Orb.in_portal) {
        return;
    }
    Orb_MoveX(g_Orb.velocity_x);
    g_Orb.top = Math16_Wrap(g_Orb.top + Orb_UpdateVelocityY());
    g_Orb.frames_outside_portal = Math16_Wrap(g_Orb.frames_outside_portal + 1);
    if (M_Left(g_Orb.velocity_x)) {
        g_Orb.rotation_frame = Math16_Wrap(g_Orb.rotation_frame + 1);
    }
    if (M_Right(g_Orb.velocity_x)) {
        g_Orb.rotation_frame = Math16_Wrap(g_Orb.rotation_frame - 1);
    }
    if (g_Orb.rotation_frame >= ORB_IMAGE_COUNT * ORB_FRAMES_PER_IMAGE - 2) {
        g_Orb.rotation_frame = 0;
    }
    if (g_Orb.rotation_frame < 0) {
        g_Orb.rotation_frame = (ORB_IMAGE_COUNT - 1) * ORB_FRAMES_PER_IMAGE;
    }
    if (g_Orb.top > ORB_BOUNDS_MAX_Y) {
        Orb_NewForce(ORB_FORCE_SURFACE, 0.78);
        g_Orb.top = ORB_BOUNDS_MAX_Y;
        *card_combo = 0;
    }
    if (g_Orb.top < ORB_BOUNDS_MIN_Y) {
        Orb_NewForce(ORB_FORCE_TOP, 0.0);
        g_Orb.top = ORB_BOUNDS_MIN_Y;
    }
}

bool Orb_PlayerHitTest(
    const ORB_PLAYER *const player, const int16_t repel_friction)
{
    const int32_t delta_x = player->left - g_Orb.left;
    const int32_t delta_y = player->top - g_Orb.top;
    const int32_t abs_x = delta_x < 0 ? -delta_x : delta_x;
    const int32_t abs_y = delta_y < 0 ? -delta_y : delta_y;
    if (repel_friction == ORB_REPEL_NONE) {
        return !player->invincible && !player->invincible_against_orb
            && abs_x < 24 && abs_y < 16;
    }
    if (abs_x >= 40 || abs_y >= 32) {
        return false;
    }
    if (repel_friction < ORB_REPEL_3_X_UNCHANGED) {
        if (delta_x > 0) {
            g_Orb.velocity_x = ORB_X_LEFT_4;
        } else if (delta_x == 0) {
            g_Orb.velocity_x = ORB_X_STILL;
            if (MasterRng_Next(&g_Rng) % 8 == 0) {
                g_Orb.velocity_x = ORB_X_LEFT_4;
            }
            if (MasterRng_Next(&g_Rng) % 8 == 4) {
                g_Orb.velocity_x = ORB_X_LEFT_4;
            }
        } else {
            g_Orb.velocity_x = ORB_X_RIGHT_4;
        }
        Orb_NewForce(ORB_FORCE_IMMEDIATE, repel_friction / 2 - 13);
    } else {
        switch (repel_friction) {
        case ORB_REPEL_3_X_LEFT_4:
            g_Orb.velocity_x = ORB_X_LEFT_4;
            break;
        case ORB_REPEL_3_X_RIGHT_4:
            g_Orb.velocity_x = ORB_X_RIGHT_4;
            break;
        case ORB_REPEL_3_X_RIGHT_8:
            g_Orb.velocity_x = ORB_X_RIGHT_8;
            break;
        case ORB_REPEL_3_X_LEFT_8:
            g_Orb.velocity_x = ORB_X_LEFT_8;
            break;
        default:
            break;
        }
        Orb_NewForce(ORB_FORCE_IMMEDIATE, -10.0);
    }
    return false;
}

ORB_COMPOSITE Orb_BeginComposite(
    const int16_t stage_id, int16_t *const card_combo)
{
    Orb_AdvanceMotion(card_combo);
    return (ORB_COMPOSITE) { .boss = stage_id % 5 == 4 };
}

ORB_ACTION Orb_NextAction(ORB_COMPOSITE *const composite, const bool player_hit)
{
    for (;;) {
        ORB_ACTION_KIND kind;
        switch (composite->phase) {
        case 0:
            kind = ORB_ACTION_DELAY_ONE;
            break;
        case 1:
            kind = ORB_ACTION_PELLETS;
            break;
        case 2:
            kind = composite->boss ? ORB_ACTION_DONE : ORB_ACTION_CARDS_HITTEST;
            break;
        case 3:
            kind = composite->boss ? ORB_ACTION_DONE : ORB_ACTION_OBSTACLES;
            break;
        case 4:
            kind =
                g_Orb.in_portal ? ORB_ACTION_DONE : ORB_ACTION_PLAYER_HITTEST;
            break;
        case 5:
            kind = g_Orb.in_portal ? ORB_ACTION_DONE : ORB_ACTION_ERASE;
            break;
        case 6:
            kind = composite->boss ? ORB_ACTION_DONE : ORB_ACTION_CARDS_RENDER;
            break;
        case 7:
            kind = g_Orb.in_portal || player_hit ? ORB_ACTION_DONE
                                                 : ORB_ACTION_PUT;
            break;
        default:
            g_Orb.previous_left = g_Orb.left;
            g_Orb.previous_top = g_Orb.top;
            return (ORB_ACTION) { .kind = ORB_ACTION_DONE };
        }
        composite->phase++;
        if (kind != ORB_ACTION_DONE) {
            return (ORB_ACTION) {
                kind,
                kind == ORB_ACTION_ERASE ? g_Orb.previous_left : g_Orb.left,
                kind == ORB_ACTION_ERASE ? g_Orb.previous_top : g_Orb.top,
                Orb_Image()
            };
        }
    }
}

void Orb_RenderAction(const PTN *const sprites, const ORB_ACTION action)
{
    if (action.kind != ORB_ACTION_ERASE && action.kind != ORB_ACTION_PUT) {
        return;
    }
    if (action.kind == ORB_ACTION_ERASE) {
        Sprite_PtnErase(
            sprites, ORB_FIRST_IMAGE + action.image, -1, action.left,
            action.top);
    } else {
        Sprite_PtnPut(
            sprites, ORB_FIRST_IMAGE + action.image, -1, action.left,
            action.top);
    }
}
