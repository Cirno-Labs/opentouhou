#include "bot.h"

#include "test.h"

#include <string.h>

// A minimal input struct with the seven buttons the bot drives, so this
// test does not need a whole game's input schema.
typedef struct {
    bool shoot, bomb, focus, up, down, left, right;
} M_INPUT;

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(M_INPUT, shoot, nullptr, nullptr, nullptr),
    INPUT_BUTTON(M_INPUT, bomb, nullptr, nullptr, nullptr),
    INPUT_BUTTON(M_INPUT, focus, nullptr, nullptr, nullptr),
    INPUT_BUTTON(M_INPUT, up, nullptr, nullptr, nullptr),
    INPUT_BUTTON(M_INPUT, down, nullptr, nullptr, nullptr),
    INPUT_BUTTON(M_INPUT, left, nullptr, nullptr, nullptr),
    INPUT_BUTTON(M_INPUT, right, nullptr, nullptr, nullptr),
};
static const INPUT_SCHEMA m_Schema = INPUT_SCHEMA_OF(M_INPUT, m_Actions, 1);

static const VEC2 m_NoThreats[1] = {};

// A play area edge offset, straight above the player, far enough away
// that it never enters the repulsion radius or the home pull by itself
// in these tests unless a test overrides it.
static const VEC2 m_FarEdges[4] = {
    { -1000.0f, 0.0f },
    { 1000.0f, 0.0f },
    { 0.0f, -1000.0f },
    { 0.0f, 1000.0f },
};

static void M_Run(
    const uint16_t seed, const bool passive, const VEC2 *const threats,
    const int32_t count, M_INPUT *const out, const int32_t frames)
{
    BOT bot;
    Bot_Init(&bot, &m_Schema, seed, passive);
    for (int32_t i = 0; i < frames; i++) {
        Bot_NextInput(&bot, threats, count, &out[i]);
    }
}

TEST(same_seed_types_the_same_inputs)
{
    M_INPUT a[500];
    M_INPUT b[500];
    M_Run(1, false, m_FarEdges, 4, a, 500);
    M_Run(1, false, m_FarEdges, 4, b, 500);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
}

TEST(different_seeds_type_different_inputs)
{
    M_INPUT a[500];
    M_INPUT b[500];
    M_Run(1, false, m_FarEdges, 4, a, 500);
    M_Run(2, false, m_FarEdges, 4, b, 500);
    CHECK(memcmp(a, b, sizeof(a)) != 0);
}

TEST(always_holds_shoot)
{
    M_INPUT inputs[200];
    M_Run(1, false, m_FarEdges, 4, inputs, 200);
    for (int32_t i = 0; i < 200; i++) {
        CHECK(inputs[i].shoot);
    }
}

TEST(repulsion_steers_away_from_a_threat)
{
    // A threat to the upper right of the player, close enough to repel
    // (not an edge: neither coordinate is 0); the bot should steer away,
    // down and to the left.
    const VEC2 threats[1] = { { BOT_THREAT_RADIUS / 2.0f,
                                -BOT_THREAT_RADIUS / 2.0f } };
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, threats, 1, &input);
    CHECK(input.left);
    CHECK(!input.right);
    CHECK(input.down);
    CHECK(!input.up);
}

TEST(a_threat_outside_the_radius_does_not_steer)
{
    // A threat past the radius contributes no repulsion, so with the
    // same seed the input is exactly as if it were not there.
    const VEC2 threats[1] = { { BOT_THREAT_RADIUS * 2.0f,
                                BOT_THREAT_RADIUS * 2.0f } };
    M_INPUT with_threat;
    M_INPUT without_threat;
    BOT a;
    BOT b;
    Bot_Init(&a, &m_Schema, 1, false);
    Bot_Init(&b, &m_Schema, 1, false);
    Bot_NextInput(&a, threats, 1, &with_threat);
    Bot_NextInput(&b, m_NoThreats, 0, &without_threat);
    CHECK(memcmp(&with_threat, &without_threat, sizeof(with_threat)) == 0);
}

TEST(an_edge_offset_steers_away_from_the_wall)
{
    // The right wall is close; one coordinate is exactly 0, as
    // list_threats builds it, so the bot reads it as an edge and steers
    // left, away from it.
    const VEC2 threats[1] = { { BOT_THREAT_RADIUS / 2.0f, 0.0f } };
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, threats, 1, &input);
    CHECK(input.left);
}

TEST(an_edge_never_triggers_focus_or_bomb)
{
    // Edges keep the bot off the walls but are not enemy threats: being
    // right against one must not focus or bomb.
    const VEC2 threats[1] = { { BOT_BOMB_DISTANCE / 2.0f, 0.0f } };
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, threats, 1, &input);
    CHECK(!input.focus);
    CHECK(!input.bomb);
}

TEST(focuses_when_a_real_threat_is_close)
{
    const VEC2 threats[1] = { { BOT_FOCUS_DISTANCE - 1.0f, 1.0f } };
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, threats, 1, &input);
    CHECK(input.focus);
}

TEST(does_not_focus_when_nothing_is_close)
{
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, m_NoThreats, 0, &input);
    CHECK(!input.focus);
}

TEST(bombs_when_a_real_threat_is_very_close)
{
    const VEC2 threats[1] = { { BOT_BOMB_DISTANCE - 1.0f, 1.0f } };
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, threats, 1, &input);
    CHECK(input.bomb);
}

TEST(bomb_has_a_cooldown)
{
    const VEC2 threats[1] = { { BOT_BOMB_DISTANCE - 1.0f, 1.0f } };
    M_INPUT first;
    M_INPUT second;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
    Bot_NextInput(&bot, threats, 1, &first);
    Bot_NextInput(&bot, threats, 1, &second);
    CHECK(first.bomb);
    CHECK(!second.bomb);
}

TEST(passive_never_focuses_or_bombs_even_when_very_close)
{
    const VEC2 threats[1] = { { BOT_BOMB_DISTANCE - 1.0f, 1.0f } };
    M_INPUT input;
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, true);
    Bot_NextInput(&bot, threats, 1, &input);
    CHECK(!input.focus);
    CHECK(!input.bomb);
}

TEST(passive_wanders_regardless_of_threats)
{
    // Passive mode is deterministic too, and does not steer away from a
    // threat it is given: it only wanders, as --passive promises for a
    // game-over run.
    const VEC2 threats[1] = { { BOT_THREAT_RADIUS / 2.0f, 0.0f } };
    M_INPUT a[200];
    M_INPUT b[200];
    M_Run(1, true, threats, 1, a, 200);
    M_Run(1, true, m_NoThreats, 0, b, 200);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
}
