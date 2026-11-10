#include "th06/player.h"

#include "th06/angle.h"
#include "th06/anm.h"
#include "th06/bomb.h"
#include "th06/bullet.h"
#include "th06/chain.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/scene.h"
#include "th06/timer.h"

#include "core/base/utils.h"
#include "core/math/x87.h"

#define M_CALC_PRIORITY 7
#define M_DRAW_PRIORITY 5
#define M_DRAW2_PRIORITY 7
#define M_ANM_SLOT 5
#define M_FIRST_SPRITE 0x400
#define M_SCRIPT_STILL 0x400
#define M_SCRIPT_TURN_LEFT 0x401
#define M_SCRIPT_FROM_LEFT 0x402
#define M_SCRIPT_TURN_RIGHT 0x403
#define M_SCRIPT_FROM_RIGHT 0x404
#define M_SCRIPT_OPTION 0x480
#define M_SCRIPT_HIT_OFFSET 0x20
#define M_POWER_LEVEL_COUNT 9
#define M_OPTIONS_MIN_POWER 8
#define M_OPTION_SLIDE_FRAMES 8
#define M_FIRE_FRAMES 30
#define M_DEATH_FRAMES 30
#define M_RESPAWN_FRAMES 30
#define M_INVULNERABLE_FRAMES 240
#define M_START_BOMB_WINDOW 8
#define M_RESPAWN_BOMB_WINDOW 6
#define M_RESPAWN_CLEAR_FRAMES 90
#define M_GRAZE_MARGIN 20.0f
#define M_LASER_GRAZE_MARGIN 48.0f
#define M_HALF_PI 1.5707964f
#define M_HOMING_FRAMES 40
#define M_MAX_SHOT_SPEED 10.0f
#define M_NO_TARGET -100.0f
#define M_EFFECT_SHOT_HIT ((EFFECT_TYPE)(EFFECT_PARTICLE + 1))
#define M_EFFECT_DEATH ((EFFECT_TYPE)(EFFECT_PARTICLE + 2))
#define M_COLOR_WHITE 0xffffffffu
#define M_COLOR_SHOCKWAVE 0xff4040ffu

// Holds one shot of a power level. A shot fires on the frames of the
// burst where frame % interval is phase. A laser instead fires when its
// entry phase of laser_timers is 0, and sets it to interval.
typedef struct {
    int16_t interval;
    int16_t phase;
    float offset_x, offset_y; // from the player, or from the option
    float size_x, size_y;
    float angle;
    float speed;
    int16_t damage;
    uint8_t option; // 0 for the player, else the option from 1
    uint8_t type; // PLAYER_BULLET_TYPE
    int16_t script;
} M_SHOT;

// Holds the shots of a power level, which lasts while the power is below
// max_power.
typedef struct {
    int32_t count;
    int32_t max_power;
    const M_SHOT *shots;
} M_SHOT_TABLE;

// Tells M_FireShots what happened to a shot of the power level.
typedef enum {
    M_FIRE_FIRED = 0,
    M_FIRE_FIRED_LAST = 1,
    M_FIRE_SKIPPED = -1,
    M_FIRE_SKIPPED_LAST = -2,
} M_FIRE;

typedef struct {
    float speed;
    float speed_focused;
} M_SPEEDS;

static const M_SHOT m_Shots0_0[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 48, 0, 0, 0x440 },
};

static const M_SHOT m_Shots0_1[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 48, 0, 0, 0x440 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -2.0943952f, 10.0f, 14, 1, 1, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.0471976f, 10.0f, 14, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_2[] = {
    { 5, 0, -4.0f, 0.0f, 12.0f, 12.0f, -1.5882497f, 12.0f, 30, 0, 0, 0x440 },
    { 5, 0, 4.0f, 0.0f, 12.0f, 12.0f, -1.553343f, 12.0f, 30, 0, 0, 0x440 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -2.0943952f, 10.0f, 14, 1, 1, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.0471976f, 10.0f, 14, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_3[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6755161f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 30, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4660766f, 12.0f, 24, 0, 0, 0x440 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -2.0943952f, 10.0f, 14, 1, 1, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.0471976f, 10.0f, 14, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_4[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 30, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 24, 0, 0, 0x440 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -2.0943952f, 10.0f, 12, 1, 1, 0x441 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.0471976f, 10.0f, 12, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_5[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 29, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 24, 0, 0, 0x440 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -2.0943952f, 10.0f, 9, 1, 1, 0x441 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.0471976f, 10.0f, 9, 2, 1, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -2.6179938f, 10.0f, 12, 1, 1, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -0.5235988f, 10.0f, 12, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_6[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 28, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 24, 0, 0, 0x440 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.9198623f, 10.0f, 10, 1, 1, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.2217305f, 10.0f, 10, 2, 1, 0x441 },
    { 30, 10, 0.0f, 0.0f, 12.0f, 12.0f, -2.268928f, 10.0f, 9, 1, 1, 0x441 },
    { 30, 10, 0.0f, 0.0f, 12.0f, 12.0f, -0.87266463f, 10.0f, 9, 2, 1, 0x441 },
    { 30, 20, 0.0f, 0.0f, 12.0f, 12.0f, -2.6179938f, 10.0f, 11, 1, 1, 0x441 },
    { 30, 20, 0.0f, 0.0f, 12.0f, 12.0f, -0.5235988f, 10.0f, 11, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_7[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 28, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 24, 0, 0, 0x440 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.9198623f, 10.0f, 8, 1, 1, 0x441 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.2217305f, 10.0f, 8, 2, 1, 0x441 },
    { 15, 5, 0.0f, 0.0f, 12.0f, 12.0f, -2.268928f, 10.0f, 8, 1, 1, 0x441 },
    { 15, 5, 0.0f, 0.0f, 12.0f, 12.0f, -0.87266463f, 10.0f, 8, 2, 1, 0x441 },
    { 15, 10, 0.0f, 0.0f, 12.0f, 12.0f, -2.6179938f, 10.0f, 8, 1, 1, 0x441 },
    { 15, 10, 0.0f, 0.0f, 12.0f, 12.0f, -0.5235988f, 10.0f, 8, 2, 1, 0x441 },
};

static const M_SHOT m_Shots0_8[] = {
    { 5, 0, -8.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 23, 0, 0, 0x440 },
    { 5, 0, -8.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 8.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 24, 0, 0, 0x440 },
    { 5, 0, 8.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 23, 0, 0, 0x440 },
    { 16, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.9198623f, 10.0f, 10, 1, 1, 0x441 },
    { 16, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.2217305f, 10.0f, 10, 2, 1, 0x441 },
    { 16, 4, 0.0f, 0.0f, 12.0f, 12.0f, -2.268928f, 10.0f, 8, 1, 1, 0x441 },
    { 16, 4, 0.0f, 0.0f, 12.0f, 12.0f, -0.87266463f, 10.0f, 8, 2, 1, 0x441 },
    { 16, 8, 0.0f, 0.0f, 12.0f, 12.0f, -2.6179938f, 10.0f, 7, 1, 1, 0x441 },
    { 16, 8, 0.0f, 0.0f, 12.0f, 12.0f, -0.5235988f, 10.0f, 7, 2, 1, 0x441 },
    { 16, 12, 0.0f, 0.0f, 12.0f, 12.0f, -2.9670599f, 10.0f, 10, 1, 1, 0x441 },
    { 16, 12, 0.0f, 0.0f, 12.0f, 12.0f, -0.17453289f, 10.0f, 10, 2, 1, 0x441 },
};

static const M_SHOT m_Shots1_0[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 48, 0, 0, 0x440 },
};

static const M_SHOT m_Shots1_1[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 48, 0, 0, 0x440 },
    { 15, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 1, 0, 0x442 },
    { 15, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_2[] = {
    { 5, 0, -4.0f, 0.0f, 12.0f, 12.0f, -1.5882497f, 12.0f, 32, 0, 0, 0x440 },
    { 5, 0, 4.0f, 0.0f, 12.0f, 12.0f, -1.553343f, 12.0f, 32, 0, 0, 0x440 },
    { 10, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 1, 0, 0x442 },
    { 10, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_3[] = {
    { 5, 0, -4.0f, 0.0f, 12.0f, 12.0f, -1.5882497f, 12.0f, 30, 0, 0, 0x440 },
    { 5, 0, 4.0f, 0.0f, 12.0f, 12.0f, -1.553343f, 12.0f, 30, 0, 0, 0x440 },
    { 8, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 1, 0, 0x442 },
    { 8, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_4[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 28, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 20, 0, 0, 0x440 },
    { 8, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 1, 0, 0x442 },
    { 8, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_5[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6929694f, 12.0f, 16, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 27, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4486233f, 12.0f, 16, 0, 0, 0x440 },
    { 5, 0, 8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 1, 0, 0x442 },
    { 5, 0, 8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 12, 2, 0, 0x442 },
    { 8, 0, -8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 8, 0, -8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_6[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 12.0f, 16, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 22, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 12.0f, 16, 0, 0, 0x440 },
    { 3, 0, 8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 3, 0, 8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
    { 5, 0, -8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 5, 0, -8.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_7[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.850049f, 12.0f, 9, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 12.0f, 17, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 12.0f, 17, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.2915437f, 12.0f, 9, 0, 0, 0x440 },
    { 3, 0, 12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 3, 0, 12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
    { 5, 0, -12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 5, 0, -12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
    { 10, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 10, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
};

static const M_SHOT m_Shots1_8[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.850049f, 12.0f, 9, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 12.0f, 17, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 12.0f, 17, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.2915437f, 12.0f, 9, 0, 0, 0x440 },
    { 3, 0, 12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 3, 0, 12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
    { 3, 0, -12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 3, 0, -12.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
    { 5, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 1, 0, 0x442 },
    { 5, 0, 0.0f, -16.0f, 12.0f, 40.0f, -1.5707964f, 22.0f, 10, 2, 0, 0x442 },
};

static const M_SHOT m_Shots2_0[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 48, 0, 0, 0x440 },
};

static const M_SHOT m_Shots2_1[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 36, 0, 0, 0x440 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 3.0f, 18, 1, 2, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 3.0f, 18, 2, 2, 0x441 },
};

static const M_SHOT m_Shots2_2[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 32, 0, 0, 0x440 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 16, 1, 2, 0x441 },
    { 30, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 16, 2, 2, 0x441 },
    { 30, 15, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 10, 1, 2, 0x441 },
    { 30, 15, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 10, 2, 2, 0x441 },
};

static const M_SHOT m_Shots2_3[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 32, 0, 0, 0x440 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 15, 1, 2, 0x441 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 15, 2, 2, 0x441 },
    { 15, 15, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 10, 1, 2, 0x441 },
    { 15, 15, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 10, 2, 2, 0x441 },
};

static const M_SHOT m_Shots2_4[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 32, 0, 0, 0x440 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 16, 1, 2, 0x442 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 16, 2, 2, 0x442 },
    { 15, 20, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 11, 1, 2, 0x442 },
    { 15, 20, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 11, 2, 2, 0x442 },
};

static const M_SHOT m_Shots2_5[] = {
    { 5, 0, -8.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 16, 0, 0, 0x440 },
    { 5, 0, 8.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 16, 0, 0, 0x440 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 16, 1, 2, 0x442 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 16, 2, 2, 0x442 },
    { 15, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 3.0f, 10, 1, 2, 0x442 },
    { 15, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 3.0f, 10, 2, 2, 0x442 },
};

static const M_SHOT m_Shots2_6[] = {
    { 5, 0, -8.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 13, 0, 0, 0x440 },
    { 5, 0, 8.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 13, 0, 0, 0x440 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 3.0f, 16, 1, 2, 0x443 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 3.0f, 16, 2, 2, 0x443 },
    { 10, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 3.0f, 10, 1, 2, 0x443 },
    { 10, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 3.0f, 10, 2, 2, 0x443 },
};

static const M_SHOT m_Shots2_7[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6406095f, 12.0f, 8, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 12, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5009832f, 12.0f, 8, 0, 0, 0x440 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 3.0f, 15, 1, 2, 0x443 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 3.0f, 15, 2, 2, 0x443 },
    { 10, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 3.0f, 10, 1, 2, 0x443 },
    { 10, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 3.0f, 10, 2, 2, 0x443 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.3613569f, 3.0f, 9, 1, 2, 0x443 },
    { 15, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7802359f, 3.0f, 9, 2, 2, 0x443 },
};

static const M_SHOT m_Shots2_8[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6406095f, 12.0f, 8, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 12, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5009832f, 12.0f, 8, 0, 0, 0x440 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 3.0f, 14, 1, 2, 0x444 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 3.0f, 14, 2, 2, 0x444 },
    { 10, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.43117f, 3.0f, 10, 1, 2, 0x444 },
    { 10, 5, 0.0f, 0.0f, 12.0f, 12.0f, -1.7104228f, 3.0f, 10, 2, 2, 0x444 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.3089969f, 3.0f, 10, 1, 2, 0x444 },
    { 10, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.8325958f, 3.0f, 10, 2, 2, 0x444 },
};

static const M_SHOT m_Shots3_0[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 48, 0, 0, 0x440 },
};

static const M_SHOT m_Shots3_1[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 32, 0, 0, 0x440 },
    { 120, 0, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 1, 3, 0x445 },
    { 120, 1, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 2, 3, 0x445 },
};

static const M_SHOT m_Shots3_2[] = {
    { 5, 0, 0.0f, -8.0f, 12.0f, 24.0f, -1.5707964f, 12.0f, 32, 0, 0, 0x440 },
    { 170, 0, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 1, 3, 0x445 },
    { 170, 1, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 2, 3, 0x445 },
};

static const M_SHOT m_Shots3_3[] = {
    { 5, 0, -8.0f, -8.0f, 12.0f, 24.0f, -1.605703f, 12.0f, 22, 0, 0, 0x440 },
    { 5, 0, 8.0f, -8.0f, 12.0f, 24.0f, -1.5358897f, 12.0f, 22, 0, 0, 0x440 },
    { 200, 0, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 1, 3, 0x445 },
    { 200, 1, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 2, 3, 0x445 },
};

static const M_SHOT m_Shots3_4[] = {
    { 5, 0, -8.0f, -8.0f, 12.0f, 24.0f, -1.605703f, 12.0f, 22, 0, 0, 0x440 },
    { 5, 0, 8.0f, -8.0f, 12.0f, 24.0f, -1.5358897f, 12.0f, 22, 0, 0, 0x440 },
    { 210, 0, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 1, 3, 0x446 },
    { 210, 1, 0.0f, 0.0f, 10.0f, 480.0f, -1.5707964f, 3.0f, 3, 2, 3, 0x446 },
};

static const M_SHOT m_Shots3_5[] = {
    { 5, 0, -8.0f, -8.0f, 12.0f, 24.0f, -1.605703f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 8.0f, -8.0f, 12.0f, 24.0f, -1.5358897f, 12.0f, 20, 0, 0, 0x440 },
    { 230, 0, 0.0f, 0.0f, 15.0f, 480.0f, -1.5707964f, 3.0f, 4, 1, 3, 0x446 },
    { 230, 1, 0.0f, 0.0f, 15.0f, 480.0f, -1.5707964f, 3.0f, 4, 2, 3, 0x446 },
};

static const M_SHOT m_Shots3_6[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 12.0f, 15, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 12.0f, 15, 0, 0, 0x440 },
    { 250, 0, 0.0f, 0.0f, 15.0f, 480.0f, -1.5707964f, 3.0f, 4, 1, 3, 0x446 },
    { 250, 1, 0.0f, 0.0f, 15.0f, 480.0f, -1.5707964f, 3.0f, 4, 2, 3, 0x446 },
};

static const M_SHOT m_Shots3_7[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 12.0f, 15, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 12.0f, 15, 0, 0, 0x440 },
    { 270, 0, 0.0f, 0.0f, 20.0f, 480.0f, -1.5707964f, 3.0f, 5, 1, 3, 0x447 },
    { 270, 1, 0.0f, 0.0f, 20.0f, 480.0f, -1.5707964f, 3.0f, 5, 2, 3, 0x447 },
};

static const M_SHOT m_Shots3_8[] = {
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.7453293f, 12.0f, 12, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.6580628f, 12.0f, 15, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.5707964f, 12.0f, 20, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.4835299f, 12.0f, 15, 0, 0, 0x440 },
    { 5, 0, 0.0f, 0.0f, 12.0f, 12.0f, -1.3962635f, 12.0f, 12, 0, 0, 0x440 },
    { 330, 0, 0.0f, 0.0f, 20.0f, 480.0f, -1.5707964f, 3.0f, 6, 1, 3, 0x447 },
    { 330, 1, 0.0f, 0.0f, 20.0f, 480.0f, -1.5707964f, 3.0f, 6, 2, 3, 0x447 },
};

static const M_SHOT_TABLE m_ShotTables[][M_POWER_LEVEL_COUNT] = {
    {
        { 1, 8, m_Shots0_0 },
        { 3, 16, m_Shots0_1 },
        { 4, 32, m_Shots0_2 },
        { 5, 48, m_Shots0_3 },
        { 5, 64, m_Shots0_4 },
        { 7, 80, m_Shots0_5 },
        { 9, 96, m_Shots0_6 },
        { 9, 127, m_Shots0_7 },
        { 12, 999, m_Shots0_8 },
    },
    {
        { 1, 8, m_Shots1_0 },
        { 3, 16, m_Shots1_1 },
        { 4, 32, m_Shots1_2 },
        { 4, 48, m_Shots1_3 },
        { 5, 64, m_Shots1_4 },
        { 7, 80, m_Shots1_5 },
        { 7, 96, m_Shots1_6 },
        { 11, 127, m_Shots1_7 },
        { 11, 999, m_Shots1_8 },
    },
    {
        { 1, 8, m_Shots2_0 },
        { 3, 16, m_Shots2_1 },
        { 5, 32, m_Shots2_2 },
        { 5, 48, m_Shots2_3 },
        { 5, 64, m_Shots2_4 },
        { 6, 80, m_Shots2_5 },
        { 6, 96, m_Shots2_6 },
        { 9, 127, m_Shots2_7 },
        { 9, 999, m_Shots2_8 },
    },
    {
        { 1, 8, m_Shots3_0 },
        { 3, 16, m_Shots3_1 },
        { 3, 32, m_Shots3_2 },
        { 4, 48, m_Shots3_3 },
        { 4, 64, m_Shots3_4 },
        { 4, 80, m_Shots3_5 },
        { 5, 96, m_Shots3_6 },
        { 5, 127, m_Shots3_7 },
        { 7, 999, m_Shots3_8 },
    },
};

static const M_SPEEDS m_Speeds[] = {
    { 4.0f, 2.0f },
    { 4.0f, 2.0f },
    { 5.0f, 2.5f },
    { 5.0f, 2.5f },
};

PLAYER g_Player;

static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;
static CHAIN_ELEM m_Draw2Elem;

static int32_t M_ShotType(void)
{
    return g_GameManager.run.character * 2 + g_GameManager.run.shot_type;
}

static void M_Box(
    const VEC3 pos, const VEC3 size, VEC3 *const min, VEC3 *const max)
{
    min->x = pos.x - size.x / 2.0f;
    min->y = pos.y - size.y / 2.0f;
    max->x = size.x / 2.0f + pos.x;
    max->y = size.y / 2.0f + pos.y;
}

static bool M_Overlap(
    const VEC3 a_min, const VEC3 a_max, const VEC3 b_min, const VEC3 b_max)
{
    return !(
        a_min.x > b_max.x || a_max.x < b_min.x || a_min.y > b_max.y
        || a_max.y < b_min.y);
}

static bool M_TouchesClearArea(const VEC3 min, const VEC3 max)
{
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        const PLAYER_CLEAR_AREA *const area = &g_Player.bomb.clear_areas[i];
        if (area->width == 0.0f) {
            continue;
        }
        const VEC3 area_min = {
            area->x - area->width / 2.0f,
            area->y - area->height / 2.0f,
            0.0f,
        };
        const VEC3 area_max = {
            area->width / 2.0f + area->x,
            area->height / 2.0f + area->y,
            0.0f,
        };
        if (M_Overlap(area_min, area_max, min, max)) {
            return true;
        }
    }
    return false;
}

static bool M_IsDyingOrRespawning(void)
{
    return g_Player.state.value == PLAYER_DYING
        || g_Player.state.value == PLAYER_RESPAWNING;
}

static void M_Graze(const VEC3 pos)
{
    if (!g_Player.bomb.running) {
        if (g_GameManager.player.graze < 9999) {
            g_GameManager.player.graze++;
        }
        if (g_GameManager.player.graze_total < 999999) {
            g_GameManager.player.graze_total++;
        }
    }
    const VEC3 mid = Vec3_Scale(Vec3_Add(g_Player.pos, pos), 1.0f / 2.0f);
    Effect_Spawn(EFFECT_SMALL_PARTICLE, mid, 1, M_COLOR_WHITE);
    g_GameManager.score.value += 500;
    GameManager_AddRank(6);
}

static void M_Die(void)
{
    g_Enemies.spell_card.capturable = false;
    Effect_Spawn(EFFECT_SHOCKWAVE, g_Player.pos, 1, M_COLOR_SHOCKWAVE);
    Effect_Spawn(M_EFFECT_DEATH, g_Player.pos, 16, M_COLOR_WHITE);
    g_Player.state.value = PLAYER_DYING;
    Timer_Set(&g_Player.state.timer, 0);
    g_GameManager.stats.deaths++;
    for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
        Timer_Set(&g_Player.shots.laser_timers[i], 2);
    }
}

// Makes a rising shot weaker and larger after a hit.
static void M_Weaken(PLAYER_BULLET *const bullet)
{
    bullet->damage = (int16_t)(bullet->damage / 4);
    if (bullet->damage == 0) {
        bullet->damage = 1;
    }
    switch (bullet->vm.script_index) {
    case 0x441:
        bullet->size.x = 32.0f;
        bullet->size.y = 32.0f;
        break;
    case 0x442:
        bullet->size.x = 42.0f;
        bullet->size.y = 42.0f;
        break;
    case 0x443:
    case 0x444:
        bullet->size.x = 48.0f;
        bullet->size.y = 48.0f;
        break;
    default:
        break;
    }
    if (bullet->timer.current % 6 == 0) {
        Effect_Spawn(M_EFFECT_SHOT_HIT, bullet->pos, 1, M_COLOR_WHITE);
    }
}

static M_FIRE M_FireShot(
    PLAYER_BULLET *const bullet, const int32_t index, const int32_t frame)
{
    const M_SHOT_TABLE *const levels = m_ShotTables[M_ShotType()];
    int32_t level = 0;
    while (level < M_POWER_LEVEL_COUNT - 1
           && g_GameManager.player.power >= levels[level].max_power) {
        level++;
    }
    const M_SHOT_TABLE *const table = &levels[level];
    const M_SHOT *const shot = &table->shots[index];
    const bool last = index >= table->count - 1;
    bool fires;
    if (shot->type == PLAYER_BULLET_LASER) {
        TIMER *const timer = &g_Player.shots.laser_timers[shot->phase];
        fires = timer->current == 0;
        if (fires) {
            Timer_Set(timer, shot->interval);
            bullet->laser = shot->phase;
            bullet->option = shot->option;
            bullet->laser_offset_x = shot->offset_x;
        }
    } else {
        fires = frame % shot->interval == shot->phase;
    }
    if (!fires) {
        return last ? M_FIRE_SKIPPED_LAST : M_FIRE_SKIPPED;
    }
    Anm_StartScript(&bullet->vm, shot->script);
    bullet->pos = shot->option == 0 ? g_Player.pos
                                    : g_Player.options.pos[shot->option - 1];
    bullet->pos.x += shot->offset_x;
    bullet->pos.y += shot->offset_y;
    bullet->pos.z = 0.495f;
    bullet->size = (VEC3) { shot->size_x, shot->size_y, 1.0f };
    bullet->angle = shot->angle;
    bullet->speed = shot->speed;
    bullet->velocity.x = X87_Cos(shot->angle) * shot->speed;
    bullet->velocity.y = X87_Sin(shot->angle) * shot->speed;
    Timer_Set(&bullet->timer, 0);
    bullet->type = (PLAYER_BULLET_TYPE)shot->type;
    bullet->damage = shot->damage;
    return last ? M_FIRE_FIRED_LAST : M_FIRE_FIRED;
}

// Fires the shots of the power level for frame of the burst, each into
// the next free bullet. A shot that does not fire leaves the bullet for
// the next shot. When the bullets run out, the shots left do not fire.
static void M_FireShots(const int32_t frame)
{
    int32_t index = 0;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state != PLAYER_BULLET_FREE) {
            continue;
        }
        for (;;) {
            const M_FIRE fire = M_FireShot(bullet, index, frame);
            if (fire == M_FIRE_FIRED || fire == M_FIRE_FIRED_LAST) {
                bullet->vm.pos.x = bullet->pos.x;
                bullet->vm.pos.y = bullet->pos.y;
                bullet->vm.pos.z = 0.495f;
                bullet->state = PLAYER_BULLET_FLYING;
            }
            if (fire == M_FIRE_FIRED_LAST || fire == M_FIRE_SKIPPED_LAST) {
                return;
            }
            index++;
            if (fire == M_FIRE_FIRED) {
                break;
            }
        }
    }
}

static void M_UpdateFiring(void)
{
    TIMER *const timer = &g_Player.shots.fire_timer;
    if (timer->current < 0) {
        return;
    }
    const bool master_spark = g_Player.bomb.running
        && g_GameManager.run.character == 1 && g_GameManager.run.shot_type == 1;
    if (Timer_HasTicked(timer) && !master_spark) {
        M_FireShots(timer->current);
    }
    Timer_Tick(timer);
    if (timer->current >= M_FIRE_FRAMES || M_IsDyingOrRespawning()) {
        Timer_Set(timer, -1);
    }
}

static void M_Home(PLAYER_BULLET *const bullet)
{
    const VEC3 target = g_Player.homing_target;
    float x;
    float y;
    float len;
    if (target.x > M_NO_TARGET && bullet->timer.current < M_HOMING_FRAMES
        && Timer_HasTicked(&bullet->timer)) {
        x = target.x - bullet->pos.x;
        y = target.y - bullet->pos.y;
        len = X87_Sqrt(x * x + y * y) / (bullet->speed / 4.0f);
        if (len < 1.0f) {
            len = 1.0f;
        }
        x = x / len + bullet->velocity.x;
        y = y / len + bullet->velocity.y;
        len = X87_Sqrt(x * x + y * y);
        bullet->speed = len > M_MAX_SHOT_SPEED ? M_MAX_SHOT_SPEED : len;
        if (bullet->speed < 1.0f) {
            bullet->speed = 1.0f;
        }
        bullet->velocity.x = x * bullet->speed / len;
        bullet->velocity.y = y * bullet->speed / len;
    } else if (bullet->speed < M_MAX_SHOT_SPEED) {
        bullet->speed += 0.33333334f;
        x = bullet->velocity.x;
        y = bullet->velocity.y;
        len = X87_Sqrt(x * x + y * y);
        bullet->velocity.x = x * bullet->speed / len;
        bullet->velocity.y = y * bullet->speed / len;
    }
}

// Keeps a laser on its option, reaching from the top of the play area.
// The laser then still moves by its velocity in the frame.
static void M_HoldLaser(PLAYER_BULLET *const bullet)
{
    const int32_t frames = g_Player.shots.laser_timers[bullet->laser].current;
    if (frames == 70 || frames == 1) {
        bullet->vm.pending_interrupt = 1;
    }
    bullet->pos = g_Player.options.pos[bullet->option - 1];
    bullet->pos.x += bullet->laser_offset_x;
    bullet->pos.y /= 2.0f;
    bullet->pos.z = 0.44f;
    bullet->vm.scale_y = bullet->pos.y * 2.0f / 14.0f;
    bullet->size.y = bullet->pos.y * 2.0f;
}

static void M_UpdateBullets(void)
{
    for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
        TIMER *const timer = &g_Player.shots.laser_timers[i];
        if (timer->current != 0) {
            timer->current -= 1;
        }
    }
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state == PLAYER_BULLET_FREE) {
            continue;
        }
        switch (bullet->type) {
        case PLAYER_BULLET_HOMING:
            if (bullet->state == PLAYER_BULLET_FLYING) {
                M_Home(bullet);
            }
            break;
        case PLAYER_BULLET_RISING:
            if (bullet->state == PLAYER_BULLET_FLYING) {
                bullet->velocity.y -= 0.3f;
            }
            break;
        case PLAYER_BULLET_LASER:
            M_HoldLaser(bullet);
            break;
        default:
            break;
        }
        bullet->pos.x += bullet->velocity.x;
        bullet->pos.y += bullet->velocity.y;
        bullet->vm.pos = bullet->pos;
        // A shot freed here still runs its script and timer this frame.
        if (bullet->type != PLAYER_BULLET_LASER
            && !GameManager_IsInPlayArea(
                bullet->pos.x, bullet->pos.y, bullet->vm.sprite_data->width,
                bullet->vm.sprite_data->height)) {
            bullet->state = PLAYER_BULLET_FREE;
        }
        if (Anm_Step(&bullet->vm)) {
            bullet->state = PLAYER_BULLET_FREE;
        }
        Timer_Tick(&bullet->timer);
    }
}

// Moves the options with the player, and slides them in while focused
// and out otherwise. A slide that turns back starts from where the other
// one was.
static void M_MoveOptions(void)
{
    TIMER *const timer = &g_Player.options.timer;
    const bool focused = g_Player.move.focused;
    float side_x = 0.0f;
    float side_y = 0.0f;
    float t;
    g_Player.options.pos[0] = g_Player.pos;
    g_Player.options.pos[1] = g_Player.pos;
    if (g_GameManager.player.power < M_OPTIONS_MIN_POWER) {
        g_Player.options.state = PLAYER_OPTIONS_NONE;
    } else if (g_Player.options.state == PLAYER_OPTIONS_NONE) {
        g_Player.options.state = PLAYER_OPTIONS_OUT;
    }
    switch (g_Player.options.state) {
    case PLAYER_OPTIONS_NONE:
        Timer_Set(timer, 0);
        break;
    case PLAYER_OPTIONS_OUT:
        side_x = 24.0f;
        Timer_Set(timer, 0);
        if (!focused) {
            break;
        }
        g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
        [[fallthrough]];
    case PLAYER_OPTIONS_SLIDING_IN:
    slide_in:
        Timer_Tick(timer);
        t = (float)timer->current / (float)M_OPTION_SLIDE_FRAMES;
        side_y = (1.0f - t) * 32.0f + -32.0f;
        t = t * t;
        side_x = -16.0f * t + 24.0f;
        if (timer->current >= M_OPTION_SLIDE_FRAMES) {
            g_Player.options.state = PLAYER_OPTIONS_IN;
        }
        if (!focused) {
            g_Player.options.state = PLAYER_OPTIONS_SLIDING_OUT;
            Timer_Set(timer, M_OPTION_SLIDE_FRAMES - timer->current);
            goto slide_out;
        }
        break;
    case PLAYER_OPTIONS_IN:
        side_x = 8.0f;
        side_y = -32.0f;
        Timer_Set(timer, 0);
        if (focused) {
            break;
        }
        g_Player.options.state = PLAYER_OPTIONS_SLIDING_OUT;
        [[fallthrough]];
    case PLAYER_OPTIONS_SLIDING_OUT:
    slide_out:
        Timer_Tick(timer);
        t = (float)timer->current / (float)M_OPTION_SLIDE_FRAMES;
        side_y = 32.0f * t + -32.0f;
        t = t * t;
        t = 1.0f - t;
        side_x = -16.0f * t + 24.0f;
        if (timer->current >= M_OPTION_SLIDE_FRAMES) {
            g_Player.options.state = PLAYER_OPTIONS_OUT;
        }
        if (focused) {
            g_Player.options.state = PLAYER_OPTIONS_SLIDING_IN;
            Timer_Set(timer, M_OPTION_SLIDE_FRAMES - timer->current);
            goto slide_in;
        }
        break;
    }
    g_Player.options.pos[0].x -= side_x;
    g_Player.options.pos[1].x += side_x;
    g_Player.options.pos[0].y += side_y;
    g_Player.options.pos[1].y += side_y;
}

// Starts the turning script on a change of the horizontal move.
static void M_Turn(const float dx)
{
    const float last_x = g_Player.move.last_x;
    if (dx < 0.0f && last_x >= 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_TURN_LEFT);
    } else if (dx == 0.0f && last_x < 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_FROM_LEFT);
    }
    if (dx > 0.0f && last_x <= 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_TURN_RIGHT);
    } else if (dx == 0.0f && last_x > 0.0f) {
        Anm_StartScript(&g_Player.vm, M_SCRIPT_FROM_RIGHT);
    }
}

// Moves the player by the direction held. Up wins over down, and right
// over left.
static void M_Move(void)
{
    const TH06_INPUT *const held = &g_Buttons.held;
    const bool focused = held->focus;
    g_Player.move.focused = focused;
    const float straight =
        focused ? g_Player.move.speed_focused : g_Player.move.speed;
    const float diagonal = focused ? g_Player.move.speed_diagonal_focused
                                   : g_Player.move.speed_diagonal;
    const float step = (held->up || held->down) && (held->left || held->right)
        ? diagonal
        : straight;
    float dx = 0.0f;
    float dy = 0.0f;
    if (held->right) {
        dx = step;
    } else if (held->left) {
        dx = -step;
    }
    if (held->up) {
        dy = -step;
    } else if (held->down) {
        dy = step;
    }
    M_Turn(dx);
    g_Player.move.last_x = dx;
    g_Player.pos.x += dx * g_Player.move.speed_scale_x;
    g_Player.pos.y += dy * g_Player.move.speed_scale_y;

    const PLAY_RECT *const area = &g_GameManager.player_area;
    if (g_Player.pos.x < area->x) {
        g_Player.pos.x = area->x;
    } else if (area->x + area->width < g_Player.pos.x) {
        g_Player.pos.x = area->x + area->width;
    }
    if (g_Player.pos.y < area->y) {
        g_Player.pos.y = area->y;
    } else if (area->y + area->height < g_Player.pos.y) {
        g_Player.pos.y = area->y + area->height;
    }
    g_Player.box.hitbox_min = Vec3_Sub(g_Player.pos, g_Player.box.hitbox_size);
    g_Player.box.hitbox_max = Vec3_Add(g_Player.pos, g_Player.box.hitbox_size);
    g_Player.box.graze_min = Vec3_Sub(g_Player.pos, g_Player.box.graze_size);
    g_Player.box.graze_max = Vec3_Add(g_Player.pos, g_Player.box.graze_size);
    M_MoveOptions();

    if (held->shoot && !Gui_IsMsgRunning()
        && g_Player.shots.fire_timer.current < 0) {
        Timer_Set(&g_Player.shots.fire_timer, 0);
    }
}

static void M_StartBomb(void)
{
    g_GameManager.stats.bombs_used++;
    g_GameManager.player.bombs--;
    g_Player.bomb.running = true;
    Timer_Set(&g_Player.bomb.timer, 0);
    g_Player.bomb.duration = 999;
    Bomb_Update();
    g_Enemies.spell_card.capturable = false;
    GameManager_SubRank(200);
    g_Enemies.spell_card.bombed =
        g_Enemies.spell_card.state != ENEMY_SPELL_NONE;
}

static void M_DropItems(void)
{
    const VEC3 pos = g_Player.pos;
    g_GameManager.player.power_item_combo = 0;
    if (g_GameManager.player.lives > 0) {
        Item_Spawn(pos, ITEM_BIG_POWER, ITEM_THROWN);
        for (int32_t i = 0; i < 5; i++) {
            Item_Spawn(pos, ITEM_POWER, ITEM_THROWN);
        }
        if (g_GameManager.player.power <= 16) {
            g_GameManager.player.power = 0;
        } else {
            g_GameManager.player.power -= 16;
        }
    } else {
        for (int32_t i = 0; i < 5; i++) {
            Item_Spawn(pos, ITEM_FULL_POWER, ITEM_THROWN);
        }
        g_GameManager.player.power = 0;
        g_GameManager.score.next_extend = -1;
    }
    GameManager_SubRank(1600);
}

static void M_UpdateRespawning(void)
{
    PLAYER *const player = &g_Player;
    const int32_t frames = player->state.timer.current;
    player->state.respawn_clear_frames = M_RESPAWN_CLEAR_FRAMES;
    const float progress = 1.0f - (float)frames / (float)M_RESPAWN_FRAMES;
    player->vm.scale_y = 2.0f * progress + 1.0f;
    player->vm.scale_x = 1.0f - 1.0f * progress;
    player->vm.blend_add = true;
    player->move.speed_scale_y = 1.0f;
    player->move.speed_scale_x = 1.0f;
    player->vm.color =
        (uint32_t)(frames * 255 / M_RESPAWN_FRAMES) << 24 | 0xffffffu;
    player->state.bomb_window = 0;
    if (frames >= M_RESPAWN_FRAMES) {
        player->state.value = PLAYER_INVULNERABLE;
        player->vm.scale_x = 1.0f;
        player->vm.scale_y = 1.0f;
        player->vm.color = M_COLOR_WHITE;
        player->vm.blend_add = false;
        Timer_Set(&player->state.timer, M_INVULNERABLE_FRAMES);
        player->state.bomb_window = M_RESPAWN_BOMB_WINDOW;
    }
}

// Waits out the bomb window, which drops the items when it closes, then
// shrinks the player away. At the end, it starts the respawn at once, or
// opens the continue menu without starting it when no life is left.
static void M_UpdateDying(void)
{
    PLAYER *const player = &g_Player;
    if (player->state.bomb_window != 0) {
        player->state.bomb_window--;
        if (player->state.bomb_window == 0) {
            M_DropItems();
        }
        return;
    }
    const float frames = (float)player->state.timer.current;
    const float progress = frames / (float)M_DEATH_FRAMES;
    player->vm.scale_y = 3.0f * progress + 1.0f;
    player->vm.scale_x = 1.0f - 1.0f * progress;
    player->vm.color =
        (uint32_t)X87_Ftol(255.0f - frames * 255.0f / (float)M_DEATH_FRAMES)
            << 24
        | 0xffffffu;
    player->vm.blend_add = true;
    player->move.last_x = 0.0f;
    if (player->state.timer.current < M_DEATH_FRAMES) {
        return;
    }
    player->state.value = PLAYER_RESPAWNING;
    player->pos.x = g_GameManager.play_area.width / 2.0f;
    player->pos.y = g_GameManager.play_area.height - 64.0f;
    player->pos.z = 0.2f;
    Timer_Set(&player->state.timer, 0);
    player->vm.scale_x = 3.0f;
    player->vm.scale_y = 3.0f;
    Anm_StartScript(&player->vm, M_SCRIPT_STILL);
    if (g_GameManager.player.lives <= 0) {
        g_GameManager.mode.in_continue_menu = true;
        return;
    }
    g_GameManager.player.lives--;
    if (g_GameManager.run.difficulty < 4 && !g_GameManager.mode.in_practice) {
        g_GameManager.player.bombs = (int8_t)g_GameManager.run.start_bombs;
    } else {
        g_GameManager.player.bombs = 3;
    }
    M_UpdateRespawning();
}

static void M_UpdateInvulnerable(void)
{
    PLAYER *const player = &g_Player;
    player->state.timer.current -= 1;
    if (player->state.timer.current <= 0) {
        player->state.value = PLAYER_ALIVE;
        Timer_Set(&player->state.timer, 0);
        player->vm.color_op = false;
        player->vm.color = M_COLOR_WHITE;
    } else if (player->state.timer.current % 8 < 2) {
        player->vm.color_op = true;
        player->vm.color = 0xff404040u;
    } else {
        player->vm.color_op = false;
        player->vm.color = M_COLOR_WHITE;
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    PLAYER *const player = &g_Player;
    if (g_GameManager.mode.frozen) {
        return CHAIN_CONTINUE;
    }
    // Clears only the sizes: a bomb that sets the size of a clear area
    // without its place reuses the place of the frame before.
    for (int32_t i = 0; i < PLAYER_BOMB_HIT_COUNT; i++) {
        player->bomb.hits[i].size.x = 0.0f;
    }
    for (int32_t i = 0; i < PLAYER_CLEAR_AREA_COUNT; i++) {
        player->bomb.clear_areas[i].width = 0.0f;
    }
    if (player->bomb.running) {
        Bomb_Update();
    } else if (
        !Gui_IsMsgRunning() && player->state.bomb_window != 0
        && g_GameManager.player.bombs > 0 && g_Buttons.pressed.bomb) {
        M_StartBomb();
    }
    if (player->state.value == PLAYER_DYING) {
        M_UpdateDying();
    } else if (player->state.value == PLAYER_RESPAWNING) {
        M_UpdateRespawning();
    }
    if (player->state.respawn_clear_frames != 0) {
        player->state.respawn_clear_frames--;
        Bullet_ClearAll();
    }
    if (player->state.value == PLAYER_INVULNERABLE) {
        M_UpdateInvulnerable();
    } else {
        Timer_Tick(&player->state.timer);
    }
    if (!M_IsDyingOrRespawning()) {
        M_Move();
    }
    Anm_Step(&player->vm);
    M_UpdateBullets();
    if (player->options.state != PLAYER_OPTIONS_NONE) {
        Anm_Step(&player->option_vms[0]);
        Anm_Step(&player->option_vms[1]);
    }
    player->homing_target = (VEC3) { -999.0f, -999.0f, 0.0f };
    M_UpdateFiring();
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    return CHAIN_CONTINUE;
}

// Starts the player respawning with its timer past the end, so the first
// update makes it invulnerable at once and clears the bullets for 90
// frames.
static RESULT M_Added(void *const arg)
{
    static const char *const paths[] = {
        "data/player00.anm",
        "data/player01.anm",
    };
    PLAYER *const player = &g_Player;
    const uint8_t character = g_GameManager.run.character;
    if (character < ARRAY_SIZE(paths)) {
        if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
            MUST(Anm_Load(M_ANM_SLOT, paths[character], M_FIRST_SPRITE));
        }
        Anm_StartScript(&player->vm, M_SCRIPT_STILL);
    }
    player->pos.x = g_GameManager.play_area.width / 2.0f;
    player->pos.y = g_GameManager.play_area.height - 64.0f;
    player->pos.z = 0.49f;
    player->options.pos[0].z = 0.49f;
    player->options.pos[1].z = 0.49f;
    for (int32_t i = 0; i < PLAYER_BOMB_HIT_COUNT; i++) {
        player->bomb.hits[i].size.x = 0.0f;
    }
    player->box.hitbox_size = (VEC3) { 1.25f, 1.25f, 5.0f };
    player->box.graze_size = (VEC3) { 12.0f, 12.0f, 5.0f };
    const M_SPEEDS *const speeds = &m_Speeds[M_ShotType()];
    player->move.speed = speeds->speed;
    player->move.speed_focused = speeds->speed_focused;
    player->move.speed_diagonal = speeds->speed / X87_Sqrt(2.0f);
    player->move.speed_diagonal_focused =
        speeds->speed_focused / X87_Sqrt(2.0f);
    player->state.value = PLAYER_RESPAWNING;
    Timer_Set(&player->state.timer, 120);
    player->options.state = PLAYER_OPTIONS_NONE;
    Anm_StartScript(&player->option_vms[0], M_SCRIPT_OPTION);
    Anm_StartScript(&player->option_vms[1], M_SCRIPT_OPTION + 1);
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        player->shots.bullets[i].state = PLAYER_BULLET_FREE;
    }
    Timer_Set(&player->shots.fire_timer, -1);
    player->bomb.running = false;
    for (int32_t i = 0; i < PLAYER_LASER_COUNT; i++) {
        Timer_Set(&player->shots.laser_timers[i], 0);
    }
    player->move.speed_scale_y = 1.0f;
    player->move.speed_scale_x = 1.0f;
    player->state.bomb_window = M_START_BOMB_WINDOW;
    return OK;
}

static void M_Deleted(void *const arg)
{
    if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
        Anm_Release(M_ANM_SLOT);
    }
}

RESULT Player_Register(void)
{
    g_Player = (PLAYER) {};
    Timer_Set(&g_Player.state.timer, 0);
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    MUST(Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY));
    Chain_InitElem(&m_Draw2Elem, M_Draw, nullptr);
    return Chain_AddDraw(&g_Chain, &m_Draw2Elem, M_DRAW2_PRIORITY);
}

void Player_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
    Chain_Remove(&m_Draw2Elem);
}

float Player_AngleFrom(const VEC3 pos)
{
    const float x = g_Player.pos.x - pos.x;
    const float y = g_Player.pos.y - pos.y;
    if (y == 0.0f && x == 0.0f) {
        return M_HALF_PI;
    }
    return X87_Atan2(y, x);
}

float Player_AngleTo(const VEC3 pos)
{
    const float x = pos.x - g_Player.pos.x;
    const float y = pos.y - g_Player.pos.y;
    if (y == 0.0f && x == 0.0f) {
        return M_HALF_PI;
    }
    return X87_Atan2(y, x);
}

PLAYER_CONTACT Player_CheckHit(const VEC3 pos, const VEC3 size)
{
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    if (M_TouchesClearArea(min, max)) {
        return PLAYER_CONTACT_CLEARED;
    }
    if (!M_Overlap(
            g_Player.box.hitbox_min, g_Player.box.hitbox_max, min, max)) {
        return PLAYER_CONTACT_NONE;
    }
    if (g_Player.state.value == PLAYER_ALIVE) {
        M_Die();
    }
    return PLAYER_CONTACT_BOX;
}

PLAYER_CONTACT Player_CheckGraze(const VEC3 pos, const VEC3 size)
{
    const VEC3 min = {
        pos.x - size.x / 2.0f - M_GRAZE_MARGIN,
        pos.y - size.y / 2.0f - M_GRAZE_MARGIN,
        0.0f,
    };
    const VEC3 max = {
        size.x / 2.0f + pos.x + M_GRAZE_MARGIN,
        size.y / 2.0f + pos.y + M_GRAZE_MARGIN,
        0.0f,
    };
    if (M_TouchesClearArea(min, max)) {
        return PLAYER_CONTACT_CLEARED;
    }
    if (M_IsDyingOrRespawning()
        || !M_Overlap(
            g_Player.box.hitbox_min, g_Player.box.hitbox_max, min, max)) {
        return PLAYER_CONTACT_NONE;
    }
    M_Graze(pos);
    return PLAYER_CONTACT_BOX;
}

void Player_CheckLaserHit(
    const VEC3 pos, const VEC3 size, const VEC3 origin, const float angle,
    const bool can_graze)
{
    const VEC3 offset = Vec3_Sub(g_Player.pos, origin);
    VEC3 turned = {};
    Angle_Rotate2D(&turned, &offset, angle);
    const VEC3 center = Vec3_Add(turned, origin);
    const VEC3 hit_min = Vec3_Sub(center, g_Player.box.hitbox_size);
    const VEC3 hit_max = Vec3_Add(center, g_Player.box.hitbox_size);
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    if (M_Overlap(hit_min, hit_max, min, max)) {
        if (g_Player.state.value == PLAYER_ALIVE) {
            M_Die();
        }
        return;
    }
    if (!can_graze) {
        return;
    }
    min.x -= M_LASER_GRAZE_MARGIN;
    min.y -= M_LASER_GRAZE_MARGIN;
    max.x += M_LASER_GRAZE_MARGIN;
    max.y += M_LASER_GRAZE_MARGIN;
    if (!M_Overlap(hit_min, hit_max, min, max) || M_IsDyingOrRespawning()) {
        return;
    }
    M_Graze(g_Player.pos);
}

bool Player_TouchesGrazeBox(const VEC3 pos, const VEC3 size)
{
    if (g_Player.state.value != PLAYER_ALIVE
        && g_Player.state.value != PLAYER_INVULNERABLE) {
        return false;
    }
    VEC3 min;
    VEC3 max;
    M_Box(pos, size, &min, &max);
    return M_Overlap(g_Player.box.graze_min, g_Player.box.graze_max, min, max);
}

int32_t Player_CalcDamage(
    const VEC3 pos, const VEC3 size, bool *const hit_by_bomb)
{
    int32_t damage = 0;
    VEC3 enemy_min;
    VEC3 enemy_max;
    M_Box(pos, size, &enemy_min, &enemy_max);
    *hit_by_bomb = false;
    for (int32_t i = 0; i < PLAYER_BULLET_COUNT; i++) {
        PLAYER_BULLET *const bullet = &g_Player.shots.bullets[i];
        if (bullet->state == PLAYER_BULLET_FREE
            || (bullet->state != PLAYER_BULLET_FLYING
                && bullet->type != PLAYER_BULLET_RISING)) {
            continue;
        }
        VEC3 min;
        VEC3 max;
        M_Box(bullet->pos, bullet->size, &min, &max);
        if (!M_Overlap(min, max, enemy_min, enemy_max)) {
            continue;
        }
        if (!g_Player.bomb.running) {
            damage += bullet->damage;
        } else {
            damage += bullet->damage / 3 != 0 ? bullet->damage / 3 : 1;
        }
        if (bullet->type == PLAYER_BULLET_RISING) {
            M_Weaken(bullet);
        }
        if (bullet->type != PLAYER_BULLET_LASER) {
            if (bullet->state == PLAYER_BULLET_FLYING) {
                Anm_StartScript(
                    &bullet->vm, bullet->vm.script_index + M_SCRIPT_HIT_OFFSET);
                Effect_Spawn(M_EFFECT_SHOT_HIT, bullet->pos, 1, M_COLOR_WHITE);
                bullet->pos.z = 0.1f;
            }
            bullet->state = PLAYER_BULLET_HIT;
            bullet->velocity.x /= 8.0f;
            bullet->velocity.y /= 8.0f;
        } else {
            g_Player.shots.hit_counter++;
            if (g_Player.shots.hit_counter % 8 == 0) {
                VEC3 at = pos;
                at.x = bullet->pos.x;
                Effect_Spawn(M_EFFECT_SHOT_HIT, at, 1, M_COLOR_WHITE);
            }
        }
    }
    for (int32_t i = 0; i < PLAYER_BOMB_HIT_COUNT; i++) {
        PLAYER_BOMB_HIT *const hit = &g_Player.bomb.hits[i];
        if (hit->size.x <= 0.0f) {
            continue;
        }
        VEC3 min;
        VEC3 max;
        M_Box(hit->pos, hit->size, &min, &max);
        if (!M_Overlap(min, max, enemy_min, enemy_max)) {
            continue;
        }
        damage += hit->damage;
        hit->damage_dealt += hit->damage;
        g_Player.shots.hit_counter++;
        if (g_Player.shots.hit_counter % 4 == 0) {
            Effect_Spawn(EFFECT_BOMB_SPARK, pos, 1, M_COLOR_WHITE);
        }
        if (g_Player.bomb.running) {
            *hit_by_bomb = true;
        }
    }
    return damage;
}
