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

static void M_Load(BOT *const bot, const char *const text)
{
    CHECK(Result_Log(
        Bot_LoadText(bot, text, strlen(text), &m_Schema), LOG_LEVEL_ERROR));
}

TEST(a_hold_presses_the_buttons_of_its_roles)
{
    BOT bot;
    M_Load(&bot, m_Roles);
    M_INPUT input;
    const uint32_t roles = 1u << BOT_ROLE_shoot | 1u << BOT_ROLE_left;
    Bot_WriteInput(&bot, roles, 5, &input);
    CHECK(input.shoot && input.left);
    CHECK(!input.right && !input.bomb && !input.focus);
}

TEST(a_role_of_several_frames_leads_the_hold)
{
    BOT bot;
    M_Load(&bot, "[roles]\nbomb = bomb+left . bomb\nshoot = shoot\n");
    const uint32_t roles = 1u << BOT_ROLE_bomb | 1u << BOT_ROLE_shoot;
    CHECK_EQ_INT(Bot_GetLeadFrames(&bot, roles), 3);
    CHECK_EQ_INT(Bot_GetLeadFrames(&bot, 1u << BOT_ROLE_shoot), 0);
    M_INPUT input;
    Bot_WriteInput(&bot, roles, 0, &input);
    CHECK(input.bomb && input.left && !input.shoot);
    Bot_WriteInput(&bot, roles, 1, &input);
    CHECK(!input.bomb && !input.left && !input.shoot);
    Bot_WriteInput(&bot, roles, 2, &input);
    CHECK(input.bomb && !input.shoot);
    Bot_WriteInput(&bot, roles, 3, &input);
    CHECK(input.shoot && !input.bomb);
}

TEST(a_role_that_bot_ini_leaves_out_presses_nothing)
{
    BOT bot;
    M_Load(&bot, "[roles]\nfocus = focus\nbomb = .\n");
    CHECK_EQ_INT(Bot_GetRoles(&bot), 1u << BOT_ROLE_focus);
    M_INPUT input;
    Bot_WriteInput(&bot, 1u << BOT_ROLE_shoot | 1u << BOT_ROLE_bomb, 0, &input);
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
        CHECK(!Result_Ignore(
            Bot_LoadText(&bot, bad[i], strlen(bad[i]), &m_Schema)));
    }
}
