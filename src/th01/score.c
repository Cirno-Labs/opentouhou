#include "th01/score.h"

#include "th01/scale2x.h"

#include <string.h>

SCORE_SAVES g_ScoreSaves;

static const char *const m_Ranks[] = {
    "　イージー　",
    "　ノーマル　",
    "　ハード　　",
    "ルナティック",
};
static const char *const m_Places[] = {
    "　靈　神　", "太元帥明王", "　天　仙　", "　神　仙　", "　地　仙　",
    "　人　仙　", "　仙　女　", "陰　陽　師", "靈　媒　師", "修　業　者",
};
static const char *const m_Symbols[] = {
    "！", "？", "＃", "＆", "＊", "§", "☆",  "◆",  "∞",
    "∴",  "♂",  "♀",  "…",  "“",  "”", "／", "．", "・",
};
static const char *const m_Headers[] = {
    "　霊　位　",
    "　　名　　前　　",
    "　　得　　点　　",
    "ステージ・ルート",
};
static const char m_Title[] = "東方靈異伝　強者の記録　";
static const char m_Digits[][4] = {
    "０", "１", "２", "３", "４", "５", "６", "７", "８", "９",
};

static void M_Character(const uint32_t codepoint, char out[9])
{
    out[0] = (char)(0xe0 | (codepoint >> 12));
    out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[2] = (char)(0x80 | (codepoint & 0x3f));
    out[3] = '\0';
}

static void M_Cell(const int32_t row, const int32_t column, char out[9])
{
    if (row < 4) {
        M_Character(
            (row < 2 ? 0xff41u : 0xff21u) + (uint32_t)((row % 2) * 18 + column),
            out);
    } else if (row == 4) {
        strcpy(out, m_Symbols[column]);
    } else if (column < 10) {
        strcpy(out, m_Digits[column]);
    } else {
        strcpy(
            out,
            column == 10       ? "SP"
                : column == 15 ? "←"
                : column == 16 ? "→"
                               : "終");
    }
}

static void M_Draw(
    SCORE_SCENE *const s, const char *const text, const int32_t left,
    const int32_t top, const uint8_t color, const GAME_TEXT_WEIGHT weight,
    const bool reverse, const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .reverse = reverse,
                                    .clear_background = clear };
    GameText_Draw(&s->assets->font, text, left, top, color, &style);
}

static void M_Number(
    SCORE_SCENE *const s, const int32_t value, const int32_t digits,
    const int32_t left, const int32_t top, const uint8_t color,
    const GAME_TEXT_WEIGHT weight)
{
    char full[96] = "";
    uint32_t divisor = 1;
    for (int32_t i = 1; i < digits; i++) {
        divisor *= 10;
    }
    bool put = false;
    for (int32_t i = 0; i < digits; i++) {
        const uint32_t digit = ((uint32_t)value / divisor) % 10;
        put |= digit != 0;
        strcat(full, put ? m_Digits[digit] : "　");
        divisor /= 10;
    }
    M_Draw(s, full, left, top, color, weight, false, false);
}

static void M_Alphabet(
    SCORE_SCENE *const s, const int32_t row, const int32_t column,
    const bool selected)
{
    const int32_t left = 32 + column * 32, top = 240 + row * 24;
    Video_Restore16((GFX_RECT) { left, top, 32, 16 });
    char text[9];
    M_Cell(row, column, text);
    M_Draw(
        s, text, left, top, selected ? 3 : 7, GAME_TEXT_BOLD, selected, false);
}

static void M_Name(SCORE_SCENE *const s)
{
    const int32_t top = 64 + s->place * 16;
    Video_Restore16((GFX_RECT) { 144, top, 128, 16 });
    for (int32_t i = 0; i < 8; i++) {
        M_Draw(
            s, s->slots[i], 144 + i * 16, top, 3, GAME_TEXT_BOLD, false, false);
    }
    M_Draw(
        s, "＿", 144 + s->cursor * 16, top, 3, GAME_TEXT_NORMAL, false, false);
}

static void M_Initial(SCORE_SCENE *const s)
{
    const bool gameover = s->context == SCORE_GAMEOVER;
    Video_SelectDrawingPage(1);
    M_Draw(s, m_Title, 0, gameover ? 384 : 0, 7, GAME_TEXT_BOLD, false, true);
    M_Draw(
        s, m_Ranks[s->rank], 192, gameover ? 384 : 0, 7, GAME_TEXT_BOLD, false,
        true);
    Video_SelectDrawingPage(0);
    Scale2x_Copy(48, 0, 0, gameover ? 384 : 0, 288, 16);
    if (gameover) {
        Video_Copy(0, (GFX_RECT) { 0, 384, 288, 16 }, 1, 0, 384);
    }
    static const int32_t columns[] = { 32, 144, 352, 512 };
    for (int32_t i = 0; i < 4; i++) {
        M_Draw(
            s, m_Headers[i], columns[i], 48, 3, GAME_TEXT_BLACK, false, false);
    }
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        const SCORE_ENTRY *const e = &g_ScoreSaves.tables[s->rank].entries[i];
        const uint8_t color = i == s->place ? 3 : 7;
        const int32_t top = 64 + i * 16;
        M_Draw(s, m_Places[i], 32, top, color, GAME_TEXT_BOLD, false, false);
        M_Draw(
            s, i == s->place ? "＿＿＿＿＿＿＿＿" : e->name, 144, top, color,
            GAME_TEXT_BOLD, false, false);
        M_Number(s, e->score, 7, 352, top, color, GAME_TEXT_BLACK);
        if (e->stage < 40) {
            M_Number(s, e->stage, 2, 528, top, color, GAME_TEXT_BOLD);
        } else if (e->stage == 50 || e->stage == 60) {
            M_Draw(
                s, e->stage == 50 ? "魔界" : "地獄", 528, top, color,
                GAME_TEXT_BOLD, false, false);
        }
        M_Draw(s, "－", 560, top, color, GAME_TEXT_BOLD, false, false);
        M_Draw(s, e->route, 576, top, color, GAME_TEXT_BOLD, false, false);
    }
    GameInput_ResetGame();
    if (s->place == SCORE_PLACES) {
        g_GameInput.ok = true;
        g_GameInput.shot = true;
        s->phase = SCORE_RELEASE;
        return;
    }
    for (int32_t row = 0; row < 6; row++) {
        for (int32_t col = 0; col < 18; col++) {
            if (((row == 1 || row == 3) && col >= 8)
                || (row == 5 && col > 10 && col < 15)) {
                continue;
            }
            M_Alphabet(s, row, col, row == 0 && col == 0);
        }
    }
    s->phase = SCORE_ENTRY_INPUT;
}

static void M_Complete(SCORE_SCENE *const s)
{
    SCORE_ENTRY *const entry = &g_ScoreSaves.tables[s->rank].entries[s->place];
    entry->name[0] = '\0';
    for (int32_t i = 0; i < 8; i++) {
        strcat(entry->name, s->slots[i]);
    }
    g_GameInput.ok = false;
    ScoreData_Queue(&g_ScoreSaves, s->rank);
    s->phase = SCORE_DONE;
}

static bool M_Shot(
    SCORE_SCENE *const s, const int32_t row, const int32_t column)
{
    const bool was_last = s->cursor == 7;
    if (row == 5 && column >= 15) {
        if (column == 17) {
            return true;
        }
        if (column == 15 && s->cursor > 0) {
            s->cursor--;
        } else if (column == 16 && s->cursor < 7) {
            s->cursor++;
        }
    } else {
        if (row == 5 && column == 10) {
            strcpy(s->slots[s->cursor], "　");
        } else {
            M_Cell(row, column, s->slots[s->cursor]);
        }
        if (s->cursor < 7) {
            s->cursor++;
        }
    }
    const bool jump =
        was_last && s->context == SCORE_GAMEOVER && !(row == 5 && column >= 15);
    M_Name(s);
    if (jump) {
        M_Alphabet(s, s->row, s->column, false);
        s->row = 5;
        s->column = 17;
        M_Alphabet(s, s->row, s->column, true);
    }
    return false;
}

bool Score_OnInput(SCORE_SCENE *const s)
{
    const int32_t old_row = s->row, old_column = s->column;
    if (g_GameInput.up) {
        s->row = s->row == 0 ? 5 : s->row - 1;
        if ((s->row == 1 || s->row == 3) && s->column >= 8) {
            s->row--;
        }
        if (s->row == 5 && s->column > 10 && s->column < 15) {
            s->row = 4;
        }
        g_GameInput.up = false;
    } else if (g_GameInput.down) {
        s->row = s->row == 5 ? 0 : s->row + 1;
        if ((s->row == 1 || s->row == 3) && s->column >= 8) {
            s->row++;
        }
        if (s->row == 5 && s->column > 10 && s->column < 15) {
            s->row = 0;
        }
        g_GameInput.down = false;
    } else if (g_GameInput.horizontal == GAME_INPUT_LEFT) {
        s->column = s->column == 0 ? 17 : s->column - 1;
        if ((s->row == 1 || s->row == 3) && s->column >= 8) {
            s->column = 7;
        }
        if (s->row == 5 && s->column > 10 && s->column < 15) {
            s->column = 10;
        }
        g_GameInput.horizontal = 0;
    } else if (g_GameInput.horizontal == GAME_INPUT_RIGHT) {
        s->column = s->column == 17 ? 0 : s->column + 1;
        if ((s->row == 1 || s->row == 3) && s->column >= 8) {
            s->column = 0;
        }
        if (s->row == 5 && s->column > 10 && s->column < 15) {
            s->column = 15;
        }
        g_GameInput.horizontal = 0;
    } else if (g_GameInput.shot) {
        if (M_Shot(s, s->row, s->column)) {
            return true;
        }
        g_GameInput.shot = false;
        return false;
    } else if (g_GameInput.strike) {
        M_Shot(s, 5, 15);
        g_GameInput.strike = false;
        return false;
    } else {
        return false;
    }
    M_Alphabet(s, old_row, old_column, false);
    M_Alphabet(s, s->row, s->column, true);
    return false;
}

void Score_Begin(
    SCORE_SCENE *const s, const SCORE_ASSETS *const assets,
    const SCORE_CONTEXT context, const int32_t rank,
    const SCORE_ENTRY *const entry)
{
    *s = (SCORE_SCENE) { .assets = assets, .context = context, .rank = rank };
    SCORE_ENTRY inserted = *entry;
    strcpy(inserted.name, "                ");
    s->place = ScoreData_Insert(&g_ScoreSaves.tables[rank], &inserted);
    for (int32_t i = 0; i < 8; i++) {
        strcpy(s->slots[i], "  ");
    }
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    if (context == SCORE_ENDING) {
        M_Initial(s);
        return;
    }
    Video_SelectDrawingPage(0);
    Video_Clear(0);
    Palette_ShowSolid(0, 0, 0);
    const PI_IMAGE *const image = &assets->gameover;
    memcpy(g_Video.pages[1], image->pixels, VIDEO_PAGE_SIZE);
    memcpy(g_Video.pages[0], image->pixels, VIDEO_PAGE_SIZE);
    Palette_Init(&s->palette);
    Palette_Import(&s->palette.saved, image->palette);
    Palette_BeginFade(&s->fade, &s->palette, PALETTE_FADE_BLACK_IN);
    Frame_BeginWait(&s->wait);
    s->phase = SCORE_FADE_WAIT;
}

bool Score_Tick(
    SCORE_SCENE *const s, const TH01_INPUT *const raw,
    const bool reference_edge)
{
    if (reference_edge) {
        Frame_VSync();
    }
    for (;;) {
        switch (s->phase) {
        case SCORE_FADE_WAIT:
            if (!Frame_WaitReady(&s->wait)) {
                return false;
            }
            Palette_FadeStep(&s->fade, &s->palette);
            if (s->fade.remaining != 0) {
                Frame_BeginWait(&s->wait);
                return false;
            }
            M_Initial(s);
            break;
        case SCORE_ENTRY_DELAY:
            if (s->context == SCORE_ENDING ? (uint16_t)g_VSync.count1 < 4
                                           : !Frame_DelayReady(4)) {
                return false;
            }
            s->inputs++;
            s->phase = SCORE_ENTRY_INPUT;
            break;
        case SCORE_ENTRY_INPUT: {
            if (s->context == SCORE_ENDING) {
                GameInput_SenseEnding(raw);
            } else {
                GameInput_SenseGame(raw, false);
            }
            const bool action = g_GameInput.up || g_GameInput.down
                || g_GameInput.horizontal == GAME_INPUT_LEFT
                || g_GameInput.horizontal == GAME_INPUT_RIGHT
                || g_GameInput.shot || g_GameInput.strike;
            const bool enter = Score_OnInput(s);
            if (enter || g_GameInput.ok
                || (s->context == SCORE_GAMEOVER && s->inputs > 1000)) {
                M_Complete(s);
                return true;
            }
            if (!action) {
                return false;
            }
            if (s->context == SCORE_ENDING) {
                g_VSync.count1 = 0;
            }
            s->phase = SCORE_ENTRY_DELAY;
            break;
        }
        case SCORE_RELEASE:
            if (s->context == SCORE_ENDING) {
                GameInput_SenseEnding(raw);
            } else {
                GameInput_SenseGame(raw, false);
            }
            if (!g_GameInput.ok || !g_GameInput.shot) {
                s->phase = SCORE_DONE;
                return true;
            }
            return false;
        case SCORE_DONE:
            return true;
        }
    }
}
