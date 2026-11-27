#include "th02/shots/fire.h"

#include "th02/input.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/shots/private.h"
#include "th02/sound_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

typedef struct {
    int16_t offset;
    uint8_t angle;
    uint8_t last;
    bool lift;
} BODY_SHOT;

typedef struct {
    int16_t offset;
    int16_t x;
    int16_t y;
    uint8_t last;
} OPTION_SHOT;

typedef struct {
    void (*prepare)(uint8_t level);
    BODY_SHOT (*body)(uint8_t index);
    bool (*option_ready)(uint8_t level);
    OPTION_SHOT (*option)(uint8_t level, uint8_t index);
} SHOT_STRATEGY;

static const int16_t m_OptionOffsets[] = { 0, 48, -8, 56, -16, 64 };

static void M_PrepareC(const uint8_t level)
{
    p_Shots->fire_cycle++;
    if (level >= 2) {
        p_Shots->weapon.parameter[0] = 124;
    }
}

static BODY_SHOT M_BodyA(const uint8_t i)
{
    SHOTS_WEAPON *const weapon = &p_Shots->weapon;
    const uint8_t level = weapon->level;
    BODY_SHOT shot = { 8, 0xc0, 0, false };
    if (level == 1) {
        shot.angle = Random_RingU8And(3) + 0xbe;
    } else if (level == 2 || level == 3) {
        shot.offset = i * 16;
        shot.last = 1;
        if (level == 2) {
            shot.angle = Random_RingU8And(7) + 0xbc;
        }
    } else if (level == 4 || level == 5) {
        const int spread = level == 5 ? 2 : 0;
        shot.last = 2;
        shot.angle = i == 0 ? 0xc0 : i == 1 ? 0xbb - spread : 0xc5 + spread;
    } else if (level == 6 || level == 7) {
        shot.last = 3;
        shot.offset = i < 2 ? i * 16 : 8;
        if (i >= 2) {
            shot.angle = level == 6 ? Random_RingU8And(31) + 0xb1
                : i == 2            ? 0xb8
                                    : 0xc8;
        }
    } else if (level == 8) {
        static const uint8_t angles[] = { 0xc0, 0xb8, 0xc8, 0xb0, 0xd0 };
        shot.last = 4;
        shot.angle = angles[i];
    } else if (level == 9) {
        static const int16_t offsets[] = { 0, 16, 8, 8, -24, 40 };
        shot.last = 5;
        shot.offset = offsets[i];
        if (i == 2) {
            weapon->parameter[0] = 52;
        } else if (i == 4) {
            weapon->parameter[0] = 54;
        }
        if (i >= 2) {
            shot.angle = i == 2 ? 0xb3
                : i == 3        ? 0xcd
                : i == 4        ? 0xc0 - weapon->spread
                                : 0xc0 + weapon->spread;
        }
    }
    return shot;
}

static BODY_SHOT M_BodyB(const uint8_t i)
{
    SHOTS_WEAPON *const weapon = &p_Shots->weapon;
    const uint8_t level = weapon->level;
    BODY_SHOT shot = { 8, 0xc0, 0, false };
    if (level == 1 || level == 2) {
        shot.angle = level == 1 ? Random_RingU8And(3) + 0xbe
                                : Random_RingU8And(7) + 0xbc;
    } else if (level >= 3 && level <= 5) {
        shot.last = 1;
        shot.offset = i * 16;
        if (level == 3) {
            shot.angle = Random_RingU8And(7) + 0xbc;
        } else if (level == 5) {
            shot.angle = i == 0 ? 0xbe : 0xc2;
        }
    } else if (level == 6 || level == 7) {
        const int spread = level == 7 ? 2 : 0;
        shot.last = 2;
        shot.angle = i == 0 ? 0xc0 : i == 1 ? 0xbb - spread : 0xc5 + spread;
    } else if (level == 8) {
        shot.last = 3;
        shot.offset = i < 2 ? i * 16 : 8;
        if (i >= 2) {
            shot.angle = Random_RingU8And(31) + 0xb1;
        }
    } else if (level == 9) {
        shot.last = 3;
        shot.offset = (i & 1) * 16;
        if (i == 2) {
            weapon->parameter[0] = 50;
        }
        if (i >= 2) {
            if (p_Shots->target.y > 0) {
                const int16_t y = Math16_Wrap(
                    (int32_t)p_Shots->target.y - g_Player->current.y - 32);
                const int16_t x = Math16_Wrap(
                    (int32_t)p_Shots->target.x - g_Player->current.x
                    - shot.offset);
                shot.angle = Math16_Angle(y, x) + Random_RingU8And(7) - 3;
            } else {
                shot.angle = i == 2 ? 0xb7 : 0xc9;
            }
        }
    }
    return shot;
}

static BODY_SHOT M_BodyC(const uint8_t i)
{
    SHOTS_WEAPON *const weapon = &p_Shots->weapon;
    const uint8_t level = weapon->level;
    BODY_SHOT shot = { 8, 0xc0, 0, false };
    if (level >= 3 && level <= 5) {
        shot.angle = Random_RingU8And(3) + 0xbe;
    } else if (level >= 6 && level <= 8) {
        shot.angle = Random_RingU8And(7) + 0xbc;
    } else if (level == 9) {
        shot.last = p_Shots->fire_cycle % 3 == 0 ? 4 : 0;
        if (i != 0) {
            if (i == 1) {
                weapon->parameter[0] = 49;
            }
            shot.offset = (i & 1) != 0 ? -16 : 32;
            shot.lift = i >= 3;
        }
    }
    return shot;
}

static bool M_OptionReadyB(const uint8_t level)
{
    if (level < 2) {
        return false;
    }
    p_Shots->unused_target_x = level == 9 ? p_Shots->target.x : -1;
    return p_Shots->option_active == 0;
}

static bool M_OptionReadyC(const uint8_t level)
{
    if (level < 2) {
        return false;
    }
    if (level == 2) {
        return p_Shots->option_active == 0;
    }
    if (level == 3) {
        return p_Shots->fire_cycle % 5 == 0;
    }
    if (level < 6) {
        return (p_Shots->fire_cycle & 3) == 0;
    }
    if (level < 9) {
        return p_Shots->fire_cycle % 3 == 0;
    }
    return (p_Shots->fire_cycle & 1) == 0;
}

static OPTION_SHOT M_OptionB(const uint8_t level, const uint8_t i)
{
    OPTION_SHOT shot = { m_OptionOffsets[i], 0, -4, 1 };
    if (level >= 8) {
        static const int16_t x[] = { 0, 0, -16, 16, -32, 32 };
        static const int16_t y[] = { -54, -54, -50, -50, -40, -40 };
        shot.x = x[i];
        shot.y = y[i];
        shot.last = 5;
    } else if (level >= 5) {
        static const int16_t x[] = { 0, 0, -3, 3 };
        shot.x = x[i];
        shot.y = i < 2 ? -16 : -6;
        shot.last = 3;
    }
    return shot;
}

static OPTION_SHOT M_OptionC(const uint8_t level, const uint8_t i)
{
    OPTION_SHOT shot = { m_OptionOffsets[i], 0, -4, 1 };
    if (level == 4) {
        shot.y = -16;
    } else if (level >= 5 && level <= 8) {
        static const int16_t x[][4] = { { 4, -4, 0, 0 },
                                        { 6, -6, 0, 0 },
                                        { 6, -6, 0, 0 },
                                        { 10, -10, -4, 4 } };
        static const int16_t y[] = { -16, -24, -26, -48 };
        shot.x = x[level - 5][i];
        shot.y = y[level - 5];
        shot.last = 3;
    } else if (level == 9) {
        static const int16_t x[] = { -10, 10, 20, -20, -7, 7 };
        static const int16_t y[] = { -160, -160, -140, -140, -120, -120 };
        shot.x = x[i];
        shot.y = y[i];
        shot.last = (p_Shots->fire_cycle & 3) == 0 ? 5 : 3;
    }
    return shot;
}

void Shots_ResetTarget(void)
{
    p_Shots->target = (ENTITY_POINT) { -1, -1 };
    p_Shots->unused_target_x = -1;
}

void Shots_SetWeaponLevel(const uint8_t level)
{
    p_Shots->weapon.level = level;
}

void Shots_SetTarget(const ENTITY_POINT target)
{
    p_Shots->target = target;
}

static void M_Parameters(SHOTS_WEAPON *const weapon)
{
    weapon->parameter[0] = weapon->level == 9 ? weapon->maximum[0] : 0x34;
    weapon->parameter[1] = weapon->level == 9 ? weapon->maximum[1] : 0x3f;
}

void Fire_Emit(const uint8_t back)
{
    const uint8_t type = g_Player->shot_type;
    ASSERT(type < 3 && back < 2 && p_Shots->weapon.level <= 9);
    static const SHOT_STRATEGY strategies[] = {
        { .body = M_BodyA },
        { .body = M_BodyB,
          .option_ready = M_OptionReadyB,
          .option = M_OptionB },
        { .prepare = M_PrepareC,
          .body = M_BodyC,
          .option_ready = M_OptionReadyC,
          .option = M_OptionC },
    };
    const SHOT_STRATEGY *const strategy = &strategies[type];
    const uint8_t level = p_Shots->weapon.level;
    if (strategy->prepare != nullptr) {
        strategy->prepare(level);
    }
    bool main_done = false;
    uint8_t i = 0;
    for (size_t slot = 0; slot < SHOT_COUNT; slot++) {
        SHOT *const shot = &p_Shots->slots[slot];
        if (shot->state != ENTITY_FREE) {
            continue;
        }
        if (!main_done) {
            const BODY_SHOT body = strategy->body(i);
            shot->state = ENTITY_ACTIVE;
            shot->age = 0;
            shot->position[back] = (ENTITY_POINT) {
                Math16_FromPixel(
                    Math16_Wrap(g_Player->current.x + body.offset)),
                Math16_FromPixel(Math16_Wrap(g_Player->current.y + 32))
            };
            if (body.lift) {
                shot->position[back].y =
                    Math16_Wrap(shot->position[back].y - 256);
            }
            const MATH16_VECTOR velocity = Math16_Vector(192, body.angle);
            shot->velocity = (ENTITY_POINT) { velocity.x, velocity.y };
            shot->sprite = p_Shots->weapon.parameter[0];
            shot->from_option = 0;
            if (i == body.last) {
                if (strategy->option == nullptr) {
                    break;
                }
                main_done = true;
                i = 0;
                continue;
            }
        } else {
            if (!strategy->option_ready(level)) {
                return;
            }
            const OPTION_SHOT option = strategy->option(level, i);
            shot->state = ENTITY_ACTIVE;
            shot->age = 1;
            shot->position[back] =
                (ENTITY_POINT) { Math16_FromPixel(Math16_Wrap(
                                     g_Player->option[back].x + option.offset)),
                                 Math16_FromPixel(g_Player->option[back].y) };
            shot->velocity = (ENTITY_POINT) { option.x, option.y };
            shot->sprite = p_Shots->weapon.parameter[1];
            shot->from_option = 1;
            if (i == option.last) {
                p_Shots->option_active = 1;
                return;
            }
        }
        i++;
    }
    if (strategy->option == nullptr) {
        p_Shots->option_active = 0;
    }
}

void Shots_Fire(const uint8_t back, const uint16_t keys)
{
    SHOTS_WEAPON *const weapon = &p_Shots->weapon;
    if ((keys & GAME_KEY_SHOT) != 0) {
        if (weapon->cycle[0] == 255) {
            weapon->cycle[0] = weapon->cooldown[0] = 0;
        } else if (weapon->cycle[1] == 255) {
            weapon->cycle[1] = weapon->cooldown[1] = 0;
        }
    } else if (weapon->cycle[0] >= 3) {
        weapon->cycle[0] = 255;
        weapon->cycle[1] = 4;
    } else if (weapon->cycle[1] >= 2) {
        weapon->cycle[1] = 255;
    }
    bool first_fired = false;
    if (weapon->cycle[0] != 255) {
        if (weapon->cooldown[0] == 0) {
            M_Parameters(weapon);
            Fire_Emit(back);
            SoundEffects_Play(SOUND_EFFECT_SHOT);
            weapon->cooldown[0] = 8;
            weapon->cycle[0]++;
            first_fired = true;
        } else {
            weapon->cooldown[0]--;
        }
    }
    if (!first_fired && weapon->cycle[1] < 2) {
        if (weapon->cooldown[1] == 0) {
            M_Parameters(weapon);
            if (weapon->level == 9) {
                if ((keys
                     & (GAME_KEY_DOWN | GAME_KEY_DOWN_LEFT
                        | GAME_KEY_DOWN_RIGHT))
                    != 0) {
                    weapon->spread = (int8_t)(uint8_t)(weapon->spread + 5);
                    if (weapon->spread > 26) {
                        weapon->spread = 26;
                    }
                } else if ((keys & 0xf00f) == 0) {
                    weapon->spread = (int8_t)(uint8_t)(weapon->spread - 5);
                    if (weapon->spread < 0) {
                        weapon->spread = 0;
                    }
                }
            }
            Fire_Emit(back);
            weapon->cooldown[1] = 8;
            weapon->cycle[1]++;
        } else {
            SoundEffects_Play(SOUND_EFFECT_SHOT);
            weapon->cooldown[1]--;
        }
    }
}
