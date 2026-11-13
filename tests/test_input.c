#include "core/input.h"

#include "core/base/log.h"

#include "input_backend_fake.h"
#include "test.h"

#include <string.h>

typedef struct {
    bool shoot;
    bool left;
    float move_x;
    float move_y;
} GAME_INPUT;

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(GAME_INPUT, shoot, "z", "south", "joy_button_0"),
    INPUT_BUTTON(
        GAME_INPUT, left, "left, keypad_4", "dpad_left, left_stick_left",
        "joy_hat_left"),
    INPUT_STICK(GAME_INPUT, move, "left_stick", "joy_axis_0, joy_axis_1"),
};
static const INPUT_SCHEMA m_Schema = INPUT_SCHEMA_OF(GAME_INPUT, m_Actions, 1);

static const INPUT_SCHEMA m_TwoPlayerSchema =
    INPUT_SCHEMA_OF(GAME_INPUT, m_Actions, 2);

typedef struct {
    bool fullscreen;
    bool screenshot;
} HOTKEYS;

static const INPUT_ACTION m_HotkeyActions[] = {
    INPUT_BUTTON(HOTKEYS, fullscreen, "f11", "guide", nullptr),
    INPUT_BUTTON(HOTKEYS, screenshot, "p", nullptr, nullptr),
};
static const INPUT_SCHEMA m_HotkeySchema = {
    .actions = m_HotkeyActions,
    .count = ARRAY_SIZE(m_HotkeyActions),
    .size = sizeof(HOTKEYS),
    .players = 1,
};

static INI *M_Parse(const char *const text)
{
    INI *ini;
    CHECK(Result_Log(
        Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini),
        LOG_LEVEL_ERROR));
    return ini;
}

typedef struct {
    bool confirm;
    bool up;
    bool focus;
} MENU_INPUT;

static const INPUT_ACTION m_MenuActions[] = {
    INPUT_BUTTON(MENU_INPUT, confirm, "return", "start", nullptr),
    INPUT_BUTTON(MENU_INPUT, up, "up", nullptr, nullptr),
    INPUT_BUTTON(MENU_INPUT, focus, "left_alt", nullptr, nullptr),
};
static const INPUT_SCHEMA m_MenuSchema =
    INPUT_SCHEMA_OF(MENU_INPUT, m_MenuActions, 1);

typedef struct {
    bool fullscreen;
} CHORD_HOTKEYS;

static const INPUT_ACTION m_ChordActions[] = {
    INPUT_BUTTON(
        CHORD_HOTKEYS, fullscreen, "alt+return", "back+start", nullptr),
};
static const INPUT_SCHEMA m_ChordSchema = {
    .actions = m_ChordActions,
    .count = ARRAY_SIZE(m_ChordActions),
    .size = sizeof(CHORD_HOTKEYS),
    .players = 1,
};

#define KEY_RETURN 0x28
#define KEY_UP 0x52
#define KEY_LEFT_SHIFT 0xE1
#define KEY_LEFT_ALT 0xE2
#define KEY_RIGHT_ALT 0xE6
#define BUTTON_BACK 6
#define BUTTON_START 7

static bool M_HasBindingNamed(
    const INPUT_BINDING *const bindings, const size_t count,
    const char *const name)
{
    for (size_t i = 0; i < count; i++) {
        char other[INPUT_BINDING_NAME_SIZE];
        Input_BindingName(bindings[i], other);
        if (strcmp(other, name) == 0) {
            return true;
        }
    }
    return false;
}

static void M_CheckName(const INPUT_BINDING binding, const char *const name)
{
    char actual[INPUT_BINDING_NAME_SIZE];
    Input_BindingName(binding, actual);
    CHECK_EQ_STR(actual, name);
}

static INPUT_STATE *M_CreateWithChords(void)
{
    INPUT_STATE *const input = Input_Create(&m_MenuSchema, &g_HeapAllocator);
    Input_AddSchema(input, &m_ChordSchema);
    return input;
}

static void M_UpdateWithChords(
    INPUT_STATE *const input, MENU_INPUT *const menu,
    CHORD_HOTKEYS *const hotkeys)
{
    Input_Update(input, menu);
    Input_Read(input, &m_ChordSchema, hotkeys);
}

TEST(create_sets_defaults)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INPUT_BINDING bindings[8];
    const size_t count = Input_GetBindings(input, &m_Schema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 3);
    CHECK(M_HasBindingNamed(bindings, count, "z"));
    CHECK(M_HasBindingNamed(bindings, count, "south"));
    CHECK(M_HasBindingNamed(bindings, count, "joy_button_0"));
    Input_Destroy(input);
}

TEST(update_reads_keyboard_button)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_SetKey(0x1D, true); // z
    GAME_INPUT game;
    Input_Update(input, &game);
    CHECK(game.shoot);
    FakeInput_SetKey(0x1D, false);
    Input_Update(input, &game);
    CHECK(!game.shoot);
    Input_Destroy(input);
}

TEST(update_reads_gamepad_button_for_assigned_player)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    FakeInput_SetButton(1, 0, true); // south
    GAME_INPUT game;
    Input_Update(input, &game);
    CHECK(game.shoot);
    Input_Destroy(input);
}

TEST(stick_inside_deadzone_is_zero)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    FakeInput_SetAxis(1, 0, 0.05f);
    FakeInput_SetAxis(1, 1, 0.0f);
    GAME_INPUT game;
    Input_Update(input, &game);
    CHECK_EQ_INT((int)(game.move_x * 1000), 0);
    CHECK_EQ_INT((int)(game.move_y * 1000), 0);
    Input_Destroy(input);
}

TEST(stick_at_full_lean_is_one)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    FakeInput_SetAxis(1, 0, 1.0f);
    FakeInput_SetAxis(1, 1, 0.0f);
    GAME_INPUT game;
    Input_Update(input, &game);
    CHECK_EQ_INT((int)(game.move_x * 1000), 1000);
    Input_Destroy(input);
}

TEST(stick_is_quantized)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    FakeInput_SetAxis(1, 0, 0.5f);
    FakeInput_SetAxis(1, 1, 0.0f);
    GAME_INPUT game;
    Input_Update(input, &game);
    const float step = 1.0f / 32767.0f;
    const float quotient = game.move_x / step;
    CHECK_EQ_INT(
        (int)(quotient + (quotient >= 0 ? 0.5f : -0.5f)), (long long)quotient);
    Input_Destroy(input);
}

TEST(two_player_game_assigns_controllers_in_connection_order)
{
    INPUT_STATE *const input =
        Input_Create(&m_TwoPlayerSchema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad1", "aabb");
    FakeInput_Connect(2, INPUT_DEVICE_GAMEPAD, "pad2", "ccdd");
    GAME_INPUT game[2];
    Input_Update(input, game);
    INPUT_CONTROLLER controllers[4];
    const size_t count = Input_ListControllers(input, controllers, 4);
    CHECK_EQ_INT((long long)count, 2);
    CHECK_EQ_INT(controllers[0].player, 0);
    CHECK_EQ_INT(controllers[1].player, 1);
    Input_Destroy(input);
}

TEST(assign_controller_overrides_the_default)
{
    INPUT_STATE *const input =
        Input_Create(&m_TwoPlayerSchema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad1", "aabb");
    GAME_INPUT game[2];
    Input_Update(input, game);
    Input_AssignController(input, 1, 1);
    INPUT_CONTROLLER controllers[4];
    Input_ListControllers(input, controllers, 4);
    CHECK_EQ_INT(controllers[0].player, 1);
    Input_Destroy(input);
}

TEST(bind_adds_and_unbind_removes)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    const INPUT_BINDING binding = { INPUT_DEVICE_KEYBOARD, 1, { 0x1B } }; // x
    Input_Bind(input, &m_Schema, 0, 0, binding);
    INPUT_BINDING bindings[8];
    size_t count = Input_GetBindings(input, &m_Schema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 4);
    Input_Unbind(input, &m_Schema, 0, 0, binding);
    count = Input_GetBindings(input, &m_Schema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 3);
    Input_Destroy(input);
}

TEST(reset_bindings_restores_defaults)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    const INPUT_BINDING binding = { INPUT_DEVICE_KEYBOARD, 1, { 0x1B } }; // x
    Input_Bind(input, &m_Schema, 0, 0, binding);
    Input_ResetBindings(input, &m_Schema, 0);
    INPUT_BINDING bindings[8];
    const size_t count = Input_GetBindings(input, &m_Schema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 3);
    Input_Destroy(input);
}

TEST(load_reads_bindings_and_leaves_missing_keys_at_defaults)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INI *const ini = M_Parse("[input.keyboard]\nshoot = x\n");
    Input_Load(input, ini);
    INPUT_BINDING bindings[8];
    size_t count = Input_GetBindings(input, &m_Schema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 3);
    CHECK(M_HasBindingNamed(bindings, count, "x"));
    CHECK(M_HasBindingNamed(bindings, count, "south"));
    count = Input_GetBindings(input, &m_Schema, 0, 1, bindings, 8);
    CHECK(count > 0);
    Ini_Free(ini);
    Input_Destroy(input);
}

TEST(load_keeps_eight_keyboard_bindings_beside_the_pad_defaults)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INI *const ini =
        M_Parse("[input.keyboard]\nshoot = a, b, c, d, e, f, g, h, i\n");
    Input_Load(input, ini);
    INPUT_BINDING bindings[32];
    const size_t count =
        Input_GetBindings(input, &m_Schema, 0, 0, bindings, 32);
    CHECK_EQ_INT((long long)count, 10);
    CHECK(M_HasBindingNamed(bindings, count, "h"));
    CHECK(!M_HasBindingNamed(bindings, count, "i"));
    CHECK(M_HasBindingNamed(bindings, count, "south"));
    CHECK(M_HasBindingNamed(bindings, count, "joy_button_0"));
    INI *const saved = M_Parse("");
    Input_Save(input, saved);
    CHECK_EQ_STR(
        Ini_Get(saved, "input.keyboard", "shoot"), "a, b, c, d, e, f, g, h");
    Ini_Free(saved);
    Ini_Free(ini);
    Input_Destroy(input);
}

TEST(load_of_empty_value_clears_bindings)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INI *const ini = M_Parse("[input.keyboard]\nshoot = \n");
    Input_Load(input, ini);
    INPUT_BINDING bindings[8];
    const size_t count = Input_GetBindings(input, &m_Schema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 2);
    CHECK(!M_HasBindingNamed(bindings, count, "z"));
    Ini_Free(ini);
    Input_Destroy(input);
}

TEST(save_then_load_round_trips_bindings)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INI *const ini = M_Parse("");
    Input_Save(input, ini);
    INPUT_STATE *const reloaded = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_Load(reloaded, ini);
    INPUT_BINDING a[8];
    INPUT_BINDING b[8];
    const size_t ca = Input_GetBindings(input, &m_Schema, 0, 1, a, 8);
    const size_t cb = Input_GetBindings(reloaded, &m_Schema, 0, 1, b, 8);
    CHECK_EQ_INT((long long)ca, (long long)cb);
    for (size_t i = 0; i < ca; i++) {
        CHECK_EQ_INT(a[i].device, b[i].device);
        CHECK_EQ_INT(a[i].count, b[i].count);
        CHECK_EQ_INT(a[i].codes[0], b[i].codes[0]);
    }
    Ini_Free(ini);
    Input_Destroy(input);
    Input_Destroy(reloaded);
}

TEST(debounce_reports_only_new_presses)
{
    GAME_INPUT previous = { .shoot = true, .left = false };
    GAME_INPUT current = { .shoot = true, .left = true };
    GAME_INPUT out;
    Input_Debounce(&m_Schema, &previous, &current, &out);
    CHECK(!out.shoot);
    CHECK(out.left);
    CHECK_EQ_INT((int)out.move_x, 0);
}

TEST(binding_name_round_trips)
{
    INPUT_BINDING binding;
    CHECK(Input_BindingFromName("left_shift", &binding));
    M_CheckName(binding, "left_shift");
    CHECK(Input_BindingFromName("south", &binding));
    M_CheckName(binding, "south");
    CHECK(Input_BindingFromName("joy_axis_2+", &binding));
    M_CheckName(binding, "joy_axis_2+");
    CHECK(Input_BindingFromName("joy_hat_left", &binding));
    M_CheckName(binding, "joy_hat_left");
    CHECK(Input_BindingFromName("joy_button_3", &binding));
    M_CheckName(binding, "joy_button_3");
    CHECK(!Input_BindingFromName("not_a_real_binding", &binding));
}

TEST(get_device_isolates_keyboard_from_gamepad)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_SetKey(0x1D, true); // z
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    GAME_INPUT game;
    Input_Update(input, &game);
    CHECK(game.shoot);
    GAME_INPUT gamepad_only;
    Input_GetDevice(input, INPUT_DEVICE_GAMEPAD, &gamepad_only);
    CHECK(!gamepad_only.shoot);
    Input_Destroy(input);
}

TEST(last_pressed_reports_a_new_key)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INPUT_BINDING out;
    CHECK(!Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &out));
    FakeInput_SetKey(0x1B, true); // x
    CHECK(Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &out));
    M_CheckName(out, "x");
    CHECK(!Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &out));
    FakeInput_SetKey(0x1B, false);
    Input_Destroy(input);
}

TEST(read_gives_an_added_schema_from_the_same_poll)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_AddSchema(input, &m_HotkeySchema);
    FakeInput_SetKey(0x1D, true); // z
    FakeInput_SetKey(0x13, true); // p
    GAME_INPUT game;
    Input_Update(input, &game);
    HOTKEYS hotkeys;
    Input_Read(input, &m_HotkeySchema, &hotkeys);
    CHECK(game.shoot);
    CHECK(hotkeys.screenshot);
    CHECK(!hotkeys.fullscreen);
    GAME_INPUT first;
    Input_Read(input, &m_Schema, &first);
    CHECK(first.shoot);
    FakeInput_SetKey(0x1D, false);
    FakeInput_SetKey(0x13, false);
    Input_Destroy(input);
}

TEST(binding_functions_name_the_schema)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_AddSchema(input, &m_HotkeySchema);
    INPUT_BINDING binding;
    CHECK(Input_BindingFromName("x", &binding));
    Input_Bind(input, &m_HotkeySchema, 0, 1, binding);
    INPUT_BINDING bindings[8];
    size_t count = Input_GetBindings(input, &m_HotkeySchema, 0, 1, bindings, 8);
    CHECK_EQ_INT((long long)count, 2);
    CHECK(M_HasBindingNamed(bindings, count, "x"));
    count = Input_GetBindings(input, &m_Schema, 0, 1, bindings, 8);
    CHECK(!M_HasBindingNamed(bindings, count, "x"));
    Input_Bind(input, &m_Schema, 0, 1, binding);
    count = Input_GetBindings(input, &m_Schema, 0, 1, bindings, 8);
    CHECK(M_HasBindingNamed(bindings, count, "x"));
    Input_Unbind(input, &m_HotkeySchema, 0, 1, binding);
    count = Input_GetBindings(input, &m_HotkeySchema, 0, 1, bindings, 8);
    CHECK_EQ_INT((long long)count, 1);
    count = Input_GetBindings(input, &m_Schema, 0, 1, bindings, 8);
    CHECK(M_HasBindingNamed(bindings, count, "x"));
    Input_Bind(input, &m_HotkeySchema, 0, 0, binding);
    Input_ResetBindings(input, &m_HotkeySchema, 0);
    count = Input_GetBindings(input, &m_HotkeySchema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 2);
    CHECK(!M_HasBindingNamed(bindings, count, "x"));
    count = Input_GetBindings(input, &m_Schema, 0, 1, bindings, 8);
    CHECK(M_HasBindingNamed(bindings, count, "x"));
    Input_Destroy(input);
}

TEST(every_schema_saves_into_the_input_sections)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_AddSchema(input, &m_HotkeySchema);
    INPUT_BINDING binding;
    CHECK(Input_BindingFromName("f12", &binding));
    Input_Bind(input, &m_HotkeySchema, 0, 1, binding);
    INI *const ini = M_Parse("");
    Input_Save(input, ini);
    CHECK_EQ_STR(Ini_Get(ini, "input.keyboard", "screenshot"), "p, f12");
    CHECK_EQ_STR(Ini_Get(ini, "input.gamepad", "fullscreen"), "guide");
    CHECK_EQ_STR(Ini_Get(ini, "input.keyboard", "shoot"), "z");
    CHECK(Ini_Get(ini, "hotkeys.keyboard", "screenshot") == nullptr);

    INPUT_STATE *const reloaded = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_AddSchema(reloaded, &m_HotkeySchema);
    Input_Load(reloaded, ini);
    INPUT_BINDING bindings[8];
    size_t count =
        Input_GetBindings(reloaded, &m_HotkeySchema, 0, 1, bindings, 8);
    CHECK_EQ_INT((long long)count, 2);
    CHECK(M_HasBindingNamed(bindings, count, "f12"));
    count = Input_GetBindings(reloaded, &m_Schema, 0, 0, bindings, 8);
    CHECK(M_HasBindingNamed(bindings, count, "z"));
    Ini_Free(ini);
    Input_Destroy(input);
    Input_Destroy(reloaded);
}

TEST(chord_is_held_when_modifiers_go_down_first)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    Input_Destroy(input);
}

TEST(chord_is_held_when_every_part_goes_down_in_one_update)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    Input_Destroy(input);
}

TEST(chord_on_gamepad_is_held)
{
    INPUT_STATE *const input = M_CreateWithChords();
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetButton(1, BUTTON_BACK, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    FakeInput_SetButton(1, BUTTON_START, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    CHECK(!menu.confirm);
    Input_Destroy(input);
}

TEST(chord_does_not_start_when_trigger_goes_down_first)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(menu.confirm);
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);
    CHECK(menu.confirm);
    Input_Destroy(input);
}

TEST(releasing_any_part_ends_the_chord)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    FakeInput_SetKey(KEY_LEFT_ALT, false);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);

    FakeInput_SetKey(KEY_RETURN, false);
    M_UpdateWithChords(input, &menu, &hotkeys);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    FakeInput_SetKey(KEY_RETURN, false);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);
    Input_Destroy(input);
}

TEST(chord_suppresses_its_parts_in_other_schemas)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(menu.focus);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    CHECK(!menu.confirm);
    CHECK(!menu.focus);
    FakeInput_SetKey(KEY_LEFT_ALT, false);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(!hotkeys.fullscreen);
    CHECK(menu.confirm);
    Input_Destroy(input);
}

TEST(held_keys_without_a_chord_binding_suppress_nothing)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_LEFT_SHIFT, true);
    FakeInput_SetKey(KEY_UP, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(menu.up);
    Input_Destroy(input);
}

TEST(generic_modifier_matches_either_side)
{
    INPUT_STATE *const input = M_CreateWithChords();
    MENU_INPUT menu;
    CHORD_HOTKEYS hotkeys;
    FakeInput_SetKey(KEY_RIGHT_ALT, true);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    CHECK(!menu.confirm);
    FakeInput_SetKey(KEY_RIGHT_ALT, false);
    FakeInput_SetKey(KEY_RETURN, false);
    M_UpdateWithChords(input, &menu, &hotkeys);
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    FakeInput_SetKey(KEY_RETURN, true);
    M_UpdateWithChords(input, &menu, &hotkeys);
    CHECK(hotkeys.fullscreen);
    Input_Destroy(input);
}

TEST(chord_name_round_trips)
{
    static const char *const names[] = {
        "alt+return",
        "back+start",
        "ctrl+shift+q",
        "left_ctrl+print_screen",
        "joy_axis_2++joy_button_1",
        "joy_hat_up+joy_axis_0-",
    };
    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        INPUT_BINDING binding;
        CHECK(Input_BindingFromName(names[i], &binding));
        M_CheckName(binding, names[i]);
        INPUT_BINDING again;
        char name[INPUT_BINDING_NAME_SIZE];
        Input_BindingName(binding, name);
        CHECK(Input_BindingFromName(name, &again));
        CHECK_EQ_INT(again.device, binding.device);
        CHECK_EQ_INT(again.count, binding.count);
        CHECK(memcmp(again.codes, binding.codes, binding.count * 2) == 0);
    }
    INPUT_BINDING binding;
    CHECK(Input_BindingFromName(" ctrl + shift + q ", &binding));
    M_CheckName(binding, "ctrl+shift+q");
    CHECK_EQ_INT(binding.count, 3);
}

TEST(binding_from_name_rejects_bad_chords)
{
    INPUT_BINDING binding;
    CHECK(!Input_BindingFromName("alt+south", &binding));
    CHECK(!Input_BindingFromName("back+joy_button_0", &binding));
    CHECK(Input_BindingFromName("a+b+c+d", &binding));
    CHECK(!Input_BindingFromName("a+b+c+d+e", &binding));
    CHECK(!Input_BindingFromName("alt+", &binding));
    CHECK(!Input_BindingFromName("", &binding));
}

TEST(last_pressed_reports_held_modifiers)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    INPUT_BINDING out;
    FakeInput_SetKey(KEY_LEFT_ALT, true);
    CHECK(Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &out));
    M_CheckName(out, "left_alt");
    FakeInput_SetKey(KEY_RETURN, true);
    CHECK(Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &out));
    M_CheckName(out, "alt+return");
    FakeInput_SetKey(KEY_RETURN, false);
    FakeInput_SetKey(KEY_LEFT_SHIFT, true);
    CHECK(Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &out));
    M_CheckName(out, "alt+left_shift");
    Input_Destroy(input);
}

TEST(last_pressed_reports_held_buttons)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "aabb");
    INPUT_BINDING out;
    FakeInput_SetButton(1, BUTTON_BACK, true);
    CHECK(Input_LastPressed(input, INPUT_DEVICE_GAMEPAD, &out));
    M_CheckName(out, "back");
    FakeInput_SetButton(1, BUTTON_START, true);
    CHECK(Input_LastPressed(input, INPUT_DEVICE_GAMEPAD, &out));
    M_CheckName(out, "back+start");
    Input_Destroy(input);
}

TEST(load_reads_chords_and_skips_chords_with_a_whole_stick)
{
    INPUT_STATE *const input = Input_Create(&m_Schema, &g_HeapAllocator);
    Input_AddSchema(input, &m_ChordSchema);
    INI *const ini = M_Parse(
        "[input.keyboard]\nfullscreen = ctrl+f, alt+return\n"
        "[input.gamepad]\nmove = back+left_stick, right_stick\n");
    Input_Load(input, ini);
    INPUT_BINDING bindings[8];
    size_t count = Input_GetBindings(input, &m_ChordSchema, 0, 0, bindings, 8);
    CHECK_EQ_INT((long long)count, 3);
    CHECK(M_HasBindingNamed(bindings, count, "ctrl+f"));
    CHECK(M_HasBindingNamed(bindings, count, "alt+return"));
    count = Input_GetBindings(input, &m_Schema, 0, 2, bindings, 8);
    CHECK(M_HasBindingNamed(bindings, count, "right_stick"));
    CHECK(!M_HasBindingNamed(bindings, count, "back+left_stick"));
    Ini_Free(ini);
    Input_Destroy(input);
}
