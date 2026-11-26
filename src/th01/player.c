#include "th01/player.h"
#include "th01/hud.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/shots.h"
#include "th01/sound.h"

#include "pc98/zun/math16.h"

#include "core/base/assert.h"

#include <string.h>

static PLAYER m_Player;
const PLAYER *const g_Player = &m_Player;

void Player_Init(void)
{
    memset(&m_Player, 0, sizeof(m_Player));
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

static int8_t M_Wrap8(const int32_t value)
{
    const uint8_t low = (uint8_t)value;
    return low <= INT8_MAX ? (int8_t)low : (int8_t)((int16_t)low - 256);
}

enum {
    M_PUT,
    M_ERASE,
};

static void M_Draw(
    const PLAYER_SPRITES *const sprites, const int32_t kind,
    const int16_t image)
{
    if (kind == M_PUT) {
        Sprite_PtnPut(sprites->miko, image, -1, m_Player.left, PLAYER_TOP);
    } else {
        Sprite_PtnErase(sprites->miko, image, -1, m_Player.left, PLAYER_TOP);
    }
}

static void M_Overlap(
    const PLAYER_SPRITES *const sprites, const bool tall,
    const int16_t previous_left, const int16_t previous_image,
    const int16_t image)
{
    Sprite_PlayerOverlap(
        tall ? sprites->miko_ac : sprites->miko_ac2, image, previous_image,
        Math16_Wrap(m_Player.left - 8), tall ? 352 : PLAYER_TOP,
        Math16_Wrap(previous_left - 8), tall ? 352 : PLAYER_TOP);
}

static void M_BosDraw(
    const PLAYER_SPRITES *const sprites, const bool tall, const int32_t kind,
    const int16_t image)
{
    const BOS *const bos = tall ? sprites->miko_ac : sprites->miko_ac2;
    const int16_t left = Math16_Wrap(m_Player.left - 8);
    const int16_t top = tall ? 352 : PLAYER_TOP;
    if (kind == M_PUT) {
        Sprite_PlayerPut(bos, image, left, top);
    } else {
        Sprite_PlayerErase(bos, image, left, top);
    }
}

static void M_ActionSound(void)
{
    Sound_Effect(SOUND_EFFECT_STRIKE);
}

static void M_Repel(const int16_t friction)
{
    const ORB_PLAYER contact = {
        m_Player.left,
        PLAYER_TOP,
        m_Player.invincible,
        m_Player.invincible_against_orb,
    };
    if (Orb_PlayerHitTest(&contact, friction)) {
        m_Player.is_hit = true;
    }
}

static void M_Shoot(const int16_t offset)
{
    Shots_Add(Math16_Wrap(m_Player.left + 8 + offset), PLAYER_TOP);
}

static int16_t M_DashImage(const int16_t base)
{
    return base + (m_Player.dash_cycle % 8 < 4 ? 0 : 1);
}

static int16_t M_SwingImage(const int16_t frame)
{
    return frame >= 23 ? 7 : frame / 3;
}

static int16_t M_FlipImage(const int16_t frame, const int16_t base)
{
    return frame < 4 ? base : (frame - 7) / 4 + base;
}

static void M_SpecialStart(const bool deflect)
{
    if (deflect) {
        m_Player.deflecting = true;
    }
    M_ActionSound();
    m_Player.combo_enabled = false;
}

static void M_SelectStrike(const int8_t direction)
{
    const int8_t same =
        direction == PLAYER_FACING_RIGHT ? GAME_INPUT_RIGHT : GAME_INPUT_LEFT;
    const int8_t other =
        direction == PLAYER_FACING_RIGHT ? GAME_INPUT_LEFT : GAME_INPUT_RIGHT;
    m_Player.submode =
        (g_GameInput->horizontal == other ? PLAYER_SPECIAL_FLIPKICK_STATIONARY
             : g_GameInput->horizontal == same ? PLAYER_SPECIAL_SLIDEKICK
                                               : PLAYER_SPECIAL_FLIPKICK_MOVING)
        | direction;
}

static void M_RegularMode(const PLAYER_SPRITES *const sprites)
{
    int16_t image;
    if (g_GameInput->horizontal == GAME_INPUT_RIGHT
        || g_GameInput->horizontal == GAME_INPUT_LEFT) {
        const bool right = g_GameInput->horizontal == GAME_INPUT_RIGHT;
        m_Player.dash_direction =
            right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
        M_Draw(sprites, M_ERASE, m_Player.previous_ptn);
        Player_MoveAndClamp(right ? 4 : -4);
        image = M_DashImage(right ? M_RIGHT_DASH : M_LEFT_DASH);
        M_Draw(sprites, M_PUT, image);
        m_Player.previous_ptn = image;
        m_Player.submode = g_GameInput->horizontal;
    } else {
        if (m_Player.submode == GAME_INPUT_NONE) {
            image = m_Player.dash_direction != PLAYER_FACING_RIGHT
                ? M_LEFT_STILL
                : M_RIGHT_STILL;
            if (m_Player.previous_ptn != image) {
                M_Draw(sprites, M_ERASE, m_Player.previous_ptn);
            }
            M_Draw(sprites, M_PUT, image);
            m_Player.previous_ptn = image;
        }
        m_Player.submode = GAME_INPUT_NONE;
    }
    if (g_GameInput->shot) {
        M_Draw(sprites, M_ERASE, m_Player.previous_ptn);
        m_Player.mode = PLAYER_MODE_SHOOT;
        m_Player.mode_frame = 0;
        m_Player.submode = g_GameInput->horizontal == GAME_INPUT_BOTH
            ? GAME_INPUT_NONE
            : g_GameInput->horizontal;
        if (m_Player.submode == GAME_INPUT_NONE) {
            image = m_Player.dash_direction != PLAYER_FACING_RIGHT
                ? M_LEFT_CAST
                : M_RIGHT_CAST;
        } else {
            image = m_Player.submode == GAME_INPUT_RIGHT ? M_RIGHT_SHOOT
                                                         : M_LEFT_SHOOT;
        }
        M_Draw(sprites, M_PUT, image);
        m_Player.previous_ptn = image;
    }
    if (g_GameInput->strike) {
        M_Draw(sprites, M_ERASE, m_Player.previous_ptn);
        if (g_GameInput->horizontal == GAME_INPUT_NONE
            || g_GameInput->horizontal == GAME_INPUT_BOTH) {
            m_Player.mode_frame = 0;
            m_Player.mode = PLAYER_MODE_SWING_OR_SLIDE;
            m_Player.submode = GAME_INPUT_NONE;
            m_Player.deflecting = true;
        } else {
            M_ActionSound();
            m_Player.mode_frame = 0;
            m_Player.mode = PLAYER_MODE_SWING_OR_SLIDE;
            m_Player.submode = g_GameInput->horizontal % GAME_INPUT_BOTH;
            m_Player.sliding = true;
            m_Player.combo_enabled = false;
            M_BosDraw(
                sprites, false, M_PUT,
                m_Player.submode == GAME_INPUT_RIGHT ? 3 : 1);
        }
        m_Player.invincible_against_orb = true;
    }
}

static void M_ShootMode(const PLAYER_SPRITES *const sprites)
{
    if (m_Player.submode == GAME_INPUT_NONE) {
        if (m_Player.mode_frame == 0) {
            M_Shoot(0);
        } else if (m_Player.mode_frame >= 2) {
            m_Player.bomb_frame = 0;
            m_Player.mode = PLAYER_MODE_REGULAR;
            GameInput_Consume(GAME_INPUT_PRESS_SHOT);
            m_Player.submode = GAME_INPUT_BOTH;
        }
    } else if (
        m_Player.submode == GAME_INPUT_RIGHT
        || m_Player.submode == GAME_INPUT_LEFT) {
        const bool right = m_Player.submode == GAME_INPUT_RIGHT;
        m_Player.dash_direction =
            right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
        M_Draw(sprites, M_ERASE, m_Player.previous_ptn);
        Player_MoveAndClamp(right ? 4 : -4);
        const int16_t base = right ? M_RIGHT_SHOOT : M_LEFT_SHOOT;
        M_Draw(sprites, M_PUT, base + (m_Player.mode_frame & 3));
        m_Player.previous_ptn =
            base + (right ? m_Player.mode_frame % 3 : m_Player.mode_frame & 3);
        if (m_Player.mode_frame >= 1) {
            M_Shoot(0);
            m_Player.bomb_frame = 0;
            GameInput_Consume(GAME_INPUT_PRESS_SHOT);
            m_Player.mode = PLAYER_MODE_REGULAR;
        }
    }
    m_Player.mode_frame = M_Wrap8(m_Player.mode_frame + 1);
}

static void M_SwingOrSlide(const PLAYER_SPRITES *const sprites)
{
    if (m_Player.submode == GAME_INPUT_NONE) {
        if (m_Player.mode_frame < 1) {
            m_Player.submode = g_GameInput->horizontal % GAME_INPUT_BOTH;
            if (m_Player.submode != GAME_INPUT_NONE) {
                m_Player.mode_frame = 0;
                m_Player.deflecting = false;
                M_ActionSound();
                m_Player.sliding = true;
            }
        } else {
            if (m_Player.mode_frame % 3 == 0) {
                const int16_t image = M_SwingImage(m_Player.mode_frame + 1);
                M_Overlap(sprites, true, m_Player.left, image - 1, image);
            }
            if (m_Player.mode_frame == m_Player.deflect_duration) {
                m_Player.deflecting = false;
            }
            if (m_Player.mode_frame >= 23) {
                m_Player.mode = PLAYER_MODE_REGULAR;
                M_BosDraw(
                    sprites, true, M_ERASE, M_SwingImage(m_Player.mode_frame));
                m_Player.submode = GAME_INPUT_BOTH;
                m_Player.mode_frame = 0;
                GameInput_Consume(GAME_INPUT_PRESS_STRIKE);
                m_Player.bomb_frame = 0;
                m_Player.invincible_against_orb = false;
            }
        }
    } else if (
        m_Player.submode == GAME_INPUT_RIGHT
        || m_Player.submode == GAME_INPUT_LEFT) {
        const bool right = m_Player.submode == GAME_INPUT_RIGHT;
        const int16_t base = right ? 2 : 0;
        const int16_t previous_left = m_Player.left;
        Player_MoveAndClamp(right ? 6 : -6);
        M_Overlap(
            sprites, false, previous_left, base + 1 - m_Player.mode_frame % 2,
            base + m_Player.mode_frame % 2);
        if (m_Player.mode_frame >= 5 && !g_GameInput->strike
            && !g_GameInput->shot) {
            m_Player.combo_enabled = true;
        }
        if (m_Player.mode_frame >= 12) {
            m_Player.mode = PLAYER_MODE_REGULAR;
            M_BosDraw(sprites, false, M_ERASE, base + m_Player.mode_frame % 2);
            m_Player.submode = GAME_INPUT_BOTH;
            m_Player.mode_frame = 0;
            m_Player.sliding = false;
            const int8_t direction =
                right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT;
            if (m_Player.combo_enabled && g_GameInput->strike) {
                M_SelectStrike(direction);
                m_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
                M_SpecialStart(m_Player.submode < PLAYER_SPECIAL_SLIDEKICK);
                return;
            } else if (m_Player.combo_enabled && g_GameInput->shot) {
                m_Player.submode = PLAYER_SPECIAL_SHOTCOMBO | direction;
                m_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
                M_SpecialStart(true);
                return;
            }
            GameInput_Consume(GAME_INPUT_PRESS_STRIKE);
            m_Player.bomb_frame = 0;
            if (right) {
                m_Player.combo_enabled = false;
            }
            m_Player.invincible_against_orb = false;
        }
    }
    m_Player.mode_frame = M_Wrap8(m_Player.mode_frame + 1);
    if (m_Player.submode == GAME_INPUT_NONE) {
        if (m_Player.mode_frame < 21) {
            M_Repel(m_Player.mode_frame);
        }
    } else if (m_Player.submode == GAME_INPUT_RIGHT) {
        M_Repel(ORB_REPEL_3_X_RIGHT_4);
    } else if (m_Player.submode == GAME_INPUT_LEFT) {
        M_Repel(ORB_REPEL_3_X_LEFT_4);
    }
}

static void M_Special(const PLAYER_SPRITES *const sprites)
{
    const bool right = (m_Player.submode & 1) == PLAYER_FACING_RIGHT;
    const int16_t special = m_Player.submode & ~1;
    const int16_t frame = m_Player.mode_frame;
    int16_t image;
    int16_t duration;
    if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
        || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
        const int16_t base = right ? 14 : 8;
        const int16_t previous = M_FlipImage(frame - 1, base);
        image = M_FlipImage(frame, base);
        if (special == PLAYER_SPECIAL_FLIPKICK_MOVING) {
            const int16_t previous_left = m_Player.left;
            Player_MoveAndClamp(right ? 2 : -2);
            M_Overlap(sprites, true, previous_left, previous, image);
        } else if (previous != image) {
            M_Overlap(sprites, true, m_Player.left, previous, image);
        } else {
            M_BosDraw(sprites, true, M_PUT, image);
        }
        if (frame == 20) {
            m_Player.deflecting = false;
        }
        if (frame >= 20 && !g_GameInput->strike && !g_GameInput->shot) {
            m_Player.combo_enabled = true;
        }
        duration = 28;
    } else if (special == PLAYER_SPECIAL_SLIDEKICK) {
        image = right ? 5 : 4;
        const int16_t previous_left = m_Player.left;
        Player_MoveAndClamp(right ? 6 : -6);
        M_Overlap(sprites, false, previous_left, image, image);
        duration = 13;
    } else {
        const int16_t base = right ? 24 : 20;
        image = frame / 6 + base;
        M_Overlap(sprites, true, m_Player.left, (frame - 1) / 6 + base, image);
        switch (frame) {
        case 1:
            M_Shoot(-8);
            break;
        case 4:
            M_Shoot(8);
            break;
        case 7:
            M_Shoot(0);
            break;
        case 10:
            M_Shoot(-16);
            break;
        case 13:
            M_Shoot(0);
            break;
        case 16:
            M_Shoot(16);
            break;
        case 19:
            M_Shoot(0);
            break;
        default:
            break;
        }
        if (frame == 4) {
            m_Player.deflecting = false;
        }
        duration = 19;
    }
    m_Player.mode_frame = M_Wrap8(m_Player.mode_frame + 1);
    if (m_Player.mode_frame > duration) {
        M_BosDraw(sprites, special != PLAYER_SPECIAL_SLIDEKICK, M_ERASE, image);
        m_Player.bomb_frame = 0;
        m_Player.mode_frame = 0;
        m_Player.sliding = false;
        m_Player.deflecting = false;
        if (m_Player.mode < PLAYER_MODE_SPECIAL_THIRD) {
            if (m_Player.combo_enabled && g_GameInput->strike) {
                if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
                    || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
                    M_SelectStrike(
                        right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT);
                }
                m_Player.mode++;
                M_SpecialStart(m_Player.submode < PLAYER_SPECIAL_SLIDEKICK);
                return;
            } else if (m_Player.combo_enabled && g_GameInput->shot) {
                if (special == PLAYER_SPECIAL_FLIPKICK_MOVING
                    || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
                    m_Player.submode = PLAYER_SPECIAL_SHOTCOMBO
                        | (right ? PLAYER_FACING_RIGHT : PLAYER_FACING_LEFT);
                }
                m_Player.mode++;
                M_SpecialStart(true);
                return;
            }
        }
        m_Player.invincible_against_orb = false;
        m_Player.mode = PLAYER_MODE_REGULAR;
    } else if (
        special == PLAYER_SPECIAL_FLIPKICK_MOVING
        || special == PLAYER_SPECIAL_FLIPKICK_STATIONARY) {
        M_Repel(m_Player.mode_frame / 2);
    } else if (special == PLAYER_SPECIAL_SLIDEKICK) {
        M_Repel(right ? ORB_REPEL_3_X_RIGHT_8 : ORB_REPEL_3_X_LEFT_8);
    } else if (m_Player.mode_frame < 16) {
        M_Repel(m_Player.mode_frame);
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
    m_Player.dash_cycle = 0;
    m_Player.mode_frame = 0;
    m_Player.mode = PLAYER_MODE_REGULAR;
    m_Player.submode = -1;
    m_Player.dash_direction = PLAYER_FACING_LEFT;
    m_Player.bomb_flag = PLAYER_BOMB_NONE;
    m_Player.bomb_damaging = false;
    m_Player.sliding = false;
    m_Player.deflecting = false;
    m_Player.previous_ptn = M_LEFT_STILL;
    m_Player.deflect_duration = difficulty == SETTINGS_DIFFICULTY_EASY ? 15
        : difficulty == SETTINGS_DIFFICULTY_HARD                       ? 10
        : difficulty == SETTINGS_DIFFICULTY_LUNATIC                    ? 8
                                                                       : 12;
    m_Player.bombing = false;
    m_Player.combo_enabled = false;
    GameInput_Consume(GAME_INPUT_PRESS_BOMB);
    m_Player.invincible_against_orb = false;
}

void Player_MoveAndClamp(const int16_t delta)
{
    m_Player.left = Math16_Wrap(m_Player.left + delta);
    if (m_Player.left < 0) {
        m_Player.left = 0;
    } else if (m_Player.left >= 608) {
        m_Player.left = 608;
    }
}

void Player_Hit(void)
{
    m_Player.is_hit = true;
}

void Player_ClearHit(void)
{
    m_Player.is_hit = false;
}

void Player_EndInvincibility(void)
{
    m_Player.invincible = false;
    m_Player.invincibility_time = 0;
}

void Player_TickInvincibility(void)
{
    if (m_Player.invincibility_time > 1) {
        m_Player.invincibility_time--;
        m_Player.invincible = true;
    } else if (m_Player.invincibility_time == 1) {
        Player_EndInvincibility();
    }
}

void Player_EndBomb(void)
{
    m_Player.invincibility_time = 100;
    m_Player.bomb_damaging = false;
}

void Player_StopActions(void)
{
    m_Player.deflecting = false;
    m_Player.bomb_damaging = false;
}

void Player_Center(void)
{
    m_Player.left = 304;
}

void Player_MoveTo(const int16_t left)
{
    m_Player.left = left;
}

void Player_BecomeInvincible(void)
{
    m_Player.invincible = true;
}

void Player_ResumeInvincibility(void)
{
    if (m_Player.invincibility_time > 1) {
        m_Player.invincible = true;
    }
}

void Player_GrantInvincibility(const int16_t frames)
{
    m_Player.invincibility_time = frames;
}

void Player_StartBombDamage(void)
{
    m_Player.bomb_damaging = true;
}

void Player_SetBombFrame(const uint32_t frame)
{
    m_Player.bomb_frame = frame;
}

void Player_AdvanceBombFrame(void)
{
    m_Player.bomb_frame++;
}

void Player_SetBombs(const int16_t count)
{
    m_Player.remaining_bombs = count;
}

void Player_GainBomb(void)
{
    m_Player.remaining_bombs++;
}

bool Player_BeginUpdate(
    PLAYER_BOMB_CONTINUATION *const continuation,
    const PLAYER_SPRITES *const sprites)
{
    ASSERT(!continuation->pending);
    m_Player.dash_cycle = M_Wrap8(m_Player.dash_cycle + 1) & 7;
    if ((m_Player.remaining_bombs != 0 || m_Player.bombing)
        && m_Player.bomb_flag >= PLAYER_BOMB_START) {
        if (m_Player.bomb_flag == PLAYER_BOMB_START) {
            m_Player.bomb_frame = 0;
            m_Player.bomb_flag = PLAYER_BOMB_ACTIVE;
            m_Player.deflecting = true;
            m_Player.bombing = true;
            const int16_t previous_bombs = m_Player.remaining_bombs;
            m_Player.remaining_bombs = Math16_Wrap(previous_bombs - 1);
            Hud_BombsPut(previous_bombs);
        }
        M_Repel(1);
        *continuation = (PLAYER_BOMB_CONTINUATION) { .pending = true };
        return true;
    } else if (
        g_GameInput->bomb && m_Player.mode != PLAYER_MODE_SPECIAL_FIRST
        && m_Player.remaining_bombs != 0) {
        m_Player.bomb_flag = PLAYER_BOMB_START;
        GameInput_Consume(GAME_INPUT_PRESS_BOMB);
        Pellets_SpeedRaise(1);
    } else if (m_Player.mode == PLAYER_MODE_REGULAR) {
        M_RegularMode(sprites);
    } else if (m_Player.mode == PLAYER_MODE_SHOOT) {
        M_ShootMode(sprites);
    } else if (m_Player.mode == PLAYER_MODE_SWING_OR_SLIDE) {
        M_SwingOrSlide(sprites);
    } else if (
        m_Player.mode >= PLAYER_MODE_SPECIAL_FIRST
        && m_Player.mode <= PLAYER_MODE_SPECIAL_THIRD) {
        M_Special(sprites);
    }
    return false;
}

void Player_FinishBombUpdate(
    PLAYER_BOMB_CONTINUATION *const continuation,
    const PLAYER_SPRITES *const sprites, const bool done)
{
    ASSERT(continuation->pending);
    continuation->pending = false;

    m_Player.bomb_flag = done ? PLAYER_BOMB_NONE : PLAYER_BOMB_ACTIVE;
    if (done) {
        m_Player.bombing = false;
        GameInput_Consume(GAME_INPUT_PRESS_BOMB);
        M_Draw(sprites, M_PUT, M_LEFT_STILL);
        m_Player.previous_ptn = M_LEFT_STILL;
    } else if (m_Player.bomb_frame > 60) {
        M_Draw(sprites, M_PUT, M_LEFT_CAST);
    }
    GameInput_Consume(GAME_INPUT_PRESS_SHOT);
    GameInput_Consume(GAME_INPUT_PRESS_STRIKE);
    m_Player.mode = PLAYER_MODE_REGULAR;
    if (m_Player.bomb_flag == PLAYER_BOMB_NONE) {
        m_Player.deflecting = false;
    }
}
