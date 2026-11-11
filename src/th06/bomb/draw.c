#include "th06/bomb/draw.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/game_manager.h"
#include "th06/player.h"

#include "core/math/x87.h"

static void M_Dim(void)
{
    const PLAYER *const player = &g_Player;
    int32_t alpha;
    if (player->bomb.timer.current < 60) {
        const float t = (float)player->bomb.timer.current * 176.0f / 60.0f;
        alpha = t >= 176.0f ? 176 : X87_Ftol(t);
    } else if (player->bomb.timer.current >= player->bomb.duration - 60) {
        const float t =
            ((float)player->bomb.duration - (float)player->bomb.timer.current)
            * 176.0f / 60.0f;
        alpha = t < 0.0f ? 0 : X87_Ftol(t);
    } else {
        alpha = 176;
    }
    Anm_DrawRect((GFX_RECT) { 32, 16, 384, 448 }, (uint32_t)alpha << 24);
}

static void M_Place(ANM_VM *const vm, const VEC3 pos, const bool offset)
{
    vm->pos = offset ? Vec3_Add(pos, vm->pos_offset) : pos;
    vm->pos.x += g_GameManager.play_area.x;
    vm->pos.y += g_GameManager.play_area.y;
    vm->pos.z = 0.0f;
}

static void M_ReimuA(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < PLAYER_BOMB_PART_COUNT; i++) {
        if (g_Player.bomb.parts[i].state == 0) {
            continue;
        }
        for (int32_t j = 0; j < 4; j++) {
            ANM_VM *const vm = &g_Player.bomb.vms[i * 4 + j];
            M_Place(vm, g_Player.bomb.parts[i].pos, true);
            Anm_DrawUnrotated(vm, draw);
        }
    }
}

static void M_ReimuB(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < 4; i++) {
        ANM_VM *const vm = &g_Player.bomb.vms[i];
        M_Place(vm, g_Player.bomb.parts[i].pos, true);
        Anm_Draw(vm, draw);
    }
}

static void M_MarisaA(const ANM_DRAW *const draw)
{
    ANM_DRAW trail = *draw;
    for (int32_t i = 0; i < PLAYER_BOMB_PART_COUNT; i++) {
        ANM_VM *const vm = &g_Player.bomb.vms[i];
        M_Place(vm, g_Player.bomb.parts[i].pos, false);
        vm->scale_x = 3.2f;
        vm->scale_y = 3.2f;
        trail.part = 0;
        Anm_Draw(vm, &trail);
        vm->pos = Vec3_Sub(
            vm->pos, Vec3_Scale(g_Player.bomb.parts[i].velocity, 6.0f));
        vm->pos.x += -32.0f;
        vm->pos.y += -32.0f;
        vm->pos.z = 0.0f;
        vm->scale_x = 2.2f;
        vm->scale_y = 2.2f;
        trail.part = 1;
        Anm_Draw(vm, &trail);
        vm->pos = Vec3_Sub(
            vm->pos, Vec3_Scale(g_Player.bomb.parts[i].velocity, 2.0f));
        vm->pos.x += 64.0f;
        vm->pos.y += 64.0f;
        vm->pos.z = 0.0f;
        vm->pos = Vec3_Sub(
            vm->pos, Vec3_Scale(g_Player.bomb.parts[i].velocity, 2.0f));
        vm->pos.x += -32.0f;
        vm->pos.y += -32.0f;
        vm->pos.z = 0.0f;
        vm->scale_x = 1.0f;
        vm->scale_y = 1.0f;
        trail.part = 2;
        Anm_Draw(vm, &trail);
    }
}

static void M_MarisaB(const ANM_DRAW *const draw)
{
    for (int32_t i = 0; i < 4; i++) {
        ANM_VM *const vm = &g_Player.bomb.vms[i];
        if (vm->sprite_data == nullptr) {
            continue;
        }
        float angle = 0.62831855f * (float)i / 3.0f - 3.1415927f + 1.2566371f;
        vm->pos = g_Player.pos;
        vm->pos.x +=
            X87_Cos(angle) * vm->sprite_data->height * vm->scale_y / 2.0f;
        vm->pos.y +=
            X87_Sin(angle) * vm->sprite_data->height * vm->scale_y / 2.0f;
        angle = 1.5707964f - angle;
        vm->rotation.z = Angle_AddNormalize(angle, 3.1415927f);
        vm->pos.x += g_GameManager.play_area.x;
        vm->pos.y += g_GameManager.play_area.y;
        vm->pos.z = 0.0f;
        Anm_Draw(vm, draw);
    }
}

void Bomb_Draw(void)
{
    if (!g_Player.bomb.running) {
        return;
    }
    M_Dim();
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.interpolate = true;
    switch (g_GameManager.run.character * 2 + g_GameManager.run.shot_type) {
    case 0:
        M_ReimuA(&draw);
        break;
    case 1:
        M_ReimuB(&draw);
        break;
    case 2:
        M_MarisaA(&draw);
        break;
    case 3:
        M_MarisaB(&draw);
        break;
    }
}
