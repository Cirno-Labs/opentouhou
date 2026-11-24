#include "th01/score.h"

#include "th01/scale2x.h"

#include <string.h>

SCORE_SAVES g_ScoreSaves;
SCORE_SCENE g_ScoreScene;

static const SCORE_ASSETS *m_Assets;

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
    const char *const text, const int32_t left, const int32_t top,
    const uint8_t color, const GAME_TEXT_WEIGHT weight, const bool reverse,
    const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .reverse = reverse,
                                    .clear_background = clear };
    GameText_Draw(&m_Assets->font, text, left, top, color, &style);
}

static void M_Number(
    const int32_t value, const int32_t digits, const int32_t left,
    const int32_t top, const uint8_t color, const GAME_TEXT_WEIGHT weight)
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
    M_Draw(full, left, top, color, weight, false, false);
}

static void M_Alphabet(
    const int32_t row, const int32_t column, const bool selected)
{
    const int32_t left = 32 + column * 32, top = 240 + row * 24;
    Video_Restore16((GFX_RECT) { left, top, 32, 16 });
    char text[9];
    M_Cell(row, column, text);
    M_Draw(text, left, top, selected ? 3 : 7, GAME_TEXT_BOLD, selected, false);
}

static void M_Name(void)
{
    const int32_t top = 64 + g_ScoreScene.place * 16;
    Video_Restore16((GFX_RECT) { 144, top, 128, 16 });
    for (int32_t i = 0; i < 8; i++) {
        M_Draw(
            g_ScoreScene.slots[i], 144 + i * 16, top, 3, GAME_TEXT_BOLD, false,
            false);
    }
    M_Draw(
        "＿", 144 + g_ScoreScene.cursor * 16, top, 3, GAME_TEXT_NORMAL, false,
        false);
}

static void M_Initial(void)
{
    const bool gameover = g_ScoreScene.context == SCORE_GAMEOVER;
    Video_SelectDrawingPage(1);
    M_Draw(m_Title, 0, gameover ? 384 : 0, 7, GAME_TEXT_BOLD, false, true);
    M_Draw(
        m_Ranks[g_ScoreScene.rank], 192, gameover ? 384 : 0, 7, GAME_TEXT_BOLD,
        false, true);
    Video_SelectDrawingPage(0);
    Scale2x_Copy(48, 0, 0, gameover ? 384 : 0, 288, 16);
    if (gameover) {
        Video_Copy(0, (GFX_RECT) { 0, 384, 288, 16 }, 1, 0, 384);
    }
    static const int32_t columns[] = { 32, 144, 352, 512 };
    for (int32_t i = 0; i < 4; i++) {
        M_Draw(m_Headers[i], columns[i], 48, 3, GAME_TEXT_BLACK, false, false);
    }
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        const SCORE_ENTRY *const e =
            &g_ScoreSaves.tables[g_ScoreScene.rank].entries[i];
        const uint8_t color = i == g_ScoreScene.place ? 3 : 7;
        const int32_t top = 64 + i * 16;
        M_Draw(m_Places[i], 32, top, color, GAME_TEXT_BOLD, false, false);
        M_Draw(
            i == g_ScoreScene.place ? "＿＿＿＿＿＿＿＿" : e->name, 144, top,
            color, GAME_TEXT_BOLD, false, false);
        M_Number(e->score, 7, 352, top, color, GAME_TEXT_BLACK);
        if (e->stage < 40) {
            M_Number(e->stage, 2, 528, top, color, GAME_TEXT_BOLD);
        } else if (e->stage == 50 || e->stage == 60) {
            M_Draw(
                e->stage == 50 ? "魔界" : "地獄", 528, top, color,
                GAME_TEXT_BOLD, false, false);
        }
        M_Draw("－", 560, top, color, GAME_TEXT_BOLD, false, false);
        M_Draw(e->route, 576, top, color, GAME_TEXT_BOLD, false, false);
    }
    GameInput_ResetGame();
    if (g_ScoreScene.place == SCORE_PLACES) {
        g_GameInput.ok = true;
        g_GameInput.shot = true;
        g_ScoreScene.phase = SCORE_RELEASE;
        return;
    }
    for (int32_t row = 0; row < 6; row++) {
        for (int32_t col = 0; col < 18; col++) {
            if (((row == 1 || row == 3) && col >= 8)
                || (row == 5 && col > 10 && col < 15)) {
                continue;
            }
            M_Alphabet(row, col, row == 0 && col == 0);
        }
    }
    g_ScoreScene.phase = SCORE_ENTRY_INPUT;
}

static void M_Complete(void)
{
    SCORE_ENTRY *const entry =
        &g_ScoreSaves.tables[g_ScoreScene.rank].entries[g_ScoreScene.place];
    entry->name[0] = '\0';
    for (int32_t i = 0; i < 8; i++) {
        strcat(entry->name, g_ScoreScene.slots[i]);
    }
    g_GameInput.ok = false;
    ScoreData_Queue(&g_ScoreSaves, g_ScoreScene.rank);
    g_ScoreScene.phase = SCORE_DONE;
}

static bool M_Shot(const int32_t row, const int32_t column)
{
    const bool was_last = g_ScoreScene.cursor == 7;
    if (row == 5 && column >= 15) {
        if (column == 17) {
            return true;
        }
        if (column == 15 && g_ScoreScene.cursor > 0) {
            g_ScoreScene.cursor--;
        } else if (column == 16 && g_ScoreScene.cursor < 7) {
            g_ScoreScene.cursor++;
        }
    } else {
        if (row == 5 && column == 10) {
            strcpy(g_ScoreScene.slots[g_ScoreScene.cursor], "　");
        } else {
            M_Cell(row, column, g_ScoreScene.slots[g_ScoreScene.cursor]);
        }
        if (g_ScoreScene.cursor < 7) {
            g_ScoreScene.cursor++;
        }
    }
    const bool jump = was_last && g_ScoreScene.context == SCORE_GAMEOVER
        && !(row == 5 && column >= 15);
    M_Name();
    if (jump) {
        M_Alphabet(g_ScoreScene.row, g_ScoreScene.column, false);
        g_ScoreScene.row = 5;
        g_ScoreScene.column = 17;
        M_Alphabet(g_ScoreScene.row, g_ScoreScene.column, true);
    }
    return false;
}

bool Score_OnInput(void)
{
    const int32_t old_row = g_ScoreScene.row, old_column = g_ScoreScene.column;
    if (g_GameInput.up) {
        g_ScoreScene.row = g_ScoreScene.row == 0 ? 5 : g_ScoreScene.row - 1;
        if ((g_ScoreScene.row == 1 || g_ScoreScene.row == 3)
            && g_ScoreScene.column >= 8) {
            g_ScoreScene.row--;
        }
        if (g_ScoreScene.row == 5 && g_ScoreScene.column > 10
            && g_ScoreScene.column < 15) {
            g_ScoreScene.row = 4;
        }
        g_GameInput.up = false;
    } else if (g_GameInput.down) {
        g_ScoreScene.row = g_ScoreScene.row == 5 ? 0 : g_ScoreScene.row + 1;
        if ((g_ScoreScene.row == 1 || g_ScoreScene.row == 3)
            && g_ScoreScene.column >= 8) {
            g_ScoreScene.row++;
        }
        if (g_ScoreScene.row == 5 && g_ScoreScene.column > 10
            && g_ScoreScene.column < 15) {
            g_ScoreScene.row = 0;
        }
        g_GameInput.down = false;
    } else if (g_GameInput.horizontal == GAME_INPUT_LEFT) {
        g_ScoreScene.column =
            g_ScoreScene.column == 0 ? 17 : g_ScoreScene.column - 1;
        if ((g_ScoreScene.row == 1 || g_ScoreScene.row == 3)
            && g_ScoreScene.column >= 8) {
            g_ScoreScene.column = 7;
        }
        if (g_ScoreScene.row == 5 && g_ScoreScene.column > 10
            && g_ScoreScene.column < 15) {
            g_ScoreScene.column = 10;
        }
        g_GameInput.horizontal = 0;
    } else if (g_GameInput.horizontal == GAME_INPUT_RIGHT) {
        g_ScoreScene.column =
            g_ScoreScene.column == 17 ? 0 : g_ScoreScene.column + 1;
        if ((g_ScoreScene.row == 1 || g_ScoreScene.row == 3)
            && g_ScoreScene.column >= 8) {
            g_ScoreScene.column = 0;
        }
        if (g_ScoreScene.row == 5 && g_ScoreScene.column > 10
            && g_ScoreScene.column < 15) {
            g_ScoreScene.column = 15;
        }
        g_GameInput.horizontal = 0;
    } else if (g_GameInput.shot) {
        if (M_Shot(g_ScoreScene.row, g_ScoreScene.column)) {
            return true;
        }
        g_GameInput.shot = false;
        return false;
    } else if (g_GameInput.strike) {
        M_Shot(5, 15);
        g_GameInput.strike = false;
        return false;
    } else {
        return false;
    }
    M_Alphabet(old_row, old_column, false);
    M_Alphabet(g_ScoreScene.row, g_ScoreScene.column, true);
    return false;
}

void Score_Begin(
    const SCORE_ASSETS *const assets, const SCORE_CONTEXT context,
    const int32_t rank, const SCORE_ENTRY *const entry)
{
    memset(&g_ScoreScene, 0, sizeof(g_ScoreScene));
    m_Assets = assets;
    g_ScoreScene.context = context;
    g_ScoreScene.rank = rank;
    SCORE_ENTRY inserted = *entry;
    strcpy(inserted.name, "                ");
    g_ScoreScene.place =
        ScoreData_Insert(&g_ScoreSaves.tables[rank], &inserted);
    for (int32_t i = 0; i < 8; i++) {
        strcpy(g_ScoreScene.slots[i], "  ");
    }
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    if (context == SCORE_ENDING) {
        M_Initial();
        return;
    }
    Video_SelectDrawingPage(0);
    Video_Clear(0);
    Palette_ShowSolid(0, 0, 0);
    const PI_IMAGE *const image = &assets->gameover;
    memcpy(g_Video.pages[1], image->pixels, VIDEO_PAGE_SIZE);
    memcpy(g_Video.pages[0], image->pixels, VIDEO_PAGE_SIZE);
    Palette_Init(&g_ScoreScene.palette);
    Palette_Import(&g_ScoreScene.palette.saved, image->palette);
    Palette_BeginFade(
        &g_ScoreScene.fade, &g_ScoreScene.palette, PALETTE_FADE_BLACK_IN);
    Frame_BeginWait(&g_ScoreScene.wait);
    g_ScoreScene.phase = SCORE_FADE_WAIT;
}

bool Score_Tick(const TH01_INPUT *const raw, const bool reference_edge)
{
    if (reference_edge) {
        Frame_VSync();
    }
    for (;;) {
        switch (g_ScoreScene.phase) {
        case SCORE_FADE_WAIT:
            if (!Frame_WaitReady(&g_ScoreScene.wait)) {
                return false;
            }
            Palette_FadeStep(&g_ScoreScene.fade, &g_ScoreScene.palette);
            if (g_ScoreScene.fade.remaining != 0) {
                Frame_BeginWait(&g_ScoreScene.wait);
                return false;
            }
            M_Initial();
            break;
        case SCORE_ENTRY_DELAY:
            if (g_ScoreScene.context == SCORE_ENDING
                    ? (uint16_t)g_VSync.count1 < 4
                    : !Frame_DelayReady(4)) {
                return false;
            }
            g_ScoreScene.inputs++;
            g_ScoreScene.phase = SCORE_ENTRY_INPUT;
            break;
        case SCORE_ENTRY_INPUT: {
            if (g_ScoreScene.context == SCORE_ENDING) {
                GameInput_SenseEnding(raw);
            } else {
                GameInput_SenseGame(raw, false);
            }
            const bool action = g_GameInput.up || g_GameInput.down
                || g_GameInput.horizontal == GAME_INPUT_LEFT
                || g_GameInput.horizontal == GAME_INPUT_RIGHT
                || g_GameInput.shot || g_GameInput.strike;
            const bool enter = Score_OnInput();
            if (enter || g_GameInput.ok
                || (g_ScoreScene.context == SCORE_GAMEOVER
                    && g_ScoreScene.inputs > 1000)) {
                M_Complete();
                return true;
            }
            if (!action) {
                return false;
            }
            if (g_ScoreScene.context == SCORE_ENDING) {
                g_VSync.count1 = 0;
            }
            g_ScoreScene.phase = SCORE_ENTRY_DELAY;
            break;
        }
        case SCORE_RELEASE:
            if (g_ScoreScene.context == SCORE_ENDING) {
                GameInput_SenseEnding(raw);
            } else {
                GameInput_SenseGame(raw, false);
            }
            if (!g_GameInput.ok || !g_GameInput.shot) {
                g_ScoreScene.phase = SCORE_DONE;
                return true;
            }
            return false;
        case SCORE_DONE:
            return true;
        }
    }
}

void Score_Close(void)
{
    memset(&g_ScoreScene, 0, sizeof(g_ScoreScene));
    m_Assets = nullptr;
}
