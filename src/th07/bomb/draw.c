#include "th07/bomb/draw.h"

#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/stage.h"

#include "core/base/utils.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"

#define M_ORB_COUNT 8
#define M_ORB_SPRITES 4
#define M_BARRIER_COUNT 4
#define M_BOUNDARY_SPRITES 3
#define M_STAR_COUNT 8
#define M_MILKY_WAY_STARS 24
#define M_LASER_COUNT 3
#define M_SPARK_SPRITES 4
#define M_KNIFE_COUNT 0x60
#define M_SQUARE_COUNT 4
#define M_PRIVATE_SQUARE_COUNT 2
#define M_TINT_FRAMES 60

static void M_Tint(void)
{
    const PLAYER *const player = &g_Player;
    const int32_t frame = player->bomb.timer.current;
    int32_t shade;
    if (frame < M_TINT_FRAMES) {
        shade = 0x80 - frame * 80 / 60;
    } else if (frame >= player->bomb.duration - M_TINT_FRAMES) {
        shade = 0x80 - (player->bomb.duration - frame) * 80 / 60;
    } else {
        shade = 0x30;
    }
    Anm_ResetColorModulation();
    Stage_SetTint(0x80u << 24 | (uint32_t)shade * 0x010101u);
    g_Stage.tint.keep = true;
}

// Moves vm to pos in the play area, at depth 0.
static void M_Place(ANM_VM *const vm, const VEC3 pos)
{
    vm->pos = pos;
    vm->pos.x = g_GameManager.play_area.x + vm->pos.x;
    vm->pos.y = g_GameManager.play_area.y + vm->pos.y;
    vm->pos.z = 0.0f;
}

static void M_Turn(ANM_VM *const vm, const float angle)
{
    vm->rotation.z = angle;
    vm->rotation_changed = true;
}

static void M_DrawOrbs(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_ORB_COUNT; i++) {
        PLAYER_BOMB_PART *const orb = &g_Player.bomb.parts[i];
        if (orb->state == 0) {
            continue;
        }
        for (int32_t k = 0; k < M_ORB_SPRITES; k++) {
            ANM_VM *const vm = &orb->vms[k];
            M_Place(vm, Vec3_Add(orb->pos, vm->pos_offset));
            Anm_DrawUnrotated(vm, draw);
        }
    }
}

static void M_ReimuA(const ANM_DRAW *const draw)
{
    M_DrawOrbs(draw);
}

static void M_ReimuAFocused(const ANM_DRAW *const draw)
{
    M_DrawOrbs(draw);
}

static void M_ReimuB(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_BARRIER_COUNT; i++) {
        PLAYER_BOMB_PART *const barrier = &g_Player.bomb.parts[i];
        ANM_VM *const vm = &barrier->vms[0];
        M_Place(vm, Vec3_Add(barrier->pos, vm->pos_offset));
        Anm_Draw(vm, draw);
    }
}

static void M_ReimuBFocused(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_BOUNDARY_SPRITES; i++) {
        ANM_VM *const vm = &g_Player.bomb.parts[0].vms[i];
        M_Place(vm, Vec3_Add(g_Player.bomb.start_pos, vm->pos_offset));
        Anm_Draw(vm, draw);
    }
}

// Draws each star three times, the trail behind it; the third draw moves
// twice before it draws, as th06's does.
static void M_MarisaA(const ANM_DRAW *const draw)
{
    ANM_DRAW trail = *draw;
    for (int32_t i = 0; i < M_STAR_COUNT; i++) {
        PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[i];
        ANM_VM *const vm = &star->vms[0];
        M_Place(vm, star->pos);
        vm->scale_x = 3.2f;
        vm->scale_y = 3.2f;
        trail.part = 0;
        Anm_Draw(vm, &trail);
        vm->pos = Vec3_Sub(vm->pos, Vec3_Scale(star->velocity, 6.0f));
        vm->pos.x += -32.0f;
        vm->pos.y += -32.0f;
        vm->pos.z = 0.0f;
        vm->scale_x = 2.2f;
        vm->scale_y = 2.2f;
        trail.part = 1;
        Anm_Draw(vm, &trail);
        vm->pos = Vec3_Sub(vm->pos, Vec3_Scale(star->velocity, 2.0f));
        vm->pos.x += 64.0f;
        vm->pos.y += 64.0f;
        vm->pos.z = 0.0f;
        vm->pos = Vec3_Sub(vm->pos, Vec3_Scale(star->velocity, 2.0f));
        vm->pos.x += -32.0f;
        vm->pos.y += -32.0f;
        vm->pos.z = 0.0f;
        vm->scale_x = 1.0f;
        vm->scale_y = 1.0f;
        trail.part = 2;
        Anm_Draw(vm, &trail);
    }
}

static void M_MarisaAFocused(const ANM_DRAW *const draw)
{
    static const struct {
        int32_t trail; // -1 for the star itself
        float z, scale;
    } passes[] = {
        { -1, 0.3f, 3.2f },
        { 3, 0.32f, 2.2f },
        { 7, 0.34f, 1.3f },
    };
    ANM_DRAW trail = *draw;
    for (int32_t i = 0; i < M_MILKY_WAY_STARS; i++) {
        PLAYER_BOMB_PART *const star = &g_Player.bomb.parts[i];
        if (star->state == 0) {
            continue;
        }
        ANM_VM *const vm = &star->vms[0];
        for (int32_t k = 0; k < (int32_t)ARRAY_SIZE(passes); k++) {
            M_Place(
                vm,
                passes[k].trail < 0 ? star->pos : star->trail[passes[k].trail]);
            vm->pos.z = passes[k].z;
            vm->scale_x = passes[k].scale;
            vm->scale_y = passes[k].scale;
            trail.part = (uint32_t)k;
            Anm_Draw(vm, &trail);
        }
    }
}

// Places vm half its sprite's height along angle from the player, turned
// along it.
static void M_PlaceBeam(ANM_VM *const vm, const float angle)
{
    vm->pos = g_Player.pos;
    vm->pos.x += X87_Cos(angle) * vm->sprite_data->height * vm->scale_y / 2.0f;
    vm->pos.y += X87_Sin(angle) * vm->sprite_data->height * vm->scale_y / 2.0f;
    M_Turn(vm, Angle_AddNormalize(angle, MATH_HALF_PI));
    M_Place(vm, vm->pos);
}

static void M_MarisaB(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_LASER_COUNT; i++) {
        PLAYER_BOMB_PART *const laser = &g_Player.bomb.parts[i];
        M_PlaceBeam(&laser->vms[0], laser->laser_angle);
        Anm_Draw(&laser->vms[0], draw);
    }
}

static void M_MarisaBFocused(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_SPARK_SPRITES; i++) {
        ANM_VM *const vm = &g_Player.bomb.parts[0].vms[i];
        const float angle =
            0.62831855f * (float)i / 3.0f - MATH_PI + 1.2566371f;
        M_PlaceBeam(vm, angle);
        Anm_Draw(vm, draw);
    }
}

static void M_SakuyaA(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
        PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[i];
        if (knife->state == 0) {
            continue;
        }
        ANM_VM *const vm = &knife->vms[0];
        M_Turn(vm, Angle_AddNormalize(knife->angle, MATH_HALF_PI));
        // Leaves out the play area's offset, unlike the focused bomb.
        vm->pos = knife->pos;
        vm->pos.z = 0.0f;
        Anm_Draw(vm, draw);
    }
}

static void M_SakuyaAFocused(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_KNIFE_COUNT; i++) {
        PLAYER_BOMB_PART *const knife = &g_Player.bomb.parts[i];
        if (knife->state == 0) {
            continue;
        }
        ANM_VM *const vm = &knife->vms[0];
        M_Turn(vm, Angle_AddNormalize(knife->angle, MATH_HALF_PI));
        M_Place(vm, knife->pos);
        Anm_Draw(vm, draw);
    }
}

// Draws each square at its stored position, moved into the play area and
// back.
static void M_SakuyaB(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < M_SQUARE_COUNT; i++) {
        PLAYER_BOMB_PART *const square = &g_Player.bomb.parts[i];
        if (square->state == 0) {
            continue;
        }
        ANM_VM *const vm = &square->vms[0];
        M_Place(vm, vm->pos);
        Anm_Draw(vm, draw);
        vm->pos.x -= g_GameManager.play_area.x;
        vm->pos.y -= g_GameManager.play_area.y;
    }
}

// Draws each square, then every fourth position of its trail, fading out.
static void M_SakuyaBFocused(const ANM_DRAW *const draw)
{
    ANM_DRAW trail = *draw;
    for (int32_t i = 0; i < M_PRIVATE_SQUARE_COUNT; i++) {
        PLAYER_BOMB_PART *const square = &g_Player.bomb.parts[i];
        if (square->state == 0) {
            continue;
        }
        ANM_VM *const vm = &square->vms[0];
        const int32_t alpha = (int32_t)(vm->color >> 24);
        M_Place(vm, square->pos);
        trail.part = 0;
        Anm_Draw(vm, &trail);
        for (int32_t k = 3; k < PLAYER_BOMB_PART_TRAIL_COUNT; k += 4) {
            Anm_SetAlpha(vm, alpha - alpha * k / 32);
            M_Place(vm, square->trail[k]);
            trail.part = (uint32_t)k;
            Anm_Draw(vm, &trail);
        }
        Anm_SetAlpha(vm, alpha);
    }
}

void Bomb_Draw(const ANM_DRAW *const draw)
{
    M_Tint();
    switch (g_GameManager.run.shot_index * 2
            + (g_Player.bomb.focused ? 1 : 0)) {
    case 0:
        M_ReimuA(draw);
        break;
    case 1:
        M_ReimuAFocused(draw);
        break;
    case 2:
        M_ReimuB(draw);
        break;
    case 3:
        M_ReimuBFocused(draw);
        break;
    case 4:
        M_MarisaA(draw);
        break;
    case 5:
        M_MarisaAFocused(draw);
        break;
    case 6:
        M_MarisaB(draw);
        break;
    case 7:
        M_MarisaBFocused(draw);
        break;
    case 8:
        M_SakuyaA(draw);
        break;
    case 9:
        M_SakuyaAFocused(draw);
        break;
    case 10:
        M_SakuyaB(draw);
        break;
    case 11:
        M_SakuyaBFocused(draw);
        break;
    default:
        break;
    }
}
