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
