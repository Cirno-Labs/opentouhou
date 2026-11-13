// SPDX-License-Identifier: GPL-3.0-only

#include "th06/game_manager.h"

#include "th06/bullet.h"
#include "th06/ascii.h"
#include "th06/ecl.h"
#include "th06/effect.h"
#include "th06/enemy.h"
#include "th06/game.h"
#include "th06/gui.h"
#include "th06/player.h"
#include "th06/scene.h"
#include "th06/screen_effect.h"
#include "th06/sound.h"
#include "th06/stage.h"
#include "th06/replay.h"
#include "th06/screens/result.h"

#include "core/io/reader.h"

#define M_CALC_PRIORITY 4
#define M_DRAW_PRIORITY 2
#define M_SPELL_CARD_COUNT 64
#define M_SPELL_CARD_BYTES 64

static const uint32_t m_ExtendScores[] = {
    10000000, 20000000, 40000000, 60000000, 1900000000,
};

static const struct {
    int32_t value, min, max;
} m_Ranks[] = {
    { 16, 12, 20 }, { 16, 10, 32 }, { 16, 10, 32 },
    { 16, 10, 32 }, { 16, 14, 18 },
};

static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

static void M_InitSpellCard(SPELL_CARD_RECORD *const card, const uint16_t index)
{
    uint8_t bytes[M_SPELL_CARD_BYTES];
    for (int32_t i = 0; i < M_SPELL_CARD_BYTES; i += 2) {
        const uint16_t value = Rng_U16(&g_Rng);
        bytes[i] = (uint8_t)value;
        bytes[i + 1] = (uint8_t)(value >> 8);
    }
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes));
    Reader_Skip(&reader, 9);
    Reader_ReadBytes(
        &reader, card->header.reserved, sizeof(card->header.reserved));
    card->bonus = Reader_ReadI32(&reader);
    Reader_Skip(&reader, 2);
    card->checksum = Reader_ReadU8(&reader);
    Reader_ReadBytes(&reader, card->history, sizeof(card->history));
    Reader_ReadBytes(&reader, card->name_bytes, sizeof(card->name_bytes));
    card->name[0] = '\0';
    card->header.magic = 0x4b544143;
    card->header.size = M_SPELL_CARD_BYTES;
    card->header.size2 = M_SPELL_CARD_BYTES;
    card->header.version = 0x10;
    card->index = index;
    card->attempts = 0;
    card->captures = 0;
}

static void M_NewGame(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->run.start_bombs = (uint8_t)gm->player.bombs;
    gm->run.start_lives = (uint8_t)gm->player.lives;
    gm->play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    gm->player_area = (PLAY_RECT) { 8.0f, 16.0f, 368.0f, 416.0f };
    gm->run.progress_frames = 0;
    gm->score.shown = 0;
    gm->score.value = 0;
    gm->score.step = 0;
    gm->score.high = 100000;
    gm->player.power = 0;
    gm->stats.continues_used = 0;
    if (gm->run.stage >= 6) {
        gm->run.difficulty = 4;
    }
    gm->score.next_extend = gm->run.difficulty < 4 ? 0 : 4;
    gm->player.power_item_combo = 0;
    gm->rank.value = 8;
    gm->player.graze_total = 0;
    gm->player.point_items_total = 0;
    for (uint16_t i = 0; i < M_SPELL_CARD_COUNT; i++) {
        M_InitSpellCard(&gm->records.spell_cards[i], i);
    }
    ResultScreen_LoadGameRecords(
        &gm->score.high, gm->records.spell_cards, gm->records.clears,
        gm->records.practice, gm->run.character * 2 + gm->run.shot_type,
        gm->run.difficulty);
    if (gm->mode.in_practice) {
        gm->score.high =
            gm->records
                .practice[gm->run.character * 2 + gm->run.shot_type]
                         [gm->run.stage][gm->run.difficulty]
                .score;
    }
    gm->rank.value = m_Ranks[gm->run.difficulty].value;
    gm->rank.min = m_Ranks[gm->run.difficulty].min;
    gm->rank.max = m_Ranks[gm->run.difficulty].max;
    gm->stats.deaths = 0;
    gm->stats.bombs_used = 0;
    gm->stats.spell_cards_captured = 0;
}

static RESULT M_Added(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
        M_NewGame();
    } else {
        gm->score.shown = gm->score.value;
        gm->score.step = 0;
    }
    gm->rank.sub = 0;
    gm->player.point_items = 0;
    gm->player.graze = 0;
    gm->mode.paused = 0;
    gm->run.stage++;
    if (!gm->mode.in_replay) {
        CLEAR_RECORD *const clear =
            &gm->records.clears[gm->run.character * 2 + gm->run.shot_type];
        if (gm->stats.continues_used == 0
            && clear->without_continues[gm->run.difficulty]
                < gm->run.stage - 1) {
            clear->without_continues[gm->run.difficulty] =
                (uint8_t)(gm->run.stage - 1);
        }
        if (clear->with_continues[gm->run.difficulty] < gm->run.stage - 1) {
            clear->with_continues[gm->run.difficulty] =
                (uint8_t)(gm->run.stage - 1);
        }
    }
    if (gm->mode.in_practice) {
        if (gm->run.stage == 2) {
            gm->player.power = 0x40;
        } else if (gm->run.stage != 1) {
            gm->player.power = 0x80;
        }
    }
    bool replay_failed = false;
    if (gm->mode.in_replay) {
        replay_failed = !Result_Log(
            Replay_Register(REPLAY_PLAY, gm->mode.replay_path),
            LOG_LEVEL_ERROR);
        while (m_ExtendScores[gm->score.next_extend] <= gm->score.shown) {
            gm->score.next_extend++;
        }
        gm->rank.min = m_Ranks[gm->run.difficulty].min;
        gm->rank.max = m_Ranks[gm->run.difficulty].max;
    }
    g_Rng.generated = 0;
    gm->run.rng_seed = g_Rng.seed;
    MUST(Stage_Register(), "stage background");
    MUST(Player_Register(), "player");
    MUST(Bullet_Register(), "bullets");
    MUST(Enemy_Register(), "enemies");
    MUST(Ecl_Load(), "enemy scripts");
    MUST(Effect_Register(), "effects");
    MUST(Gui_Register(), "interface");
    if (!gm->mode.in_replay) {
        Result_Ignore(Replay_Register(REPLAY_RECORD, "replay/th6_00.rpy"));
    }
    if (!gm->mode.in_demo && g_Stage.data.song_paths[0] != nullptr) {
        Sound_PlayMusic(g_Stage.data.song_paths[0]);
    }
    gm->mode.in_continue_menu = false;
    gm->mode.playing = true;
    if (Scene_GetRequested() != SCENE_NEXT_STAGE) {
        ResultScreen_ResetFrames();
    }
    gm->mode.frozen = false;
    gm->score.value = 0;
    gm->mode.game_cleared = false;
    AsciiText_Initialize();
    if (replay_failed) {
        Scene_Request(SCENE_TITLE);
    }
    return OK;
}

static void M_Deleted(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    if (!gm->mode.in_demo) {
        Sound_StopMusic();
    }
    Stage_Remove();
    Bullet_Remove();
    Player_Remove();
    Enemy_Remove();
    Ecl_Release();
    Effect_Remove();
    Gui_Remove();
    Replay_EndStage();
    gm->mode.playing = false;
    AsciiText_Initialize();
}

static CHAIN_STATUS M_Calc(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    if (gm->mode.in_demo) {
        if (Buttons_WasPressed(&g_Buttons, 0xffff)) {
            Scene_Request(SCENE_TITLE);
        }
        gm->mode.demo_frames++;
        if (gm->mode.demo_frames == 3600) {
            ScreenEffect_FadeOut(120, 0);
        }
        if (gm->mode.demo_frames >= 3720) {
            Scene_Request(SCENE_TITLE);
        }
    }
    if (!gm->mode.in_continue_menu && gm->mode.paused == 0 && !gm->mode.in_demo
        && g_Buttons.pressed.menu) {
        gm->mode.paused = 1;
        gm->play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    }
    gm->mode.playing = !gm->mode.in_continue_menu && gm->mode.paused == 0;
    gm->camera.viewport = (VIEWPORT) {
        .x = (float)(uint32_t)gm->play_area.x,
        .y = (float)(uint32_t)gm->play_area.y,
        .width = (float)(uint32_t)gm->play_area.width,
        .height = (float)(uint32_t)gm->play_area.height,
        .min_z = 0.5f,
        .max_z = 1.0f,
    };
    GameManager_SetUpCamera(0.0f);
    if (gm->mode.paused == 1 || gm->mode.paused == 2
        || gm->mode.in_continue_menu) {
        return CHAIN_STOP;
    }
    if (gm->score.value >= 1000000000) {
        gm->score.value = 999999990;
    }
    if (gm->score.shown != gm->score.value) {
        if (gm->score.value < gm->score.shown) {
            gm->score.value = gm->score.shown;
        }
        uint32_t step = (gm->score.value - gm->score.shown) >> 5;
        if (step >= 0x1343e) {
            step = 0x1343e;
        } else if (step < 10) {
            step = 10;
        }
        step -= step % 10;
        if (gm->score.step < step) {
            gm->score.step = step;
        }
        if (gm->score.shown + gm->score.step > gm->score.value) {
            gm->score.step = gm->score.value - gm->score.shown;
        }
        gm->score.shown += gm->score.step;
        if (gm->score.shown >= gm->score.value) {
            gm->score.step = 0;
            gm->score.shown = gm->score.value;
        }
        if (gm->score.next_extend >= 0
            && m_ExtendScores[gm->score.next_extend] <= gm->score.shown) {
            if (gm->player.lives < 8) {
                gm->player.lives++;
                Sound_Queue(SOUND_EXTEND);
            }
            gm->score.next_extend++;
            GameManager_AddRank(200);
        }
        if (gm->score.high < gm->score.shown) {
            gm->score.high = gm->score.shown;
        }
    }
    gm->run.stage_frames++;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    GAME_MANAGER *const gm = arg;
    if (gm->mode.paused != 0) {
        gm->mode.paused = 2;
    }
    return CHAIN_CONTINUE;
}

RESULT GameManager_Register(void)
{
    Chain_InitElem(&m_CalcElem, M_Calc, &g_GameManager);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    g_GameManager.run.stage_frames = 0;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, &g_GameManager);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void GameManager_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}
