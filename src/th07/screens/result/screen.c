#include "th07/screens/result/screen.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/buttons.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/play_time.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screens/result/private.h"
#include "th07/sound.h"

#include "core/base/color.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include <stdio.h>
#include <string.h>

#define M_CALC_PRIORITY 14
#define M_DRAW_PRIORITY 13
#define M_ANM_SLOT 0x2a
#define M_SPRITE_COUNT 0x29
#define M_TEXT_SPRITE_COUNT 0xf
#define M_FIRST_SCRIPT 0x900
#define M_FIRST_TEXT_SPRITE 0x715
#define M_DIVIDER_SPRITE 0x910
#define M_MENU_ITEM_COUNT 9
#define M_PHANTASM_ITEM 5
#define M_QUIT_ITEM 8
#define M_CHOICE_SPRITE 0x13
#define M_BOARD_SPRITE 0x10
#define M_SLOT_FRAME_SPRITE 0x12
#define M_SLOT_HEADER_SPRITE 0x18
#define M_SLOT_SPRITE 0x19
#define M_PANEL_SPRITE 0x28
#define M_STATS_TEXT_COUNT 0xe
#define M_ALPHABET_SIZE 0x60
#define M_ALPHABET_SPACE 0x5e
#define M_ALPHABET_END 0x5f
#define M_NAME_LENGTH 8
#define M_CLEAR_STAGE 99
#define M_SPELL_PAGE_SIZE 10
#define M_SPELL_PAGE_COUNT 15
#define M_SPELL_SHOT_COUNT 7

#define M_UP 0x10
#define M_DOWN 0x20
#define M_LEFT 0x40
#define M_RIGHT 0x80
#define M_MENU 0x8
#define M_CONFIRM 0x1001
#define M_CANCEL 0xa
#define M_CLOSE_STATS 0x100b
#define M_UNLOCK_RESET 0x160b

typedef enum {
    M_STATE_OPEN_MENU = 0,
    M_STATE_MENU = 1,
    M_STATE_LEAVE = 2,
    M_STATE_SCORES_EASY = 3,
    M_STATE_SCORES_NORMAL = 4,
    M_STATE_SCORES_HARD = 5,
    M_STATE_SCORES_LUNATIC = 6,
    M_STATE_SCORES_EXTRA = 7,
    M_STATE_SCORES_PHANTASM = 8,
    M_STATE_SPELL_CARDS = 9,
    M_STATE_NAME_ENTRY = 10,
    M_STATE_REPLAY_PROMPT = 11,
    M_STATE_REPLAY_REFUSED = 12,
    M_STATE_REPLAY_SLOTS = 13,
    M_STATE_REPLAY_NAME = 14,
    M_STATE_REPLAY_OVERWRITE = 15,
    M_STATE_STATISTICS = 16,
    M_STATE_STATISTICS_LEAVE = 17,
    M_STATE_PRACTICE = 18,
    M_STATE_PLAY_STATS_OPEN = 20,
    M_STATE_PLAY_STATS = 21,
    M_STATE_PLAY_STATS_CLOSE = 22,
} M_STATE;

typedef struct {
    bool score_loaded;
    int32_t frame;
    M_STATE state;
    M_STATE list_state;
    int32_t cursor;
    int32_t saved_cursor;
    int32_t saved_spell_page;
    int32_t replay_slot;
    int32_t char_index;
    // Moves with up and down over the spell cards: the shot whose records
    // they show, SCORE_ALL_SHOTS for every shot.
    int32_t spell_shot;
    int32_t shown_spell_shot;
    bool spell_rows_sliding;
    // Holds the shot whose name text_sprites[0] shows, or -1 to render it.
    int32_t shot;
    // Holds the spell card page that text_sprites show, or -1 to render it.
    int32_t spell_page;
    int32_t difficulty; // the menu item whose list is shown
    int32_t unlock_progress;
    bool name_entry_at_end; // score.dat holds a last name
    uint8_t name[12];
    int32_t captured_counts[M_SPELL_SHOT_COUNT];
    uint8_t shown_run_seconds;
    ANM_VM sprites[M_SPRITE_COUNT];
    ANM_VM text_sprites[M_TEXT_SPRITE_COUNT];
    ANM_VM divider;
    SCORE_LIST scores[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];
    // Points at the entry being named in its list, or at entry when it
    // ranks below the tenth.
    HIGH_SCORE *named;
    HIGH_SCORE entry;
    LAST_NAME_RECORD last_name;
    SAVED_REPLAY slots[SAVED_REPLAY_SLOT_COUNT];
    char replay_date[6];
    uint32_t replay_score;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
} M_SCREEN;

// Holds the glyphs of the name entry grid, 16 to a row. The last two cells
// stand for a space and for the end of the name.
static const char m_NameAlphabet[M_ALPHABET_SIZE + 1] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ.,:;_@abcdefghijklmnopqrstuvwxyz+-/*=%"
    "0123456789#!?'\"$(){}[]<>&\\|~^ --";

// Names the shots by shot index, then every shot.
static const char *const m_ShotNames[M_SPELL_SHOT_COUNT] = {
    "博麗 霊夢 (霊)　", "博麗 霊夢 (夢)　", "霧雨 魔理沙 (魔)",
    "霧雨 魔理沙 (恋)", "十六夜 咲夜 (幻)", "十六夜 咲夜 (時)",
    "全主人公合計  　",
};

static const char *const m_ShotTypeNames[SCORE_SHOT_COUNT] = {
    "ReimuA ", "ReimuB ", "MarisaA", "MarisaB", "SakuyaA", "SakuyaB",
};

static const char *const m_DifficultyNames[SCORE_DIFFICULTY_COUNT] = {
    "      Easy", "    Normal", "      Hard",
    "   Lunatic", "     Extra", "  Phantasm",
};

static M_SCREEN m_Screen;

static bool M_Pressed(const uint16_t mask)
{
    return Buttons_WasPressed(&g_Buttons, mask);
}

static bool M_PressedOrRepeated(const uint16_t mask)
{
    return Buttons_WasPressedOrRepeated(&g_Buttons, mask);
}

static void M_Interrupt(M_SCREEN *const screen, const int16_t interrupt)
{
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        screen->sprites[i].pending_interrupt = interrupt;
    }
}

static void M_SetAlpha(ANM_VM *const vm, const int32_t alpha)
{
    vm->color = (vm->color & 0xffffff) | ((uint32_t)alpha << 24);
}

static bool M_RecordsNoScore(void)
{
    return g_GameManager.run.score_disabled || g_GameManager.mode.in_replay;
}

static float M_Slowdown(void)
{
    const RESULT_SCREEN_CLOCK *const clock = ResultSaves_GetClock();
    float rate =
        (clock->achieved_frames / clock->expected_frames - 0.5f) * 2.0f;
    if (rate < 0.0f) {
        rate = 0.0f;
    } else if (rate >= 1.0f) {
        rate = 1.0f;
    }
    return (1.0f - rate) * 100.0f;
}

// Copies the NUL-terminated name at source, as the original's strcpy
// does, cut to fit size bytes.
static void M_CopyName(
    uint8_t *const dest, const size_t size, const uint8_t *const source,
    const size_t source_size)
{
    size_t length = 0;
    while (length < source_size && length < size - 1 && source[length] != 0) {
        length++;
    }
    memcpy(dest, source, length);
    dest[length] = 0;
}

// Returns -1 for up and 1 for down when the value moved, else 0.
static int32_t M_MoveVertical(int32_t *const value, const int32_t count)
{
    if (M_PressedOrRepeated(M_UP)) {
        (*value)--;
        if (*value < 0) {
            *value += count;
        }
        Sound_Queue(SOUND_SELECT);
        return -1;
    }
    if (M_PressedOrRepeated(M_DOWN)) {
        (*value)++;
        if (*value >= count) {
            *value -= count;
        }
        Sound_Queue(SOUND_SELECT);
        return 1;
    }
    return 0;
}

static bool M_MoveCursorHorizontal(M_SCREEN *const screen, const int32_t count)
{
    if (M_PressedOrRepeated(M_LEFT)) {
        screen->cursor--;
        if (screen->cursor < 0) {
            screen->cursor += count;
        }
        Sound_Queue(SOUND_SELECT);
        return true;
    }
    if (M_PressedOrRepeated(M_RIGHT)) {
        screen->cursor++;
        if (screen->cursor >= count) {
            screen->cursor -= count;
        }
        Sound_Queue(SOUND_SELECT);
        return true;
    }
    return false;
}

// Moves the name entry cursor over the grid, skipping the empty cells.
static void M_MoveCharIndex(M_SCREEN *const screen)
{
    if (M_PressedOrRepeated(M_UP)) {
        do {
            screen->char_index -= 16;
            if (screen->char_index < 0) {
                screen->char_index += M_ALPHABET_SIZE;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_DOWN)) {
        do {
            screen->char_index += 16;
            if (screen->char_index >= M_ALPHABET_SIZE) {
                screen->char_index -= M_ALPHABET_SIZE;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_LEFT)) {
        do {
            screen->char_index--;
            if (screen->char_index % 16 == 15) {
                screen->char_index += 16;
            }
            if (screen->char_index < 0) {
                screen->char_index = 15;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_RIGHT)) {
        do {
            screen->char_index++;
            if (screen->char_index % 16 == 0) {
                screen->char_index -= 16;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
}

// Returns the position that the next character of a name goes to. A
// cursor at the end of the name replaces the last character.
static int32_t M_NamePosition(const M_SCREEN *const screen)
{
    return screen->cursor >= M_NAME_LENGTH ? M_NAME_LENGTH - 1 : screen->cursor;
}

// Writes the chosen character at the cursor. Returns false for the end
// mark, which writes nothing.
static bool M_TypeCharacter(M_SCREEN *const screen, uint8_t *const name)
{
    const int32_t position = M_NamePosition(screen);
    if (screen->char_index < M_ALPHABET_SPACE) {
        name[position] = (uint8_t)m_NameAlphabet[screen->char_index];
    } else if (screen->char_index == M_ALPHABET_SPACE) {
        name[position] = ' ';
    } else {
        return false;
    }
    return true;
}

static void M_AdvanceNameCursor(M_SCREEN *const screen)
{
    if (screen->cursor < M_NAME_LENGTH) {
        screen->cursor++;
        if (screen->cursor == M_NAME_LENGTH) {
            screen->char_index = M_ALPHABET_END;
        }
    }
    Sound_Queue(SOUND_OK);
}

// Moves the cursor back and blanks both the character there and the one
// where the cursor was.
static void M_EraseNameCharacter(M_SCREEN *const screen, uint8_t *const name)
{
    const int32_t position = M_NamePosition(screen);
    if (screen->cursor > 0) {
        screen->cursor--;
        name[position] = ' ';
        name[screen->cursor] = ' ';
    }
    Sound_Queue(SOUND_CANCEL);
}

static void M_RenderShotName(M_SCREEN *const screen)
{
    ANM_VM *const vm = &screen->text_sprites[0];
    Result_Log(
        Anm_RenderTextCentered(
            vm, 0xffffff, 0, "%s", m_ShotNames[screen->shot]),
        LOG_LEVEL_WARNING);
    M_SetAlpha(vm, 0xff);
}

static void M_EndNameEntry(M_SCREEN *const screen)
{
    screen->state = M_STATE_STATISTICS;
    screen->frame = 0;
    M_Interrupt(screen, 2);
    M_CopyName(
        screen->name, sizeof(screen->name), screen->named->name,
        sizeof(screen->named->name));
    M_CopyName(
        screen->last_name.name, sizeof(screen->last_name.name), screen->name,
        sizeof(screen->name));
}

static void M_StartNameEntry(M_SCREEN *const screen)
{
    GAME_MANAGER *const gm = &g_GameManager;
    screen->shot = gm->run.shot_index;
    screen->difficulty = gm->run.difficulty;
    M_Interrupt(screen, (int16_t)(screen->difficulty + 3));
    M_RenderShotName(screen);
    HIGH_SCORE *const entry = &screen->entry;
    entry->shot = (uint8_t)screen->shot;
    entry->difficulty = (uint8_t)screen->difficulty;
    entry->score = gm->score.value;
    entry->continues_used = gm->stats.continues_used;
    entry->magic = HIGH_SCORE_MAGIC;
    entry->stage =
        gm->mode.game_cleared ? M_CLEAR_STAGE : (uint8_t)gm->run.stage;
    entry->is_new = true;
    M_CopyName(
        entry->name, sizeof(entry->name), screen->last_name.name,
        sizeof(screen->last_name.name));
    memcpy(entry->date, ResultSaves_GetClock()->date, sizeof(entry->date));
    entry->slowdown = M_Slowdown();
    SCORE_LIST *const list = &screen->scores[screen->difficulty][screen->shot];
    const int32_t rank = ScoreList_Insert(list, entry);
    if (rank >= SCORE_LIST_SIZE) {
        screen->named = entry;
        M_EndNameEntry(screen);
        return;
    }
    screen->named = &list->entries[rank];
    screen->cursor = 0;
    if (screen->name_entry_at_end) {
        screen->char_index = M_ALPHABET_END;
    }
    screen->name[0] = 0;
}

static void M_UpdateNameEntry(M_SCREEN *const screen)
{
    GAME_MANAGER *const gm = &g_GameManager;
    if (M_RecordsNoScore()) {
        screen->state = M_STATE_STATISTICS;
        screen->frame = 0;
        memcpy(
            gm->records.spell_cards, gm->records.saved_spell_cards,
            sizeof(gm->records.spell_cards));
        return;
    }
    if (screen->frame == 0) {
        M_StartNameEntry(screen);
        if (screen->state != M_STATE_NAME_ENTRY) {
            return;
        }
    }
    if (screen->frame < 30) {
        return;
    }
    M_MoveCharIndex(screen);
    uint8_t *const name = screen->named->name;
    if (M_PressedOrRepeated(M_CONFIRM)) {
        if (!M_TypeCharacter(screen, name)) {
            Sound_Queue(SOUND_CANCEL);
            M_EndNameEntry(screen);
            return;
        }
        M_AdvanceNameCursor(screen);
    }
    if (M_PressedOrRepeated(M_CANCEL)) {
        M_EraseNameCharacter(screen, name);
    }
    if (M_Pressed(M_MENU)) {
        Sound_Queue(SOUND_CANCEL);
        M_EndNameEntry(screen);
    }
}

static void M_Leave(M_SCREEN *const screen)
{
    screen->frame = 0;
    Sound_Queue(SOUND_CANCEL);
    screen->state = M_STATE_LEAVE;
    M_Interrupt(screen, 2);
}

static void M_HighlightChoice(M_SCREEN *const screen)
{
    ANM_VM *const yes = &screen->sprites[M_CHOICE_SPRITE];
    ANM_VM *const no = &screen->sprites[M_CHOICE_SPRITE + 1];
    const bool first = screen->cursor == 0;
    yes->color = (yes->color & 0xff000000) | (first ? 0xff6060 : 0x606060);
    no->color = (no->color & 0xff000000) | (first ? 0x606060 : 0xff6060);
}

static void M_ListSlots(M_SCREEN *const screen)
{
    if (screen->frame == 0) {
        for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
            const SAVED_REPLAY *const slot = ResultSaves_GetSlot(i);
            if (slot->valid) {
                screen->slots[i] = *slot;
            }
        }
    }
    if (screen->frame < 20) {
        return;
    }
    M_MoveVertical(&screen->cursor, SAVED_REPLAY_SLOT_COUNT);
    screen->replay_slot = screen->cursor;
    if (M_Pressed(M_CONFIRM)) {
        Sound_Queue(SOUND_OK);
        screen->replay_slot = screen->cursor;
        screen->frame = 0;
        memcpy(
            screen->replay_date, ResultSaves_GetClock()->date,
            sizeof(screen->replay_date));
        screen->replay_score = g_GameManager.score.value;
        const bool used = screen->slots[screen->cursor].valid;
        M_Interrupt(screen, used ? 0xd : 0x11);
        screen->sprites[screen->replay_slot + M_SLOT_SPRITE].pending_interrupt =
            0x10;
        screen->state = used ? M_STATE_REPLAY_OVERWRITE : M_STATE_REPLAY_NAME;
        screen->cursor = 0;
        screen->char_index = 0;
        if (screen->name_entry_at_end) {
            screen->char_index = M_ALPHABET_END;
        }
    }
    if (M_Pressed(M_CANCEL)) {
        Sound_Queue(SOUND_CANCEL);
        screen->state = M_STATE_REPLAY_PROMPT;
        M_Interrupt(screen, 2);
        screen->frame = 0;
    }
}

static void M_ChooseSlot(M_SCREEN *const screen)
{
    Sound_Queue(SOUND_OK);
    screen->state = M_STATE_REPLAY_SLOTS;
    M_Interrupt(screen, 0xc);
    screen->frame = 0;
    M_ListSlots(screen);
}

static void M_SaveReplay(const M_SCREEN *const screen)
{
    size_t length = 0;
    while (length < sizeof(screen->name) && screen->name[length] != 0) {
        length++;
    }
    char *name;
    if (!Result_Log(
            Cp932_Decode((const char *)screen->name, length, &name),
            LOG_LEVEL_WARNING)) {
        name = Str_Dup("");
    }
    const REPLAY_SAVE_INFO info = {
        .name = name,
        .date = ResultSaves_GetClock()->date,
        .slowdown = M_Slowdown(),
    };
    char *const path = ResultScreen_GetSlotPath(screen->replay_slot);
    Replay_Save(path, &info);
    Mem_Free(path);
    Mem_Free(name);
}

static void M_UpdateReplayPrompt(M_SCREEN *const screen)
{
    if (screen->frame == 60) {
        int16_t interrupt = 0xb;
        if (M_RecordsNoScore()) {
            interrupt = 0x13;
        } else if (g_GameManager.stats.continues_used != 0) {
            interrupt = 0xe;
        }
        M_Interrupt(screen, interrupt);
        if (interrupt != 0xb) {
            screen->state = M_STATE_REPLAY_REFUSED;
        }
        screen->cursor = 0;
    }
    M_HighlightChoice(screen);
    if (screen->frame < 80) {
        return;
    }
    M_MoveCursorHorizontal(screen, 2);
    if (M_Pressed(M_CANCEL) || M_Pressed(M_MENU)) {
        M_Leave(screen);
        return;
    }
    if (M_Pressed(M_CONFIRM)) {
        if (screen->cursor == 0) {
            M_ChooseSlot(screen);
        } else {
            M_Leave(screen);
        }
    }
}

static void M_UpdateReplayName(M_SCREEN *const screen)
{
    if (screen->frame < 30) {
        return;
    }
    M_MoveCharIndex(screen);
    if (M_PressedOrRepeated(M_CONFIRM)) {
        if (!M_TypeCharacter(screen, screen->name)) {
            M_SaveReplay(screen);
            screen->frame = 0;
            screen->state = M_STATE_LEAVE;
            M_Interrupt(screen, 2);
            M_CopyName(
                screen->last_name.name, sizeof(screen->last_name.name),
                screen->name, sizeof(screen->name));
        }
        // The original goes on after the save as after any character.
        M_AdvanceNameCursor(screen);
    }
    if (M_PressedOrRepeated(M_CANCEL)) {
        M_EraseNameCharacter(screen, screen->name);
    }
    if (M_Pressed(M_MENU)) {
        M_ChooseSlot(screen);
    }
}

static void M_UpdateReplayOverwrite(M_SCREEN *const screen)
{
    M_HighlightChoice(screen);
    if (screen->frame < 20) {
        return;
    }
    M_MoveCursorHorizontal(screen, 2);
    if (M_Pressed(M_CANCEL) || M_Pressed(M_MENU)) {
        M_ChooseSlot(screen);
        return;
    }
    if (M_Pressed(M_CONFIRM)) {
        screen->frame = 0;
        if (screen->cursor == 0) {
            M_Interrupt(screen, 0x11);
            screen->sprites[screen->replay_slot + M_SLOT_SPRITE]
                .pending_interrupt = 0x10;
            screen->state = M_STATE_REPLAY_NAME;
        } else {
            M_ChooseSlot(screen);
        }
    }
}

static void M_UpdateReplaySaving(M_SCREEN *const screen)
{
    switch (screen->state) {
    case M_STATE_REPLAY_PROMPT:
        M_UpdateReplayPrompt(screen);
        break;
    case M_STATE_REPLAY_REFUSED:
        if (screen->frame >= 20
            && (M_Pressed(M_CONFIRM) || M_Pressed(M_CANCEL))) {
            M_Leave(screen);
        }
        break;
    case M_STATE_REPLAY_SLOTS:
        M_ListSlots(screen);
        break;
    case M_STATE_REPLAY_NAME:
        M_UpdateReplayName(screen);
        break;
    case M_STATE_REPLAY_OVERWRITE:
        M_UpdateReplayOverwrite(screen);
        break;
    default:
        break;
    }
}

static void M_UpdateStatistics(M_SCREEN *const screen)
{
    ANM_VM *const panel = &screen->sprites[M_PANEL_SPRITE];
    if (screen->state == M_STATE_STATISTICS) {
        if (screen->frame <= 30) {
            panel->pending_interrupt = 0x12;
        }
        if (screen->frame >= 90 && M_Pressed(M_CONFIRM)) {
            panel->pending_interrupt = 2;
            screen->frame = 0;
            screen->state = M_STATE_STATISTICS_LEAVE;
        }
    } else if (screen->frame >= 30) {
        screen->frame = 59;
        screen->state = M_STATE_REPLAY_PROMPT;
    }
}

// Highlights the menu item at the cursor, and hides Phantasm and moves the
// items below it up while it is closed. Returns whether it is open.
static bool M_HighlightMenu(M_SCREEN *const screen)
{
    for (int32_t i = 0; i < M_MENU_ITEM_COUNT; i++) {
        ANM_VM *const sprite = &screen->sprites[i];
        if (i == screen->cursor) {
            sprite->color = COLOR_WHITE;
            sprite->pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        } else {
            sprite->color = COLOR_ARGB(0xb0, 0xff, 0xff, 0xff);
            sprite->pos_offset = (VEC3) {};
        }
    }
    if (GameManager_IsPhantasmOpen()) {
        return true;
    }
    screen->sprites[M_PHANTASM_ITEM].visible = false;
    for (int32_t i = M_PHANTASM_ITEM + 1; i < M_MENU_ITEM_COUNT; i++) {
        screen->sprites[i].pos_offset.y += -32.0f;
    }
    return false;
}

static void M_OpenMenu(M_SCREEN *const screen)
{
    if (screen->frame != 0) {
        return;
    }
    M_Interrupt(screen, 1);
    if (M_HighlightMenu(screen)) {
        screen->sprites[M_PHANTASM_ITEM].visible = true;
    }
}

static void M_Quit(M_SCREEN *const screen)
{
    M_Interrupt(screen, 2);
    screen->state = M_STATE_LEAVE;
    Sound_Queue(SOUND_CANCEL);
    screen->frame = 0;
}

static void M_OpenList(
    M_SCREEN *const screen, const int16_t interrupt, const M_STATE state)
{
    M_Interrupt(screen, interrupt);
    screen->difficulty = screen->cursor;
    screen->state = state;
    screen->list_state = state;
    screen->frame = 0;
    screen->shot = -1;
}

static void M_UpdateMenu(M_SCREEN *const screen)
{
    const int32_t step = M_MoveVertical(&screen->cursor, M_MENU_ITEM_COUNT);
    if (screen->cursor == M_PHANTASM_ITEM && !GameManager_IsPhantasmOpen()) {
        screen->cursor += step;
    }
    M_HighlightMenu(screen);
    if (M_Pressed(M_CANCEL)) {
        if (screen->cursor == M_QUIT_ITEM) {
            M_Quit(screen);
            return;
        }
        screen->cursor = M_QUIT_ITEM;
        Sound_Queue(SOUND_CANCEL);
    }
    if (!M_Pressed(M_CONFIRM)) {
        return;
    }
    switch (screen->cursor) {
    case 6:
        M_OpenList(screen, 10, M_STATE_SPELL_CARDS);
        screen->cursor = screen->saved_spell_page;
        screen->spell_page = -1;
        break;
    case 7:
        M_OpenList(screen, 9, M_STATE_PLAY_STATS_OPEN);
        break;
    case M_QUIT_ITEM:
        M_Quit(screen);
        break;
    default:
        M_OpenList(
            screen, (int16_t)(screen->cursor + 3),
            (M_STATE)(screen->cursor + M_STATE_SCORES_EASY));
        screen->cursor = screen->saved_cursor;
        screen->spell_page = -1;
        break;
    }
}

// Goes back to the menu and opens it in the same calc.
static void M_ReturnToMenu(M_SCREEN *const screen, int32_t *const saved)
{
    Sound_Queue(SOUND_CANCEL);
    screen->state = M_STATE_OPEN_MENU;
    screen->frame = 0;
    M_Interrupt(screen, 1);
    *saved = screen->cursor;
    screen->cursor = screen->difficulty;
    M_OpenMenu(screen);
}

// Counts the button sequence that marks every stage cleared: three presses
// of up, two of cheat, two of down and three of quit, with focus or skip
// held. The next calc applies it.
static void M_UpdateUnlock(M_SCREEN *const screen)
{
    if ((g_Buttons.word & 0x104) == 0) {
        screen->unlock_progress = 0;
        return;
    }
    if (screen->unlock_progress >= 10) {
        for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
            for (int32_t k = 0; k < SCORE_DIFFICULTY_COUNT; k++) {
                g_GameManager.records.clears[shot].no_continues[k] =
                    M_CLEAR_STAGE;
                g_GameManager.records.clears[shot].with_continues[k] =
                    M_CLEAR_STAGE;
            }
        }
        screen->unlock_progress = 0;
        Sound_Queue(SOUND_EXTEND);
        return;
    }
    const uint16_t step = screen->unlock_progress < 3 ? M_UP
        : screen->unlock_progress < 5                 ? 0x2000
        : screen->unlock_progress < 7                 ? M_DOWN
                                                      : 0x200;
    if (M_Pressed(step)) {
        screen->unlock_progress++;
    } else if (M_Pressed(M_UNLOCK_RESET)) {
        screen->unlock_progress = 0;
    }
}

static void M_UpdateScores(M_SCREEN *const screen)
{
    if (screen->shot != screen->cursor && screen->frame == 20) {
        screen->shot = screen->cursor;
        M_RenderShotName(screen);
    }
    if (screen->frame < 30) {
        return;
    }
    if (M_MoveCursorHorizontal(screen, SCORE_SHOT_COUNT)) {
        screen->frame = 0;
        M_Interrupt(screen, (int16_t)(screen->difficulty + 3));
    }
    if (M_Pressed(M_CANCEL)) {
        M_ReturnToMenu(screen, &screen->saved_cursor);
    }
}

static void M_RenderSpellCards(M_SCREEN *const screen)
{
    const int32_t first = screen->spell_page * M_SPELL_PAGE_SIZE;
    for (int32_t i = first; i < first + M_SPELL_PAGE_SIZE; i++) {
        if (i >= SPELL_CARD_COUNT) {
            break;
        }
        const SPELL_CARD_RECORD *const card =
            &g_GameManager.records.spell_cards[i];
        ANM_VM *const vm = &screen->text_sprites[i % M_SPELL_PAGE_SIZE];
        if (card->attempts[SCORE_ALL_SHOTS] == 0) {
            Result_Log(
                Anm_RenderText(vm, 0xffffff, 0, "？？？？？"),
                LOG_LEVEL_WARNING);
        } else {
            size_t length = 0;
            while (length < sizeof(card->name) && card->name[length] != 0) {
                length++;
            }
            char *name;
            if (Result_Log(
                    Cp932_Decode((const char *)card->name, length, &name),
                    LOG_LEVEL_WARNING)) {
                Result_Log(
                    Anm_RenderText(vm, 0xffffff, 0, "%s", name),
                    LOG_LEVEL_WARNING);
                Mem_Free(name);
            }
        }
        M_SetAlpha(vm, 0xff);
    }
    ANM_VM *const title = &screen->text_sprites[M_SPELL_PAGE_SIZE];
    Result_Log(
        Anm_RenderText(
            title, 0xffffff, 0, "%s %3d枚中%3d枚取得（キャラ切り替え↓↑）",
            m_ShotNames[screen->shown_spell_shot], SPELL_CARD_COUNT,
            screen->captured_counts[screen->spell_shot]),
        LOG_LEVEL_WARNING);
    M_SetAlpha(title, 0xff);
}

static void M_UpdateSpellCards(M_SCREEN *const screen)
{
    if ((screen->spell_page != screen->cursor
         || screen->shown_spell_shot != screen->spell_shot)
        && screen->frame == 20) {
        screen->spell_page = screen->cursor;
        screen->shown_spell_shot = screen->spell_shot;
        M_RenderSpellCards(screen);
    }
    if (screen->frame < 30) {
        return;
    }
    if (M_MoveCursorHorizontal(screen, M_SPELL_PAGE_COUNT)) {
        screen->frame = 0;
        M_Interrupt(screen, 10);
    } else if (M_MoveVertical(&screen->spell_shot, M_SPELL_SHOT_COUNT) != 0) {
        screen->frame = 0;
        screen->spell_rows_sliding = true;
    }
    if (M_Pressed(M_CANCEL)) {
        M_ReturnToMenu(screen, &screen->saved_spell_page);
    }
}

// Renders label and the counts by difficulty, then the totals. Leaves out
// Phantasm while it is closed.
static void M_RenderCounts(
    ANM_VM *const vm, const char *const label,
    const int32_t counts[PLAY_STATS_DIFFICULTY_COUNT])
{
    char text[160];
    int32_t length = snprintf(text, sizeof(text), "%s", label);
    const bool phantasm = GameManager_IsPhantasmOpen();
    for (int32_t d = 0; d < PLAY_STATS_DIFFICULTY_COUNT; d++) {
        if (d != 5 || phantasm) {
            length += snprintf(
                text + length, sizeof(text) - (size_t)length, " %6d",
                counts[d]);
        }
    }
    Result_Log(Anm_RenderText(vm, 0xffffff, 0, "%s", text), LOG_LEVEL_WARNING);
}

static void M_RenderRunTime(M_SCREEN *const screen)
{
    const PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    Result_Log(
        Anm_RenderText(
            &screen->text_sprites[0], 0xffffff, 0,
            "総起動時間   %.2d:%.2d:%.2d", stats->run_hours, stats->run_minutes,
            stats->run_seconds),
        LOG_LEVEL_WARNING);
}

static void M_RenderPlayStats(M_SCREEN *const screen)
{
    PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    const PLAY_COUNTS *const counts = stats->counts;
    VEC3 pos = { 56.0f, 128.0f, 0.0f };
    ANM_VM *vm = &screen->text_sprites[0];
    vm->pos = pos;
    PlayTime_AddRun();
    M_RenderRunTime(screen);
    PlayTime_AddRun();
    screen->shown_run_seconds = (uint8_t)stats->run_seconds;
    vm++;
    pos.y += 17.0f;
    vm->pos = pos;
    Result_Log(
        Anm_RenderText(
            vm, 0xffffff, 0, "総プレイ時間 %.2d:%.2d:%.2d", stats->play_hours,
            stats->play_minutes, stats->play_seconds),
        LOG_LEVEL_WARNING);
    vm++;
    pos.y += 17.0f;
    vm->pos = pos;
    Result_Log(
        Anm_RenderText(
            vm, 0xffffff, 0, "%s",
            GameManager_IsPhantasmOpen()
                ? "プレイ回数　　　 　Easy 　Norm 　Hard 　Luna  Extra "
                  "Phants  Total"
                : "プレイ回数　　　 　Easy 　Norm 　Hard 　Luna  Extra  "
                  "Total"),
        LOG_LEVEL_WARNING);
    for (int32_t i = 0; i <= SCORE_SHOT_COUNT; i++) {
        vm++;
        pos.y += 17.0f;
        vm->pos = pos;
        // The rows count the plays of each shot, then those of every shot.
        const int32_t column = i < SCORE_SHOT_COUNT ? i + 1 : 0;
        int32_t plays[PLAY_STATS_DIFFICULTY_COUNT];
        for (int32_t d = 0; d < PLAY_STATS_DIFFICULTY_COUNT; d++) {
            plays[d] = counts[d].plays[column];
        }
        M_RenderCounts(vm, m_ShotNames[i], plays);
    }
    stats->counts[6].clears = counts[0].clears + counts[1].clears
        + counts[2].clears + counts[3].clears + counts[4].clears
        + counts[5].clears;
    static const char *const labels[4] = {
        "クリア回数  　　",
        "コンティニュー  ",
        "プラクティス　  ",
        "リトライ回数  　",
    };
    int32_t rows[4][PLAY_STATS_DIFFICULTY_COUNT];
    for (int32_t d = 0; d < PLAY_STATS_DIFFICULTY_COUNT; d++) {
        rows[0][d] = counts[d].clears;
        rows[1][d] = counts[d].continues;
        rows[2][d] = counts[d].practices;
        rows[3][d] = counts[d].retries;
    }
    pos.y += 17.0f;
    for (int32_t i = 0; i < 4; i++) {
        vm++;
        pos.y += 17.0f;
        vm->pos = pos;
        M_RenderCounts(vm, labels[i], rows[i]);
    }
}

// Returns true once the play statistics have closed.
static bool M_UpdatePlayStats(M_SCREEN *const screen)
{
    switch (screen->state) {
    case M_STATE_PLAY_STATS_OPEN:
        if (screen->frame == 1) {
            M_RenderPlayStats(screen);
        }
        if (screen->frame < 40) {
            for (int32_t i = 0; i < M_STATS_TEXT_COUNT; i++) {
                M_SetAlpha(&screen->text_sprites[i], screen->frame * 0xff / 40);
            }
        } else {
            screen->state = M_STATE_PLAY_STATS;
        }
        break;
    case M_STATE_PLAY_STATS:
        if (screen->frame % 60 == 0) {
            PlayTime_AddRun();
            const uint32_t seconds =
                g_GameManager.records.play_stats.run_seconds;
            if (seconds != screen->shown_run_seconds) {
                M_RenderRunTime(screen);
                screen->shown_run_seconds = (uint8_t)seconds;
            }
        }
        if (M_Pressed(M_CLOSE_STATS)) {
            screen->state = M_STATE_PLAY_STATS_CLOSE;
            screen->frame = 0;
        }
        break;
    default:
        if (screen->frame < 20) {
            for (int32_t i = 0; i < M_STATS_TEXT_COUNT; i++) {
                M_SetAlpha(
                    &screen->text_sprites[i], 0xff - screen->frame * 0xff / 20);
            }
        } else {
            screen->state = M_STATE_OPEN_MENU;
            screen->frame = 0;
            return true;
        }
        break;
    }
    return false;
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_SCREEN *const screen = arg;
    switch (screen->state) {
    case M_STATE_OPEN_MENU:
        M_OpenMenu(screen);
        if (screen->frame < 20) {
            break;
        }
        screen->state = M_STATE_MENU;
        screen->frame = 0;
        [[fallthrough]];
    case M_STATE_MENU:
        M_UpdateMenu(screen);
        break;
    case M_STATE_LEAVE:
        if (screen->frame < 60) {
            break;
        }
        Scene_Request(SCENE_TITLE);
        return CHAIN_REMOVE;
    case M_STATE_SCORES_HARD:
        M_UpdateUnlock(screen);
        [[fallthrough]];
    case M_STATE_SCORES_EASY:
    case M_STATE_SCORES_NORMAL:
    case M_STATE_SCORES_LUNATIC:
    case M_STATE_SCORES_EXTRA:
    case M_STATE_SCORES_PHANTASM:
        M_UpdateScores(screen);
        break;
    case M_STATE_SPELL_CARDS:
        M_UpdateSpellCards(screen);
        break;
    case M_STATE_NAME_ENTRY:
        M_UpdateNameEntry(screen);
        break;
    case M_STATE_REPLAY_PROMPT:
    case M_STATE_REPLAY_REFUSED:
    case M_STATE_REPLAY_SLOTS:
    case M_STATE_REPLAY_NAME:
    case M_STATE_REPLAY_OVERWRITE:
        M_UpdateReplaySaving(screen);
        break;
    case M_STATE_STATISTICS:
    case M_STATE_STATISTICS_LEAVE:
        M_UpdateStatistics(screen);
        break;
    case M_STATE_PLAY_STATS_OPEN:
    case M_STATE_PLAY_STATS:
    case M_STATE_PLAY_STATS_CLOSE:
        if (M_UpdatePlayStats(screen)) {
            M_OpenMenu(screen);
        }
        break;
    default:
        break;
    }
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        Anm_Step(&screen->sprites[i]);
    }
    screen->frame++;
    return CHAIN_CONTINUE;
}

static ANM_DRAW M_Draw(const uint32_t part)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_SCREEN);
    draw.interpolate = true;
    draw.part = part;
    return draw;
}

static void M_DrawSprite(const ANM_VM *const vm, const uint32_t part)
{
    const ANM_DRAW draw = M_Draw(part);
    Anm_DrawUnrotated(vm, &draw);
}

// Returns the list of difficulty and shot. The original indexes its lists
// as one array, so a shot past the end of a row reaches into the next
// difficulty. Returns nullptr outside the array.
static const SCORE_LIST *M_List(
    const M_SCREEN *const screen, const int32_t difficulty, const int32_t shot)
{
    const int32_t index = difficulty * SCORE_SHOT_COUNT + shot;
    if (index < 0 || index >= SCORE_DIFFICULTY_COUNT * SCORE_SHOT_COUNT) {
        return nullptr;
    }
    return &screen->scores[index / SCORE_SHOT_COUNT][index % SCORE_SHOT_COUNT];
}

static void M_AddNameCursor(
    const M_SCREEN *const screen, const VEC3 pos, const char *const format)
{
    char name[9] = "        ";
    name[M_NamePosition(screen)] = '_';
    AsciiText_AddFormat(pos, format, name);
}

static void M_AddEntry(VEC3 pos, const HIGH_SCORE *const entry)
{
    char name[sizeof(entry->name) + 1] = {};
    memcpy(name, entry->name, sizeof(entry->name));
    const int32_t continues = (int8_t)entry->continues_used;
    if (entry->stage <= 6) {
        AsciiText_AddFormat(
            pos, "%8s %9d%1d(%d)", name, entry->score, continues, entry->stage);
    } else if (entry->stage == 7 || entry->stage == 8) {
        AsciiText_AddFormat(
            pos, "%8s %9d%1d(1)", name, entry->score, continues);
    } else {
        AsciiText_AddFormat(
            pos, "%8s %9d%1d(C)", name, entry->score, continues);
    }
    char date[sizeof(entry->date) + 1] = {};
    memcpy(date, entry->date, sizeof(entry->date));
    pos.x += 320.0f;
    AsciiText_AddFormat(pos, " %5s   %3.2f", date, (double)entry->slowdown);
}

static void M_DrawScores(M_SCREEN *const screen, VEC3 pos)
{
    ANM_VM *const title = &screen->text_sprites[0];
    title->pos = pos;
    title->pos.x += 64.0f;
    M_DrawSprite(title, 0x100);
    pos.y += 18.0f;
    pos.x += 24.0f;
    AsciiText_SetColor(COLOR_ARGB(0xff, 0xe0, 0xe0, 0xef));
    AsciiText_AddString(pos, "No  Name      Score(Stage)  Date   Slow");
    pos.y += 18.0f;
    const SCORE_LIST *const list =
        M_List(screen, screen->difficulty, screen->shot);
    if (list == nullptr) {
        return;
    }
    const bool naming = screen->state == M_STATE_NAME_ENTRY;
    for (int32_t i = 0; i < SCORE_LIST_SIZE; i++) {
        const HIGH_SCORE *const entry = &list->entries[i];
        if (!naming) {
            AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0xc0, 0xc0));
        } else if (entry->is_new) {
            AsciiText_SetColor(COLOR_ARGB(0xff, 0xf0, 0xf0, 0xff));
        } else {
            AsciiText_SetColor(COLOR_ARGB(0xc0, 0xff, 0xc0, 0xc0));
        }
        AsciiText_AddFormat(pos, "%2d", i + 1);
        pos.x += 48.0f;
        if (naming && entry->is_new) {
            M_AddNameCursor(screen, pos, "%8s");
        }
        M_AddEntry(pos, entry);
        pos.y += 18.0f;
        pos.x -= 48.0f;
    }
}

static void M_DrawSpellCards(M_SCREEN *const screen, VEC3 pos)
{
    ANM_VM *const title = &screen->text_sprites[M_SPELL_PAGE_SIZE];
    title->pos = pos;
    M_DrawSprite(title, 0x100 + M_SPELL_PAGE_SIZE);
    pos.y += 16.0f;
    const int32_t shot = screen->shown_spell_shot;
    for (int32_t i = 0; i < M_SPELL_PAGE_SIZE; i++) {
        const int32_t index = screen->spell_page * M_SPELL_PAGE_SIZE + i;
        if (index >= SPELL_CARD_COUNT) {
            break;
        }
        if (index >= 0) {
            const SPELL_CARD_RECORD *const card =
                &g_GameManager.records.spell_cards[index];
            ANM_VM *const divider = &screen->divider;
            divider->pos = (VEC3) { pos.x + 320.0f, pos.y + 16.0f, pos.z };
            divider->scale_x = 2.375f;
            M_DrawSprite(divider, 0x200 + (uint32_t)i);
            ANM_VM *const text = &screen->text_sprites[i];
            text->pos = pos;
            if (card->attempts[shot] == 0) {
                AsciiText_SetColor(COLOR_ARGB(0xc0, 0xc0, 0xc0, 0xff));
            } else if (card->captures[shot] == 0) {
                AsciiText_SetColor(COLOR_ARGB(0xff, 0xc0, 0xa0, 0xa0));
            } else {
                AsciiText_SetColor(
                    COLOR_ARGB(0xff, 0xf0, 0xf0, 0xff) - (uint32_t)i * 0x80800);
            }
            AsciiText_AddFormat(pos, "No.%.2d", index + 1);
            text->pos.x += 96.0f;
            M_DrawSprite(text, 0x100 + (uint32_t)i);
            const VEC3 rate = { pos.x + 496.0f, pos.y, pos.z };
            if (card->attempts[shot] == 0) {
                AsciiText_AddString(rate, "---/---");
            } else {
                AsciiText_AddFormat(
                    rate, "%3d/%3d", card->captures[shot],
                    card->attempts[shot]);
            }
            AsciiText_SetColor(COLOR_ARGB(0xff, 0xa0, 0x80, 0x90));
            g_AsciiText.scale_x = 0.8f;
            g_AsciiText.scale_y = 0.8f;
            if (card->attempts[shot] != 0) {
                AsciiText_AddFormat(
                    (VEC3) { pos.x + 424.0f, pos.y - 13.0f, pos.z },
                    "MaxBonus %8d", card->max_bonus[shot]);
            }
            g_AsciiText.scale_x = 1.0f;
            g_AsciiText.scale_y = 1.0f;
        }
        if (!screen->spell_rows_sliding) {
            pos.y += 33.0f;
        } else if (screen->frame < 20) {
            pos.y += (float)((20 - screen->frame) * 33 / 20);
        } else {
            pos.y += (float)((screen->frame - 20) * 33 / 20);
        }
    }
    if (screen->frame >= 40) {
        screen->spell_rows_sliding = false;
    }
}

static void M_DrawAlphabet(const M_SCREEN *const screen)
{
    VEC3 pos = { 160.0f, 356.0f, 0.0f };
    for (int32_t row = 0; row < 6; row++) {
        int32_t column;
        for (column = 0; column < 16; column++) {
            const int32_t index = row * 16 + column;
            float shift = 0.0f;
            if (screen->char_index == index) {
                AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0xff, 0xc0));
                float scale;
                if (screen->frame % 64 < 32) {
                    scale = 0.8f * (float)(screen->frame % 32) / 32.0f + 1.2f;
                } else {
                    scale = 2.0f - 0.8f * (float)(screen->frame % 32) / 32.0f;
                }
                g_AsciiText.scale_x = scale;
                g_AsciiText.scale_y = scale;
                shift = -(scale - 1.0f) * 8.0f;
            } else {
                AsciiText_SetColor(COLOR_ARGB(0xc0, 0xc0, 0xc0, 0xc0));
                g_AsciiText.scale_x = 1.0f;
                g_AsciiText.scale_y = 1.0f;
            }
            char glyph[2] = { m_NameAlphabet[index], 0 };
            if (row == 5 && column == 14) {
                glyph[0] = (char)0x80;
            } else if (row == 5 && column == 15) {
                glyph[0] = (char)0x81;
            }
            AsciiText_AddString(
                (VEC3) { pos.x + shift, pos.y + shift, pos.z }, glyph);
            pos.x += 20.0f;
        }
        pos.x -= (float)(column * 20);
        pos.y += 18.0f;
    }
}

static void M_DrawSlots(M_SCREEN *const screen)
{
    for (int32_t i = 0; i < 6; i++) {
        M_DrawSprite(
            &screen->sprites[M_SLOT_FRAME_SPRITE + i], 0x300 + (uint32_t)i);
    }
    AsciiText_AddString(
        screen->sprites[M_SLOT_HEADER_SPRITE].pos,
        "No.   Name     Date   Player Score");
    const int32_t shot = g_GameManager.run.shot_index;
    char name[sizeof(screen->name) + 1] = {};
    memcpy(name, screen->name, sizeof(screen->name));
    char date[sizeof(screen->replay_date) + 1] = {};
    memcpy(date, screen->replay_date, sizeof(screen->replay_date));
    for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
        const VEC3 pos = screen->sprites[M_SLOT_SPRITE + i].pos;
        AsciiText_SetColor(
            i == screen->replay_slot ? COLOR_ARGB(0xff, 0xff, 0x80, 0x80)
                                     : COLOR_ARGB(0xff, 0x80, 0x80, 0x80));
        const SAVED_REPLAY *const slot = &screen->slots[i];
        if (screen->state == M_STATE_REPLAY_NAME) {
            AsciiText_AddFormat(
                pos, "No.%.2d %8s %5s  %7s %9d0", i + 1, name, date,
                m_ShotTypeNames[shot], screen->replay_score);
            AsciiText_SetColor(COLOR_ARGB(0xff, 0xf0, 0xf0, 0xff));
            M_AddNameCursor(screen, pos, "      %8s");
        } else if (!slot->valid) {
            AsciiText_AddFormat(
                pos, "No.%.2d -------- --/--  -------          0", i + 1);
        } else {
            AsciiText_AddFormat(
                pos, "No.%.2d %8s %5s  %7s %9d0", i + 1, slot->summary.name,
                slot->summary.date, m_ShotTypeNames[slot->summary.shot],
                slot->summary.score);
        }
    }
}

static void M_DrawStatistics(const M_SCREEN *const screen)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    const ANM_VM *const panel = &screen->sprites[M_PANEL_SPRITE];
    AsciiText_SetColor(panel->color);
    const float frames = (float)gm->run.progress_frames;
    float progress = gm->run.difficulty < 4 ? frames / 180621.0f
        : gm->run.difficulty == 4           ? frames / 80000.0f
                                            : frames / 85000.0f;
    VEC3 pos = panel->pos;
    pos.x += 210.0f;
    pos.y += 32.0f;
    AsciiText_AddFormat(pos, "%9d", gm->score.shown);
    pos.x += 126.0f;
    AsciiText_AddFormat(pos, "%1d", gm->stats.continues_used);
    pos.x -= 126.0f;
    pos.y += 22.0f;
    AsciiText_AddString(pos, m_DifficultyNames[gm->run.difficulty]);
    pos.x += 14.0f;
    pos.y += 22.0f;
    if (!gm->mode.game_cleared) {
        if (progress >= 1.0f) {
            progress = 0.99f;
        }
        AsciiText_AddFormat(pos, "    %3.2f%%", (double)(progress * 100.0f));
    } else {
        AsciiText_AddString(pos, "      100%");
    }
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", gm->stats.continues_used);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", (int32_t)gm->stats.deaths);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", (int32_t)gm->stats.bombs_used);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", gm->stats.spell_cards_captured);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "    %3.2f%%", (double)M_Slowdown());
    AsciiText_SetColor(COLOR_WHITE);
}

static CHAIN_STATUS M_DrawScreen(void *const arg)
{
    M_SCREEN *const screen = arg;
    VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    viewport->x = 0.0f;
    viewport->y = 0.0f;
    viewport->width = 640.0f;
    viewport->height = 480.0f;
    Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        ANM_VM sprite = screen->sprites[i];
        sprite.pos = Vec3_Add(sprite.pos, sprite.pos_offset);
        M_DrawSprite(&sprite, (uint32_t)i);
    }
    const ANM_VM *const board = &screen->sprites[M_BOARD_SPRITE];
    if (board->pos.x < 640.0f) {
        if (screen->list_state != M_STATE_SPELL_CARDS) {
            M_DrawScores(screen, board->pos);
        } else {
            M_DrawSpellCards(screen, board->pos);
        }
    }
    if (screen->state == M_STATE_NAME_ENTRY
        || screen->state == M_STATE_REPLAY_NAME) {
        M_DrawAlphabet(screen);
    }
    g_AsciiText.scale_x = 1.0f;
    g_AsciiText.scale_y = 1.0f;
    if (screen->state >= M_STATE_REPLAY_PROMPT
        && screen->state <= M_STATE_REPLAY_OVERWRITE) {
        M_DrawSlots(screen);
    }
    AsciiText_SetColor(COLOR_WHITE);
    if (screen->state == M_STATE_STATISTICS
        || screen->state == M_STATE_STATISTICS_LEAVE) {
        M_DrawStatistics(screen);
    }
    if (screen->state >= M_STATE_PLAY_STATS_OPEN) {
        for (int32_t i = 0; i < M_STATS_TEXT_COUNT; i++) {
            M_DrawSprite(&screen->text_sprites[i], 0x100 + (uint32_t)i);
        }
    }
    return CHAIN_CONTINUE;
}

static RESULT M_LoadGraphics(M_SCREEN *const screen)
{
    MUST(Anm_LoadSurface(0, "data/result/result.jpg"));
    MUST(Anm_Load(M_ANM_SLOT, "data/result00.anm", M_FIRST_SCRIPT));
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        ANM_VM *const sprite = &screen->sprites[i];
        Anm_CreateVm(sprite);
        Anm_StartScript(sprite, M_FIRST_SCRIPT + i);
    }
    Anm_CreateVm(&screen->divider);
    Anm_InitVm(&screen->divider);
    Anm_SetSprite(&screen->divider, M_DIVIDER_SPRITE);
    for (int32_t i = 0; i < M_TEXT_SPRITE_COUNT; i++) {
        ANM_VM *const sprite = &screen->text_sprites[i];
        Anm_CreateVm(sprite);
        Anm_InitVm(sprite);
        Anm_SetSprite(sprite, M_FIRST_TEXT_SPRITE + i);
        sprite->anchor = 3;
        sprite->font_width = 15;
        sprite->font_height = 15;
    }
    return OK;
}

// Counts the spell cards captured by each shot and by every shot.
static void M_CountCaptures(M_SCREEN *const screen)
{
    for (int32_t i = 0; i < M_SPELL_SHOT_COUNT; i++) {
        screen->captured_counts[i] = 0;
        for (int32_t j = 0; j < SPELL_CARD_COUNT; j++) {
            const SPELL_CARD_RECORD *const card =
                &g_GameManager.records.spell_cards[j];
            if (card->header.magic == SPELL_CARD_MAGIC
                && card->header.version == 1 && card->captures[i] != 0) {
                screen->captured_counts[i]++;
            }
        }
    }
}

static RESULT M_Added(void *const arg)
{
    M_SCREEN *const screen = arg;
    GAME_MANAGER *const gm = &g_GameManager;
    GameManager_IsPhantasmOpen();
    MUST(M_LoadGraphics(screen));
    screen->saved_cursor = 0;
    screen->score_loaded = true;
    ResultRecords_LoadLists(screen->scores);
    screen->name_entry_at_end = ResultRecords_LoadLastName(&screen->last_name);
    if (screen->state != M_STATE_NAME_ENTRY
        && screen->state != M_STATE_PRACTICE) {
        const SCORE_FILE *const file = ResultSaves_GetScoreFile();
        ScoreFile_LoadSpellCards(file, gm->records.spell_cards);
        ScoreFile_LoadClears(file, gm->records.clears);
        GameManager_IsPhantasmOpen();
        ScoreFile_LoadPractice(file, gm->records.practice);
    }
    if (screen->state == M_STATE_PRACTICE) {
        const int32_t stage = gm->run.stage - 1;
        FAIL_IF(
            stage < 0 || stage >= SCORE_STAGE_COUNT
                || gm->run.difficulty >= SCORE_PRACTICE_DIFFICULTY_COUNT,
            "no practice score for stage %d, difficulty %d", gm->run.stage,
            gm->run.difficulty);
        PRACTICE_RECORD *const record =
            &gm->records
                 .practice[gm->run.shot_index][stage][gm->run.difficulty];
        if (record->score < gm->score.value) {
            record->score = gm->score.value;
        }
        screen->state = M_STATE_REPLAY_PROMPT;
        M_CopyName(
            screen->name, sizeof(screen->name), screen->last_name.name,
            sizeof(screen->last_name.name));
    }
    M_CountCaptures(screen);
    screen->spell_shot = SCORE_ALL_SHOTS;
    screen->shown_spell_shot = SCORE_ALL_SHOTS;
    screen->spell_rows_sliding = false;
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_SCREEN *const screen = arg;
    if (screen->score_loaded) {
        ResultRecords_WriteScore(screen->scores, &screen->last_name);
    }
    screen->score_loaded = false;
    for (int32_t slot = M_ANM_SLOT; slot < M_ANM_SLOT + 4; slot++) {
        Anm_Release(slot);
    }
    Anm_ReleaseSurface(0);
    Chain_Remove(&screen->draw);
}

RESULT ResultScreen_Register(const int32_t mode)
{
    M_SCREEN *const screen = &m_Screen;
    memset(screen, 0, sizeof(*screen));
    screen->cursor = 1;
    Chain_InitElem(&screen->calc, M_Update, screen);
    screen->calc.added = M_Added;
    screen->calc.deleted = M_Deleted;
    if (mode != 0) {
        screen->state = g_GameManager.mode.in_practice ? M_STATE_PRACTICE
                                                       : M_STATE_NAME_ENTRY;
    }
    MUST(Chain_AddCalc(&g_Chain, &screen->calc, M_CALC_PRIORITY));
    Chain_InitElem(&screen->draw, M_DrawScreen, screen);
    return Chain_AddDraw(&g_Chain, &screen->draw, M_DRAW_PRIORITY);
}
