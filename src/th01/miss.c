#include "th01/miss.h"
#include "th01/hud.h"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shots.h"
#include "th01/timer.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

#include "core/base/assert.h"

MISS g_Miss;

static void M_Orb(const PTN *const stage)
{
    Orb_Put(stage, g_Orb.left, g_Orb.top, 0);
}

static void M_Effect(const PTN *const miko, const PTN *const stage)
{
    PLAYER *const player = &g_Player;
    Sprite_Restore16(player->left, PLAYER_TOP, 32, 32);
    Sprite_Restore16(g_Miss.effect_left, PLAYER_TOP, 32, 32);
    player->left = Math16_Wrap(
        player->left
        + (g_Miss.frame < 10 ? 16 : (304 - g_Miss.target_right) / 20));
    g_Miss.effect_left = Math16_Wrap(
        g_Miss.effect_left
        + (g_Miss.frame < 10 ? -16 : (304 - g_Miss.target_left) / 20));
    if (player->left >= 608) {
        player->left = 608;
    }
    if (g_Miss.effect_left < 0) {
        g_Miss.effect_left = 0;
    }
    M_Orb(stage);
    Sprite_PtnPut(miko, 20 + g_Miss.frame % 2, -1, player->left, PLAYER_TOP);
    Sprite_PtnPut(
        miko, 20 + g_Miss.frame % 2, -1, g_Miss.effect_left, PLAYER_TOP);
    g_Miss.phase = MISS_EFFECT_DELAY;
}

static void M_Reset(const PTN *const miko, const PTN *const stage)
{
    PLAYER *const player = &g_Player;
    Sprite_Restore16(Math16_Wrap(player->left - 8), 352, 48, 48);
    Shots_EraseAndReset();
    Pellets_Decay();
    Player_Reset(g_Session.settings.difficulty);
    g_Miss.target_right = Math16_Wrap(player->left + 160);
    if (g_Miss.target_right >= 608) {
        g_Miss.target_right = 608;
    }
    g_Miss.target_left = Math16_Wrap(player->left - 160);
    if (g_Miss.target_left < 0) {
        g_Miss.target_left = 0;
    }
    g_Miss.effect_left = player->left;
    g_Miss.frame = 0;
    M_Effect(miko, stage);
}

static void M_Finish(
    const PTN *const miko, const PTN *const stage, const int16_t extra_bombs)
{
    PLAYER *const player = &g_Player;
    Sprite_Restore16(player->left, PLAYER_TOP, 32, 32);
    Sprite_Restore16(g_Miss.effect_left, PLAYER_TOP, 32, 32);
    Hud_LivesPut(Math16_Wrap(g_Session.lives + 1));
    const int16_t previous = player->remaining_bombs;
    player->remaining_bombs =
        Math16_Wrap(player->remaining_bombs + extra_bombs);
    if (player->remaining_bombs > 5) {
        player->remaining_bombs = 5;
    } else {
        Hud_BombsPut(previous);
    }
    StageTimer_ExtendAndPut();
    Pellets_SpeedLower(0, -2);
    g_Miss.phase = MISS_DONE;
}

RESULT Miss_ValidateSprites(const PTN *const miko, const PTN *const stage)
{
    FAIL_IF(
        miko->count < 24 || miko->pixels == nullptr,
        "Incomplete miss PTN sprites");
    FAIL_IF(
        stage->count < ORB_FIRST_IMAGE + ORB_IMAGE_COUNT
            || stage->pixels == nullptr,
        "Incomplete Orb PTN sprites");
    return OK;
}

void Miss_Begin(const PTN *const miko, const PTN *const stage)
{
    ASSERT(g_Miss.phase == MISS_DONE);
    Sprite_Restore16(g_Player.left, PLAYER_TOP, 32, 32);
    M_Orb(stage);
    const int16_t image = MasterRng_Next(&g_Rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(miko, image, -1, g_Player.left, PLAYER_TOP);
    g_Miss.frame = 0;
    g_Miss.phase = MISS_SHAKE_WAIT;
    Frame_BeginWait(&g_Miss.wait);
}

bool Miss_Resume(
    const PTN *const miko, const PTN *const stage, const int16_t extra_bombs)
{
    for (;;) {
        switch (g_Miss.phase) {
        case MISS_DONE:
            return true;
        case MISS_SHAKE_WAIT:
            if (!Frame_WaitReady(&g_Miss.wait)) {
                return false;
            }
            Video_SetScroll((int16_t)(400 - g_Miss.frame % 2 * 8));
            g_Miss.phase = MISS_SHAKE_DELAY;
            break;
        case MISS_SHAKE_DELAY:
            if (!Frame_DelayReady(2)) {
                return false;
            }
            g_Miss.frame++;
            g_Miss.phase =
                g_Miss.frame < 16 ? MISS_SHAKE_WAIT : MISS_SCROLL_RESET_WAIT;
            Frame_BeginWait(&g_Miss.wait);
            return false;
        case MISS_SCROLL_RESET_WAIT:
            if (!Frame_WaitReady(&g_Miss.wait)) {
                return false;
            }
            Video_SetScroll(0);
            M_Reset(miko, stage);
            break;
        case MISS_EFFECT_DELAY: {
            const uint16_t delay =
                (uint16_t)(g_Miss.frame < 6 ? 1 : (30 - g_Miss.frame) / 10 + 1);
            if (!Frame_DelayReady(delay)) {
                return false;
            }
            g_Miss.frame++;
            if (g_Miss.frame == 30) {
                M_Finish(miko, stage, extra_bombs);
                return true;
            }
            M_Effect(miko, stage);
            break;
        }
        }
    }
}
