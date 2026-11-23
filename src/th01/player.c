#include "th01/player.h"
#include "th01/rng.h"

#include "pc98/zun/math16.h"

#include "core/base/assert.h"

#include <string.h>

PLAYER g_Player;

void Player_Init(void)
{
    memset(&g_Player, 0, sizeof(g_Player));
}

enum {
    M_LEFT_STILL = 0,
    M_LEFT_DASH = 1,
    M_LEFT_CAST = 3,
    M_LEFT_SHOOT = 4,
    M_RIGHT_STILL = 10,
    M_RIGHT_DASH = 11,
    M_RIGHT_CAST = 13,
    M_RIGHT_SHOOT = 14,
};

typedef struct {
    PLAYER_OUTPUT output;
} PLAYER_UPDATE;

static int8_t M_Wrap8(const int32_t value)
{
    const uint8_t low = (uint8_t)value;
    return low <= INT8_MAX ? (int8_t)low : (int8_t)((int16_t)low - 256);
}

static void M_Draw(
    const PLAYER_UPDATE *const update, const PLAYER_ACTION_KIND kind,
    const int16_t image)
{
    update->output.action(
        update->output.user,
        (PLAYER_ACTION) {
            .kind = kind,
            .left = g_Player.left,
            .image = image,
        });
}

static void M_Overlap(
    const PLAYER_UPDATE *const update, const bool tall,
    const int16_t previous_left, const int16_t previous_image,
    const int16_t image)
{
    update->output.action(
        update->output.user,
        (PLAYER_ACTION) {
            .kind = tall ? PLAYER_ACTION_BOS_48_OVERLAP
                         : PLAYER_ACTION_BOS_32_OVERLAP,
            .left = g_Player.left,
            .previous_left = previous_left,
            .image = image,
            .previous_image = previous_image,
        });
}

static void M_Effect(const PLAYER_UPDATE *const update)
{
    update->output.action(
        update->output.user,
        (PLAYER_ACTION) {
            .kind = PLAYER_ACTION_EFFECT,
            .effect = 11,
        });
}

static void M_Repel(const PLAYER_UPDATE *const update, const int16_t friction)
{
    const ORB_PLAYER contact = {
        g_Player.left,
        PLAYER_TOP,
        g_Player.invincible,
        g_Player.invincible_against_orb,
    };
    if (Orb_PlayerHitTest(&contact, friction)) {
        g_Player.is_hit = true;
    }
}

static void M_ShotAction(void *const user, const SHOTS_ACTION action)
{
    const PLAYER_UPDATE *const update = user;
    update->output.action(
        update->output.user,
        (PLAYER_ACTION) {
            .kind = PLAYER_ACTION_SHOT,
            .shot = action,
        });
}

static void M_Shoot(PLAYER_UPDATE *const update, const int16_t offset)
{
    Shots_Add(
        Math16_Wrap(g_Player.left + 8 + offset), PLAYER_TOP,
        (SHOTS_OUTPUT) { M_ShotAction, update });
}

static int16_t M_DashImage(const int16_t base)
{
    return base + (g_Player.dash_cycle % 8 < 4 ? 0 : 1);
}

static int16_t M_SwingImage(const int16_t frame)
{
    return frame >= 23 ? 7 : frame / 3;
}

static int16_t M_FlipImage(const int16_t frame, const int16_t base)
{
    return frame < 4 ? base : (frame - 7) / 4 + base;
}

static void M_SpecialStart(
    const PLAYER_UPDATE *const update, const bool deflect)
{
    if (deflect) {
        g_Player.deflecting = true;
    }
    M_Effect(update);
    g_Player.combo_enabled = false;
}

static void M_SelectStrike(
    const PLAYER_UPDATE *const update, const int8_t direction)
{
    const int8_t same =
        direction == PLAYER_FACING_RIGHT ? GAME_INPUT_RIGHT : GAME_INPUT_LEFT;
    const int8_t other =
        direction == PLAYER_FACING_RIGHT ? GAME_INPUT_LEFT : GAME_INPUT_RIGHT;
    g_Player.submode =
        (g_GameInput.horizontal == other ? PLAYER_SPECIAL_FLIPKICK_STATIONARY
             : g_GameInput.horizontal == same ? PLAYER_SPECIAL_SLIDEKICK
                                              : PLAYER_SPECIAL_FLIPKICK_MOVING)
        | direction;
}

static void M_Regular(PLAYER_UPDATE *const update)
{
    int16_t image;
    if (g_GameInput.horizontal == GAME_INPUT_RIGHT
        || g_GameInput.horizontal == GAME_INPUT_LEFT) {
        const bool right = g_GameInput.horizontal == GAME_INPUT_RIGHT;
        g_Player.dash_direction =
            right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, g_Player.previous_ptn);
        Player_MoveAndClamp(right ? 4 : -4);
        image = M_DashImage(right ? M_RIGHT_DASH : M_LEFT_DASH);
        M_Draw(update, PLAYER_ACTION_PTN_PUT, image);
        g_Player.previous_ptn = image;
        g_Player.submode = g_GameInput.horizontal;
    } else {
        if (g_Player.submode == GAME_INPUT_NONE) {
            image = g_Player.dash_direction != PLAYER_FACING_RIGHT
                ? M_LEFT_STILL
                : M_RIGHT_STILL;
            if (g_Player.previous_ptn != image) {
                M_Draw(update, PLAYER_ACTION_PTN_ERASE, g_Player.previous_ptn);
            }
            M_Draw(update, PLAYER_ACTION_PTN_PUT, image);
            g_Player.previous_ptn = image;
        }
        g_Player.submode = GAME_INPUT_NONE;
    }
    if (g_GameInput.shot) {
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, g_Player.previous_ptn);
        g_Player.mode = PLAYER_MODE_SHOOT;
        g_Player.mode_frame = 0;
        g_Player.submode = g_GameInput.horizontal == GAME_INPUT_BOTH
            ? GAME_INPUT_NONE
            : g_GameInput.horizontal;
        if (g_Player.submode == GAME_INPUT_NONE) {
            image = g_Player.dash_direction != PLAYER_FACING_RIGHT
                ? M_LEFT_CAST
                : M_RIGHT_CAST;
        } else {
            image = g_Player.submode == GAME_INPUT_RIGHT ? M_RIGHT_SHOOT
                                                         : M_LEFT_SHOOT;
        }
        M_Draw(update, PLAYER_ACTION_PTN_PUT, image);
        g_Player.previous_ptn = image;
    }
    if (g_GameInput.strike) {
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, g_Player.previous_ptn);
        if (g_GameInput.horizontal == GAME_INPUT_NONE
            || g_GameInput.horizontal == GAME_INPUT_BOTH) {
            g_Player.mode_frame = 0;
            g_Player.mode = PLAYER_MODE_SWING_OR_SLIDE;
            g_Player.submode = GAME_INPUT_NONE;
            g_Player.deflecting = true;
        } else {
            M_Effect(update);
            g_Player.mode_frame = 0;
            g_Player.mode = PLAYER_MODE_SWING_OR_SLIDE;
            g_Player.submode = g_GameInput.horizontal % GAME_INPUT_BOTH;
            g_Player.sliding = true;
            g_Player.combo_enabled = false;
            M_Draw(
                update, PLAYER_ACTION_BOS_32_PUT,
                g_Player.submode == GAME_INPUT_RIGHT ? 3 : 1);
        }
        g_Player.invincible_against_orb = true;
    }
}

static void M_ShootMode(PLAYER_UPDATE *const update)
{
    if (g_Player.submode == GAME_INPUT_NONE) {
        if (g_Player.mode_frame == 0) {
            M_Shoot(update, 0);
        } else if (g_Player.mode_frame >= 2) {
            g_Player.bomb_frame = 0;
            g_Player.mode = PLAYER_MODE_REGULAR;
            g_GameInput.shot = false;
            g_Player.submode = GAME_INPUT_BOTH;
        }
    } else if (
        g_Player.submode == GAME_INPUT_RIGHT
        || g_Player.submode == GAME_INPUT_LEFT) {
        const bool right = g_Player.submode == GAME_INPUT_RIGHT;
        g_Player.dash_direction =
            right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, g_Player.previous_ptn);
        Player_MoveAndClamp(right ? 4 : -4);
        const int16_t base = right ? M_RIGHT_SHOOT : M_LEFT_SHOOT;
        M_Draw(update, PLAYER_ACTION_PTN_PUT, base + (g_Player.mode_frame & 3));
        g_Player.previous_ptn =
            base + (right ? g_Player.mode_frame % 3 : g_Player.mode_frame & 3);
        if (g_Player.mode_frame >= 1) {
            M_Shoot(update, 0);
            g_Player.bomb_frame = 0;
            g_GameInput.shot = false;
            g_Player.mode = PLAYER_MODE_REGULAR;
        }
    }
    g_Player.mode_frame = M_Wrap8(g_Player.mode_frame + 1);
}

static void M_SwingOrSlide(PLAYER_UPDATE *const update)
{
    if (g_Player.submode == GAME_INPUT_NONE) {
        if (g_Player.mode_frame < 1) {
            g_Player.submode = g_GameInput.horizontal % GAME_INPUT_BOTH;
            if (g_Player.submode != GAME_INPUT_NONE) {
                g_Player.mode_frame = 0;
                g_Player.deflecting = false;
                M_Effect(update);
                g_Player.sliding = true;
            }
        } else {
            if (g_Player.mode_frame % 3 == 0) {
                const int16_t image = M_SwingImage(g_Player.mode_frame + 1);
                M_Overlap(update, true, g_Player.left, image - 1, image);
            }
            if (g_Player.mode_frame == g_Player.deflect_duration) {
                g_Player.deflecting = false;
            }
            if (g_Player.mode_frame >= 23) {
                g_Player.mode = PLAYER_MODE_REGULAR;
                M_Draw(
                    update, PLAYER_ACTION_BOS_48_ERASE,
                    M_SwingImage(g_Player.mode_frame));
                g_Player.submode = GAME_INPUT_BOTH;
                g_Player.mode_frame = 0;
                g_GameInput.strike = false;
                g_Player.bomb_frame = 0;
                g_Player.invincible_against_orb = false;
            }
        }
    } else if (
        g_Player.submode == GAME_INPUT_RIGHT
        || g_Player.submode == GAME_INPUT_LEFT) {
        const bool right = g_Player.submode == GAME_INPUT_RIGHT;
        const int16_t base = right ? 2 : 0;
        const int16_t previous_left = g_Player.left;
        Player_MoveAndClamp(right ? 6 : -6);
        M_Overlap(
            update, false, previous_left, base + 1 - g_Player.mode_frame % 2,
            base + g_Player.mode_frame % 2);
        if (g_Player.mode_frame >= 5 && !g_GameInput.strike
            && !g_GameInput.shot) {
            g_Player.combo_enabled = true;
        }
        if (g_Player.mode_frame >= 12) {
            g_Player.mode = PLAYER_MODE_REGULAR;
            M_Draw(
                update, PLAYER_ACTION_BOS_32_ERASE,
                base + g_Player.mode_frame % 2);
            g_Player.submode = GAME_INPUT_BOTH;
            g_Player.mode_frame = 0;
            g_Player.sliding = false;
            const int8_t direction =
                right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
            if (g_Player.combo_enabled && g_GameInput.strike) {
                M_SelectStrike(update, direction);
                g_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
                M_SpecialStart(
                    update, g_Player.submode < PLAYER_SPECIAL_SLIDEKICK);
                return;
            } else if (g_Player.combo_enabled && g_GameInput.shot) {
                g_Player.submode = PLAYER_SPECIAL_SHOTCOMBO | direction;
                g_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
                M_SpecialStart(update, true);
                return;
            }
            g_GameInput.strike = false;
            g_Player.bomb_frame = 0;
            if (right) {
                g_Player.combo_enabled = false;
            }
            g_Player.invincible_against_orb = false;
        }
    }
    g_Player.mode_frame = M_Wrap8(g_Player.mode_frame + 1);
    if (g_Player.submode == GAME_INPUT_NONE) {
        if (g_Player.mode_frame < 21) {
            M_Repel(update, g_Player.mode_frame);
        }
    } else if (g_Player.submode == GAME_INPUT_RIGHT) {
        M_Repel(update, ORB_REPEL_3_X_RIGHT_4);
    } else if (g_Player.submode == GAME_INPUT_LEFT) {
        M_Repel(update, ORB_REPEL_3_X_LEFT_4);
    }
}

static void M_Special(PLAYER_UPDATE *const update)
{
    const bool right = (g_Player.submode & 1) == PLAYER_FACING_RIGHT;
    const int16_t special = g_Player.submode & ~1;
    const int16_t frame = g_Player.mode_frame;
    int16_t image;
    int16_t duration;
    if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
        || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
        const int16_t base = right ? 14 : 8;
        const int16_t previous = M_FlipImage(frame - 1, base);
        image = M_FlipImage(frame, base);
        if (special == PLAYER_SPECIAL_FLIPKICK_MOVING) {
            const int16_t previous_left = g_Player.left;
            Player_MoveAndClamp(right ? 2 : -2);
            M_Overlap(update, true, previous_left, previous, image);
        } else if (previous != image) {
            M_Overlap(update, true, g_Player.left, previous, image);
        } else {
            M_Draw(update, PLAYER_ACTION_BOS_48_PUT, image);
        }
        if (frame == 20) {
            g_Player.deflecting = false;
        }
        if (frame >= 20 && !g_GameInput.strike && !g_GameInput.shot) {
            g_Player.combo_enabled = true;
        }
        duration = 28;
    } else if (special == PLAYER_SPECIAL_SLIDEKICK) {
        image = right ? 5 : 4;
        const int16_t previous_left = g_Player.left;
        Player_MoveAndClamp(right ? 6 : -6);
        M_Overlap(update, false, previous_left, image, image);
        duration = 13;
    } else {
        const int16_t base = right ? 24 : 20;
        image = frame / 6 + base;
        M_Overlap(update, true, g_Player.left, (frame - 1) / 6 + base, image);
        switch (frame) {
        case 1:
            M_Shoot(update, -8);
            break;
        case 4:
            M_Shoot(update, 8);
            break;
        case 7:
            M_Shoot(update, 0);
            break;
        case 10:
            M_Shoot(update, -16);
            break;
        case 13:
            M_Shoot(update, 0);
            break;
        case 16:
            M_Shoot(update, 16);
            break;
        case 19:
            M_Shoot(update, 0);
            break;
        default:
            break;
        }
        if (frame == 4) {
            g_Player.deflecting = false;
        }
        duration = 19;
    }
    g_Player.mode_frame = M_Wrap8(g_Player.mode_frame + 1);
    if (g_Player.mode_frame > duration) {
        M_Draw(
            update,
            special == PLAYER_SPECIAL_SLIDEKICK ? PLAYER_ACTION_BOS_32_ERASE
                                                : PLAYER_ACTION_BOS_48_ERASE,
            image);
        g_Player.bomb_frame = 0;
        g_Player.mode_frame = 0;
        g_Player.sliding = false;
        g_Player.deflecting = false;
        if (g_Player.mode < PLAYER_MODE_SPECIAL_THIRD) {
            if (g_Player.combo_enabled && g_GameInput.strike) {
                if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
                    || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
                    M_SelectStrike(
                        update,
                        right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT);
                }
                g_Player.mode++;
                M_SpecialStart(
                    update, g_Player.submode < PLAYER_SPECIAL_SLIDEKICK);
                return;
            } else if (g_Player.combo_enabled && g_GameInput.shot) {
                if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
                    || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
                    g_Player.submode = PLAYER_SPECIAL_SHOTCOMBO
                        | (right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT);
                }
                g_Player.mode++;
                M_SpecialStart(update, true);
                return;
            }
        }
        g_Player.invincible_against_orb = false;
        g_Player.mode = PLAYER_MODE_REGULAR;
    } else if (
        special == PLAYER_SPECIAL_FLIPKICK_MOVING
        || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
        M_Repel(update, g_Player.mode_frame / 2);
    } else if (special == PLAYER_SPECIAL_SLIDEKICK) {
        M_Repel(update, right ? ORB_REPEL_3_X_RIGHT_8 : ORB_REPEL_3_X_LEFT_8);
    } else if (g_Player.mode_frame < 16) {
        M_Repel(update, g_Player.mode_frame);
    }
}

RESULT Player_ValidateSprites(const PLAYER_SPRITES *const sprites)
{
    FAIL_IF(
        sprites->miko->count < 16 || sprites->miko->pixels == nullptr,
        "Incomplete player PTN sprites");
    FAIL_IF(
        sprites->miko_ac->width != 48 || sprites->miko_ac->height != 48
            || sprites->miko_ac->count < 28
            || sprites->miko_ac->pixels == nullptr
            || sprites->miko_ac->coverage == nullptr,
        "Incomplete player 48 by 48 sprites");
    FAIL_IF(
        sprites->miko_ac2->width != 48 || sprites->miko_ac2->height != 32
            || sprites->miko_ac2->count < 6
            || sprites->miko_ac2->pixels == nullptr
            || sprites->miko_ac2->coverage == nullptr,
        "Incomplete player 48 by 32 sprites");
    return OK;
}

void Player_Reset(const int32_t difficulty)
{
    g_Player.dash_cycle = 0;
    g_Player.mode_frame = 0;
    g_Player.mode = PLAYER_MODE_REGULAR;
    g_Player.submode = -1;
    g_Player.dash_direction = PLAYER_FACING_LEFT;
    g_Player.bomb_flag = PLAYER_BOMB_NONE;
    g_Player.bomb_damaging = false;
    g_Player.sliding = false;
    g_Player.deflecting = false;
    g_Player.previous_ptn = M_LEFT_STILL;
    g_Player.deflect_duration = difficulty == SETTINGS_DIFFICULTY_EASY ? 15
        : difficulty == SETTINGS_DIFFICULTY_HARD                       ? 10
        : difficulty == SETTINGS_DIFFICULTY_LUNATIC                    ? 8
                                                                       : 12;
    g_Player.bombing = false;
    g_Player.combo_enabled = false;
    g_GameInput.bomb = false;
    g_Player.invincible_against_orb = false;
}

void Player_MoveAndClamp(const int16_t delta)
{
    g_Player.left = Math16_Wrap(g_Player.left + delta);
    if (g_Player.left < 0) {
        g_Player.left = 0;
    } else if (g_Player.left >= 608) {
        g_Player.left = 608;
    }
}

bool Player_BeginUpdate(
    PLAYER_BOMB_CONTINUATION *const continuation, const PLAYER_OUTPUT output)
{
    ASSERT(!continuation->pending);
    PLAYER_UPDATE update = { .output = output };
    g_Player.dash_cycle = M_Wrap8(g_Player.dash_cycle + 1) & 7;
    if ((g_Player.remaining_bombs != 0 || g_Player.bombing)
        && g_Player.bomb_flag >= PLAYER_BOMB_START) {
        if (g_Player.bomb_flag == PLAYER_BOMB_START) {
            g_Player.bomb_frame = 0;
            g_Player.bomb_flag = PLAYER_BOMB_ACTIVE;
            g_Player.deflecting = true;
            g_Player.bombing = true;
            g_Player.remaining_bombs =
                Math16_Wrap(g_Player.remaining_bombs - 1);
            output.action(
                output.user,
                (PLAYER_ACTION) {
                    .kind = PLAYER_ACTION_HUD_BOMBS,
                    .previous_bombs = Math16_Wrap(g_Player.remaining_bombs + 1),
                });
        }
        M_Repel(&update, 1);
        *continuation = (PLAYER_BOMB_CONTINUATION) {
            .output = output,
            .pending = true,
        };
        return true;
    } else if (
        g_GameInput.bomb && g_Player.mode != PLAYER_MODE_SPECIAL_FIRST
        && g_Player.remaining_bombs != 0) {
        g_Player.bomb_flag = PLAYER_BOMB_START;
        g_GameInput.bomb = false;
        output.action(
            output.user,
            (PLAYER_ACTION) {
                .kind = PLAYER_ACTION_PELLET_SPEED_RAISE,
                .pellet_speed_delta = 0.025f,
            });
    } else if (g_Player.mode == PLAYER_MODE_REGULAR) {
        M_Regular(&update);
    } else if (g_Player.mode == PLAYER_MODE_SHOOT) {
        M_ShootMode(&update);
    } else if (g_Player.mode == PLAYER_MODE_SWING_OR_SLIDE) {
        M_SwingOrSlide(&update);
    } else if (
        g_Player.mode >= PLAYER_MODE_SPECIAL_FIRST
        && g_Player.mode <= PLAYER_MODE_SPECIAL_THIRD) {
        M_Special(&update);
    }
    return false;
}

void Player_FinishBombUpdate(
    PLAYER_BOMB_CONTINUATION *const continuation, const bool done)
{
    ASSERT(continuation->pending);
    continuation->pending = false;

    PLAYER_UPDATE update = { .output = continuation->output };
    g_Player.bomb_flag = done ? PLAYER_BOMB_NONE : PLAYER_BOMB_ACTIVE;
    if (done) {
        g_Player.bombing = false;
        g_GameInput.bomb = false;
        M_Draw(&update, PLAYER_ACTION_PTN_PUT, M_LEFT_STILL);
        g_Player.previous_ptn = M_LEFT_STILL;
    } else if (g_Player.bomb_frame > 60) {
        M_Draw(&update, PLAYER_ACTION_PTN_PUT, M_LEFT_CAST);
    }
    g_GameInput.shot = false;
    g_GameInput.strike = false;
    g_Player.mode = PLAYER_MODE_REGULAR;
    if (g_Player.bomb_flag == PLAYER_BOMB_NONE) {
        g_Player.deflecting = false;
    }
}

void Player_UpdateAndRender(const PLAYER_OUTPUT output)
{
    PLAYER_BOMB_CONTINUATION continuation = {};
    if (Player_BeginUpdate(&continuation, output)) {
        const bool done = output.bomb_update(
            output.user, Math16_Wrap(g_Player.bomb_frame & 0xffff));
        Player_FinishBombUpdate(&continuation, done);
    }
}

void Player_RenderAction(
    const PLAYER_SPRITES *const sprites, const PTN *const stage,
    const PLAYER_ACTION action)
{
    switch (action.kind) {
    case PLAYER_ACTION_PTN_PUT:
        Sprite_PtnPut(sprites->miko, action.image, -1, action.left, PLAYER_TOP);
        break;
    case PLAYER_ACTION_PTN_ERASE:
        Sprite_PtnErase(
            sprites->miko, action.image, -1, action.left, PLAYER_TOP);
        break;
    case PLAYER_ACTION_BOS_48_PUT:
    case PLAYER_ACTION_BOS_32_PUT:
        Sprite_PlayerPut(
            action.kind == PLAYER_ACTION_BOS_48_PUT ? sprites->miko_ac
                                                    : sprites->miko_ac2,
            action.image, Math16_Wrap(action.left - 8),
            action.kind == PLAYER_ACTION_BOS_48_PUT ? 352 : PLAYER_TOP);
        break;
    case PLAYER_ACTION_BOS_48_ERASE:
    case PLAYER_ACTION_BOS_32_ERASE:
        Sprite_PlayerErase(
            action.kind == PLAYER_ACTION_BOS_48_ERASE ? sprites->miko_ac
                                                      : sprites->miko_ac2,
            action.image, Math16_Wrap(action.left - 8),
            action.kind == PLAYER_ACTION_BOS_48_ERASE ? 352 : PLAYER_TOP);
        break;
    case PLAYER_ACTION_BOS_48_OVERLAP:
    case PLAYER_ACTION_BOS_32_OVERLAP: {
        const bool tall = action.kind == PLAYER_ACTION_BOS_48_OVERLAP;
        Sprite_PlayerOverlap(
            tall ? sprites->miko_ac : sprites->miko_ac2, action.image,
            action.previous_image, Math16_Wrap(action.left - 8),
            tall ? 352 : PLAYER_TOP, Math16_Wrap(action.previous_left - 8),
            tall ? 352 : PLAYER_TOP);
        break;
    }
    case PLAYER_ACTION_SHOT:
        Shots_RenderAction(stage, action.shot);
        break;
    case PLAYER_ACTION_EFFECT:
    case PLAYER_ACTION_HUD_BOMBS:
    case PLAYER_ACTION_PELLET_SPEED_RAISE:
        break;
    }
}
