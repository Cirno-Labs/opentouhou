#include "core/app.h"

#include "th06/files.h"

typedef struct {
    bool shoot;
    bool bomb;
    bool focus;
    bool menu;
    bool up;
    bool down;
    bool left;
    bool right;
    bool skip;
    bool quit;
    bool unlock_end;
    bool unlock_start;
    bool confirm;
} TH06_INPUT;

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(TH06_INPUT, shoot, "z", "south", "joy_button_0"),
    INPUT_BUTTON(TH06_INPUT, bomb, "x", "east", "joy_button_1"),
    INPUT_BUTTON(
        TH06_INPUT, focus, "left_shift, right_shift", "west", "joy_button_2"),
    INPUT_BUTTON(TH06_INPUT, menu, "escape", "start", "joy_button_3"),
    INPUT_BUTTON(
        TH06_INPUT, up, "up, keypad_8, keypad_7, keypad_9",
        "dpad_up, left_stick_up", "joy_hat_up, joy_axis_1-"),
    INPUT_BUTTON(
        TH06_INPUT, down, "down, keypad_2, keypad_1, keypad_3",
        "dpad_down, left_stick_down", "joy_hat_down, joy_axis_1+"),
    INPUT_BUTTON(
        TH06_INPUT, left, "left, keypad_4, keypad_7, keypad_1",
        "dpad_left, left_stick_left", "joy_hat_left, joy_axis_0-"),
    INPUT_BUTTON(
        TH06_INPUT, right, "right, keypad_6, keypad_9, keypad_3",
        "dpad_right, left_stick_right", "joy_hat_right, joy_axis_0+"),
    INPUT_BUTTON(
        TH06_INPUT, skip, "left_ctrl, right_ctrl", "north", "joy_button_4"),
    INPUT_BUTTON(TH06_INPUT, quit, "q", nullptr, nullptr),
    INPUT_BUTTON(TH06_INPUT, unlock_end, "s", nullptr, nullptr),
    INPUT_BUTTON(TH06_INPUT, unlock_start, "home", nullptr, nullptr),
    INPUT_BUTTON(TH06_INPUT, confirm, "return", nullptr, nullptr),
};
static const INPUT_SCHEMA m_InputSchema =
    INPUT_SCHEMA_OF(TH06_INPUT, m_Actions, 1);

static RESULT M_Init(void)
{
    MUST(Files_Init(App_GetDataDir()));
    return OK;
}

static bool M_Tick(const void *const input, FRAME *const frame)
{
    return true;
}

static void M_Shutdown(void)
{
    Files_Shutdown();
}

const GAME_DESC g_GameDesc = {
    .id = "th06",
    .title = "東方紅魔郷 ~ the Embodiment of Scarlet Devil",
    .logical_width = 640,
    .logical_height = 480,
    .input_schema = &m_InputSchema,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
