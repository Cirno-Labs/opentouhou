#include "bot.h"

#include "test.h"

#include "core/base/utils.h"

#include <stddef.h>

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

TEST(the_bot_shoots_and_dodges_a_threat)
{
    BOT bot;
    Bot_Init(&bot, &m_Schema, 1, false);
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
    Bot_Init(&a, &m_Schema, 7, true);
    Bot_Init(&b, &m_Schema, 7, true);
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
