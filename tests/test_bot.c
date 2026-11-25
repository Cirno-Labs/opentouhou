#include "bot.h"

#include "test.h"

#include "core/base/utils.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    bool shoot;
    bool bomb;
    bool focus;
    bool up;
    bool down;
    bool left;
    bool right;
} M_INPUT;

#define M_BUTTON(field)                                                        \
    { .type = INPUT_ACTION_BUTTON,                                             \
      .name = #field,                                                          \
      .offset = offsetof(M_INPUT, field) }

static const INPUT_ACTION m_Actions[] = {
    M_BUTTON(shoot), M_BUTTON(bomb), M_BUTTON(focus), M_BUTTON(up),
    M_BUTTON(down),  M_BUTTON(left), M_BUTTON(right),
};

static const INPUT_SCHEMA m_Schema = {
    .actions = m_Actions,
    .count = ARRAY_SIZE(m_Actions),
    .size = sizeof(M_INPUT),
    .players = 1,
};

static const char m_Roles[] =
    "[roles]\n"
    "shoot = shoot\n"
    "focus = focus\n"
    "bomb = bomb\n"
    "up = up\n"
    "down = down\n"
    "left = left\n"
    "right = right\n";

static void M_Load(
    BOT *const bot, const char *const text, const BOT_STRATEGY *strategy,
    const uint16_t seed)
{
    CHECK(Result_Log(
        Bot_LoadText(bot, text, strlen(text), &m_Schema, strategy, seed),
        LOG_LEVEL_ERROR));
}

TEST(the_bot_shoots_and_dodges_a_threat)
{
    BOT bot;
    M_Load(&bot, m_Roles, &g_BotDodge, 1);
    // A bullet just right of the player and edges far away.
    const VEC2 threats[] = {
        { 5.0f, 0.5f },   { 0.0f, 200.0f },  { -200.0f, 0.0f },
        { 200.0f, 0.0f }, { 0.0f, -200.0f },
    };
    M_INPUT input;
    Bot_NextInput(&bot, threats, ARRAY_SIZE(threats), &input);
    CHECK(input.shoot);
    CHECK(input.left);
    CHECK(!input.right);
    CHECK(input.focus);
    CHECK(input.bomb);
    Bot_NextInput(&bot, threats, ARRAY_SIZE(threats), &input);
    CHECK(!input.bomb);
}

TEST(one_seed_gives_the_same_inputs)
{
    BOT a;
    BOT b;
    M_Load(&a, m_Roles, &g_BotWander, 7);
    M_Load(&b, m_Roles, &g_BotWander, 7);
    for (int32_t i = 0; i < 200; i++) {
        M_INPUT first;
        M_INPUT second;
        Bot_NextInput(&a, nullptr, 0, &first);
        Bot_NextInput(&b, nullptr, 0, &second);
        CHECK(first.up == second.up && first.down == second.down);
        CHECK(first.left == second.left && first.right == second.right);
        CHECK(first.shoot && !first.bomb && !first.focus);
    }
}

TEST(a_role_of_several_frames_plays_them_in_order)
{
    BOT bot;
    M_Load(&bot, "[roles]\nshoot = shoot+left . shoot\n", &g_BotWander, 1);
    M_INPUT input;
    Bot_NextInput(&bot, nullptr, 0, &input);
    CHECK(input.shoot && input.left && !input.right && !input.up);
    Bot_NextInput(&bot, nullptr, 0, &input);
    CHECK(!input.shoot && !input.left);
    Bot_NextInput(&bot, nullptr, 0, &input);
    CHECK(input.shoot && !input.left);
    Bot_NextInput(&bot, nullptr, 0, &input);
    CHECK(input.shoot && input.left);
}

TEST(a_role_that_bot_ini_leaves_out_presses_nothing)
{
    BOT bot;
    M_Load(&bot, "[roles]\nfocus = focus\n", &g_BotDodge, 1);
    M_INPUT input;
    Bot_NextInput(&bot, nullptr, 0, &input);
    CHECK(!input.shoot && !input.bomb);
}

TEST(an_unknown_role_or_button_fails_the_load)
{
    BOT bot;
    const char *const bad[] = {
        "[roles]\nshout = shoot\n",
        "[roles]\nshoot = shot\n",
        "[roles]\nshoot = shoot+\n",
        "[roles]\nshoot =\n",
    };
    for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
        CHECK(!Result_Ignore(Bot_LoadText(
            &bot, bad[i], strlen(bad[i]), &m_Schema, &g_BotDodge, 1)));
    }
}

TEST(strategies_are_found_by_name)
{
    CHECK(Bot_FindStrategy("dodge") == &g_BotDodge);
    CHECK(Bot_FindStrategy("wander") == &g_BotWander);
    CHECK(Bot_FindStrategy("panic") == nullptr);
}
