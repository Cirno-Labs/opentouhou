#include "th02/gameplay.h"

#include "th02/background.h"
#include "th02/bomb.h"
#include "th02/boss/fight.h"
#include "th02/enemies.h"
#include "th02/gameplay/memory.h"
#include "th02/gameplay/private.h"
#include "th02/hud.h"
#include "th02/items.h"
#include "th02/pillars.h"
#include "th02/player.h"
#include "th02/shots/fire.h"
#include "th02/sound_effects.h"
#include "th02/stage.h"
#include "th02/stage/flow.h"
#include "th02/stage/private.h"
#include "th02/stage_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define CONTINUE_INVINCIBILITY_FRAMES 200

static GAMEPLAY m_Gameplay;

static bool M_DemoInput(void)
{
    if (m_Gameplay.demo.record != nullptr) {
        if (!m_Gameplay.demo_fading) {
            uint16_t keys = g_Stage->world.keys;
            const bool playing = Demo_Input(&m_Gameplay.demo, &keys);
            Stage_SetKeys(keys);
            if (playing) {
                return true;
            }
            m_Gameplay.demo_fading = true;
            Wait_Fade(&m_Gameplay.wait, PALETTE_BLACK_OUT, 10);
        }
        if (!Wait_Ready(&m_Gameplay.wait, nullptr, 0)) {
            return false;
        }
        Stage_Quit();
        SoundEffects_Reset();
        return true;
    }
    return true;
}

static void M_BeginLoop(void)
{
    m_Gameplay.loop.phase = STAGE_TICK_BEGIN;
    m_Gameplay.loop.wait = STAGE_RUNNING;
    StageScroll_InitPages();
}

static STAGE_EVENT M_Pause(const uint16_t keys)
{
    const PAUSE_MENU_EVENT event = PauseMenu_Update(keys);
    const PAUSE_MENU_RESULT result = event.result;
    Stage_SetKeys(event.keys);
    if (result == PAUSE_MENU_FRAME) {
        Stage_SetVSyncCount(0);
        return m_Gameplay.loop.wait = STAGE_PAUSE_FRAME;
    }
    if (result == PAUSE_MENU_INPUT) {
        return m_Gameplay.loop.wait = STAGE_PAUSE_INPUT;
    }
    if (result == PAUSE_MENU_QUIT) {
        Stage_Quit();
    }
    m_Gameplay.loop.phase = STAGE_TICK_FINISH;
    return m_Gameplay.loop.wait = STAGE_RUNNING;
}

static STAGE_EVENT M_Step(const uint16_t keys)
{
    if (m_Gameplay.loop.wait == STAGE_QUIT
        || m_Gameplay.loop.wait == STAGE_NEXT) {
        return m_Gameplay.loop.wait;
    }
    const BOSS_MUSIC *const music = &m_Gameplay.music.bosses;
    TH02_SESSION *const session = m_Gameplay.session;
    for (;;) {
        switch (m_Gameplay.loop.phase) {
        case STAGE_TICK_BEGIN:
            if (g_Stage->world.quit != 0) {
                Score_Commit();
                Stage_ClearQuit();
                return m_Gameplay.loop.wait = STAGE_QUIT;
            }
            SoundEffects_Update();
            StageTitles_Update(g_Stage->world.frame);
            StageTitles_UpdateMusic();
            StageScroll_Begin();
            StageFlow_Invalidate(session->stage, session->reduce_effects);
            if (g_Stage->world.midboss_active != 0) {
                Stage_SetMidbossActive(
                    BossFight_HasMidboss(session->stage)
                    && BossFight_MidbossInvalidate(session->stage));
            }
            StageFlow_InvalidateEnemies();
            Player_Invalidate(StageScroll_Pages(), session->reduce_effects);
            Bullets_Invalidate(StageScroll_Pages());
            Pillars_Invalidate(StageScroll_Pages(), g_Stage->world.frame);
            Items_Invalidate(StageScroll_Pages());
            Sparks_Invalidate(StageScroll_Pages());
            Bomb_Invalidate(StageScroll_Pages(), m_Gameplay.bomb);
            if (g_Stage->scroll.row_changed != 0) {
                Enemies_Spawn(
                    &m_Gameplay.stage->data, g_Stage->scroll.back,
                    (int16_t)g_Stage->scroll.rows,
                    g_Stage->world.midboss_active);
                Stage_ClearRowChange();
            }
            StageEffects_Invalidate(StageScroll_Pages());
            Background_RepaintDirty();
            StageFlow_UpdateEffects();
            Grcg_Off();
            if ((uint16_t)g_Stage->world.midboss_row == g_Stage->scroll.rows) {
                Stage_SetMidbossActive(true);
            }
            StageScroll_Advance();
            m_Gameplay.loop.phase = STAGE_TICK_INPUT;
            return m_Gameplay.loop.wait = STAGE_INPUT;

        case STAGE_TICK_INPUT:
            Stage_SetKeys(keys);
            m_Gameplay.loop.phase = STAGE_TICK_DEMO;
            break;

        case STAGE_TICK_DEMO:
            if (!M_DemoInput()) {
                return m_Gameplay.loop.wait = STAGE_EXTERNAL;
            }
            Player_Controls(StageScroll_Pages(), g_Stage->world.keys);
            uint8_t slowdown = g_Stage->world.slowdown;
            Bomb_Update(
                StageScroll_Pages(), m_Gameplay.bomb, &slowdown,
                session->reduce_effects, m_Gameplay.bomb_tail,
                m_Gameplay.fix_bomb_transition_overread);
            Stage_SetSlowdown(slowdown);
            StageFlow_UpdateEnemies(
                &m_Gameplay.stage->data, &m_Gameplay.assets.patterns,
                (uint8_t)session->rank);
            m_Gameplay.loop.phase = STAGE_TICK_BOSS;
            break;

        case STAGE_TICK_BOSS: {
            g_VSync.count1 = g_Stage->world.vsync_count;
            const STAGE_FLOW_RESULT result = StageFlow_Update(
                &m_Gameplay.assets.patterns, &m_Gameplay.assets.bosses, music,
                session, &m_Gameplay.dialogue_input);
            Stage_SetVSyncCount(g_VSync.count1);
            if (result == STAGE_FLOW_WAIT) {
                return m_Gameplay.loop.wait = STAGE_EXTERNAL;
            }
            if (g_Stage->world.midboss_active != 0
                && BossFight_HasMidboss(session->stage)) {
                BossFight_MidbossUpdate(
                    &m_Gameplay.assets.patterns, session->stage,
                    (uint8_t)session->rank);
            }
            if (Player_Update(
                    StageScroll_Pages(), g_Stage->world.frame,
                    session->reduce_effects, &m_Gameplay.assets.patterns,
                    session->start_bombs)) {
                Stage_Quit();
            }
            Items_Update(
                StageScroll_Pages(), (uint8_t)session->rank,
                &m_Gameplay.assets.patterns, &m_Gameplay.assets.numbers);
            Pillars_Update(
                StageScroll_Pages(), g_Stage->world.frame,
                &m_Gameplay.assets.patterns);
            Bullets_Update(
                StageScroll_Pages(), g_Stage->world.frame,
                &m_Gameplay.assets.patterns, &m_Gameplay.assets.bullets);
            Sparks_Update(StageScroll_Pages(), &m_Gameplay.assets.sparks);
            if ((g_Stage->world.keys & GAME_KEY_CANCEL) != 0) {
                PauseMenu_Begin(&m_Gameplay.assets.pause, g_Stage->world.keys);
                m_Gameplay.loop.phase = STAGE_TICK_PAUSE;
                const STAGE_EVENT event = M_Pause(0);
                if (event != STAGE_RUNNING) {
                    return event;
                }
            } else {
                m_Gameplay.loop.phase = STAGE_TICK_FINISH;
            }
            break;
        }

        case STAGE_TICK_PAUSE: {
            uint16_t pause_keys = 0;
            if (m_Gameplay.loop.wait == STAGE_PAUSE_FRAME) {
                if (g_Stage->world.vsync_count < 1) {
                    return STAGE_PAUSE_FRAME;
                }
            } else if (m_Gameplay.loop.wait == STAGE_PAUSE_INPUT) {
                Stage_SetKeys(keys);
                pause_keys = keys;
            }
            const STAGE_EVENT event = M_Pause(pause_keys);
            if (event != STAGE_RUNNING) {
                return event;
            }
            break;
        }

        case STAGE_TICK_FINISH:
            StageScroll_Finish();
            if (g_Stage->world.slowdown == 1) {
                Stage_SetVSyncCount(0);
            }
            m_Gameplay.loop.phase = STAGE_TICK_WAIT;
            break;

        case STAGE_TICK_WAIT:
            if (g_Stage->world.slowdown > g_Stage->world.vsync_count) {
                return m_Gameplay.loop.wait = STAGE_FRAME_WAIT;
            }
            Stage_SetVSyncCount(0);
            StageScroll_Swap();
            m_Gameplay.loop.phase = STAGE_TICK_ENTER;
            break;

        case STAGE_TICK_ENTER: {
            g_VSync.count1 = g_Stage->world.vsync_count;
            const STAGE_FLOW_RESULT result = StageFlow_Enter(
                &m_Gameplay.assets.patterns, &m_Gameplay.assets.bosses, music,
                session, &m_Gameplay.dialogue_input);
            Stage_SetVSyncCount(g_VSync.count1);
            if (result == STAGE_FLOW_BOSS_STARTED) {
                StageTitles_Boss();
            }
            if (result == STAGE_FLOW_WAIT) {
                return m_Gameplay.loop.wait = STAGE_EXTERNAL;
            }
            session->frame = (int32_t)((uint32_t)session->frame + 1u);
            Stage_AdvanceFrame();
            m_Gameplay.loop.phase = STAGE_TICK_EXIT;
            break;
        }

        case STAGE_TICK_EXIT: {
            g_VSync.count1 = g_Stage->world.vsync_count;
            const STAGE_FLOW_RESULT result = StageFlow_End(
                &m_Gameplay.assets.patterns, &m_Gameplay.assets.bosses, music,
                session, &m_Gameplay.dialogue_input);
            Stage_SetVSyncCount(g_VSync.count1);
            if (result == STAGE_FLOW_WAIT) {
                return m_Gameplay.loop.wait = STAGE_EXTERNAL;
            }
            if (result == STAGE_FLOW_CLEARED && g_Stage->world.quit == 0) {
                return m_Gameplay.loop.wait = STAGE_NEXT;
            }
            Performance_Advance(g_Stage->world.frame);
            Score_Update(session->reduce_effects);
            m_Gameplay.loop.phase = STAGE_TICK_BEGIN;
            break;
        }
        }
    }
}

static void M_Stage(void)
{
    const STAGE_ASSETS *const stage = m_Gameplay.stage;
    Palette_SetToneQuiet(100);
    Palette_Import(stage->background.tiles.palette);
    Palette_Show();
    Tram_FillAttributes(4, 1, 51, 23, 0x05);
    Background_ShowStage();
    if (m_Gameplay.session->demo_num == 0
        && m_Gameplay.music.stage != nullptr) {
        m_Gameplay.music.stage(m_Gameplay.music.bosses.context);
    }
    for (uint8_t page = 0; page < 2; page++) {
        Video_AccessPage(page);
        Grcg_SetColor(11);
        Draw_BoxFill(g_Clip, 416, 0, 575, 399);
        Grcg_Off();
    }
    Hud_WipePlayfield();
    Video_ShowPage(1);
    Video_AccessPage(0);
    Stage_ResetPages();
    Hud_BeginWipe(&m_Gameplay.reveal, false);
    m_Gameplay.phase = GAMEPLAY_REVEAL;
}

static void M_FinishStageSetup(void)
{
    Items_ResetStage((uint8_t)m_Gameplay.session->rank);
    Score_InitExtends();
    m_Gameplay.phase = GAMEPLAY_PREPARE;
}

static void M_BeginStage(
    const STAGE_ASSETS *const stage, const PATTERN_BANK *const patterns,
    const BOMB_ASSETS *const bomb)
{
    TH02_SESSION *const session = m_Gameplay.session;
    ASSERT(session->stage < 6);
    ASSERT(stage->stage == session->stage);
    ASSERT(stage->data.map_count >= 3);
    ASSERT(session->shot_type < 3);
    ASSERT(bomb->shot_type == session->shot_type);
    m_Gameplay.stage = stage;
    m_Gameplay.bomb = bomb;
    BossTalk_BeginStage();
    if (!m_Gameplay.fix_bomb_transition_overread) {
        ASSERT(m_Gameplay.bomb_transition_tail != nullptr);
        memcpy(
            m_Gameplay.bomb_tail, m_Gameplay.bomb_transition_tail,
            sizeof(m_Gameplay.bomb_tail));
    }
    m_Gameplay.assets = *m_Gameplay.source_assets;
    m_Gameplay.assets.patterns = *patterns;
    StageTitles_Begin(
        &m_Gameplay.assets.titles, session->stage, session->demo_num != 0);
    m_Gameplay.phase = GAMEPLAY_MUSIC_FADE;
    Grcg_Off();
    ASSERT(
        g_Clip->left == 0 && g_Clip->top == 0 && g_Clip->right == 639
        && g_Clip->bottom == 399);
    Video_SetSplit(400, 0, 0);
    for (uint8_t page = 0; page < 2; page++) {
        Video_AccessPage(page);
        Video_Clear();
    }
    Video_AccessPage(0);
    Video_ShowPage(0);
    Tram_Fill(' ', 0x05);
    Player_ResetPosition();
    Bomb_ResetStage();
    Stage_Reset();
    Random_Seed((uint32_t)session->frame);
    Shots_SetWeaponLevel(Hud_Put(
        g_Score, session->continues_used, (uint8_t)session->rank,
        g_Player->power));
    Hud_WipePlayfield();
    StageBackdrop_Draw(&m_Gameplay.assets.backdrop);
    Bullets_ResetStage(session->rank, session->reduce_effects);
    SoundEffects_Reset();
    BossFight_BeginStage();
    Random_Fill();
    Palette_SetTone(100);
    Stage_SetSlowdown(1);
    StageFlow_Begin(session->stage, session->reduce_effects);
    Stage_Begin(session->stage, session->reduce_effects);
    Background_Reset();
    Background_ResetScroll();
    Background_SetMap(
        &stage->background.map, stage->data.map, stage->data.map_count);
    Background_SetSources(&stage->background.tiles);
    Player_ResetStage(m_Gameplay.session->shot_type);
    Stage_ClearQuit();
    Enemies_ResetStage();
    Wait_Delay(&m_Gameplay.wait, 100);
    if (m_Gameplay.music.entry == nullptr
        || MusicWait_Ready(m_Gameplay.music.entry)) {
        M_FinishStageSetup();
    }
}

static void M_ContinueReset(void)
{
    Player_ResetPosition();
    Player_ResetStage(m_Gameplay.session->shot_type);
    Stage_ClearQuit();
    Player_BeginInvincibility(CONTINUE_INVINCIBILITY_FRAMES);
    Video_AccessPage(g_Stage->scroll.front);
    Background_RepaintAll();
    Video_AccessPage(g_Stage->scroll.back);
    Background_RepaintAll();
    Bullets_RemoveAll(StageScroll_Pages());
    Palette_SetTone(100);
    Hud_BeginWipe(&m_Gameplay.reveal, false);
    m_Gameplay.phase = GAMEPLAY_CONTINUE_REVEAL;
}

static GAMEPLAY_RESULT M_Result(void)
{
    if (m_Gameplay.session->stage == 127) {
        return GAMEPLAY_ENDING;
    }
    if (m_Gameplay.phase != GAMEPLAY_DONE) {
        return GAMEPLAY_RUNNING;
    }
    if (m_Gameplay.session->demo_num != 0) {
        return GAMEPLAY_DEMO_END;
    }
    return m_Gameplay.loop.wait == STAGE_NEXT ? GAMEPLAY_CLEAR
                                              : GAMEPLAY_GAME_OVER;
}

void Gameplay_Begin(
    TH02_SESSION *const session, const GAMEPLAY_ASSETS *const assets,
    const GAMEPLAY_MUSIC *const music, const DEMO_RECORD *const demo,
    const int32_t highest_record, const bool fix_bomb_transition_overread,
    const uint8_t *const bomb_transition_tail)
{
    GameplayMemory_Shutdown();
    memset(&m_Gameplay, 0, sizeof(m_Gameplay));
    m_Gameplay.session = session;
    m_Gameplay.source_assets = assets;
    if (music != nullptr) {
        m_Gameplay.music = *music;
    }
    m_Gameplay.fix_bomb_transition_overread = fix_bomb_transition_overread;
    m_Gameplay.bomb_transition_tail = bomb_transition_tail;
    PauseMenu_Init();
    Score_Init();
    Player_Init();
    PointNumbers_Init();
    Sparks_Init();
    Items_Init();
    Bullets_Init();
    Enemies_Init();
    BossFight_Init();
    Stage_Init();
    SESSION_ENTRY entry;
    Session_EnterMain(session, &entry);
    Player_SetPower(entry.power);
    int32_t score_value = session->demo_num != 0 ? 0 : session->score;
    if (session->continues_used != 0) {
        score_value =
            session->continues_used >= 9 ? 9 : session->continues_used;
        Items_BeginSession(session->stage);
    }
    Score_InitHighest(highest_record);
    Score_BeginSession(score_value, entry.lives, entry.bombs);
    Performance_Init(entry.rank);
    if (session->demo_num != 0) {
        Player_SetPower(80);
        Performance_BeginDemo();
        Demo_Begin(&m_Gameplay.demo, demo);
    }
    Random_Reset();
}

void Gameplay_BeginStage(
    const STAGE_ASSETS *const stage, const PATTERN_BANK *const patterns,
    const BOMB_ASSETS *const bomb)
{
    ASSERT(
        m_Gameplay.phase == GAMEPLAY_READY
        || (m_Gameplay.phase == GAMEPLAY_DONE
            && m_Gameplay.loop.wait == STAGE_NEXT));
    if (m_Gameplay.phase != GAMEPLAY_READY) {
        ASSERT(g_Score->delta == 0);
    }
    if (m_Gameplay.phase == GAMEPLAY_READY) {
        GameplayMemory_Begin(
            m_Gameplay.source_assets, bomb, m_Gameplay.demo.record != nullptr);
    }
    GameplayMemory_LoadStage(stage, patterns);
    M_BeginStage(stage, patterns, bomb);
}

void Gameplay_EndCredit(const bool resume)
{
    ASSERT(m_Gameplay.phase == GAMEPLAY_DONE);
    TH02_SESSION *const session = m_Gameplay.session;
    if (session->score > session->highest_score) {
        session->highest_score = session->score;
    }
    Score_Continue((int8_t)session->start_lives, (int8_t)session->start_bombs);
    Player_SetPower(1);
    session->continues_used++;
    Score_Reset();
    Items_BeginSession(session->stage);
    Items_ResetOverflow();
    Palette_SetTone(100);
    Hud_WipePlayfield();
    if (!resume) {
        return;
    }
    Shots_SetWeaponLevel(Hud_Put(
        g_Score, session->continues_used, (uint8_t)session->rank,
        g_Player->power));
    Hud_BeginWipe(&m_Gameplay.reveal, true);
    m_Gameplay.phase = GAMEPLAY_CONTINUE_COVER;
}

GAMEPLAY_RESULT Gameplay_Update(const TH02_INPUT *const input)
{
    ASSERT(m_Gameplay.phase != GAMEPLAY_READY);
    if (m_Gameplay.phase == GAMEPLAY_DONE || m_Gameplay.session->stage == 127) {
        return M_Result();
    }
    m_Gameplay.dialogue_input = *input;
    if (m_Gameplay.phase == GAMEPLAY_CONTINUE_COVER) {
        if (!Hud_UpdateWipe(&m_Gameplay.reveal)) {
            return GAMEPLAY_RUNNING;
        }
        M_ContinueReset();
        return GAMEPLAY_RUNNING;
    }
    if (m_Gameplay.phase == GAMEPLAY_CONTINUE_REVEAL) {
        if (!Hud_UpdateWipe(&m_Gameplay.reveal)) {
            return GAMEPLAY_RUNNING;
        }
        M_BeginLoop();
        m_Gameplay.phase = GAMEPLAY_STAGE;
    }
    if (m_Gameplay.phase == GAMEPLAY_MUSIC_FADE) {
        if (!MusicWait_Ready(m_Gameplay.music.entry)) {
            return GAMEPLAY_RUNNING;
        }
        M_FinishStageSetup();
    }
    if (m_Gameplay.phase == GAMEPLAY_PREPARE) {
        if (!Wait_Ready(&m_Gameplay.wait, nullptr, 0)) {
            return GAMEPLAY_RUNNING;
        }
        M_Stage();
    }
    if (m_Gameplay.phase == GAMEPLAY_REVEAL) {
        if (!Hud_UpdateWipe(&m_Gameplay.reveal)) {
            return GAMEPLAY_RUNNING;
        }
        StageTitles_Reveal();
        M_BeginLoop();
        m_Gameplay.phase = GAMEPLAY_STAGE;
    }
    Stage_SetVSyncCount(g_VSync.count1);
    const uint16_t keys = GameInput_Sense(input);
    STAGE_EVENT event = M_Step(keys);
    while (event == STAGE_INPUT || event == STAGE_PAUSE_INPUT) {
        event = M_Step(keys);
    }
    g_VSync.count1 = g_Stage->world.vsync_count;
    if (event == STAGE_QUIT || event == STAGE_NEXT) {
        m_Gameplay.phase = GAMEPLAY_DONE;
    }
    return M_Result();
}

void Gameplay_Shutdown(void)
{
    GameplayMemory_Shutdown();
    memset(&m_Gameplay, 0, sizeof(m_Gameplay));
    PauseMenu_Init();
}
