#include "th06/screens/main.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/screen_effect.h"
#include "th06/screens/result.h"
#include "th06/settings.h"
#include "th06/sound.h"
#include "th06/strings.h"

#include "core/app.h"
#include "core/base/log.h"
#include "core/base/utils.h"

#include <stdio.h>
#include <string.h>

#define M_CALC_PRIORITY 2
#define M_DRAW_PRIORITY 0
#define M_VM_COUNT 0x7a
#define M_DRAWN_VM_COUNT 0x62
#define M_IDLE_FRAMES 720
#define M_TITLE_MUSIC "bgm/th06_01.mid"
#define M_MAPPING_COUNT 9

#define M_UP 0x10
#define M_DOWN 0x20
#define M_LEFT 0x40
#define M_RIGHT 0x80
#define M_QUIT 0x200
#define M_CONFIRM 0x1001
#define M_CANCEL 0xa
#define M_OPENING 0x160b

typedef enum {
    M_STATE_START = 0,
    M_STATE_WAIT_FOR_KEY = 1,
    M_STATE_TITLE_MENU = 2,
    M_STATE_OPTIONS = 3,
    M_STATE_QUIT = 4,
    M_STATE_PAD_CONFIG = 5,
    M_STATE_OPEN_SELECT = 6,
    M_STATE_DIFFICULTY = 7,
    M_STATE_CLOSE_SELECT = 8,
    M_STATE_CHARACTER = 9,
    M_STATE_SCORES = 10,
    M_STATE_SHOT = 11,
    M_STATE_OPEN_REPLAYS = 12,
    M_STATE_REPLAYS = 13,
    M_STATE_CLOSE_REPLAYS = 14,
    M_STATE_REPLAY_STAGE = 15,
    M_STATE_MUSIC_ROOM = 16,
    M_STATE_PRACTICE_STAGE = 17,
} M_STATE;

// Holds a pad button that a row of the pad button configuration shows.
typedef struct {
    bool bound;
    INPUT_BINDING binding;
} M_PAD_BUTTON;

// Holds the rows of the pad button configuration, in the original's order,
// which is also the order of the first actions of g_InputSchema: shoot,
// bomb, focus, menu, up, down, left, right, skip.
typedef struct {
    M_PAD_BUTTON buttons[M_MAPPING_COUNT];
} M_MAPPING;

typedef struct {
    ANM_VM sprites[M_VM_COUNT];
    int32_t cursor;
    int32_t replay_cursor;
    int32_t replay_count;
    M_STATE state;
    int32_t state_frames;
    int32_t idle_frames;
    uint32_t fade_from_color;
    uint32_t fade_to_color;
    uint32_t selected_color;
    uint32_t unselected_color;
    int32_t fade_frame;
    int32_t fade_in_frames;
    int32_t fade_out_frames;
    M_MAPPING mapping;
    // Holds the rows that the bindings had when the screen opened or when
    // the defaults were chosen, and whether they were.
    M_MAPPING committed;
    bool reset_pad;
    // Holds the window mode that the options show: 0 for fullscreen, 1 for
    // windowed.
    int32_t pending_windowed;
    SAVED_REPLAY replays[SAVED_REPLAY_COUNT];
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    bool added;
} M_MENU;

// Names the gamepad buttons that the configuration assigns, in the order
// of their codes, so that it shows each as a number, as the original shows
// its pad's button numbers. The d-pad is left out: like the sticks and the
// hats, it moves the cursor only, as the pad's directions do in the
// original.
static const char *const m_GamepadButtons[] = {
    "south",
    "east",
    "west",
    "north",
    "left_shoulder",
    "right_shoulder",
    "back",
    "start",
    "guide",
    "left_stick_press",
    "right_stick_press",
};

static const STRING m_StageNames[7] = {
    STRING_MENU_STAGE_1,     STRING_MENU_STAGE_2, STRING_MENU_STAGE_3,
    STRING_MENU_STAGE_4,     STRING_MENU_STAGE_5, STRING_MENU_STAGE_6,
    STRING_MENU_STAGE_EXTRA,
};

static const STRING m_ShotTypeNames[4] = {
    STRING_MENU_SHOT_TYPE_REIMU_A,
    STRING_MENU_SHOT_TYPE_REIMU_B,
    STRING_MENU_SHOT_TYPE_MARISA_A,
    STRING_MENU_SHOT_TYPE_MARISA_B,
};

static const STRING m_DifficultyNames[5] = {
    STRING_MENU_DIFFICULTY_EASY,  STRING_MENU_DIFFICULTY_NORMAL,
    STRING_MENU_DIFFICULTY_HARD,  STRING_MENU_DIFFICULTY_LUNATIC,
    STRING_MENU_DIFFICULTY_EXTRA,
};

static M_MENU m_Menu;
// Remembers that the first title played its music. The original plays it
// on the first start only after its three-second wait.
static bool m_MusicStarted;

static bool M_Pressed(const uint16_t mask)
{
    return Buttons_WasPressed(&g_Buttons, mask);
}

static bool M_PressedOrRepeated(const uint16_t mask)
{
    return Buttons_WasPressedOrRepeated(&g_Buttons, mask);
}

static void M_Interrupt(M_MENU *const menu, const int16_t interrupt)
{
    for (int32_t i = 0; i < M_VM_COUNT; i++) {
        menu->sprites[i].pending_interrupt = interrupt;
    }
}

static bool M_HasTexture(const ANM_VM *const vm)
{
    return vm->sprite_data != nullptr && vm->sprite_data->slot >= 0
        && Anm_GetTexture(vm->sprite_data->slot) != nullptr;
}

// Returns the window mode as the original's windowed setting: 0 for
// fullscreen, 1 for windowed.
static int32_t M_CurrentWindowed(void)
{
    return App_IsFullscreen() ? 0 : 1;
}

static bool M_AnyExtraUnlocked(void)
{
    return GameManager_IsExtraUnlocked(0, 0)
        || GameManager_IsExtraUnlocked(0, 1)
        || GameManager_IsExtraUnlocked(1, 0)
        || GameManager_IsExtraUnlocked(1, 1);
}

static int32_t M_Shot(void)
{
    return g_GameManager.run.character * 2 + g_GameManager.run.shot_type;
}

// Returns the number of practice stages: the stages cleared, at most six,
// and five on Easy, which ends at the fifth.
static int32_t M_PracticeStages(void)
{
    const int32_t cleared = g_GameManager.records.clears[M_Shot()]
                                .with_continues[g_GameManager.run.difficulty];
    int32_t stages = cleared > 6 ? 6 : cleared;
    if (g_GameManager.run.difficulty == 0 && stages == 6) {
        stages = 5;
    }
    return stages;
}

// Returns -1 when the cursor moved up, 1 when it moved down, else 0. Up
// takes precedence over down.
static int32_t M_MoveCursor(M_MENU *const menu, const int32_t count)
{
    int32_t moved = 0;
    if (M_PressedOrRepeated(M_UP)) {
        menu->cursor--;
        moved = -1;
    } else if (M_PressedOrRepeated(M_DOWN)) {
        menu->cursor++;
        moved = 1;
    } else {
        return 0;
    }
    Sound_Queue(SOUND_SELECT);
    if (menu->cursor < 0) {
        menu->cursor = count - 1;
    }
    if (menu->cursor >= count) {
        menu->cursor = 0;
    }
    return moved;
}

// Returns the number that the configuration shows for a pad button, from
// 0 to 99, or -1 for a binding that it cannot show.
static int32_t M_PadNumber(const INPUT_BINDING binding)
{
    if (binding.device == INPUT_DEVICE_KEYBOARD || binding.count != 1) {
        return -1;
    }
    char name[INPUT_BINDING_NAME_SIZE];
    Input_BindingName(binding, name);
    int32_t number;
    char extra;
    if (sscanf(name, "joy_button_%d%c", &number, &extra) == 1) {
        return number <= 99 ? number : -1;
    }
    for (size_t i = 0; i < ARRAY_SIZE(m_GamepadButtons); i++) {
        if (strcmp(name, m_GamepadButtons[i]) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static bool M_SameButton(
    const M_PAD_BUTTON *const a, const M_PAD_BUTTON *const b)
{
    return a->bound == b->bound
        && (!a->bound
            || (a->binding.device == b->binding.device
                && a->binding.codes[0] == b->binding.codes[0]));
}

// Returns the first pad button of bindings that the configuration can show.
static M_PAD_BUTTON M_FirstPadButton(
    const INPUT_BINDING *const bindings, const size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (M_PadNumber(bindings[i]) >= 0) {
            return (M_PAD_BUTTON) { true, bindings[i] };
        }
    }
    return (M_PAD_BUTTON) {};
}

static M_MAPPING M_CurrentMapping(void)
{
    M_MAPPING mapping = {};
    for (size_t action = 0; action < M_MAPPING_COUNT; action++) {
        INPUT_BINDING bindings[16];
        size_t count = App_GetBindings(action, bindings, ARRAY_SIZE(bindings));
        if (count > ARRAY_SIZE(bindings)) {
            count = ARRAY_SIZE(bindings);
        }
        mapping.buttons[action] = M_FirstPadButton(bindings, count);
    }
    return mapping;
}

// Returns the rows of the default bindings of g_InputSchema: the gamepad's
// first button, or else the joystick's.
static M_MAPPING M_DefaultMapping(void)
{
    M_MAPPING mapping = {};
    for (size_t action = 0; action < M_MAPPING_COUNT; action++) {
        const INPUT_ACTION *const entry = &g_InputSchema.actions[action];
        INPUT_BINDING bindings[16];
        size_t count = 0;
        const INPUT_DEVICE devices[] = {
            INPUT_DEVICE_GAMEPAD,
            INPUT_DEVICE_JOYSTICK,
        };
        for (size_t d = 0; d < ARRAY_SIZE(devices); d++) {
            const char *names = entry->defaults[devices[d]];
            while (names != nullptr && *names != 0 && count < 16) {
                const char *const end = strchr(names, ',');
                const size_t length =
                    end != nullptr ? (size_t)(end - names) : strlen(names);
                char name[INPUT_BINDING_NAME_SIZE] = {};
                memcpy(
                    name, names,
                    length < sizeof(name) - 1 ? length : sizeof(name) - 1);
                if (Input_BindingFromName(name, &bindings[count])) {
                    count++;
                }
                names = end != nullptr ? end + 1 : nullptr;
            }
        }
        mapping.buttons[action] = M_FirstPadButton(bindings, count);
    }
    return mapping;
}

// Gives every row that shows button the replacement, as the original's
// ReassignButton does. keep_shared spares the shoot and focus rows, which
// may share a button.
static void M_ReassignButton(
    M_MENU *const menu, const M_PAD_BUTTON *const button,
    const M_PAD_BUTTON replacement, const bool keep_shared)
{
    M_PAD_BUTTON *const buttons = menu->mapping.buttons;
    for (int32_t i = 0; i < M_MAPPING_COUNT; i++) {
        const bool shared = i == 0 || i == 2;
        if ((!keep_shared || !shared) && M_SameButton(&buttons[i], button)) {
            buttons[i] = replacement;
        }
    }
}

// Asks core/app for the bindings that make the rows true: the pad defaults
// first when they were chosen, then each row that changed.
static void M_CommitMapping(const M_MENU *const menu)
{
    if (menu->reset_pad) {
        App_RequestResetPadBindings();
    }
    for (size_t i = 0; i < M_MAPPING_COUNT; i++) {
        const M_PAD_BUTTON *const before = &menu->committed.buttons[i];
        const M_PAD_BUTTON *const after = &menu->mapping.buttons[i];
        if (M_SameButton(before, after)) {
            continue;
        }
        if (before->bound) {
            App_RequestUnbind(i, before->binding);
        }
        if (after->bound) {
            App_RequestBind(i, after->binding);
        }
    }
}

static void M_HighlightEntry(
    ANM_VM *const vm, const int32_t index, const int32_t cursor,
    const uint32_t selected_color, const uint32_t color)
{
    if (index == cursor) {
        vm->color = selected_color;
        vm->pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
    } else {
        vm->color = color;
        vm->pos_offset = (VEC3) {};
    }
}

static void M_HighlightOption(
    const M_MENU *const menu, ANM_VM *const vm, const int32_t row,
    const int32_t value, const int32_t current)
{
    if (value != current) {
        vm->color = 0x80202050;
        vm->scale_x = 1.0f;
        vm->scale_y = 1.0f;
        vm->pos_offset = (VEC3) {};
    } else {
        vm->color = 0x800000ff;
        vm->pos_offset = (VEC3) { -2.0f, -2.0f, 0.0f };
    }
    if (row != menu->cursor) {
        vm->color = (vm->color & 0xffffff) | 0x80000000;
    } else {
        vm->color = (vm->color & 0xffffff) | 0xff000000;
        vm->pos_offset =
            Vec3_Add(vm->pos_offset, (VEC3) { -4.0f, -4.0f, 0.0f });
    }
}

static void M_StartScripts(
    M_MENU *const menu, const int32_t first_vm, const int32_t first_script,
    const int32_t count, const bool colored)
{
    for (int32_t i = 0; i < count; i++) {
        ANM_VM *const vm = &menu->sprites[first_vm + i];
        Anm_StartScriptAtOrigin(vm, first_script + i);
        vm->shown = false;
        if (colored) {
            vm->color_op = true;
            vm->color = 0xff000000;
            vm->pos_offset = (VEC3) {};
        }
        vm->base_sprite = vm->sprite;
        vm->z_write_off = true;
    }
}

static RESULT M_LoadTitleResources(M_MENU *const menu)
{
    for (int32_t i = 0x1b; i <= 0x24; i++) {
        Anm_Release(i);
    }
    MUST(Anm_Load(0x15, "data/title01.anm", 0x100));
    MUST(Anm_Load(0x18, "data/title02.anm", 0x11b));
    MUST(Anm_Load(0x19, "data/title03.anm", 0x11f));
    MUST(Anm_Load(0x1a, "data/title04.anm", 0x122));
    MUST(Anm_Load(0x16, "data/title01s.anm", 0x17a));
    MUST(Anm_Load(0x17, "data/title04s.anm", 0x195));
    M_StartScripts(menu, 0, 0x100, 0x50, false);
    return Anm_LoadSurface(0, "data/title/title00.jpg");
}

static RESULT M_LoadSelectResources(M_MENU *const menu)
{
    for (int32_t i = 0x15; i <= 0x1a; i++) {
        Anm_Release(i);
    }
    MUST(Anm_LoadSurface(0, "data/title/select00.jpg"));
    MUST(Anm_Load(0x1b, "data/select01.anm", 0x150));
    MUST(Anm_Load(0x1c, "data/select02.anm", 0x153));
    MUST(Anm_Load(0x1d, "data/select03.anm", 0x15a));
    MUST(Anm_Load(0x1e, "data/select04.anm", 0x15c));
    MUST(Anm_Load(0x1f, "data/select05.anm", 0x155));
    MUST(Anm_Load(0x20, "data/slpl00a.anm", 0x156));
    MUST(Anm_Load(0x21, "data/slpl00b.anm", 0x157));
    MUST(Anm_Load(0x22, "data/slpl01a.anm", 0x158));
    MUST(Anm_Load(0x23, "data/slpl01b.anm", 0x159));
    M_StartScripts(menu, 0x50, 0x150, 0x10, true);
    return OK;
}

static RESULT M_LoadReplayResources(M_MENU *const menu)
{
    for (int32_t i = 0x15; i <= 0x1a; i++) {
        Anm_Release(i);
    }
    MUST(Anm_LoadSurface(0, "data/title/select00.jpg"));
    MUST(Anm_Load(0x24, "data/replay00.anm", 0x160));
    M_StartScripts(menu, 0x60, 0x160, 0x1a, true);
    return OK;
}

// Requests the quit after a file failed to load, as the original does.
static CHAIN_STATUS M_FailLoad(const RESULT result)
{
    Result_Log(result, LOG_LEVEL_ERROR);
    Scene_Request(SCENE_QUIT);
    return CHAIN_REMOVE;
}

static CHAIN_STATUS M_StartTitle(M_MENU *const menu)
{
    const RESULT result = M_LoadTitleResources(menu);
    if (!IS_OK(result)) {
        return M_FailLoad(result);
    }
    if (!m_MusicStarted) {
        m_MusicStarted = true;
        Sound_PlayMusic(M_TITLE_MUSIC);
    }
    for (int32_t i = 0; i < M_VM_COUNT; i++) {
        ANM_VM *const vm = &menu->sprites[i];
        vm->pending_interrupt = 1;
        vm->color_op = true;
        vm->color = 0xff000000;
        vm->pos_offset = (VEC3) {};
    }
    menu->state = M_STATE_WAIT_FOR_KEY;
    return CHAIN_CONTINUE;
}

// Returns whether the title still waits for its key.
static bool M_WaitForKey(M_MENU *const menu)
{
    if (menu->state_frames < 30 || !M_PressedOrRepeated(M_OPENING)) {
        return true;
    }
    menu->state_frames = 0;
    menu->state = M_STATE_TITLE_MENU;
    M_Interrupt(menu, 2);
    menu->sprites[menu->cursor].color = 0xffff0000;
    menu->sprites[menu->cursor].pos_offset = (VEC3) { -6.0f, -6.0f, 0.0f };
    menu->fade_from_color = 0;
    menu->fade_to_color = 0x40000000;
    menu->fade_frame = 0;
    menu->fade_in_frames = 60;
    return false;
}

static void M_OpenScreen(M_MENU *const menu, const M_STATE state)
{
    M_Interrupt(menu, 4);
    menu->state = state;
    menu->state_frames = 0;
    menu->fade_from_color = 0x40000000;
    menu->fade_to_color = 0xff000000;
    menu->fade_frame = 0;
    menu->fade_in_frames = 60;
    Sound_Queue(SOUND_OK);
}

static void M_StartSelect(M_MENU *const menu, const bool practice)
{
    M_OpenScreen(menu, M_STATE_OPEN_SELECT);
    g_GameManager.mode.in_practice = practice;
    if (g_GameManager.run.difficulty >= 4) {
        g_GameManager.run.difficulty = 1;
    }
    if (g_Settings.difficulty >= 4) {
        g_Settings.difficulty = 1;
    }
}

static void M_UpdateTitleMenu(M_MENU *const menu)
{
    const int32_t moved = M_MoveCursor(menu, 8);
    if (menu->cursor == 1 && !M_AnyExtraUnlocked()) {
        menu->cursor += moved;
    }
    for (int32_t i = 0; i < 8; i++) {
        M_HighlightEntry(
            &menu->sprites[i], i, menu->cursor, 0xffff0000, 0x80300000);
    }
    if (menu->state_frames < 20) {
        return;
    }
    if (M_Pressed(M_CONFIRM)) {
        switch (menu->cursor) {
        case 0:
            M_StartSelect(menu, false);
            break;
        case 1:
            if (M_AnyExtraUnlocked()) {
                M_OpenScreen(menu, M_STATE_OPEN_SELECT);
                g_GameManager.mode.in_practice = false;
                g_GameManager.run.difficulty = 4;
            } else {
                Sound_Queue(SOUND_CANCEL);
            }
            break;
        case 2:
            M_StartSelect(menu, true);
            break;
        case 3:
            M_OpenScreen(menu, M_STATE_OPEN_REPLAYS);
            g_GameManager.mode.in_practice = false;
            break;
        case 4:
            M_OpenScreen(menu, M_STATE_SCORES);
            break;
        case 5:
            menu->state = M_STATE_MUSIC_ROOM;
            menu->state_frames = 0;
            M_Interrupt(menu, 4);
            Sound_Queue(SOUND_OK);
            break;
        case 6:
            menu->state = M_STATE_OPTIONS;
            menu->state_frames = 0;
            M_Interrupt(menu, 3);
            menu->cursor = 0;
            menu->pending_windowed = M_CurrentWindowed();
            Sound_Queue(SOUND_OK);
            break;
        case 7:
            menu->state = M_STATE_QUIT;
            menu->state_frames = 0;
            M_Interrupt(menu, 4);
            Sound_Queue(SOUND_CANCEL);
            break;
        default:
            break;
        }
    }
    if (M_Pressed(M_QUIT)) {
        menu->state = M_STATE_QUIT;
        menu->state_frames = 0;
        M_Interrupt(menu, 4);
        Sound_Queue(SOUND_CANCEL);
    }
    if (M_Pressed(M_CANCEL)) {
        menu->cursor = 7;
        Sound_Queue(SOUND_CANCEL);
    }
}

static void M_StartDemo(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->mode.in_replay = true;
    gm->mode.in_demo = true;
    gm->mode.demo_frames = 0;
    memcpy(gm->mode.replay_path, TITLE_DEMO_PATH, sizeof(TITLE_DEMO_PATH));
    gm->run.stage = 3;
    gm->run.difficulty = 3;
    Scene_Request(SCENE_GAMEPLAY);
}

static void M_CycleSetting(
    int32_t *const value, const int32_t count, const int32_t step)
{
    Sound_Queue(SOUND_SELECT);
    *value += step;
    if (*value < 0) {
        *value = count - 1;
    } else if (*value >= count) {
        *value = 0;
    }
}

static void M_ChangeOption(M_MENU *const menu, const int32_t step)
{
    int32_t sound = g_Settings.sound ? 1 : 0;
    switch (menu->cursor) {
    case 0:
        M_CycleSetting(&g_Settings.lives, 5, step);
        break;
    case 1:
        M_CycleSetting(&g_Settings.bombs, 4, step);
        break;
    case 3:
        Sound_StopMusic();
        M_CycleSetting(&g_Settings.music, 3, step);
        Sound_PlayMusic(M_TITLE_MUSIC);
        break;
    case 4:
        M_CycleSetting(&sound, 2, step);
        g_Settings.sound = sound != 0;
        break;
    case 5:
        M_CycleSetting(&menu->pending_windowed, 2, step);
        break;
    default:
        break;
    }
}

static void M_UpdateOptions(M_MENU *const menu)
{
    M_MoveCursor(menu, 9);
    int32_t label = 8;
    for (int32_t i = 0; i < 9; i++) {
        if (i >= 5 && i <= 7) {
            M_HighlightOption(
                menu, &menu->sprites[i + 0x43], i, i, menu->cursor);
        } else {
            M_HighlightOption(
                menu, &menu->sprites[label++], i, i, menu->cursor);
        }
    }
    int32_t vm = label;
    for (int32_t i = 0; i < 5; i++) {
        M_HighlightOption(menu, &menu->sprites[vm++], 0, i, g_Settings.lives);
    }
    for (int32_t i = 0; i < 4; i++) {
        M_HighlightOption(menu, &menu->sprites[vm++], 1, i, g_Settings.bombs);
    }
    for (int32_t i = 0; i < 2; i++) {
        M_HighlightOption(menu, &menu->sprites[vm++], 2, i, -1);
    }
    for (int32_t i = 0; i < 2; i++) {
        M_HighlightOption(
            menu, &menu->sprites[vm++], 4, i, g_Settings.sound ? 1 : 0);
    }
    for (int32_t i = 0; i < 3; i++) {
        M_HighlightOption(
            menu, &menu->sprites[0x4d + i], 3, i, g_Settings.music);
    }
    for (int32_t i = 0; i < 2; i++) {
        M_HighlightOption(
            menu, &menu->sprites[0x4b + i], 5, i, menu->pending_windowed);
    }
    if (menu->state_frames < 0x20) {
        return;
    }
    if (M_PressedOrRepeated(M_LEFT)) {
        M_ChangeOption(menu, -1);
    }
    if (M_Pressed(M_CANCEL)) {
        menu->cursor = 8;
        Sound_Queue(SOUND_CANCEL);
    }
    if (M_PressedOrRepeated(M_RIGHT)) {
        M_ChangeOption(menu, 1);
    }
    if (!M_Pressed(M_CONFIRM)) {
        return;
    }
    switch (menu->cursor) {
    case 7:
        menu->state = M_STATE_PAD_CONFIG;
        menu->state_frames = 0;
        M_Interrupt(menu, 5);
        menu->cursor = 0;
        Sound_Queue(SOUND_OK);
        // The original also unbinds the pad's up and down while the screen
        // is open, so that they move the cursor only.
        menu->mapping = M_CurrentMapping();
        menu->committed = menu->mapping;
        menu->reset_pad = false;
        break;
    case 6:
        Sound_StopMusic();
        g_Settings.lives = 2;
        g_Settings.bombs = 3;
        g_Settings.music = SETTINGS_MUSIC_WAVE;
        g_Settings.sound = true;
        g_Settings.difficulty = 1;
        Sound_PlayMusic(M_TITLE_MUSIC);
        break;
    case 8:
        menu->state = M_STATE_TITLE_MENU;
        menu->state_frames = 0;
        M_Interrupt(menu, 2);
        menu->cursor = 6;
        Sound_Queue(SOUND_CANCEL);
        // The original restarts to change the window mode. core/app
        // changes it after the tick instead.
        if (menu->pending_windowed != M_CurrentWindowed()) {
            App_RequestFullscreen(menu->pending_windowed == 0);
        }
        break;
    default:
        break;
    }
}

static void M_UpdatePadConfig(M_MENU *const menu)
{
    M_MoveCursor(menu, 11);
    int32_t vm = 0x22;
    for (int32_t i = 0; i < 11; i++) {
        M_HighlightEntry(
            &menu->sprites[vm++], i, menu->cursor, menu->selected_color,
            menu->unselected_color);
    }
    for (int32_t i = 0; i < M_MAPPING_COUNT; i++) {
        ANM_VM *const sprite = &menu->sprites[vm++];
        sprite->visible = menu->mapping.buttons[i].bound;
        if (sprite->visible) {
            M_HighlightEntry(
                sprite, i, menu->cursor, menu->selected_color,
                menu->unselected_color);
        }
    }
    for (int32_t i = 0; i < M_MAPPING_COUNT * 2; i++) {
        ANM_VM *const sprite = &menu->sprites[vm++];
        const M_PAD_BUTTON *const button = &menu->mapping.buttons[i / 2];
        sprite->visible = button->bound;
        if (!sprite->visible) {
            continue;
        }
        const int32_t number = M_PadNumber(button->binding);
        Anm_SetSprite(sprite, (i % 2 == 0 ? number / 10 : number % 10) + 0x100);
        sprite->base_sprite = sprite->sprite;
        M_HighlightEntry(
            sprite, i / 2, menu->cursor, menu->selected_color,
            menu->unselected_color);
    }
    if (menu->state_frames < 0x20) {
        return;
    }
    INPUT_BINDING pressed;
    if (App_GetPadPress(&pressed) && M_PadNumber(pressed) >= 0
        && menu->cursor < M_MAPPING_COUNT) {
        Sound_Queue(SOUND_OK);
        M_PAD_BUTTON *const row = &menu->mapping.buttons[menu->cursor];
        const M_PAD_BUTTON button = { true, pressed };
        M_ReassignButton(
            menu, &button, *row, menu->cursor == 0 || menu->cursor == 2);
        *row = button;
    }
    if (!M_Pressed(M_CONFIRM)) {
        return;
    }
    if (menu->cursor == 9) {
        menu->mapping = M_DefaultMapping();
        menu->committed = menu->mapping;
        menu->reset_pad = true;
    } else if (menu->cursor == 10) {
        menu->state = M_STATE_OPTIONS;
        menu->state_frames = 0;
        M_Interrupt(menu, 3);
        menu->cursor = 7;
        Sound_Queue(SOUND_CANCEL);
        M_CommitMapping(menu);
    }
}

static void M_ClearOtherPair(
    M_MENU *const menu, const int32_t first, const int32_t keep)
{
    for (int32_t i = 0; i < 2; i++) {
        if (i != keep) {
            menu->sprites[first + i * 2].pending_interrupt = 0;
            menu->sprites[first + i * 2 + 1].pending_interrupt = 0;
        }
    }
}

static void M_OpenDifficulty(M_MENU *const menu)
{
    if (g_GameManager.run.difficulty < 4) {
        M_Interrupt(menu, 6);
        menu->cursor = g_Settings.difficulty;
    } else {
        M_Interrupt(menu, 0x12);
        menu->cursor = 0;
    }
}

static void M_UpdateDifficulty(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    ANM_VM *const entries = &menu->sprites[0x51];
    if (gm->run.difficulty < 4) {
        M_MoveCursor(menu, 4);
        for (int32_t i = 0; i < 4; i++) {
            if (i != menu->cursor) {
                entries[i].color = 0x60000000;
                entries[i].pos_offset = (VEC3) {};
                entries[i].fade_time = 0;
            } else {
                entries[i].color = 0xff000000;
                entries[i].pos_offset = (VEC3) { -6.0f, -6.0f, 0.0f };
            }
        }
        entries[4].visible = false;
    } else {
        for (int32_t i = 0; i < 4; i++) {
            entries[i].visible = false;
        }
        entries[4].color = 0xff000000;
        entries[4].pos_offset = (VEC3) { -6.0f, -6.0f, 0.0f };
    }
    if (M_Pressed(M_CANCEL)) {
        menu->state = M_STATE_CLOSE_SELECT;
        menu->state_frames = 0;
        M_Interrupt(menu, 4);
        Sound_Queue(SOUND_CANCEL);
        if (gm->run.difficulty < 4) {
            g_Settings.difficulty = menu->cursor;
            menu->cursor = gm->mode.in_practice ? 2 : 0;
        } else {
            menu->cursor = 1;
        }
        return;
    }
    if (!M_Pressed(M_CONFIRM)) {
        return;
    }
    menu->state = M_STATE_CHARACTER;
    menu->state_frames = 0;
    M_Interrupt(menu, 7);
    Sound_Queue(SOUND_OK);
    if (gm->run.difficulty < 4) {
        menu->sprites[menu->cursor + 0x51].pending_interrupt = 8;
        gm->run.difficulty = menu->cursor;
        menu->cursor = gm->run.character;
    } else {
        menu->sprites[0x55].pending_interrupt = 8;
        gm->run.difficulty = 4;
        menu->cursor = GameManager_IsExtraUnlocked(gm->run.character, 0)
                || GameManager_IsExtraUnlocked(gm->run.character, 1)
            ? gm->run.character
            : 1 - gm->run.character;
    }
    g_Settings.difficulty = gm->run.difficulty;
    M_ClearOtherPair(menu, 0x56, menu->cursor);
}

static bool M_CharacterLocked(const int32_t character)
{
    return g_GameManager.run.difficulty == 4
        && !GameManager_IsExtraUnlocked(character, 0)
        && !GameManager_IsExtraUnlocked(character, 1);
}

static void M_ShowCharacters(
    M_MENU *const menu, const int16_t chosen, const int16_t other)
{
    for (int32_t i = 0; i < 2; i++) {
        const int16_t interrupt = i == menu->cursor ? chosen : other;
        menu->sprites[0x56 + i * 2].pending_interrupt = interrupt;
        menu->sprites[0x56 + i * 2 + 1].pending_interrupt = interrupt;
    }
}

static void M_UpdateCharacter(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    if (menu->state_frames < 30) {
        return;
    }
    // Left moves the cursor forward and right moves it back. A locked
    // character on the left skips the check of right in that calc.
    bool skip_right = false;
    if (M_PressedOrRepeated(M_LEFT)) {
        menu->cursor = (menu->cursor + 1) % 2;
        if (M_CharacterLocked(menu->cursor)) {
            menu->cursor = (menu->cursor + 1) % 2;
            skip_right = true;
        } else {
            Sound_Queue(SOUND_SELECT);
            M_ShowCharacters(menu, 9, 0xc);
        }
    }
    if (!skip_right && M_PressedOrRepeated(M_RIGHT)) {
        menu->cursor = (menu->cursor + 1) % 2;
        if (M_CharacterLocked(menu->cursor)) {
            menu->cursor = (menu->cursor + 1) % 2;
        } else {
            Sound_Queue(SOUND_SELECT);
            M_ShowCharacters(menu, 10, 0xb);
        }
    }
    if (M_Pressed(M_CANCEL)) {
        menu->state = M_STATE_DIFFICULTY;
        menu->state_frames = 0;
        M_OpenDifficulty(menu);
        Sound_Queue(SOUND_CANCEL);
        return;
    }
    if (!M_Pressed(M_CONFIRM)) {
        return;
    }
    menu->state = M_STATE_SHOT;
    menu->state_frames = 0;
    M_Interrupt(menu, 0xd);
    menu->sprites[gm->run.difficulty + 0x51].pending_interrupt = 0;
    M_ClearOtherPair(menu, 0x56, menu->cursor);
    M_ClearOtherPair(menu, 0x5c, menu->cursor);
    gm->run.character = (uint8_t)menu->cursor;
    if (gm->run.difficulty < 4
        || GameManager_IsExtraUnlocked(gm->run.character, gm->run.shot_type)) {
        menu->cursor = gm->run.shot_type;
    } else {
        menu->cursor = 1 - gm->run.shot_type;
    }
    Sound_Queue(SOUND_OK);
}

// Starts a game from the selects. The Extra stage and practice always
// start with two lives and three bombs.
static void M_StartGame(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->player.lives = (int8_t)g_Settings.lives;
    gm->player.bombs = (int8_t)g_Settings.bombs;
    if (gm->run.difficulty == 4 || gm->mode.in_practice) {
        gm->player.lives = 2;
        gm->player.bombs = 3;
    }
    Scene_Request(SCENE_GAMEPLAY);
    Sound_Queue(SOUND_OK);
    gm->mode.in_replay = false;
    Sound_StopMusic();
}

static void M_BackToSelects(M_MENU *const menu, const int16_t interrupt)
{
    M_Interrupt(menu, interrupt);
    menu->sprites[g_GameManager.run.difficulty + 0x51].pending_interrupt = 0;
    M_ClearOtherPair(menu, 0x56, g_GameManager.run.character);
    M_ClearOtherPair(menu, 0x5c, g_GameManager.run.character);
}

static CHAIN_STATUS M_UpdateShot(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    M_MoveCursor(menu, 2);
    if (gm->run.difficulty == 4
        && !GameManager_IsExtraUnlocked(gm->run.character, menu->cursor)) {
        menu->cursor = 1 - menu->cursor;
    }
    for (int32_t i = 0; i < 2; i++) {
        menu->sprites[0x5c + i * 2 + 1].color_op = true;
    }
    for (int32_t i = 0; i < 2; i++) {
        ANM_VM *const vm = &menu->sprites[gm->run.character * 2 + 0x5c + i];
        vm->color_op = true;
        vm->shown = true;
        if (i != menu->cursor) {
            vm->color = 0xa0000000;
            vm->pos_offset = (VEC3) {};
        } else {
            vm->color = 0xff202020;
            vm->pos_offset = (VEC3) { -6.0f, -6.0f, 0.0f };
        }
    }
    if (menu->state_frames < 30) {
        return CHAIN_CONTINUE;
    }
    if (M_Pressed(M_CANCEL)) {
        menu->state = M_STATE_CHARACTER;
        menu->state_frames = 0;
        M_Interrupt(menu, 7);
        M_ClearOtherPair(menu, 0x5c, gm->run.character);
        menu->sprites[gm->run.difficulty + 0x51].pending_interrupt = 0;
        Sound_Queue(SOUND_CANCEL);
        gm->run.shot_type = (uint8_t)menu->cursor;
        menu->cursor = gm->run.character;
        M_ClearOtherPair(menu, 0x56, menu->cursor);
        return CHAIN_CONTINUE;
    }
    if (!M_Pressed(M_CONFIRM)) {
        return CHAIN_CONTINUE;
    }
    gm->run.shot_type = (uint8_t)menu->cursor;
    if (!gm->mode.in_practice) {
        gm->run.stage = gm->run.difficulty < 4 ? 0 : 6;
        M_StartGame();
        return CHAIN_REMOVE;
    }
    menu->state = M_STATE_PRACTICE_STAGE;
    menu->state_frames = 0;
    M_BackToSelects(menu, 0x13);
    menu->cursor = gm->run.last_practice_stage;
    if (menu->cursor >= M_PracticeStages()) {
        menu->cursor = 0;
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_UpdatePracticeStage(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    M_MoveCursor(menu, M_PracticeStages());
    if (menu->state_frames < 30) {
        return CHAIN_CONTINUE;
    }
    if (M_Pressed(M_CANCEL)) {
        menu->state = M_STATE_SHOT;
        menu->state_frames = 0;
        M_BackToSelects(menu, 0xd);
        menu->cursor = gm->run.shot_type;
        Sound_Queue(SOUND_OK);
        return CHAIN_CONTINUE;
    }
    if (!M_Pressed(M_CONFIRM)) {
        return CHAIN_CONTINUE;
    }
    gm->run.stage = menu->cursor;
    gm->run.last_practice_stage = menu->cursor;
    M_StartGame();
    return CHAIN_REMOVE;
}

static bool M_StagePresent(const M_MENU *const menu, const int32_t index)
{
    return menu->replays[menu->replay_cursor].summary.stages[index + 1];
}

static void M_ListReplays(M_MENU *const menu)
{
    menu->replay_count = ResultScreen_ListReplays(menu->replays);
    menu->fade_from_color = 0;
    menu->fade_out_frames = menu->fade_in_frames;
    menu->fade_in_frames = 0;
    menu->state = M_STATE_REPLAYS;
    M_Interrupt(menu, 15);
    menu->cursor = 0;
}

static void M_UpdateReplays(M_MENU *const menu)
{
    if (menu->state_frames < 40) {
        return;
    }
    if (menu->replay_count != 0) {
        M_MoveCursor(menu, menu->replay_count);
        menu->replay_cursor = menu->cursor;
        if (M_Pressed(M_CONFIRM)) {
            menu->state = M_STATE_REPLAY_STAGE;
            for (int32_t i = 0x61; i < 0x61 + 0x19; i++) {
                menu->sprites[i].pending_interrupt = 0x11;
            }
            // The original reaches past its sprites for a cursor on a
            // later page.
            const int32_t row = menu->replay_cursor + 0x63;
            if (row < M_VM_COUNT) {
                menu->sprites[row].pending_interrupt = 0x10;
            }
            menu->state_frames = 0;
            menu->cursor = 0;
            Sound_Queue(SOUND_OK);
            while (!M_StagePresent(menu, menu->cursor)) {
                menu->cursor++;
                if (menu->cursor >= 7) {
                    break;
                }
            }
        }
    }
    if (M_Pressed(M_CANCEL)) {
        menu->state = M_STATE_CLOSE_REPLAYS;
        menu->state_frames = 0;
        M_Interrupt(menu, 4);
        Sound_Queue(SOUND_CANCEL);
        menu->cursor = 0;
    }
}

static CHAIN_STATUS M_UpdateReplayStage(M_MENU *const menu)
{
    if (menu->state_frames < 40) {
        return CHAIN_CONTINUE;
    }
    const int32_t moved = M_MoveCursor(menu, 7);
    if (moved < 0) {
        while (!M_StagePresent(menu, menu->cursor)) {
            menu->cursor--;
            if (menu->cursor < 0) {
                menu->cursor = 6;
            }
        }
    } else if (moved > 0) {
        while (!M_StagePresent(menu, menu->cursor)) {
            menu->cursor++;
            if (menu->cursor >= 7) {
                menu->cursor = 0;
            }
        }
    }
    if (M_Pressed(M_CONFIRM)) {
        const SAVED_REPLAY *const replay = &menu->replays[menu->replay_cursor];
        if (Result_Log(
                TitleScene_SetUpReplay(
                    &replay->summary, replay->path, menu->cursor),
                LOG_LEVEL_ERROR)) {
            return CHAIN_REMOVE;
        }
    }
    if (M_Pressed(M_CANCEL)) {
        menu->state = M_STATE_REPLAYS;
        menu->state_frames = 0;
        M_Interrupt(menu, 4);
        Sound_Queue(SOUND_CANCEL);
        M_Interrupt(menu, 15);
        menu->cursor = menu->replay_cursor;
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Leave(const int32_t frames, const SCENE scene)
{
    if (frames < 60) {
        return CHAIN_CONTINUE;
    }
    Scene_Request(scene);
    return CHAIN_REMOVE;
}

// Runs the calc of the state. CHAIN_REMOVE leaves the title before the
// sprites step.
static CHAIN_STATUS M_UpdateState(M_MENU *const menu)
{
    CHAIN_STATUS status = CHAIN_CONTINUE;
    switch (menu->state) {
    case M_STATE_START:
        if (M_StartTitle(menu) == CHAIN_REMOVE) {
            return CHAIN_REMOVE;
        }
        [[fallthrough]];
    case M_STATE_WAIT_FOR_KEY:
        menu->idle_frames++;
        if (g_Buttons.word != 0) {
            menu->idle_frames = 0;
        }
        if (menu->idle_frames >= M_IDLE_FRAMES) {
            M_StartDemo();
            return CHAIN_REMOVE;
        }
        if (M_WaitForKey(menu)) {
            break;
        }
        menu->idle_frames = 0;
        [[fallthrough]];
    case M_STATE_TITLE_MENU:
        M_UpdateTitleMenu(menu);
        if (g_Buttons.word != 0) {
            menu->idle_frames = 0;
        }
        menu->idle_frames++;
        if (menu->idle_frames >= M_IDLE_FRAMES) {
            M_StartDemo();
            return CHAIN_REMOVE;
        }
        break;
    case M_STATE_OPEN_REPLAYS:
        if (menu->state_frames == 60) {
            const RESULT result = M_LoadReplayResources(menu);
            if (!IS_OK(result)) {
                return M_FailLoad(result);
            }
            M_ListReplays(menu);
        }
        break;
    case M_STATE_CLOSE_REPLAYS:
    case M_STATE_CLOSE_SELECT:
        if (menu->state_frames == 36) {
            menu->state = M_STATE_START;
            menu->state_frames = 0;
        }
        break;
    case M_STATE_REPLAYS:
        M_UpdateReplays(menu);
        break;
    case M_STATE_REPLAY_STAGE:
        status = M_UpdateReplayStage(menu);
        break;
    case M_STATE_OPTIONS:
        M_UpdateOptions(menu);
        break;
    case M_STATE_PAD_CONFIG:
        M_UpdatePadConfig(menu);
        break;
    case M_STATE_OPEN_SELECT:
        // The select opens at the 60th frame and keeps the frame count,
        // so the next state's frame gates count from 60.
        if (menu->state_frames == 60) {
            const RESULT result = M_LoadSelectResources(menu);
            if (!IS_OK(result)) {
                return M_FailLoad(result);
            }
            menu->state = M_STATE_DIFFICULTY;
            menu->fade_from_color = 0;
            menu->fade_out_frames = menu->fade_in_frames;
            menu->fade_in_frames = 0;
            M_OpenDifficulty(menu);
        }
        break;
    case M_STATE_DIFFICULTY:
        M_UpdateDifficulty(menu);
        break;
    case M_STATE_CHARACTER:
        M_UpdateCharacter(menu);
        break;
    case M_STATE_SHOT:
        status = M_UpdateShot(menu);
        break;
    case M_STATE_PRACTICE_STAGE:
        status = M_UpdatePracticeStage(menu);
        break;
    case M_STATE_QUIT:
        status = M_Leave(menu->state_frames, SCENE_QUIT);
        break;
    case M_STATE_SCORES:
        status = M_Leave(menu->state_frames, SCENE_SCORES);
        break;
    case M_STATE_MUSIC_ROOM:
        status = M_Leave(menu->state_frames, SCENE_MUSIC_ROOM);
        break;
    }
    return status;
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_MENU *const menu = arg;
    if (M_UpdateState(menu) == CHAIN_REMOVE) {
        return CHAIN_REMOVE;
    }
    menu->state_frames++;
    for (int32_t i = 0; i < M_VM_COUNT; i++) {
        if (M_HasTexture(&menu->sprites[i])) {
            Anm_Step(&menu->sprites[i]);
        }
    }
    return CHAIN_CONTINUE;
}

static ANM_DRAW M_ScreenDraw(const uint32_t part)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = Anm_ScreenView(
        (VIEWPORT) { 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f }, ANM_VIEW_SCREEN);
    draw.interpolate = true;
    draw.part = part;
    return draw;
}

static uint32_t M_FadeColor(const M_MENU *const menu, const int32_t frames)
{
    const uint32_t from = (menu->fade_from_color & 0xff000000) >> 24;
    const uint32_t alpha = ((menu->fade_to_color & 0xff000000) >> 24) - from;
    return (alpha * (uint32_t)menu->fade_frame / (uint32_t)frames + from) << 24
        | (menu->fade_to_color & 0xffffff);
}

static void M_DrawReplays(const M_MENU *const menu)
{
    AsciiText_AddString(
        menu->sprites[0x62].pos, g_Strings[STRING_MENU_REPLAY_HEADER]);
    const int32_t top = menu->replay_cursor - menu->replay_cursor % 15;
    for (int32_t i = top; i < top + 15 && i < menu->replay_count; i++) {
        const SAVED_REPLAY *const replay = &menu->replays[i];
        AsciiText_SetColor(i == menu->replay_cursor ? 0xffff8080 : 0xff808080);
        AsciiText_AddFormat(
            menu->sprites[0x63 + i - top].pos, g_Strings[STRING_MENU_REPLAY],
            replay->label, (const char *)replay->name, replay->summary.date,
            g_Strings[m_ShotTypeNames[replay->summary.shot]],
            g_Strings[m_DifficultyNames[replay->summary.difficulty]]);
    }
    if (menu->state == M_STATE_REPLAY_STAGE) {
        const REPLAY_SUMMARY *const summary =
            &menu->replays[menu->replay_cursor].summary;
        AsciiText_SetColor(0xffffffff);
        AsciiText_SetHighlight(false);
        AsciiText_AddFormat(
            menu->sprites[0x61].pos, "       %2.3f%%",
            (double)summary->slowdown);
        AsciiText_AddString(
            menu->sprites[0x72].pos, g_Strings[STRING_MENU_STAGE_HEADER]);
        for (int32_t i = 0; i < 7; i++) {
            const VEC3 pos = menu->sprites[0x73 + i].pos;
            AsciiText_SetColor(i == menu->cursor ? 0xffff8080 : 0xff808080);
            if (summary->stages[i + 1]) {
                AsciiText_AddFormat(
                    pos, g_Strings[STRING_MENU_STAGE],
                    g_Strings[m_StageNames[i]], summary->stage_scores[i + 1]);
            } else {
                AsciiText_AddFormat(
                    pos, g_Strings[STRING_MENU_STAGE_EMPTY],
                    g_Strings[m_StageNames[i]]);
            }
        }
    }
    AsciiText_SetColor(0xffffffff);
    AsciiText_SetHighlight(false);
}

static void M_DrawPracticeScores(const M_MENU *const menu)
{
    if (menu->state != M_STATE_PRACTICE_STAGE) {
        return;
    }
    VEC3 pos = { 320.0f, 200.0f, 0.0f };
    const uint32_t fade = menu->state_frames < 30
        ? (uint32_t)(menu->state_frames * 255 / 30)
        : 255;
    const int32_t stages = M_PracticeStages();
    for (int32_t i = 0; i < stages; i++) {
        if (i == menu->cursor) {
            AsciiText_SetColor(fade << 24 | 0xc0f0f0);
        } else {
            AsciiText_SetColor((fade >> 1) << 24 | 0x80c0c0);
        }
        AsciiText_AddFormat(
            pos, g_Strings[STRING_MENU_PRACTICE_SCORE], i + 1,
            g_GameManager.records
                .practice[M_Shot()][i][g_GameManager.run.difficulty]
                .score);
        pos.y += 24.0f;
    }
    AsciiText_SetColor(0xffffffff);
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    M_MENU *const menu = arg;
    if (menu->state == M_STATE_START) {
        return CHAIN_CONTINUE;
    }
    Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    const GFX_RECT screen = { 0, 0, 640, 480 };
    if (menu->fade_in_frames != 0) {
        if (menu->fade_frame < menu->fade_in_frames) {
            menu->fade_frame++;
        }
        Anm_DrawRect(screen, M_FadeColor(menu, menu->fade_in_frames));
    } else if (menu->fade_frame != 0) {
        menu->fade_frame--;
        Anm_DrawRect(screen, M_FadeColor(menu, menu->fade_out_frames));
    }
    for (int32_t i = 0; i < M_DRAWN_VM_COUNT; i++) {
        if (M_HasTexture(&menu->sprites[i])) {
            ANM_VM vm = menu->sprites[i];
            vm.pos = Vec3_Add(vm.pos, vm.pos_offset);
            const ANM_DRAW draw = M_ScreenDraw((uint32_t)i);
            Anm_Draw(&vm, &draw);
        }
    }
    if (menu->state >= M_STATE_REPLAYS && menu->state <= M_STATE_REPLAY_STAGE) {
        M_DrawReplays(menu);
    }
    M_DrawPracticeScores(menu);
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    M_MENU *const menu = arg;
    GAME_MANAGER *const gm = &g_GameManager;
    Anm_ClearMenuScripts();
    switch (Scene_Get()) {
    case SCENE_GAMEPLAY:
    case SCENE_NEXT_STAGE:
    case SCENE_GAME_RESULT:
        menu->cursor = gm->run.difficulty == 4 ? 1 : 0;
        break;
    case SCENE_SCORES:
        menu->cursor = 4;
        break;
    case SCENE_MUSIC_ROOM:
        menu->cursor = 5;
        break;
    default:
        menu->cursor = 0;
        break;
    }
    if (gm->mode.in_practice) {
        menu->cursor = 2;
    }
    gm->mode.in_practice = false;
    menu->unselected_color = 0x80004000;
    menu->selected_color = 0xff008000;
    menu->fade_from_color = 0;
    menu->fade_to_color = 0x40000000;
    menu->fade_frame = 0;
    menu->fade_in_frames = 0;
    ResultScreen_LoadTitleRecords(gm->records.clears, gm->records.practice);
    if (!gm->mode.in_demo) {
        if (m_MusicStarted) {
            Sound_PlayMusic(M_TITLE_MUSIC);
            ScreenEffect_FadeIn(120, 0xffffff);
        } else {
            ScreenEffect_FadeIn(200, 0xffffff);
        }
    }
    gm->mode.in_demo = false;
    gm->mode.demo_frames = 0;
    menu->added = true;
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_MENU *const menu = arg;
    for (int32_t i = 0x15; i <= 0x24; i++) {
        Anm_Release(i);
    }
    Anm_ReleaseSurface(0);
    Anm_ClearMenuScripts();
    Chain_Remove(&menu->draw);
    menu->added = false;
}

RESULT TitleScene_SetUp(const int32_t mode)
{
    M_MENU *const menu = &m_Menu;
    *menu = (M_MENU) {};
    for (int32_t i = 0; i < M_VM_COUNT; i++) {
        Anm_CreateVm(&menu->sprites[i]);
    }
    g_GameManager.mode.paused = 0;
    menu->state = mode != 0 ? M_STATE_OPEN_REPLAYS : M_STATE_START;
    Chain_InitElem(&menu->calc, M_Update, menu);
    menu->calc.added = M_Added;
    menu->calc.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &menu->calc, M_CALC_PRIORITY));
    Chain_InitElem(&menu->draw, M_Draw, menu);
    MUST(Chain_AddDraw(&g_Chain, &menu->draw, M_DRAW_PRIORITY));
    // A first calc of the title can take the opening key at once, and the
    // replay menu loads on its first calc.
    menu->state_frames = 60;
    return OK;
}

RESULT TitleScene_SetUpReplay(
    const REPLAY_SUMMARY *const summary, const char *const path,
    const int32_t stage)
{
    GAME_MANAGER *const gm = &g_GameManager;
    FAIL_IF(
        strlen(path) >= sizeof(gm->mode.replay_path),
        "replay path is too long");
    gm->mode.in_replay = true;
    memcpy(gm->mode.replay_path, path, strlen(path) + 1);
    gm->run.difficulty = summary->difficulty;
    gm->run.character = summary->shot / 2;
    gm->run.shot_type = summary->shot % 2;
    gm->player.lives = (int8_t)summary->lives;
    gm->player.bombs = (int8_t)summary->bombs;
    gm->run.stage = stage;
    Scene_Request(SCENE_GAMEPLAY);
    return OK;
}

void TitleScene_Remove(void)
{
    if (m_Menu.added) {
        Chain_Remove(&m_Menu.calc);
    }
}
