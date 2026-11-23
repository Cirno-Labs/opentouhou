#include "th01/miss.h"
#include "th01/rng.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

#include "core/base/assert.h"

typedef struct {
    const PTN *stage;
} MISS_SHOTS;

static void M_Orb(const MISS_CONTEXT *const context)
{
    Orb_RenderAction(
        context->stage,
        (ORB_ACTION) { .kind = ORB_ACTION_PUT,
                       .left = g_Orb.left,
                       .top = g_Orb.top,
                       .image = 0 });
}

static void M_Shot(void *const user, const SHOTS_ACTION action)
{
    const MISS_SHOTS *const context = user;
    ASSERT(action.kind == SHOTS_ACTION_RESTORE);
    Shots_RenderAction(context->stage, action);
}

static void M_Effect(MISS *const miss, const MISS_CONTEXT *const context)
{
    PLAYER *const player = &g_Player;
    Sprite_Restore16(player->left, PLAYER_TOP, 32, 32);
    Sprite_Restore16(miss->effect_left, PLAYER_TOP, 32, 32);
    player->left = Math16_Wrap(
        player->left
        + (miss->frame < 10 ? 16 : (304 - miss->target_right) / 20));
    miss->effect_left = Math16_Wrap(
        miss->effect_left
        + (miss->frame < 10 ? -16 : (304 - miss->target_left) / 20));
    if (player->left >= 608) {
        player->left = 608;
    }
    if (miss->effect_left < 0) {
        miss->effect_left = 0;
    }
    M_Orb(context);
    Sprite_PtnPut(
        context->miko, 20 + miss->frame % 2, -1, player->left, PLAYER_TOP);
    Sprite_PtnPut(
        context->miko, 20 + miss->frame % 2, -1, miss->effect_left, PLAYER_TOP);
    miss->phase = MISS_EFFECT_DELAY;
}

static void M_Reset(MISS *const miss, const MISS_CONTEXT *const context)
{
    PLAYER *const player = &g_Player;
    Sprite_Restore16(Math16_Wrap(player->left - 8), 352, 48, 48);
    MISS_SHOTS shot_context = { context->stage };
    Shots_EraseAndReset((SHOTS_OUTPUT) { M_Shot, &shot_context });
    context->pellets_decay(context->user);
    Player_Reset(context->difficulty);
    miss->target_right = Math16_Wrap(player->left + 160);
    if (miss->target_right >= 608) {
        miss->target_right = 608;
    }
    miss->target_left = Math16_Wrap(player->left - 160);
    if (miss->target_left < 0) {
        miss->target_left = 0;
    }
    miss->effect_left = player->left;
    miss->frame = 0;
    M_Effect(miss, context);
}

static void M_Finish(MISS *const miss, const MISS_CONTEXT *const context)
{
    PLAYER *const player = &g_Player;
    Sprite_Restore16(player->left, PLAYER_TOP, 32, 32);
    Sprite_Restore16(miss->effect_left, PLAYER_TOP, 32, 32);
    context->hud_lives(
        context->user, Math16_Wrap(context->remaining_lives + 1));
    const int16_t previous = player->remaining_bombs;
    player->remaining_bombs =
        Math16_Wrap(player->remaining_bombs + context->extra_bombs);
    if (player->remaining_bombs > 5) {
        player->remaining_bombs = 5;
    } else {
        context->hud_bombs(context->user, previous);
    }
    context->timer_extend(context->user);
    context->pellet_speed_lower(context->user, 0.0f, -0.05f);
    miss->phase = MISS_DONE;
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

void Miss_Begin(MISS *const miss, const MISS_CONTEXT *const context)
{
    ASSERT(miss->phase == MISS_DONE);
    Sprite_Restore16(g_Player.left, PLAYER_TOP, 32, 32);
    M_Orb(context);
    const int16_t image = MasterRng_Next(&g_Rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(context->miko, image, -1, g_Player.left, PLAYER_TOP);
    miss->frame = 0;
    miss->phase = MISS_SHAKE_WAIT;
    Frame_BeginWait(&miss->wait);
}

bool Miss_Resume(MISS *const miss, const MISS_CONTEXT *const context)
{
    for (;;) {
        switch (miss->phase) {
        case MISS_DONE:
            return true;
        case MISS_SHAKE_WAIT:
            if (!Frame_WaitReady(&miss->wait)) {
                return false;
            }
            Video_SetScroll((int16_t)(400 - miss->frame % 2 * 8));
            miss->phase = MISS_SHAKE_DELAY;
            break;
        case MISS_SHAKE_DELAY:
            if (!Frame_DelayReady(2)) {
                return false;
            }
            miss->frame++;
            miss->phase =
                miss->frame < 16 ? MISS_SHAKE_WAIT : MISS_SCROLL_RESET_WAIT;
            Frame_BeginWait(&miss->wait);
            return false;
        case MISS_SCROLL_RESET_WAIT:
            if (!Frame_WaitReady(&miss->wait)) {
                return false;
            }
            Video_SetScroll(0);
            M_Reset(miss, context);
            break;
        case MISS_EFFECT_DELAY: {
            const uint16_t delay =
                (uint16_t)(miss->frame < 6 ? 1 : (30 - miss->frame) / 10 + 1);
            if (!Frame_DelayReady(delay)) {
                return false;
            }
            miss->frame++;
            if (miss->frame == 30) {
                M_Finish(miss, context);
                return true;
            }
            M_Effect(miss, context);
            break;
        }
        }
    }
}
