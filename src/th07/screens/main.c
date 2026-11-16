#include "th07/screens/main.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/buttons.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screen_effect.h"
#include "th07/screens/result/saves.h"
#include "th07/settings.h"
#include "th07/sound.h"
#include "th07/screens/result/records.h"
#include "th07/strings.h"

#include "shell/game.h"
#include "core/base/color.h"
#include "core/base/log.h"
#include "core/base/utils.h"

#include <stdio.h>
#include <string.h>

#define M_CALC_PRIORITY 3
#define M_DRAW_PRIORITY 0
#define M_VM_COUNT 0xa4
#define M_HELP_COUNT 0xe
#define M_IDLE_FRAMES 900
#define M_MENU_IDLE_FRAMES 3600
#define M_SPLASH_FRAMES 900
#define M_PAGE 15
#define M_TITLE_MUSIC "bgm/th07_01.mid"
#define M_KEY_ROW_COUNT 9

#define M_UP 0x10
#define M_DOWN 0x20
#define M_LEFT 0x40
#define M_RIGHT 0x80
#define M_CONFIRM 0x1001
#define M_CANCEL 0xa
#define M_SKIP_SPLASH 0x1003

typedef enum {
    M_STATE_TITLE = 0,
    M_STATE_OPTIONS = 2,
    M_STATE_KEY_CONFIG = 3,
    M_STATE_DIFFICULTY = 4,
    M_STATE_CHARACTER = 5,
    M_STATE_SHOT = 6,
    M_STATE_REPLAYS = 7,
    M_STATE_PRACTICE_DIFFICULTY = 8,
    M_STATE_PRACTICE_CHARACTER = 9,
    M_STATE_PRACTICE_SHOT = 10,
    M_STATE_PRACTICE_STAGE = 11,
    M_STATE_EXTRA_DIFFICULTY = 12,
    M_STATE_EXTRA_CHARACTER = 13,
    M_STATE_EXTRA_SHOT = 14,
} M_STATE;

// Names the steps of the states. Each state starts at M_SUB_ENTER.
typedef enum {
    M_SUB_ENTER = 0,
    M_SUB_MENU = 1,
    M_SUB_QUIT = 2, // the title menu: leaving the game
    M_SUB_OPEN_OPTIONS = 3, // the title menu
    M_SUB_LEAVE = 3, // the difficulty select
    M_SUB_REPLAY_STAGE = 2,
    M_SUB_REPLAY_PLAYBACK = 3,
    M_SUB_REPLAY_LEAVE = 4,
} M_SUB_STATE;

typedef struct {
    bool bound;
    INPUT_BINDING binding;
} M_PAD_BUTTON;

// Holds the rows of the pad button configuration, in the original's order:
// shoot, bomb, focus, skip, menu, up, down, left, right.
typedef struct {
    M_PAD_BUTTON buttons[M_KEY_ROW_COUNT];
} M_MAPPING;

typedef struct {
    ANM_VM sprites[M_VM_COUNT];
    bool sprites_started;
    ANM_VM help_sprites[M_HELP_COUNT];
    int32_t help; // the help sprite shown, or -1 for none
    int32_t cursor;
    int32_t prev_cursor;
    M_STATE state;
    M_STATE prev_state;
    int32_t sub_state;
    int32_t state_frames;
    int32_t input_lockout;
    int32_t idle_frames;
    int32_t menu_idle_frames;
    bool returning_from_practice;
    SAVED_REPLAY replays[SAVED_REPLAY_COUNT];
    int32_t replay_count;
    int32_t replay_cursor;
    int32_t replay_stage;
    int32_t replay_scan;
    M_MAPPING mapping;
    // Holds the rows that the bindings had when the screen opened or when
    // the defaults were chosen, and whether they were.
    M_MAPPING committed;
    bool reset_pad;
    // Holds the window mode that the options show: 0 for fullscreen, 1 for
    // windowed.
    int32_t pending_windowed;
    bool splash;
    int32_t splash_frames;
    // Holds the previous button word of the calc that opened the splash.
    // The splash never updates it, as the original reads the buttons alone.
    uint16_t splash_previous;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    bool added;
} M_MENU;

const char *const g_DemoReplayPaths[TITLE_DEMO_COUNT] = {
    "data/demo/demorpy0.rpy",
    "data/demo/demorpy1.rpy",
    "data/demo/demorpy2.rpy",
};

static const TH07_ACTION m_KeyActions[M_KEY_ROW_COUNT] = {
    TH07_ACTION_shoot, TH07_ACTION_bomb, TH07_ACTION_focus,
    TH07_ACTION_skip,  TH07_ACTION_menu, TH07_ACTION_up,
    TH07_ACTION_down,  TH07_ACTION_left, TH07_ACTION_right,
};

// Names the gamepad buttons that the configuration assigns, in the order
// of their codes, so that it shows each as a number, as the original shows
// its pad's button numbers.
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

static const char *const m_TitleHelp[] = {
    "ゲームを開始します",
    "エキストラステージを開始します",
    "ステージを選択し、練習を開始します",
    "リプレイを鑑賞できます",
    "過去のスコアやスペルカードの取得歴を見られます",
    "音楽を聴けます",
    "各種設定できます",
    "いろいろと終了します",
};

static const char *const m_OptionsHelp[] = {
    "プレイヤーの初期数を変更します。（初期設定　３）",
    "画面の色数を変更します。３２ＢＩＴだと最も綺麗に表示されます。",
    "ＢＧＭの再生方法を変更します。（初期設定　ＷＡＶ）",
    "効果音を再生するか選択します",
    "ウィンドウかフルスクリーンか選択します",
    "弾が多い場面でわざと処理落ちさせます(スコア、リプレイ記録不可)",
    "全て初期設定にします",
    "パッド操作のボタン配置を変更します",
    "おいそれと終了します",
};

static const char *const m_KeyConfigHelp[] = {
    "ショット、決定ボタンを設定します",
    "ボム、キャンセルボタンを設定します",
    "低速移動ボタンを設定します",
    "メッセージスキップボタンを設定します",
    "ポーズボタンを設定します",
    "上移動ボタンを設定します",
    "下移動ボタンを設定します",
    "左移動ボタンを設定します",
    "右移動ボタンを設定します",
    "ショット押しっぱなしで低速移動になるようにします",
    "初期設定に戻します",
    "おおよそ終了します",
};

static const STRING m_ShotNames[6] = {
    STRING_SHOT_TYPE_REIMU_A,  STRING_SHOT_TYPE_REIMU_B,
    STRING_SHOT_TYPE_MARISA_A, STRING_SHOT_TYPE_MARISA_B,
    STRING_SHOT_TYPE_SAKUYA_A, STRING_SHOT_TYPE_SAKUYA_B,
};

static const char *const m_DifficultyNames[6] = {
    "Easy    ", "Normal  ", "Hard    ", "Lunatic ", "Extra   ", "Phantasm",
};

static const char *const m_StageNames[8] = {
    "Stage1  ", "Stage2  ", "Stage3  ", "Stage4  ",
    "Stage5  ", "Stage6  ", "Extra   ", "Phantasm",
};

static const STRING m_PracticeStageNames[6] = {
    STRING_PRACTICE_STAGE_1, STRING_PRACTICE_STAGE_2, STRING_PRACTICE_STAGE_3,
    STRING_PRACTICE_STAGE_4, STRING_PRACTICE_STAGE_5, STRING_PRACTICE_STAGE_6,
};

static M_MENU m_Menu;

static bool M_Pressed(const uint16_t mask)
{
    return Buttons_WasPressed(&g_Buttons, mask);
}

static bool M_PressedOrRepeated(const uint16_t mask)
{
    return Buttons_WasPressedOrRepeated(&g_Buttons, mask);
}

static const char *M_Name(
    const char *const *const names, const size_t count, const int32_t index)
{
    return index >= 0 && (size_t)index < count ? names[index] : "";
}

static const char *M_StringName(
    const STRING *const names, const size_t count, const int32_t index)
{
    return index >= 0 && (size_t)index < count ? g_Strings[names[index]] : "";
}

static void M_SetSprite(ANM_VM *const vm, const int32_t variant)
{
    Anm_SetSprite(vm, vm->base_sprite + variant);
}

// Shows variant 1 of count sprites from first, and variant 0 of the one at
// chosen.
static void M_Highlight(
    M_MENU *const menu, const int32_t first, const int32_t count,
    const int32_t chosen)
{
    for (int32_t i = 0; i < count; i++) {
        M_SetSprite(&menu->sprites[first + i], 1);
    }
    M_SetSprite(&menu->sprites[first + chosen], 0);
}

static void M_Interrupt(M_MENU *const menu, const int16_t interrupt)
{
    Anm_SendInterrupt(menu->sprites, M_VM_COUNT, interrupt);
}

static void M_InterruptHelp(M_MENU *const menu, const int16_t interrupt)
{
    if (menu->help >= 0) {
        menu->help_sprites[menu->help].pending_interrupt = interrupt;
    }
}

static void M_RenderHelp(
    M_MENU *const menu, const char *const *const texts, const size_t count)
{
    for (size_t i = 0; i < count; i++) {
        Result_Log(
            Anm_RenderTextCentered(
                &menu->help_sprites[i], 0xfff0e0, 0x300000, "%s", texts[i]),
            LOG_LEVEL_WARNING);
    }
}

static void M_ShowHelp(M_MENU *const menu)
{
    if (menu->prev_cursor != menu->cursor) {
        menu->help = menu->cursor;
        menu->help_sprites[menu->help].pending_interrupt = 1;
    }
    menu->prev_cursor = menu->cursor;
}

static void M_SetState(M_MENU *const menu, const M_STATE state)
{
    menu->prev_state = menu->state;
    menu->state = state;
    menu->state_frames = 0;
    menu->input_lockout = 0;
    menu->sub_state = M_SUB_ENTER;
    menu->menu_idle_frames = 0;
}

static CHAIN_STATUS M_Count(M_MENU *const menu)
{
    menu->menu_idle_frames++;
    menu->state_frames++;
    menu->input_lockout++;
    return CHAIN_CONTINUE;
}

// Returns -1 when the cursor moved up, 1 when it moved down, else 0.
static int32_t M_MoveCursor(M_MENU *const menu, const int32_t count)
{
    if (count == 0) {
        return 0;
    }
    int32_t moved;
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

static int32_t M_MoveCursorSideways(M_MENU *const menu, const int32_t count)
{
    if (count == 0) {
        return 0;
    }
    if (M_PressedOrRepeated(M_LEFT)) {
        menu->cursor--;
        if (menu->cursor < 0) {
            menu->cursor += count;
        }
        Sound_Queue(SOUND_SELECT);
        return -1;
    }
    if (M_PressedOrRepeated(M_RIGHT)) {
        menu->cursor++;
        if (menu->cursor >= count) {
            menu->cursor -= count;
        }
        Sound_Queue(SOUND_SELECT);
        return 1;
    }
    return 0;
}

// Requests the quit after a file failed to load. The original removes the
// title and leaves the game in no scene.
static CHAIN_STATUS M_FailLoad(const RESULT result)
{
    Result_Log(result, LOG_LEVEL_ERROR);
    Scene_Request(SCENE_QUIT);
    return CHAIN_REMOVE;
}

static void M_StartPlay(void)
{
    Scene_Request(SCENE_GAMEPLAY);
    g_GameManager.mode.in_replay = false;
    Sound_StopMusic();
}

static bool M_StartDemo(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->mode.demo_index = (uint8_t)((gm->mode.demo_index + 1) % 3);
    const char *const path = g_DemoReplayPaths[gm->mode.demo_index];
    memcpy(gm->mode.replay_path, path, strlen(path) + 1);
    size_t size;
    const uint8_t *const data = Replay_FindFile(path, &size);
    REPLAY_FILE *file = nullptr;
    READER reader;
    if (data != nullptr) {
        Reader_OpenBuffer(&reader, data, size, nullptr);
    }
    if (data == nullptr
        || !Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_WARNING)) {
        LOG_ERROR("Demo Play is not ready");
        menu->idle_frames = 0;
        return false;
    }
    gm->mode.in_replay = true;
    gm->mode.in_demo = true;
    gm->mode.demo_frames = 0;
    gm->run.difficulty = file->difficulty;
    gm->run.character = file->shot / 2;
    gm->run.shot_type = file->shot % 2;
    gm->run.shot_index = file->shot;
    int32_t slot = 1;
    while (slot < 7 && file->stages[slot] == nullptr) {
        slot++;
    }
    gm->run.stage = slot - 1;
    Replay_Free(file);
    Scene_Request(SCENE_GAMEPLAY);
    gm->mode.replay_speed = 0;
    return true;
}

static void M_OpenSelect(M_MENU *const menu, const M_STATE state)
{
    M_SetState(menu, state);
    M_Interrupt(menu, 5);
    M_InterruptHelp(menu, 2);
}

static void M_OpenReplays(M_MENU *const menu)
{
    g_GameManager.mode.in_practice = false;
    M_SetState(menu, M_STATE_REPLAYS);
    M_Interrupt(menu, 13);
    M_InterruptHelp(menu, 2);
}

static CHAIN_STATUS M_UpdateOptions(M_MENU *menu);

// Runs the title menu's entry at the cursor. Sets stay when the title menu
// stays and its calc goes on.
static CHAIN_STATUS M_ConfirmTitleEntry(M_MENU *const menu, bool *const stay)
{
    GAME_MANAGER *const gm = &g_GameManager;
    *stay = false;
    switch (menu->cursor) {
    case 0:
    case 2:
        gm->mode.in_practice = menu->cursor == 2;
        menu->cursor = g_Settings.difficulty;
        if (menu->cursor >= 4) {
            menu->cursor = 2;
        }
        M_OpenSelect(
            menu,
            gm->mode.in_practice ? M_STATE_PRACTICE_DIFFICULTY
                                 : M_STATE_DIFFICULTY);
        return CHAIN_CONTINUE;
    case 1:
        if (GameManager_AnyShotHasExtraOpen()) {
            gm->mode.in_practice = false;
            menu->cursor = g_Settings.difficulty == 5;
            M_OpenSelect(menu, M_STATE_EXTRA_DIFFICULTY);
            return CHAIN_CONTINUE;
        }
        // The original falls through to the replay menu.
        [[fallthrough]];
    case 3:
        M_OpenReplays(menu);
        return CHAIN_CONTINUE;
    case 5:
        Scene_Request(SCENE_MUSIC_ROOM);
        M_InterruptHelp(menu, 2);
        return CHAIN_REMOVE;
    case 4:
        Scene_Request(SCENE_SCORES);
        M_InterruptHelp(menu, 2);
        return CHAIN_REMOVE;
    case 6:
        menu->cursor = 0;
        menu->input_lockout = 0;
        menu->sub_state = M_SUB_OPEN_OPTIONS;
        menu->state_frames = 0;
        // Counts the frames once more, as the original runs the options'
        // calc here.
        M_UpdateOptions(menu);
        menu->cursor = 0;
        break;
    case 7:
        menu->sub_state = M_SUB_QUIT;
        menu->state_frames = 0;
        M_Interrupt(menu, 1);
        break;
    default:
        break;
    }
    *stay = true;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_EnterTitle(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    const SCENE previous = Scene_GetPrevious();
    if (menu->prev_state == M_STATE_TITLE && previous != SCENE_SCORES) {
        Sound_PlayMusic(M_TITLE_MUSIC);
    }
    const M_STATE prev = menu->prev_state;
    if (prev == M_STATE_TITLE || prev == M_STATE_DIFFICULTY
        || prev == M_STATE_REPLAYS || prev == M_STATE_PRACTICE_DIFFICULTY
        || prev == M_STATE_EXTRA_DIFFICULTY) {
        const RESULT result = Anm_LoadSurface(0, "data/title/title00.jpg");
        if (!IS_OK(result)) {
            return M_FailLoad(result);
        }
    }
    if (!menu->sprites_started) {
        menu->sprites_started = true;
        Anm_StartVmSeries(menu->sprites, 0x900, M_VM_COUNT);
    }
    M_Interrupt(menu, 2);
    M_Highlight(menu, 1, 8, menu->cursor);
    menu->state_frames = 0;
    menu->prev_cursor = -1;
    menu->sub_state = M_SUB_MENU;
    menu->idle_frames = 0;
    if (gm->mode.in_replay) {
        M_SetState(menu, M_STATE_REPLAYS);
        M_Interrupt(menu, 13);
        M_InterruptHelp(menu, 2);
        gm->mode.in_replay = false;
        return CHAIN_AGAIN;
    }
    if (menu->returning_from_practice) {
        M_SetState(menu, M_STATE_PRACTICE_DIFFICULTY);
        M_Interrupt(menu, 5);
        M_InterruptHelp(menu, 2);
        return CHAIN_AGAIN;
    }
    M_RenderHelp(menu, m_TitleHelp, ARRAY_SIZE(m_TitleHelp));
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_UpdateTitleMenu(M_MENU *const menu)
{
    const int32_t moved = M_MoveCursor(menu, 8);
    if (moved != 0) {
        while (!GameManager_AnyShotHasExtraOpen() && menu->cursor == 1) {
            menu->cursor += moved;
        }
        M_Highlight(menu, 1, 8, menu->cursor);
    }
    menu->idle_frames++;
    if (g_Buttons.word != 0) {
        menu->idle_frames = 0;
    }
    if (menu->idle_frames > M_IDLE_FRAMES && M_StartDemo(menu)) {
        return CHAIN_REMOVE;
    }
    M_ShowHelp(menu);
    if (menu->input_lockout < 10) {
        return M_Count(menu);
    }
    if (M_Pressed(M_CONFIRM)) {
        Sound_Queue(SOUND_OK);
        bool stay;
        const CHAIN_STATUS status = M_ConfirmTitleEntry(menu, &stay);
        if (!stay) {
            return status;
        }
    }
    if (M_Pressed(M_CANCEL)) {
        M_SetSprite(&menu->sprites[menu->cursor + 1], 1);
        menu->cursor = 7;
        M_SetSprite(&menu->sprites[menu->cursor + 1], 0);
        Sound_Queue(SOUND_CANCEL);
    }
    return M_Count(menu);
}

static CHAIN_STATUS M_UpdateTitle(M_MENU *const menu)
{
    switch (menu->sub_state) {
    case M_SUB_ENTER: {
        const CHAIN_STATUS status = M_EnterTitle(menu);
        if (status != CHAIN_CONTINUE) {
            // The original returns 1 from the switch to another state.
            return status == CHAIN_AGAIN ? CHAIN_CONTINUE : status;
        }
        return M_UpdateTitleMenu(menu);
    }
    case M_SUB_MENU:
        return M_UpdateTitleMenu(menu);
    case M_SUB_QUIT:
        if (menu->state_frames >= 60) {
            Scene_Request(SCENE_QUIT);
            return CHAIN_REMOVE;
        }
        break;
    case M_SUB_OPEN_OPTIONS:
        if (menu->state_frames >= 30) {
            M_SetState(menu, M_STATE_OPTIONS);
            menu->cursor = 0;
            menu->pending_windowed = Shell_IsFullscreen() ? 0 : 1;
            return CHAIN_CONTINUE;
        }
        break;
    default:
        break;
    }
    return M_Count(menu);
}

static void M_CycleSetting(
    int32_t *const value, const int32_t count, const int32_t step)
{
    *value += step;
    if (*value < 0) {
        *value = count - 1;
    } else if (*value >= count) {
        *value = 0;
    }
}

static void M_ToggleSetting(bool *const value)
{
    *value = !*value;
}

// Changes the option at the cursor by step. Returns whether the row has a
// choice.
static bool M_ChangeOption(M_MENU *const menu, const int32_t step)
{
    switch (menu->cursor) {
    case 0:
        M_CycleSetting(&g_Settings.lives, 5, step);
        return true;
    case 1:
        return true;
    case 2:
        Sound_StopMusic();
        M_CycleSetting(&g_Settings.music, 3, step);
        Sound_PlayMusic(M_TITLE_MUSIC);
        return true;
    case 3:
        M_ToggleSetting(&g_Settings.sound);
        return true;
    case 4:
        M_CycleSetting(&menu->pending_windowed, 2, step);
        return true;
    case 5:
        M_ToggleSetting(&g_Settings.slowdown);
        return true;
    default:
        return false;
    }
}

static CHAIN_STATUS M_LeaveOptions(M_MENU *const menu)
{
    menu->cursor = 6;
    M_SetState(menu, M_STATE_TITLE);
    Sound_Queue(SOUND_CANCEL);
    // The original restarts to change the window mode. shell/game changes
    // it after the tick instead.
    if (menu->pending_windowed != (Shell_IsFullscreen() ? 0 : 1)) {
        Shell_RequestFullscreen(menu->pending_windowed == 0);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_UpdateOptions(M_MENU *const menu)
{
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            M_Interrupt(menu, 3);
            M_Highlight(menu, 9, 9, menu->cursor);
            menu->state_frames = 0;
            menu->prev_cursor = -1;
        }
        menu->sub_state = M_SUB_MENU;
        M_RenderHelp(menu, m_OptionsHelp, ARRAY_SIZE(m_OptionsHelp));
        [[fallthrough]];
    case M_SUB_MENU:
        if (M_MoveCursor(menu, 9) != 0) {
            M_Highlight(menu, 9, 9, menu->cursor);
        }
        M_ShowHelp(menu);
        M_Highlight(menu, 0x12, 5, g_Settings.lives);
        M_SetSprite(&menu->sprites[0x17], 1);
        M_SetSprite(&menu->sprites[0x18], 1);
        M_Highlight(menu, 0x19, 3, g_Settings.music);
        M_Highlight(menu, 0x1c, 2, g_Settings.sound ? 1 : 0);
        M_Highlight(menu, 0x1e, 2, menu->pending_windowed);
        M_Highlight(menu, 0x20, 2, g_Settings.slowdown ? 1 : 0);
        if (menu->input_lockout < 4) {
            break;
        }
        if (M_PressedOrRepeated(M_LEFT) && M_ChangeOption(menu, -1)) {
            Sound_Queue(SOUND_SELECT);
        }
        if (M_PressedOrRepeated(M_RIGHT) && M_ChangeOption(menu, 1)) {
            Sound_Queue(SOUND_SELECT);
        }
        if (g_Buttons.word != 0) {
            menu->menu_idle_frames = 0;
        }
        if (menu->menu_idle_frames >= M_MENU_IDLE_FRAMES) {
            return M_LeaveOptions(menu);
        }
        if (M_Pressed(M_CONFIRM)) {
            switch (menu->cursor) {
            case 6:
                g_Settings.lives = 2;
                g_Settings.bombs = 3;
                g_Settings.music = SETTINGS_MUSIC_WAVE;
                g_Settings.sound = true;
                g_Settings.slowdown = false;
                Sound_Queue(SOUND_OK);
                break;
            case 7:
                menu->cursor = 0;
                M_SetState(menu, M_STATE_KEY_CONFIG);
                Sound_Queue(SOUND_OK);
                return CHAIN_CONTINUE;
            case 8:
                return M_LeaveOptions(menu);
            default:
                break;
            }
        }
        if (M_Pressed(M_CANCEL)) {
            if (menu->cursor == 8) {
                return M_LeaveOptions(menu);
            }
            M_SetSprite(&menu->sprites[menu->cursor + 9], 1);
            menu->cursor = 8;
            M_SetSprite(&menu->sprites[menu->cursor + 9], 0);
            Sound_Queue(SOUND_CANCEL);
        }
        break;
    default:
        break;
    }
    return M_Count(menu);
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
    for (size_t row = 0; row < M_KEY_ROW_COUNT; row++) {
        INPUT_BINDING bindings[16];
        size_t count = Shell_GetBindings(
            m_KeyActions[row], bindings, ARRAY_SIZE(bindings));
        if (count > ARRAY_SIZE(bindings)) {
            count = ARRAY_SIZE(bindings);
        }
        mapping.buttons[row] = M_FirstPadButton(bindings, count);
    }
    return mapping;
}

// Returns the rows of the default bindings of g_InputSchema: the gamepad's
// first button, or else the joystick's.
static M_MAPPING M_DefaultMapping(void)
{
    M_MAPPING mapping = {};
    for (size_t row = 0; row < M_KEY_ROW_COUNT; row++) {
        const INPUT_ACTION *const entry =
            &g_InputSchema.actions[m_KeyActions[row]];
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
                while (names != nullptr && *names == ' ') {
                    names++;
                }
            }
        }
        mapping.buttons[row] = M_FirstPadButton(bindings, count);
    }
    return mapping;
}

// Gives every row that shows button the replacement, as the original's
// ReassignButton does. Unlike th06's, it spares no row.
static void M_ReassignButton(
    M_MENU *const menu, const M_PAD_BUTTON *const button,
    const M_PAD_BUTTON replacement)
{
    for (int32_t i = 0; i < M_KEY_ROW_COUNT; i++) {
        if (M_SameButton(&menu->mapping.buttons[i], button)) {
            menu->mapping.buttons[i] = replacement;
        }
    }
}

// Asks shell/game for the bindings that make the rows true: the pad defaults
// first when they were chosen, then each row that changed.
static void M_CommitMapping(const M_MENU *const menu)
{
    if (menu->reset_pad) {
        Shell_RequestResetPadBindings();
    }
    for (size_t i = 0; i < M_KEY_ROW_COUNT; i++) {
        const M_PAD_BUTTON *const before = &menu->committed.buttons[i];
        const M_PAD_BUTTON *const after = &menu->mapping.buttons[i];
        if (M_SameButton(before, after)) {
            continue;
        }
        if (before->bound) {
            Shell_RequestUnbind(m_KeyActions[i], before->binding);
        }
        if (after->bound) {
            Shell_RequestBind(m_KeyActions[i], after->binding);
        }
    }
}

// Shows a row's button as two digits on vm and the sprite after it, or
// hides both when the row has none.
static void M_ShowButtonDigits(ANM_VM *const vm, const M_PAD_BUTTON *button)
{
    const int32_t number = button->bound ? M_PadNumber(button->binding) : -1;
    if (number < 0) {
        vm[0].visible = false;
        vm[1].visible = false;
        return;
    }
    M_SetSprite(&vm[0], number / 10 * 2);
    M_SetSprite(&vm[1], number % 10 * 2);
    vm[0].visible = true;
    vm[1].visible = true;
}

static void M_ShowMapping(M_MENU *const menu)
{
    for (int32_t i = 0; i < M_KEY_ROW_COUNT; i++) {
        M_ShowButtonDigits(
            &menu->sprites[0x2f + i * 2], &menu->mapping.buttons[i]);
    }
}

static CHAIN_STATUS M_LeaveKeyConfig(M_MENU *const menu)
{
    Sound_Queue(SOUND_CANCEL);
    M_SetState(menu, M_STATE_OPTIONS);
    M_CommitMapping(menu);
    menu->cursor = 7;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_UpdateKeyConfig(M_MENU *const menu)
{
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            M_Interrupt(menu, 4);
            M_Highlight(menu, 0x23, 12, menu->cursor);
            menu->state_frames = 0;
            // The original also unbinds the pad's up and down while the
            // screen is open, so that they move the cursor only.
            menu->mapping = M_CurrentMapping();
            menu->committed = menu->mapping;
            menu->reset_pad = false;
            M_ShowMapping(menu);
            menu->prev_cursor = -1;
        }
        menu->sub_state = M_SUB_MENU;
        M_RenderHelp(menu, m_KeyConfigHelp, ARRAY_SIZE(m_KeyConfigHelp));
        [[fallthrough]];
    case M_SUB_MENU: {
        if (M_MoveCursor(menu, 12) != 0) {
            M_Highlight(menu, 0x23, 12, menu->cursor);
        }
        M_ShowHelp(menu);
        M_ShowMapping(menu);
        M_Highlight(menu, 0x41, 2, g_Settings.shoot_focuses ? 1 : 0);
        // shell/game reports a press once, as the original assigns a held
        // button once.
        INPUT_BINDING pressed;
        if (Shell_GetPadPress(&pressed) && M_PadNumber(pressed) >= 0
            && menu->cursor < M_KEY_ROW_COUNT) {
            M_PAD_BUTTON *const row = &menu->mapping.buttons[menu->cursor];
            const M_PAD_BUTTON button = { true, pressed };
            M_ReassignButton(menu, &button, *row);
            *row = button;
            Sound_Queue(SOUND_OK);
        }
        // Left and right in one calc toggle twice.
        if (M_Pressed(M_LEFT) && menu->cursor == 9) {
            M_ToggleSetting(&g_Settings.shoot_focuses);
        }
        if (M_Pressed(M_RIGHT) && menu->cursor == 9) {
            M_ToggleSetting(&g_Settings.shoot_focuses);
        }
        if (g_Buttons.word != 0) {
            menu->menu_idle_frames = 0;
        }
        if (menu->menu_idle_frames >= M_MENU_IDLE_FRAMES) {
            return M_LeaveKeyConfig(menu);
        }
        if (M_Pressed(M_CONFIRM)) {
            if (menu->cursor == 10) {
                Sound_Queue(SOUND_OK);
                menu->mapping = M_DefaultMapping();
                menu->committed = menu->mapping;
                menu->reset_pad = true;
                g_Settings.shoot_focuses = true;
            } else if (menu->cursor == 11) {
                return M_LeaveKeyConfig(menu);
            }
        }
        break;
    }
    default:
        break;
    }
    return M_Count(menu);
}

static bool M_IsExtraState(const M_STATE state)
{
    return state == M_STATE_EXTRA_DIFFICULTY || state == M_STATE_EXTRA_CHARACTER
        || state == M_STATE_EXTRA_SHOT;
}

static bool M_CharacterOpen(const int32_t character)
{
    switch (g_Settings.difficulty) {
    case 4:
        return GameManager_HasExtraOpen(character * 2)
            || GameManager_HasExtraOpen(character * 2 + 1);
    case 5:
        return GameManager_HasPhantasmOpen(character * 2)
            || GameManager_HasPhantasmOpen(character * 2 + 1);
    default:
        return true;
    }
}

static bool M_ShotOpen(const int32_t shot)
{
    const int32_t index = g_GameManager.run.character * 2 + shot;
    switch (g_Settings.difficulty) {
    case 4:
        return GameManager_HasExtraOpen(index);
    case 5:
        return GameManager_HasPhantasmOpen(index);
    default:
        return true;
    }
}

// Moves the cursor on to the next open entry, as the original's loops do.
// The selects open only with one entry open; the count stops a hang anyway.
static void M_SkipLocked(
    M_MENU *const menu, const int32_t count, bool (*const open)(int32_t))
{
    for (int32_t i = 0; i < count && !open(menu->cursor); i++) {
        menu->cursor++;
        if (menu->cursor >= count) {
            menu->cursor -= count;
        }
    }
}

// Shows or hides the five sprites of a character: its two shot types, its
// portrait and two more.
static void M_ShowCharacter(
    M_MENU *const menu, const int32_t character, const bool visible)
{
    if (character >= 3) {
        return;
    }
    const int32_t indices[] = {
        0x48 + character * 3, 0x49 + character * 3, 0x47 + character * 3,
        0x50 + character,     0x53 + character,
    };
    for (size_t i = 0; i < ARRAY_SIZE(indices); i++) {
        menu->sprites[indices[i]].visible = visible;
    }
}

static void M_ShowCharacters(M_MENU *const menu, const bool visible)
{
    for (int32_t character = 0; character < 3; character++) {
        M_ShowCharacter(menu, character, visible);
    }
}

// Sends 9 to the chosen character's sprites and 8 to the others'. clear
// also makes the others transparent.
static void M_ChooseCharacter(M_MENU *const menu, const bool clear)
{
    for (int32_t character = 0; character < 3; character++) {
        const int32_t indices[] = {
            0x47 + character * 3,
            0x50 + character,
            0x53 + character,
        };
        for (size_t i = 0; i < ARRAY_SIZE(indices); i++) {
            ANM_VM *const vm = &menu->sprites[indices[i]];
            vm->pending_interrupt = character == menu->cursor ? 9 : 8;
            if (clear && character != menu->cursor) {
                vm->color &= 0xffffff;
            }
        }
    }
}

static void M_MarkDifficulty(M_MENU *const menu)
{
    const int32_t difficulty = g_Settings.difficulty;
    if (difficulty < 4) {
        menu->sprites[difficulty + 0x43].pending_interrupt = 9;
    } else if (!GameManager_IsPhantasmOpen()) {
        menu->sprites[0xa1].pending_interrupt = 9;
    } else {
        menu->sprites[difficulty + 0x9e].pending_interrupt = 9;
    }
}

static M_STATE M_SelectState(const M_STATE normal, const M_STATE practice)
{
    return g_GameManager.mode.in_practice ? practice : normal;
}

static void M_HighlightDifficulty(M_MENU *const menu)
{
    if (!M_IsExtraState(menu->state)) {
        M_Highlight(menu, 0x43, 4, menu->cursor);
    } else {
        M_Highlight(menu, 0xa2, 2, menu->cursor);
    }
}

static CHAIN_STATUS M_UpdateDifficulty(M_MENU *const menu)
{
    const bool extra = M_IsExtraState(menu->state);
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            const M_STATE prev = menu->prev_state;
            if (prev != M_STATE_CHARACTER && prev != M_STATE_PRACTICE_CHARACTER
                && prev != M_STATE_EXTRA_CHARACTER) {
                const RESULT result =
                    Anm_LoadSurface(0, "data/title/select00.jpg");
                if (!IS_OK(result)) {
                    return M_FailLoad(result);
                }
            }
            menu->cursor = g_Settings.difficulty;
            if (!extra) {
                M_Interrupt(menu, 7);
            } else if (!GameManager_IsPhantasmOpen()) {
                M_Interrupt(menu, 12);
                menu->cursor = 4;
            } else {
                M_Interrupt(menu, 22);
            }
            if (!extra) {
                if (menu->cursor >= 4) {
                    menu->cursor = 1;
                }
            } else {
                menu->cursor -= 4;
                if (menu->cursor < 0) {
                    menu->cursor = 0;
                }
            }
            M_HighlightDifficulty(menu);
            menu->state_frames = 0;
            menu->help = -1;
        }
        if (menu->returning_from_practice) {
            M_SetState(menu, M_STATE_PRACTICE_CHARACTER);
            menu->cursor = 0;
            return CHAIN_CONTINUE;
        }
        if (menu->input_lockout == 30) {
            menu->sub_state = M_SUB_MENU;
        }
        break;
    case M_SUB_MENU: {
        const int32_t count =
            !extra ? 4 : (GameManager_IsPhantasmOpen() ? 2 : 1);
        if (M_MoveCursor(menu, count) != 0 && (!extra || count == 2)) {
            M_HighlightDifficulty(menu);
        }
        if (M_Pressed(M_CONFIRM)) {
            g_Settings.difficulty = extra ? menu->cursor + 4 : menu->cursor;
            Sound_Queue(SOUND_OK);
            M_SetState(
                menu,
                extra ? M_STATE_EXTRA_CHARACTER
                      : M_SelectState(
                            M_STATE_CHARACTER, M_STATE_PRACTICE_CHARACTER));
            menu->cursor = 0;
            return CHAIN_CONTINUE;
        }
        if (M_Pressed(M_CANCEL)) {
            g_Settings.difficulty = extra ? menu->cursor + 4 : menu->cursor;
            Sound_Queue(SOUND_CANCEL);
            menu->sub_state = M_SUB_LEAVE;
            menu->state_frames = 0;
            M_Interrupt(menu, 6);
        }
        break;
    }
    case M_SUB_LEAVE:
        if (menu->state_frames >= 30) {
            M_SetState(menu, M_STATE_TITLE);
            if (!extra) {
                menu->cursor = g_GameManager.mode.in_practice ? 2 : 0;
            } else {
                menu->cursor = 1;
            }
            g_GameManager.mode.in_practice = false;
            return CHAIN_CONTINUE;
        }
        break;
    default:
        break;
    }
    return M_Count(menu);
}

static CHAIN_STATUS M_UpdateCharacter(M_MENU *const menu)
{
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            M_Interrupt(menu, 8);
            M_MarkDifficulty(menu);
            menu->cursor = g_GameManager.run.character;
            M_SkipLocked(menu, 3, M_CharacterOpen);
            M_ShowCharacters(menu, false);
            M_ShowCharacter(menu, menu->cursor, true);
            M_ChooseCharacter(menu, true);
            menu->state_frames = 0;
        }
        if (menu->returning_from_practice) {
            M_SetState(menu, M_STATE_PRACTICE_SHOT);
            menu->cursor = 0;
            return CHAIN_CONTINUE;
        }
        if (menu->input_lockout == 30) {
            menu->sub_state = M_SUB_MENU;
        }
        break;
    case M_SUB_MENU:
        if (M_MoveCursorSideways(menu, 3) != 0) {
            M_SkipLocked(menu, 3, M_CharacterOpen);
            M_ShowCharacters(menu, true);
            M_ChooseCharacter(menu, false);
        }
        if (M_Pressed(M_CONFIRM)) {
            g_GameManager.run.character = (uint8_t)menu->cursor;
            Sound_Queue(SOUND_OK);
            M_SetState(
                menu,
                menu->state == M_STATE_EXTRA_CHARACTER
                    ? M_STATE_EXTRA_SHOT
                    : M_SelectState(M_STATE_SHOT, M_STATE_PRACTICE_SHOT));
            menu->cursor = 0;
            return CHAIN_CONTINUE;
        }
        if (M_Pressed(M_CANCEL)) {
            Sound_Queue(SOUND_CANCEL);
            g_GameManager.run.character = (uint8_t)menu->cursor;
            M_SetState(
                menu,
                menu->state == M_STATE_EXTRA_CHARACTER
                    ? M_STATE_EXTRA_DIFFICULTY
                    : M_SelectState(
                          M_STATE_DIFFICULTY, M_STATE_PRACTICE_DIFFICULTY));
            menu->cursor = 0;
            return CHAIN_CONTINUE;
        }
        break;
    default:
        break;
    }
    return M_Count(menu);
}

// Shows the chosen character's portrait and shot types.
static void M_ShowShots(M_MENU *const menu)
{
    const int32_t character = g_GameManager.run.character;
    if (character < 3) {
        M_ShowCharacter(menu, character, true);
        menu->sprites[0x50 + character].visible = false;
        menu->sprites[0x53 + character].visible = false;
    }
}

static void M_HighlightShot(M_MENU *const menu)
{
    const int32_t first = 0x48 + g_GameManager.run.character * 3;
    M_SetSprite(&menu->sprites[first + 1 - menu->cursor], 1);
    M_SetSprite(&menu->sprites[first + menu->cursor], 0);
}

static CHAIN_STATUS M_UpdateShot(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            M_Interrupt(menu, 10);
            M_MarkDifficulty(menu);
            M_ShowCharacters(menu, false);
            menu->cursor = gm->run.shot_type;
            M_SkipLocked(menu, 2, M_ShotOpen);
            M_ShowShots(menu);
            if (gm->run.character < 3) {
                M_HighlightShot(menu);
            }
            menu->state_frames = 0;
        }
        if (menu->returning_from_practice) {
            M_SetState(menu, M_STATE_PRACTICE_STAGE);
            menu->returning_from_practice = false;
            menu->cursor = gm->run.stage - 1;
            return CHAIN_CONTINUE;
        }
        if (menu->input_lockout == 30) {
            menu->sub_state = M_SUB_MENU;
        }
        break;
    case M_SUB_MENU:
        if (M_MoveCursor(menu, 2) != 0) {
            M_SkipLocked(menu, 2, M_ShotOpen);
            if (gm->run.character < 3) {
                M_HighlightShot(menu);
            }
        }
        if (M_Pressed(M_CONFIRM)) {
            gm->run.shot_type = (uint8_t)menu->cursor;
            Sound_Queue(SOUND_OK);
            if (!gm->mode.in_practice) {
                gm->run.difficulty = g_Settings.difficulty;
                gm->run.stage =
                    gm->run.difficulty < 4 ? 0 : gm->run.difficulty + 2;
                M_StartPlay();
                return CHAIN_REMOVE;
            }
            menu->cursor = 0;
            M_SetState(menu, M_STATE_PRACTICE_STAGE);
            return CHAIN_AGAIN;
        }
        if (M_Pressed(M_CANCEL)) {
            Sound_Queue(SOUND_CANCEL);
            gm->run.shot_type = (uint8_t)menu->cursor;
            M_SetState(
                menu,
                menu->state == M_STATE_EXTRA_SHOT
                    ? M_STATE_EXTRA_CHARACTER
                    : M_SelectState(
                          M_STATE_CHARACTER, M_STATE_PRACTICE_CHARACTER));
            M_ShowCharacters(menu, true);
            return CHAIN_AGAIN;
        }
        break;
    default:
        break;
    }
    return M_Count(menu);
}

static int32_t M_PracticeStages(void)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    const int32_t shot = gm->run.character * 2 + gm->run.shot_type;
    int32_t count =
        gm->records.clears[shot].with_continues[g_Settings.difficulty];
    if (count >= 99) {
        count = 6;
    }
    return count;
}

static CHAIN_STATUS M_UpdatePracticeStage(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            M_Interrupt(menu, 18);
            M_ShowCharacters(menu, false);
            M_ShowShots(menu);
            menu->state_frames = 0;
            gm->mode.in_practice = true;
        }
        if (menu->input_lockout == 30) {
            menu->sub_state = M_SUB_MENU;
        }
        break;
    case M_SUB_MENU: {
        const int32_t count = M_PracticeStages();
        if (menu->cursor >= count) {
            menu->cursor = 0;
        }
        M_MoveCursor(menu, count);
        if (M_Pressed(M_CONFIRM)) {
            Sound_Queue(SOUND_OK);
            gm->run.difficulty = g_Settings.difficulty;
            gm->run.stage = menu->cursor;
            M_StartPlay();
            return CHAIN_REMOVE;
        }
        if (M_Pressed(M_CANCEL)) {
            Sound_Queue(SOUND_CANCEL);
            menu->cursor = gm->run.shot_type;
            // The original goes back to the shot select of a normal start,
            // which still starts a practice.
            M_SetState(menu, M_STATE_SHOT);
            M_ShowCharacters(menu, true);
            return CHAIN_AGAIN;
        }
        break;
    }
    default:
        break;
    }
    return M_Count(menu);
}

static bool M_StagePresent(const M_MENU *const menu, const int32_t index)
{
    return menu->replays[menu->replay_cursor].summary.stages[index + 1];
}

static void M_CopyReplays(M_MENU *const menu)
{
    menu->replay_count = ResultScreen_ListReplays(menu->replays);
    menu->replay_scan = ResultScreen_GetReplayScan();
}

static void M_ChoosePlayback(M_MENU *const menu)
{
    for (int32_t i = 0; i < 3; i++) {
        menu->sprites[0x9e + i].pending_interrupt = 21;
    }
    menu->sprites[0x9e + menu->cursor].pending_interrupt = 20;
}

static void M_MarkReplay(M_MENU *const menu, const int16_t interrupt)
{
    M_Interrupt(menu, interrupt);
    menu->sprites[menu->replay_cursor % M_PAGE + 0x87].pending_interrupt = 17;
}

static void M_UpdateReplayList(M_MENU *const menu)
{
    M_MoveCursor(menu, menu->replay_count);
    if (menu->replay_count > M_PAGE) {
        if (M_PressedOrRepeated(M_LEFT)) {
            menu->cursor -= M_PAGE;
            if (menu->cursor < 0) {
                menu->cursor += menu->replay_count;
            }
            Sound_Queue(SOUND_SELECT);
        }
        if (M_PressedOrRepeated(M_RIGHT)) {
            menu->cursor += M_PAGE;
            if (menu->cursor >= menu->replay_count) {
                menu->cursor -= menu->replay_count;
            }
            Sound_Queue(SOUND_SELECT);
        }
    }
    menu->replay_cursor = menu->cursor;
    if (menu->state_frames < 10) {
        return;
    }
    if (M_Pressed(M_CONFIRM)) {
        if (menu->replay_count == 0) {
            return;
        }
        Sound_Queue(SOUND_OK);
        menu->sub_state = M_SUB_REPLAY_STAGE;
        M_MarkReplay(menu, 15);
        menu->cursor = 0;
        while (menu->cursor < 7 && !M_StagePresent(menu, menu->cursor)) {
            menu->cursor++;
        }
        return;
    }
    if (M_Pressed(M_CANCEL)) {
        Sound_Queue(SOUND_CANCEL);
        menu->sub_state = M_SUB_REPLAY_LEAVE;
        menu->state_frames = 0;
        M_Interrupt(menu, 16);
    }
}

static void M_UpdateReplayStage(M_MENU *const menu)
{
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
    menu->replay_stage = menu->cursor;
    if (M_Pressed(M_CONFIRM)) {
        M_MarkReplay(menu, 19);
        menu->sub_state = M_SUB_REPLAY_PLAYBACK;
        menu->cursor = 0;
        M_ChoosePlayback(menu);
        return;
    }
    if (M_Pressed(M_CANCEL)) {
        menu->sub_state = M_SUB_MENU;
        menu->input_lockout = 0;
        M_Interrupt(menu, 14);
        menu->cursor = menu->replay_cursor;
    }
}

static CHAIN_STATUS M_UpdateReplayPlayback(M_MENU *const menu)
{
    if (M_MoveCursor(menu, 3) != 0) {
        M_ChoosePlayback(menu);
    }
    if (M_Pressed(M_CONFIRM)) {
        const SAVED_REPLAY *const replay = &menu->replays[menu->replay_cursor];
        if (Result_Log(
                TitleScene_SetUpReplay(
                    &replay->summary, replay->path, menu->replay_stage,
                    menu->cursor),
                LOG_LEVEL_ERROR)) {
            return CHAIN_REMOVE;
        }
    }
    if (M_Pressed(M_CANCEL)) {
        menu->sub_state = M_SUB_REPLAY_STAGE;
        menu->input_lockout = 0;
        menu->cursor = menu->replay_stage;
        M_MarkReplay(menu, 15);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_UpdateReplays(M_MENU *const menu)
{
    // A list that opened before the scan of the replay directory takes the
    // scanned list once it arrives.
    if (menu->replay_scan != ResultScreen_GetReplayScan()) {
        M_CopyReplays(menu);
        if (menu->cursor >= menu->replay_count) {
            menu->cursor = 0;
        }
        menu->replay_cursor = menu->cursor;
    }
    switch (menu->sub_state) {
    case M_SUB_ENTER:
        if (menu->input_lockout == 0) {
            if (menu->prev_state != M_STATE_REPLAYS) {
                const RESULT result =
                    Anm_LoadSurface(0, "data/title/select00.jpg");
                if (!IS_OK(result)) {
                    return M_FailLoad(result);
                }
            }
            M_Interrupt(menu, 14);
            menu->cursor = 0;
            menu->state_frames = 0;
            menu->help = -1;
            M_CopyReplays(menu);
            ResultScreen_RequestReplayScan();
        }
        if (menu->input_lockout >= 30) {
            menu->sub_state = M_SUB_MENU;
            menu->state_frames = 0;
        }
        break;
    case M_SUB_MENU:
        M_UpdateReplayList(menu);
        break;
    case M_SUB_REPLAY_STAGE:
        M_UpdateReplayStage(menu);
        break;
    case M_SUB_REPLAY_PLAYBACK:
        if (M_UpdateReplayPlayback(menu) == CHAIN_REMOVE) {
            return CHAIN_REMOVE;
        }
        break;
    case M_SUB_REPLAY_LEAVE:
        if (menu->state_frames >= 30) {
            M_SetState(menu, M_STATE_TITLE);
            menu->cursor = 3;
            return CHAIN_CONTINUE;
        }
        break;
    default:
        break;
    }
    return M_Count(menu);
}

static RESULT M_FinishAdded(M_MENU *menu);

static CHAIN_STATUS M_UpdateSplash(M_MENU *const menu)
{
    const uint16_t word = g_Buttons.word & M_SKIP_SPLASH;
    if (menu->splash_frames >= 120 && menu->splash_frames < 840 && word != 0
        && word != (menu->splash_previous & M_SKIP_SPLASH)) {
        menu->splash_frames = 840;
        Sound_Queue(SOUND_OK);
    }
    menu->splash_frames++;
    if (menu->splash_frames < M_SPLASH_FRAMES) {
        return CHAIN_CONTINUE;
    }
    menu->splash = false;
    Anm_ReleaseSurface(0);
    const RESULT result = M_FinishAdded(menu);
    return IS_OK(result) ? CHAIN_CONTINUE : M_FailLoad(result);
}

static CHAIN_STATUS M_RunState(M_MENU *const menu)
{
    switch (menu->state) {
    case M_STATE_TITLE:
        return M_UpdateTitle(menu);
    case M_STATE_REPLAYS:
        return M_UpdateReplays(menu);
    case M_STATE_OPTIONS:
        return M_UpdateOptions(menu);
    case M_STATE_KEY_CONFIG:
        return M_UpdateKeyConfig(menu);
    case M_STATE_DIFFICULTY:
    case M_STATE_PRACTICE_DIFFICULTY:
    case M_STATE_EXTRA_DIFFICULTY:
        return M_UpdateDifficulty(menu);
    case M_STATE_CHARACTER:
    case M_STATE_PRACTICE_CHARACTER:
    case M_STATE_EXTRA_CHARACTER:
        return M_UpdateCharacter(menu);
    case M_STATE_SHOT:
    case M_STATE_PRACTICE_SHOT:
    case M_STATE_EXTRA_SHOT:
        return M_UpdateShot(menu);
    case M_STATE_PRACTICE_STAGE:
        return M_UpdatePracticeStage(menu);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_MENU *const menu = arg;
    if (menu->splash) {
        return M_UpdateSplash(menu);
    }
    const CHAIN_STATUS status = M_RunState(menu);
    if (status == CHAIN_REMOVE) {
        return CHAIN_REMOVE;
    }
    if (menu->sprites_started) {
        Anm_StepVms(menu->sprites, M_VM_COUNT);
    }
    if (menu->help >= 0) {
        Anm_Step(&menu->help_sprites[menu->help]);
    }
    return status;
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

static bool M_HasTexture(const ANM_VM *const vm)
{
    return vm->sprite_data != nullptr && vm->sprite_data->slot >= 0
        && Anm_GetTexture(vm->sprite_data->slot) != nullptr;
}

static void M_DrawSplash(const M_MENU *const menu)
{
    Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    const int32_t frame = menu->splash_frames;
    const GFX_RECT screen = { 0, 0, 640, 480 };
    if (frame < 60) {
        Anm_DrawRect(screen, (uint32_t)((60 - frame) * 255 / 60) << 24);
    } else if (frame > 840) {
        Anm_DrawRect(screen, (uint32_t)((frame - 840) * 255 / 60) << 24);
    }
}

static void M_SetRowColor(const bool chosen, const uint32_t other)
{
    AsciiText_SetColor(chosen ? COLOR_WHITE : other);
}

static void M_DrawReplays(const M_MENU *const menu)
{
    const ANM_VM *text = &menu->sprites[0x86];
    AsciiText_AddString(text->pos, g_Strings[STRING_MENU_REPLAY_HEADER]);
    const int32_t top = menu->replay_cursor - menu->replay_cursor % M_PAGE;
    for (int32_t j = top; j < top + M_PAGE && j < menu->replay_count; j++) {
        const SAVED_REPLAY *const replay = &menu->replays[j];
        text++;
        AsciiText_SetHighlight(j == menu->replay_cursor);
        M_SetRowColor(
            j == menu->replay_cursor, COLOR_ARGB(0xff, 0x80, 0x80, 0x80));
        AsciiText_AddFormat(
            text->pos, g_Strings[STRING_MENU_REPLAY], replay->label,
            replay->summary.name, replay->summary.date,
            M_StringName(m_ShotNames, 6, replay->summary.shot),
            M_Name(m_DifficultyNames, 6, replay->summary.difficulty));
    }
    if (menu->sub_state == M_SUB_REPLAY_STAGE
        || menu->sub_state == M_SUB_REPLAY_PLAYBACK) {
        const REPLAY_SUMMARY *const summary =
            &menu->replays[menu->replay_cursor].summary;
        AsciiText_SetColor(COLOR_WHITE);
        AsciiText_SetHighlight(false);
        AsciiText_AddFormat(
            menu->sprites[0x85].pos, "       %2.3f%%",
            (double)summary->slowdown);
        AsciiText_AddString(
            menu->sprites[0x96].pos, g_Strings[STRING_MENU_STAGE_HEADER]);
        for (int32_t i = 0; i < 7; i++) {
            const bool chosen = i == menu->replay_stage;
            if (menu->sub_state != M_SUB_REPLAY_PLAYBACK) {
                AsciiText_SetHighlight(chosen);
                M_SetRowColor(chosen, COLOR_ARGB(0xff, 0x80, 0x80, 0x80));
            } else {
                AsciiText_SetColor(
                    chosen ? COLOR_ARGB(0x60, 0xff, 0xff, 0xff)
                           : COLOR_ARGB(0x60, 0x80, 0x80, 0x80));
            }
            const char *const name = i < 6 || summary->difficulty <= 4
                ? m_StageNames[i]
                : m_StageNames[7];
            const VEC3 pos = menu->sprites[0x97 + i].pos;
            if (summary->stages[i + 1]) {
                AsciiText_AddFormat(
                    pos, g_Strings[STRING_MENU_STAGE], name,
                    summary->stage_scores[i + 1]);
            } else {
                AsciiText_AddFormat(
                    pos, g_Strings[STRING_MENU_STAGE_EMPTY], name);
            }
        }
    }
    AsciiText_SetColor(COLOR_WHITE);
    AsciiText_SetHighlight(false);
}

static void M_DrawPracticeScores(const M_MENU *const menu)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    AsciiText_SetColor(COLOR_WHITE);
    AsciiText_SetHighlight(false);
    VEC3 pos = menu->sprites[0x83].pos;
    AsciiText_AddString(pos, g_Strings[STRING_PRACTICE_STAGE_HEADER]);
    pos.y += 16.0f;
    const int32_t shot = gm->run.character * 2 + gm->run.shot_type;
    const int32_t difficulty = g_Settings.difficulty;
    const int32_t cleared = gm->records.clears[shot].with_continues[difficulty];
    for (int32_t i = 0; i < 6; i++) {
        AsciiText_SetHighlight(i == menu->cursor);
        if (i == menu->cursor) {
            AsciiText_SetColor(COLOR_WHITE);
        } else if (i < cleared) {
            AsciiText_SetColor(COLOR_ARGB(0xff, 0xa0, 0xa0, 0xa0));
        } else {
            AsciiText_SetColor(COLOR_ARGB(0xff, 0x40, 0x40, 0x40));
        }
        AsciiText_AddFormat(
            pos, g_Strings[STRING_PRACTICE_SCORE],
            g_Strings[m_PracticeStageNames[i]],
            (int32_t)gm->records.practice[shot][i][difficulty].score,
            (int32_t)gm->records.practice[shot][i][difficulty].play_count);
        pos.y += 16.0f;
    }
    AsciiText_SetColor(COLOR_WHITE);
    AsciiText_SetHighlight(false);
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    M_MENU *const menu = arg;
    if (menu->splash) {
        M_DrawSplash(menu);
        return CHAIN_CONTINUE;
    }
    Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    if (menu->state == M_STATE_REPLAYS) {
        M_DrawReplays(menu);
    } else if (menu->state == M_STATE_PRACTICE_STAGE) {
        M_DrawPracticeScores(menu);
    }
    for (int32_t i = 0; i < M_VM_COUNT; i++) {
        if (M_HasTexture(&menu->sprites[i])) {
            ANM_VM vm = menu->sprites[i];
            vm.pos = Vec3_Add(vm.pos, vm.pos_offset);
            const ANM_DRAW draw = M_ScreenDraw((uint32_t)i);
            if (vm.rotation.z != 0.0f) {
                Anm_Draw(&vm, &draw);
            } else {
                Anm_DrawUnrotated(&vm, &draw);
            }
        }
    }
    if (menu->help >= 0) {
        const ANM_DRAW draw = M_ScreenDraw(M_VM_COUNT);
        Anm_DrawUnrotated(&menu->help_sprites[menu->help], &draw);
    }
    return CHAIN_CONTINUE;
}

// Runs the part of the original's InitTitleScene after the Phantasm splash.
static RESULT M_FinishAdded(M_MENU *const menu)
{
    GAME_MANAGER *const gm = &g_GameManager;
    menu->state = M_STATE_TITLE;
    const SCENE previous = Scene_GetPrevious();
    switch (previous) {
    case SCENE_GAMEPLAY:
    case SCENE_NEXT_STAGE:
    case SCENE_GAME_RESULT:
        menu->cursor = gm->run.difficulty >= 4;
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
    menu->returning_from_practice = false;
    if (gm->mode.in_practice) {
        menu->cursor = 2;
        menu->returning_from_practice = true;
    }
    gm->mode.in_practice = false;
    MUST(Anm_Load(0x20, "data/title01.anm", 0x900));
    if (!gm->mode.in_demo) {
        ScreenEffect_FadeIn(70, 0xffffff);
    }
    for (int32_t i = 0; i < M_HELP_COUNT; i++) {
        ANM_VM *const vm = &menu->help_sprites[i];
        Anm_StartScript(vm, 0x706);
        Anm_SetSprite(vm, vm->sprite + i);
    }
    menu->help = 0;
    gm->mode.in_demo = false;
    gm->mode.demo_frames = 0;
    return OK;
}

static RESULT M_Added(void *const arg)
{
    M_MENU *const menu = arg;
    GAME_MANAGER *const gm = &g_GameManager;
    g_GameSpeed = 1.0f;
    if (gm->mode.in_replay) {
        gm->run.character = 0;
        gm->run.shot_index = 0;
    }
    if (gm->mode.in_demo) {
        gm->mode.in_replay = false;
    }
    ResultScreen_LoadTitleRecords();
    if (gm->records.play_stats.play_hours < 7) {
        gm->run.max_continues = 3;
    } else if (gm->records.play_stats.play_hours < 14) {
        gm->run.max_continues = 4;
    } else {
        gm->run.max_continues = 5;
    }
    const bool open = GameManager_IsPhantasmOpen();
    if (!gm->records.phantasm_was_open && open) {
        gm->records.phantasm_was_open = open;
        Result_Log(
            Anm_LoadSurface(0, "data/title/phantasm.jpg"), LOG_LEVEL_WARNING);
        menu->splash = true;
        menu->splash_frames = 0;
        menu->splash_previous = g_Buttons.previous_word;
        menu->added = true;
        return OK;
    }
    gm->records.phantasm_was_open = open;
    MUST(M_FinishAdded(menu));
    menu->added = true;
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_MENU *const menu = arg;
    for (int32_t i = 0x20; i <= 0x29; i++) {
        Anm_Release(i);
    }
    Anm_ReleaseSurface(0);
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
    for (int32_t i = 0; i < M_HELP_COUNT; i++) {
        Anm_CreateVm(&menu->help_sprites[i]);
    }
    menu->help = -1;
    g_GameManager.mode.paused = 0;
    Chain_InitElem(&menu->calc, M_Update, menu);
    menu->calc.added = M_Added;
    menu->calc.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &menu->calc, M_CALC_PRIORITY));
    Chain_InitElem(&menu->draw, M_Draw, menu);
    return Chain_AddDraw(&g_Chain, &menu->draw, M_DRAW_PRIORITY);
}

RESULT TitleScene_SetUpReplay(
    const REPLAY_SUMMARY *const summary, const char *const path,
    const int32_t stage, const int32_t playback)
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
    gm->run.shot_index = summary->shot;
    gm->run.stage = gm->run.difficulty >= 5 ? 7 : stage;
    Scene_Request(SCENE_GAMEPLAY);
    gm->mode.replay_speed = (uint8_t)playback;
    Sound_StopMusic();
    return OK;
}

void TitleScene_Remove(void)
{
    if (m_Menu.added) {
        Chain_Remove(&m_Menu.calc);
    }
}
