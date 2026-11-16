#include "th01/player.h"

#include "th01/math.h"

#include "core/base/assert.h"

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
    PLAYER *player;
    GAME_INPUT *input;
    ORB *orb;
    SHOTS *shots;
    uint32_t *rng;
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
            .left = update->player->left,
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
            .left = update->player->left,
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
    PLAYER *const player = update->player;
    const ORB_PLAYER contact = {
        player->left,
        PLAYER_TOP,
        player->invincible,
        player->invincible_against_orb,
    };
    if (Orb_PlayerHitTest(update->orb, &contact, friction, update->rng)) {
        player->is_hit = true;
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
        update->shots, Math_Wrap16(update->player->left + 8 + offset),
        PLAYER_TOP, (SHOTS_OUTPUT) { M_ShotAction, update });
}

static int16_t M_DashImage(const PLAYER *const player, const int16_t base)
{
    return base + (player->dash_cycle % 8 < 4 ? 0 : 1);
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
        update->player->deflecting = true;
    }
    M_Effect(update);
    update->player->combo_enabled = false;
}

static void M_SelectStrike(
    const PLAYER_UPDATE *const update, const int8_t direction)
{
    const int8_t same =
        direction == PLAYER_FACING_RIGHT ? GAME_INPUT_RIGHT : GAME_INPUT_LEFT;
    const int8_t other =
        direction == PLAYER_FACING_RIGHT ? GAME_INPUT_LEFT : GAME_INPUT_RIGHT;
    update->player->submode =
        (update->input->horizontal == other ? PLAYER_SPECIAL_FLIPKICK_STATIONARY
             : update->input->horizontal == same
             ? PLAYER_SPECIAL_SLIDEKICK
             : PLAYER_SPECIAL_FLIPKICK_MOVING)
        | direction;
}

static void M_Regular(PLAYER_UPDATE *const update)
{
    PLAYER *const player = update->player;
    GAME_INPUT *const input = update->input;
    int16_t image;
    if (input->horizontal == GAME_INPUT_RIGHT
        || input->horizontal == GAME_INPUT_LEFT) {
        const bool right = input->horizontal == GAME_INPUT_RIGHT;
        player->dash_direction =
            right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, player->previous_ptn);
        Player_MoveAndClamp(player, right ? 4 : -4);
        image = M_DashImage(player, right ? M_RIGHT_DASH : M_LEFT_DASH);
        M_Draw(update, PLAYER_ACTION_PTN_PUT, image);
        player->previous_ptn = image;
        player->submode = input->horizontal;
    } else {
        if (player->submode == GAME_INPUT_NONE) {
            image = player->dash_direction != PLAYER_FACING_RIGHT
                ? M_LEFT_STILL
                : M_RIGHT_STILL;
            if (player->previous_ptn != image) {
                M_Draw(update, PLAYER_ACTION_PTN_ERASE, player->previous_ptn);
            }
            M_Draw(update, PLAYER_ACTION_PTN_PUT, image);
            player->previous_ptn = image;
        }
        player->submode = GAME_INPUT_NONE;
    }
    if (input->shot) {
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, player->previous_ptn);
        player->mode = PLAYER_MODE_SHOOT;
        player->mode_frame = 0;
        player->submode = input->horizontal == GAME_INPUT_BOTH
            ? GAME_INPUT_NONE
            : input->horizontal;
        if (player->submode == GAME_INPUT_NONE) {
            image = player->dash_direction != PLAYER_FACING_RIGHT
                ? M_LEFT_CAST
                : M_RIGHT_CAST;
        } else {
            image = player->submode == GAME_INPUT_RIGHT ? M_RIGHT_SHOOT
                                                        : M_LEFT_SHOOT;
        }
        M_Draw(update, PLAYER_ACTION_PTN_PUT, image);
        player->previous_ptn = image;
    }
    if (input->strike) {
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, player->previous_ptn);
        if (input->horizontal == GAME_INPUT_NONE
            || input->horizontal == GAME_INPUT_BOTH) {
            player->mode_frame = 0;
            player->mode = PLAYER_MODE_SWING_OR_SLIDE;
            player->submode = GAME_INPUT_NONE;
            player->deflecting = true;
        } else {
            M_Effect(update);
            player->mode_frame = 0;
            player->mode = PLAYER_MODE_SWING_OR_SLIDE;
            player->submode = input->horizontal % GAME_INPUT_BOTH;
            player->sliding = true;
            player->combo_enabled = false;
            M_Draw(
                update, PLAYER_ACTION_BOS_32_PUT,
                player->submode == GAME_INPUT_RIGHT ? 3 : 1);
        }
        player->invincible_against_orb = true;
    }
}

static void M_ShootMode(PLAYER_UPDATE *const update)
{
    PLAYER *const player = update->player;
    if (player->submode == GAME_INPUT_NONE) {
        if (player->mode_frame == 0) {
            M_Shoot(update, 0);
        } else if (player->mode_frame >= 2) {
            player->bomb_frame = 0;
            player->mode = PLAYER_MODE_REGULAR;
            update->input->shot = false;
            player->submode = GAME_INPUT_BOTH;
        }
    } else if (
        player->submode == GAME_INPUT_RIGHT
        || player->submode == GAME_INPUT_LEFT) {
        const bool right = player->submode == GAME_INPUT_RIGHT;
        player->dash_direction =
            right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
        M_Draw(update, PLAYER_ACTION_PTN_ERASE, player->previous_ptn);
        Player_MoveAndClamp(player, right ? 4 : -4);
        const int16_t base = right ? M_RIGHT_SHOOT : M_LEFT_SHOOT;
        M_Draw(update, PLAYER_ACTION_PTN_PUT, base + (player->mode_frame & 3));
        player->previous_ptn =
            base + (right ? player->mode_frame % 3 : player->mode_frame & 3);
        if (player->mode_frame >= 1) {
            M_Shoot(update, 0);
            player->bomb_frame = 0;
            update->input->shot = false;
            player->mode = PLAYER_MODE_REGULAR;
        }
    }
    player->mode_frame = M_Wrap8(player->mode_frame + 1);
}

static void M_SwingOrSlide(PLAYER_UPDATE *const update)
{
    PLAYER *const player = update->player;
    GAME_INPUT *const input = update->input;
    if (player->submode == GAME_INPUT_NONE) {
        if (player->mode_frame < 1) {
            player->submode = input->horizontal % GAME_INPUT_BOTH;
            if (player->submode != GAME_INPUT_NONE) {
                player->mode_frame = 0;
                player->deflecting = false;
                M_Effect(update);
                player->sliding = true;
            }
        } else {
            if (player->mode_frame % 3 == 0) {
                const int16_t image = M_SwingImage(player->mode_frame + 1);
                M_Overlap(update, true, player->left, image - 1, image);
            }
            if (player->mode_frame == player->deflect_duration) {
                player->deflecting = false;
            }
            if (player->mode_frame >= 23) {
                player->mode = PLAYER_MODE_REGULAR;
                M_Draw(
                    update, PLAYER_ACTION_BOS_48_ERASE,
                    M_SwingImage(player->mode_frame));
                player->submode = GAME_INPUT_BOTH;
                player->mode_frame = 0;
                input->strike = false;
                player->bomb_frame = 0;
                player->invincible_against_orb = false;
            }
        }
    } else if (
        player->submode == GAME_INPUT_RIGHT
        || player->submode == GAME_INPUT_LEFT) {
        const bool right = player->submode == GAME_INPUT_RIGHT;
        const int16_t base = right ? 2 : 0;
        const int16_t previous_left = player->left;
        Player_MoveAndClamp(player, right ? 6 : -6);
        M_Overlap(
            update, false, previous_left, base + 1 - player->mode_frame % 2,
            base + player->mode_frame % 2);
        if (player->mode_frame >= 5 && !input->strike && !input->shot) {
            player->combo_enabled = true;
        }
        if (player->mode_frame >= 12) {
            player->mode = PLAYER_MODE_REGULAR;
            M_Draw(
                update, PLAYER_ACTION_BOS_32_ERASE,
                base + player->mode_frame % 2);
            player->submode = GAME_INPUT_BOTH;
            player->mode_frame = 0;
            player->sliding = false;
            const int8_t direction =
                right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
            if (player->combo_enabled && input->strike) {
                M_SelectStrike(update, direction);
                player->mode = PLAYER_MODE_SPECIAL_FIRST;
                M_SpecialStart(
                    update, player->submode < PLAYER_SPECIAL_SLIDEKICK);
                return;
            } else if (player->combo_enabled && input->shot) {
                player->submode = PLAYER_SPECIAL_SHOTCOMBO | direction;
                player->mode = PLAYER_MODE_SPECIAL_FIRST;
                M_SpecialStart(update, true);
                return;
            }
            input->strike = false;
            player->bomb_frame = 0;
            if (right) {
                player->combo_enabled = false;
            }
            player->invincible_against_orb = false;
        }
    }
    player->mode_frame = M_Wrap8(player->mode_frame + 1);
    if (player->submode == GAME_INPUT_NONE) {
        if (player->mode_frame < 21) {
            M_Repel(update, player->mode_frame);
        }
    } else if (player->submode == GAME_INPUT_RIGHT) {
        M_Repel(update, ORB_REPEL_3_X_RIGHT_4);
    } else if (player->submode == GAME_INPUT_LEFT) {
        M_Repel(update, ORB_REPEL_3_X_LEFT_4);
    }
}

static void M_Special(PLAYER_UPDATE *const update)
{
    PLAYER *const player = update->player;
    GAME_INPUT *const input = update->input;
    const bool right = (player->submode & 1) == PLAYER_FACING_RIGHT;
    const int16_t special = player->submode & ~1;
    const int16_t frame = player->mode_frame;
    int16_t image;
    int16_t duration;
    if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
        || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
        const int16_t base = right ? 14 : 8;
        const int16_t previous = M_FlipImage(frame - 1, base);
        image = M_FlipImage(frame, base);
        if (special == PLAYER_SPECIAL_FLIPKICK_MOVING) {
            const int16_t previous_left = player->left;
            Player_MoveAndClamp(player, right ? 2 : -2);
            M_Overlap(update, true, previous_left, previous, image);
        } else if (previous != image) {
            M_Overlap(update, true, player->left, previous, image);
        } else {
            M_Draw(update, PLAYER_ACTION_BOS_48_PUT, image);
        }
        if (frame == 20) {
            player->deflecting = false;
        }
        if (frame >= 20 && !input->strike && !input->shot) {
            player->combo_enabled = true;
        }
        duration = 28;
    } else if (special == PLAYER_SPECIAL_SLIDEKICK) {
        image = right ? 5 : 4;
        const int16_t previous_left = player->left;
        Player_MoveAndClamp(player, right ? 6 : -6);
        M_Overlap(update, false, previous_left, image, image);
        duration = 13;
    } else {
        const int16_t base = right ? 24 : 20;
        image = frame / 6 + base;
        M_Overlap(update, true, player->left, (frame - 1) / 6 + base, image);
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
            player->deflecting = false;
        }
        duration = 19;
    }
    player->mode_frame = M_Wrap8(player->mode_frame + 1);
    if (player->mode_frame > duration) {
        M_Draw(
            update,
            special == PLAYER_SPECIAL_SLIDEKICK ? PLAYER_ACTION_BOS_32_ERASE
                                                : PLAYER_ACTION_BOS_48_ERASE,
            image);
        player->bomb_frame = 0;
        player->mode_frame = 0;
        player->sliding = false;
        player->deflecting = false;
        if (player->mode < PLAYER_MODE_SPECIAL_THIRD) {
            if (player->combo_enabled && input->strike) {
                if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
                    || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
                    M_SelectStrike(
                        update,
                        right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT);
                }
                player->mode++;
                M_SpecialStart(
                    update, player->submode < PLAYER_SPECIAL_SLIDEKICK);
                return;
            } else if (player->combo_enabled && input->shot) {
                if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
                    || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
                    player->submode = PLAYER_SPECIAL_SHOTCOMBO
                        | (right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT);
                }
                player->mode++;
                M_SpecialStart(update, true);
                return;
            }
        }
        player->invincible_against_orb = false;
        player->mode = PLAYER_MODE_REGULAR;
    } else if (
        special == PLAYER_SPECIAL_FLIPKICK_MOVING
        || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
        M_Repel(update, player->mode_frame / 2);
    } else if (special == PLAYER_SPECIAL_SLIDEKICK) {
        M_Repel(update, right ? ORB_REPEL_3_X_RIGHT_8 : ORB_REPEL_3_X_LEFT_8);
    } else if (player->mode_frame < 16) {
        M_Repel(update, player->mode_frame);
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

void Player_Reset(
    PLAYER *const player, GAME_INPUT *const input, const int32_t difficulty)
{
    player->dash_cycle = 0;
    player->mode_frame = 0;
    player->mode = PLAYER_MODE_REGULAR;
    player->submode = -1;
    player->dash_direction = PLAYER_FACING_LEFT;
    player->bomb_flag = PLAYER_BOMB_NONE;
    player->bomb_damaging = false;
    player->sliding = false;
    player->deflecting = false;
    player->previous_ptn = M_LEFT_STILL;
    player->deflect_duration = difficulty == SETTINGS_DIFFICULTY_EASY ? 15
        : difficulty == SETTINGS_DIFFICULTY_HARD                      ? 10
        : difficulty == SETTINGS_DIFFICULTY_LUNATIC                   ? 8
                                                                      : 12;
    player->bombing = false;
    player->combo_enabled = false;
    input->bomb = false;
    player->invincible_against_orb = false;
}

void Player_MoveAndClamp(PLAYER *const player, const int16_t delta)
{
    player->left = Math_Wrap16(player->left + delta);
    if (player->left < 0) {
        player->left = 0;
    } else if (player->left >= 608) {
        player->left = 608;
    }
}

bool Player_BeginUpdate(
    PLAYER_BOMB_CONTINUATION *const continuation, PLAYER *const player,
    GAME_INPUT *const input, ORB *const orb, SHOTS *const shots,
    uint32_t *const rng, const PLAYER_OUTPUT output)
{
    ASSERT(!continuation->pending);
    PLAYER_UPDATE update = { player, input, orb, shots, rng, output };
    player->dash_cycle = M_Wrap8(player->dash_cycle + 1) & 7;
    if ((player->remaining_bombs != 0 || player->bombing)
        && player->bomb_flag >= PLAYER_BOMB_START) {
        if (player->bomb_flag == PLAYER_BOMB_START) {
            player->bomb_frame = 0;
            player->bomb_flag = PLAYER_BOMB_ACTIVE;
            player->deflecting = true;
            player->bombing = true;
            player->remaining_bombs = Math_Wrap16(player->remaining_bombs - 1);
            output.action(
                output.user,
                (PLAYER_ACTION) {
                    .kind = PLAYER_ACTION_HUD_BOMBS,
                    .previous_bombs = Math_Wrap16(player->remaining_bombs + 1),
                });
        }
        M_Repel(&update, 1);
        *continuation = (PLAYER_BOMB_CONTINUATION) {
            .player = player,
            .input = input,
            .output = output,
            .pending = true,
        };
        return true;
    } else if (
        input->bomb && player->mode != PLAYER_MODE_SPECIAL_FIRST
        && player->remaining_bombs != 0) {
        player->bomb_flag = PLAYER_BOMB_START;
        input->bomb = false;
        output.action(
            output.user,
            (PLAYER_ACTION) {
                .kind = PLAYER_ACTION_PELLET_SPEED_RAISE,
                .pellet_speed_delta = 0.025f,
            });
    } else if (player->mode == PLAYER_MODE_REGULAR) {
        M_Regular(&update);
    } else if (player->mode == PLAYER_MODE_SHOOT) {
        M_ShootMode(&update);
    } else if (player->mode == PLAYER_MODE_SWING_OR_SLIDE) {
        M_SwingOrSlide(&update);
    } else if (
        player->mode >= PLAYER_MODE_SPECIAL_FIRST
        && player->mode <= PLAYER_MODE_SPECIAL_THIRD) {
        M_Special(&update);
    }
    return false;
}

void Player_FinishBombUpdate(
    PLAYER_BOMB_CONTINUATION *const continuation, const bool done)
{
    ASSERT(continuation->pending);
    continuation->pending = false;
    PLAYER *const player = continuation->player;
    GAME_INPUT *const input = continuation->input;
    PLAYER_UPDATE update = { .player = player, .output = continuation->output };
    player->bomb_flag = done ? PLAYER_BOMB_NONE : PLAYER_BOMB_ACTIVE;
    if (done) {
        player->bombing = false;
        input->bomb = false;
        M_Draw(&update, PLAYER_ACTION_PTN_PUT, M_LEFT_STILL);
        player->previous_ptn = M_LEFT_STILL;
    } else if (player->bomb_frame > 60) {
        M_Draw(&update, PLAYER_ACTION_PTN_PUT, M_LEFT_CAST);
    }
    input->shot = false;
    input->strike = false;
    player->mode = PLAYER_MODE_REGULAR;
    if (player->bomb_flag == PLAYER_BOMB_NONE) {
        player->deflecting = false;
    }
}

void Player_UpdateAndRender(
    PLAYER *const player, GAME_INPUT *const input, ORB *const orb,
    SHOTS *const shots, uint32_t *const rng, const PLAYER_OUTPUT output)
{
    PLAYER_BOMB_CONTINUATION continuation = {};
    if (Player_BeginUpdate(
            &continuation, player, input, orb, shots, rng, output)) {
        const bool done = output.bomb_update(
            output.user, player, Math_Wrap16(player->bomb_frame & 0xffff));
        Player_FinishBombUpdate(&continuation, done);
    }
}

void Player_RenderAction(
    VIDEO *const video, const PLAYER_SPRITES *const sprites,
    const PTN *const stage, const PLAYER_ACTION action)
{
    switch (action.kind) {
    case PLAYER_ACTION_PTN_PUT:
        Sprite_PtnPut(
            video, sprites->miko, action.image, -1, action.left, PLAYER_TOP);
        break;
    case PLAYER_ACTION_PTN_ERASE:
        Sprite_PtnErase(
            video, sprites->miko, action.image, -1, action.left, PLAYER_TOP);
        break;
    case PLAYER_ACTION_BOS_48_PUT:
    case PLAYER_ACTION_BOS_32_PUT:
        Sprite_PlayerPut(
            video,
            action.kind == PLAYER_ACTION_BOS_48_PUT ? sprites->miko_ac
                                                    : sprites->miko_ac2,
            action.image, Math_Wrap16(action.left - 8),
            action.kind == PLAYER_ACTION_BOS_48_PUT ? 352 : PLAYER_TOP);
        break;
    case PLAYER_ACTION_BOS_48_ERASE:
    case PLAYER_ACTION_BOS_32_ERASE:
        Sprite_PlayerErase(
            video,
            action.kind == PLAYER_ACTION_BOS_48_ERASE ? sprites->miko_ac
                                                      : sprites->miko_ac2,
            action.image, Math_Wrap16(action.left - 8),
            action.kind == PLAYER_ACTION_BOS_48_ERASE ? 352 : PLAYER_TOP);
        break;
    case PLAYER_ACTION_BOS_48_OVERLAP:
    case PLAYER_ACTION_BOS_32_OVERLAP: {
        const bool tall = action.kind == PLAYER_ACTION_BOS_48_OVERLAP;
        Sprite_PlayerOverlap(
            video, tall ? sprites->miko_ac : sprites->miko_ac2, action.image,
            action.previous_image, Math_Wrap16(action.left - 8),
            tall ? 352 : PLAYER_TOP, Math_Wrap16(action.previous_left - 8),
            tall ? 352 : PLAYER_TOP);
        break;
    }
    case PLAYER_ACTION_SHOT:
        Shots_RenderAction(video, stage, action.shot);
        break;
    case PLAYER_ACTION_EFFECT:
    case PLAYER_ACTION_HUD_BOMBS:
    case PLAYER_ACTION_PELLET_SPEED_RAISE:
        break;
    }
}
