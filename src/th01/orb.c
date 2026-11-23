#include "th01/orb.h"

#include "th01/sprite.h"

#include "th01/finite.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

static bool M_Left(const ORB_VELOCITY_X velocity)
{
    return velocity == ORB_X_LEFT_4 || velocity == ORB_X_LEFT_8;
}

static bool M_Right(const ORB_VELOCITY_X velocity)
{
    return velocity == ORB_X_RIGHT_4 || velocity == ORB_X_RIGHT_8;
}

void Orb_ResetFlight(ORB *const orb)
{
    orb->left = orb->previous_left = ORB_BOUNDS_MAX_X - 8;
    orb->top = orb->previous_top = ORB_BOUNDS_MAX_Y - 88;
    orb->force = -8.0;
    orb->force_frame = 0;
    orb->velocity_x = ORB_X_LEFT_4;
}

void Orb_AdvanceForceFrame(ORB *const orb)
{
    orb->force_frame = Math16_Wrap(orb->force_frame + 1);
}

int16_t Orb_UpdateVelocityY(ORB *const orb)
{
    const FINITE gravity = Finite_Add(
        Finite_FromDouble(orb->force), Finite_FromDouble(orb->force_frame / 5));
    orb->velocity_y = Finite_ToDouble(gravity);
    if (orb->velocity_y > 16.0) {
        orb->velocity_y = 16.0;
    } else if (orb->velocity_y < -16.0) {
        orb->velocity_y = -16.0;
    }
    return Finite_ToPixel(gravity);
}

void Orb_NewForce(
    ORB *const orb, const ORB_FORCE mode, const double immediate,
    uint32_t *const rng)
{
    const FINITE velocity = Finite_FromDouble(orb->velocity_y);
    if (mode == ORB_FORCE_SURFACE) {
        orb->force = Finite_ToDouble(
            Finite_Multiply(Finite_Negate(velocity), Finite_FromDouble(0.78)));
        if (orb->velocity_x == ORB_X_STILL && orb->force_frame < 17) {
            if (MasterRng_Next(rng) % 50 == 0) {
                orb->velocity_x = ORB_X_LEFT_4;
            }
            if (MasterRng_Next(rng) % 50 == 1) {
                orb->velocity_x = ORB_X_RIGHT_4;
            }
        }
    }
    if (mode == ORB_FORCE_TOP) {
        orb->force = Finite_ToDouble(Finite_Add(
            Finite_Negate(velocity),
            Finite_Negate(Finite_FromDouble(orb->force_frame / 4))));
    }
    if (mode == ORB_FORCE_SHOT) {
        orb->force = Finite_ToDouble(
            Finite_Add(Finite_FromDouble(-10.0), Finite_Half(velocity)));
    }
    if (mode == ORB_FORCE_IMMEDIATE) {
        orb->force = immediate;
    }
    orb->force_frame = 0;
}

void Orb_ReflectX(ORB *const orb)
{
    switch (orb->velocity_x) {
    case ORB_X_LEFT_4:
        orb->velocity_x = ORB_X_RIGHT_4;
        break;
    case ORB_X_RIGHT_4:
        orb->velocity_x = ORB_X_LEFT_4;
        break;
    case ORB_X_LEFT_8:
        orb->velocity_x = ORB_X_RIGHT_8;
        break;
    case ORB_X_RIGHT_8:
        orb->velocity_x = ORB_X_LEFT_8;
        break;
    default:
        orb->velocity_x = ORB_X_STILL;
        break;
    }
}

void Orb_ReflectY(ORB *const orb, uint32_t *const rng)
{
    Orb_NewForce(orb, ORB_FORCE_IMMEDIATE, -orb->velocity_y, rng);
}

void Orb_MoveX(ORB *const orb, const ORB_VELOCITY_X requested)
{
    switch (requested) {
    case ORB_X_LEFT_4:
        orb->left = Math16_Wrap(orb->left - 4);
        break;
    case ORB_X_RIGHT_4:
        orb->left = Math16_Wrap(orb->left + 4);
        break;
    case ORB_X_LEFT_8:
        orb->left = Math16_Wrap(orb->left - 8);
        break;
    case ORB_X_RIGHT_8:
        orb->left = Math16_Wrap(orb->left + 8);
        break;
    default:
        break;
    }
    if (orb->left <= ORB_BOUNDS_MIN_X && M_Left(orb->velocity_x)) {
        Orb_ReflectX(orb);
    }
    if (orb->left >= ORB_BOUNDS_MAX_X && M_Right(orb->velocity_x)) {
        Orb_ReflectX(orb);
    }
}

int16_t Orb_Image(const ORB *const orb)
{
    return (int16_t)(orb->rotation_frame / ORB_FRAMES_PER_IMAGE);
}

void Orb_AdvanceMotion(
    ORB *const orb, uint32_t *const rng, int16_t *const card_combo)
{
    if (orb->in_portal) {
        return;
    }
    Orb_MoveX(orb, orb->velocity_x);
    orb->top = Math16_Wrap(orb->top + Orb_UpdateVelocityY(orb));
    orb->frames_outside_portal = Math16_Wrap(orb->frames_outside_portal + 1);
    if (M_Left(orb->velocity_x)) {
        orb->rotation_frame = Math16_Wrap(orb->rotation_frame + 1);
    }
    if (M_Right(orb->velocity_x)) {
        orb->rotation_frame = Math16_Wrap(orb->rotation_frame - 1);
    }
    if (orb->rotation_frame >= ORB_IMAGE_COUNT * ORB_FRAMES_PER_IMAGE - 2) {
        orb->rotation_frame = 0;
    }
    if (orb->rotation_frame < 0) {
        orb->rotation_frame = (ORB_IMAGE_COUNT - 1) * ORB_FRAMES_PER_IMAGE;
    }
    if (orb->top > ORB_BOUNDS_MAX_Y) {
        Orb_NewForce(orb, ORB_FORCE_SURFACE, 0.78, rng);
        orb->top = ORB_BOUNDS_MAX_Y;
        *card_combo = 0;
    }
    if (orb->top < ORB_BOUNDS_MIN_Y) {
        Orb_NewForce(orb, ORB_FORCE_TOP, 0.0, rng);
        orb->top = ORB_BOUNDS_MIN_Y;
    }
}

bool Orb_PlayerHitTest(
    ORB *const orb, const ORB_PLAYER *const player,
    const int16_t repel_friction, uint32_t *const rng)
{
    const int32_t delta_x = player->left - orb->left;
    const int32_t delta_y = player->top - orb->top;
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
            orb->velocity_x = ORB_X_LEFT_4;
        } else if (delta_x == 0) {
            orb->velocity_x = ORB_X_STILL;
            if (MasterRng_Next(rng) % 8 == 0) {
                orb->velocity_x = ORB_X_LEFT_4;
            }
            if (MasterRng_Next(rng) % 8 == 4) {
                orb->velocity_x = ORB_X_LEFT_4;
            }
        } else {
            orb->velocity_x = ORB_X_RIGHT_4;
        }
        Orb_NewForce(orb, ORB_FORCE_IMMEDIATE, repel_friction / 2 - 13, rng);
    } else {
        switch (repel_friction) {
        case ORB_REPEL_3_X_LEFT_4:
            orb->velocity_x = ORB_X_LEFT_4;
            break;
        case ORB_REPEL_3_X_RIGHT_4:
            orb->velocity_x = ORB_X_RIGHT_4;
            break;
        case ORB_REPEL_3_X_RIGHT_8:
            orb->velocity_x = ORB_X_RIGHT_8;
            break;
        case ORB_REPEL_3_X_LEFT_8:
            orb->velocity_x = ORB_X_LEFT_8;
            break;
        default:
            break;
        }
        Orb_NewForce(orb, ORB_FORCE_IMMEDIATE, -10.0, rng);
    }
    return false;
}

ORB_COMPOSITE Orb_BeginComposite(
    ORB *const orb, const int16_t stage_id, uint32_t *const rng,
    int16_t *const card_combo)
{
    Orb_AdvanceMotion(orb, rng, card_combo);
    return (ORB_COMPOSITE) { .boss = stage_id % 5 == 4 };
}

ORB_ACTION Orb_NextAction(
    ORB *const orb, ORB_COMPOSITE *const composite, const bool player_hit)
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
            kind = orb->in_portal ? ORB_ACTION_DONE : ORB_ACTION_PLAYER_HITTEST;
            break;
        case 5:
            kind = orb->in_portal ? ORB_ACTION_DONE : ORB_ACTION_ERASE;
            break;
        case 6:
            kind = composite->boss ? ORB_ACTION_DONE : ORB_ACTION_CARDS_RENDER;
            break;
        case 7:
            kind =
                orb->in_portal || player_hit ? ORB_ACTION_DONE : ORB_ACTION_PUT;
            break;
        default:
            orb->previous_left = orb->left;
            orb->previous_top = orb->top;
            return (ORB_ACTION) { .kind = ORB_ACTION_DONE };
        }
        composite->phase++;
        if (kind != ORB_ACTION_DONE) {
            return (ORB_ACTION) {
                kind, kind == ORB_ACTION_ERASE ? orb->previous_left : orb->left,
                kind == ORB_ACTION_ERASE ? orb->previous_top : orb->top,
                Orb_Image(orb)
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
