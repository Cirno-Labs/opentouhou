#include "shell/game/request_queue.h"

#include "core/base/log.h"

#include "input_backend_fake.h"
#include "test.h"

#include <string.h>

typedef struct {
    bool shoot;
    bool bomb;
} GAME_INPUT;

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(GAME_INPUT, shoot, "z", "south", "joy_button_0"),
    INPUT_BUTTON(GAME_INPUT, bomb, "x", "east", "joy_button_1"),
};
static const INPUT_SCHEMA m_Schema = INPUT_SCHEMA_OF(GAME_INPUT, m_Actions, 1);

static INPUT_BINDING M_Binding(const char *const name)
{
    INPUT_BINDING binding = {};
    CHECK(Input_BindingFromName(name, &binding));
    return binding;
}

static void M_CheckName(const INPUT_BINDING binding, const char *const name)
{
    char out[INPUT_BINDING_NAME_SIZE];
    Input_BindingName(binding, out);
    CHECK_EQ_STR(out, name);
}

// Writes the names of the bindings of action, joined by ", ".
static void M_CheckBindings(
    const INPUT_STATE *const input, const size_t action,
    const char *const names)
{
    INPUT_BINDING bindings[16];
    const size_t count =
        Input_GetBindings(input, &m_Schema, 0, action, bindings, 16);
    char joined[256] = "";
    for (size_t i = 0; i < count; i++) {
        char name[INPUT_BINDING_NAME_SIZE];
        Input_BindingName(bindings[i], name);
        if (i != 0) {
            strcat(joined, ", ");
        }
        strcat(joined, name);
    }
    CHECK_EQ_STR(joined, names);
}

TEST(a_pad_press_gives_its_one_control)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    INPUT_BINDING press;
    CHECK(!ShellRequests_ReadPadPress(input, &press));
    FakeInput_SetButton(1, 0, true);
    CHECK(ShellRequests_ReadPadPress(input, &press));
    M_CheckName(press, "south");
    CHECK(!ShellRequests_ReadPadPress(input, &press));
    FakeInput_SetButton(1, 1, true);
    CHECK(ShellRequests_ReadPadPress(input, &press));
    M_CheckName(press, "east");
    FakeInput_Disconnect(1);
    Input_Destroy(input);
}

TEST(press_snapshots_keep_unbound_keys_until_a_tick)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    SHELL_INPUT_PRESSES presses = {};
    ShellRequests_PollPresses(&presses, input);
    CHECK(!presses.key_pending && !presses.pad_pending);
    FakeInput_SetKey(4, true);
    ShellRequests_PollPresses(&presses, input);
    CHECK(presses.key_pending);
    M_CheckName(presses.key, "a");
    FakeInput_SetKey(4, false);
    ShellRequests_PollPresses(&presses, input);
    CHECK(presses.key_pending);
    FakeInput_SetKey(5, true);
    ShellRequests_PollPresses(&presses, input);
    M_CheckName(presses.key, "a");
    ShellRequests_ClearPresses(&presses);
    CHECK(!presses.key_pending && !presses.pad_pending);
    ShellRequests_PollPresses(&presses, input);
    CHECK(!presses.key_pending);
    FakeInput_SetKey(5, false);
    ShellRequests_PollPresses(&presses, input);
    FakeInput_SetKey(5, true);
    ShellRequests_PollPresses(&presses, input);
    CHECK(presses.key_pending);
    M_CheckName(presses.key, "b");
    Input_Destroy(input);
}

TEST(keyboard_and_pad_snapshots_are_independent)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    SHELL_INPUT_PRESSES presses = {};
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    FakeInput_SetKey(0xe0, true);
    FakeInput_SetKey(4, true);
    FakeInput_SetButton(1, 0, true);
    ShellRequests_PollPresses(&presses, input);
    CHECK(presses.key_pending && presses.pad_pending);
    M_CheckName(presses.key, "ctrl+a");
    M_CheckName(presses.pad, "south");
    ShellRequests_ClearPresses(&presses);
    CHECK(!presses.key_pending && !presses.pad_pending);
    ShellRequests_PollPresses(&presses, input);
    CHECK(!presses.key_pending && !presses.pad_pending);
    FakeInput_SetKey(0xe1, true);
    ShellRequests_PollPresses(&presses, input);
    CHECK(presses.key_pending && !presses.pad_pending);
    M_CheckName(presses.key, "ctrl+left_shift");
    Input_Destroy(input);
}

TEST(an_ordinary_key_replaces_a_pending_modifier_or_lock)
{
    static const int prefixes[] = { 0xe1, 0x39, 0x47, 0x53 };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(*prefixes); i++) {
        INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
        SHELL_INPUT_PRESSES presses = {};
        FakeInput_SetKey(prefixes[i], true);
        ShellRequests_PollPresses(&presses, input);
        CHECK(presses.key_pending);
        CHECK_EQ_INT(presses.key.codes[presses.key.count - 1], prefixes[i]);
        FakeInput_SetKey(4, true);
        ShellRequests_PollPresses(&presses, input);
        CHECK_EQ_INT(presses.key.codes[presses.key.count - 1], 4);
        FakeInput_SetKey(4, false);
        ShellRequests_PollPresses(&presses, input);
        CHECK_EQ_INT(presses.key.codes[presses.key.count - 1], 4);
        ShellRequests_ClearPresses(&presses);
        CHECK(!presses.key_pending);
        Input_Destroy(input);
    }
}

TEST(a_pending_ordinary_key_is_kept_when_a_modifier_follows)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    SHELL_INPUT_PRESSES presses = {};
    FakeInput_SetKey(4, true);
    ShellRequests_PollPresses(&presses, input);
    FakeInput_SetKey(4, false);
    FakeInput_SetKey(0xe1, true);
    ShellRequests_PollPresses(&presses, input);
    M_CheckName(presses.key, "a");
    ShellRequests_ClearPresses(&presses);
    ShellRequests_PollPresses(&presses, input);
    CHECK(!presses.key_pending);
    Input_Destroy(input);
}

TEST(a_joystick_press_counts_and_a_key_does_not)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(2, INPUT_DEVICE_JOYSTICK, "stick", "ccdd");
    INPUT_BINDING press;
    FakeInput_SetKey(4, true);
    CHECK(!ShellRequests_ReadPadPress(input, &press));
    FakeInput_SetButton(2, 7, true);
    CHECK(ShellRequests_ReadPadPress(input, &press));
    M_CheckName(press, "joy_button_7");
    FakeInput_SetKey(4, false);
    FakeInput_Disconnect(2);
    Input_Destroy(input);
}

TEST(requests_apply_in_order_after_the_queue)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    SHELL_REQUESTS requests = { .fullscreen = -1 };
    ShellRequests_Add(
        &requests,
        (SHELL_REQUEST) { SHELL_REQUEST_UNBIND, 0, M_Binding("south") });
    ShellRequests_Add(
        &requests,
        (SHELL_REQUEST) { SHELL_REQUEST_BIND, 0, M_Binding("east") });
    ShellRequests_Add(
        &requests,
        (SHELL_REQUEST) { SHELL_REQUEST_UNBIND, 1, M_Binding("east") });
    ShellRequests_Add(
        &requests,
        (SHELL_REQUEST) { SHELL_REQUEST_BIND, 1, M_Binding("south") });
    M_CheckBindings(input, 0, "z, south, joy_button_0");
    ShellRequests_ApplyInput(&requests, input, &m_Schema);
    CHECK_EQ_INT(requests.count, 0);
    M_CheckBindings(input, 0, "z, joy_button_0, east");
    M_CheckBindings(input, 1, "x, joy_button_1, south");
    Input_Destroy(input);
}

TEST(the_pad_reset_keeps_the_keyboard)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_Unbind(input, &m_Schema, 0, 0, M_Binding("z"));
    Input_Bind(input, &m_Schema, 0, 0, M_Binding("c"));
    Input_Unbind(input, &m_Schema, 0, 0, M_Binding("south"));
    Input_Bind(input, &m_Schema, 0, 0, M_Binding("north"));
    SHELL_REQUESTS requests = { .fullscreen = -1 };
    ShellRequests_Add(
        &requests, (SHELL_REQUEST) { .type = SHELL_REQUEST_RESET_PAD });
    ShellRequests_ApplyInput(&requests, input, &m_Schema);
    M_CheckBindings(input, 0, "south, joy_button_0, c");
    M_CheckBindings(input, 1, "east, joy_button_1, x");
    Input_Destroy(input);
}

TEST(a_full_queue_drops_further_requests)
{
    SHELL_REQUESTS requests = { .fullscreen = -1 };
    for (int32_t i = 0; i < SHELL_REQUEST_COUNT + 3; i++) {
        ShellRequests_Add(
            &requests, (SHELL_REQUEST) { .type = SHELL_REQUEST_RESET_PAD });
    }
    CHECK_EQ_INT(requests.count, SHELL_REQUEST_COUNT);
}
