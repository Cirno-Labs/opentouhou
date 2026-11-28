#include "th02/program/private.h"

#include <string.h>
#include <time.h>

static PROGRAM m_Program;
PROGRAM *const p_Program = &m_Program;

// Fills in the live wall clock and local date for the real program, unless
// the caller already supplied clock samples or a date of its own.
// Th02_EnableLiveClock gates this off everywhere else, so a test, a
// replay or a capture that leaves clock_count/date at zero keeps
// AudioClock inactive and the date unspecified, instead of racing the
// real clock.
static void M_PrepareInput(void *const input)
{
    if (!p_Program->live_clock_enabled) {
        return;
    }
    TH02_INPUT *const raw = input;
    if (raw->clock_count == 0) {
        p_Program->live_clocks[0] =
            (TH02_CLOCK_SAMPLE) { .key = TH02_CLOCK_FRAME,
                                  .microseconds = Shell_GetWallClock() * 1000 };
        p_Program->live_clocks[1] = (TH02_CLOCK_SAMPLE) {
            .key = TH02_CLOCK_ORIGIN,
            .microseconds = p_Program->host_origin,
        };
        p_Program->live_clocks[2] = (TH02_CLOCK_SAMPLE) {
            .key = TH02_CLOCK_SCANOUT_KEY,
            .microseconds = p_Program->live_clocks[0].microseconds,
        };
        raw->clocks = p_Program->live_clocks;
        raw->clock_count = ARRAY_SIZE(p_Program->live_clocks);
    }
    if (raw->date == 0) {
        raw->date = 19000101;
        const time_t now = time(nullptr);
        const struct tm *const local = localtime(&now);
        if (local != nullptr) {
            raw->date = (uint32_t)(local->tm_year + 1900) * 10000
                + (uint32_t)(local->tm_mon + 1) * 100
                + (uint32_t)local->tm_mday;
        }
    }
}

RESULT Program_Init(void)
{
    p_Program->display.font_saved = false;
    p_Program->initialized = false;
    p_Program->scenes.scene = TH02_SCENE_OPENING;
    p_Program->scenes.request = MAIN_MENU_NONE;
    p_Program->scenes.save_scores = p_Program->scenes.save_settings =
        p_Program->failed = false;
    StageBonus_Init();
    p_Program->audio.pmd = nullptr;
    MusicWait_Init(&p_Program->audio.music_wait);
    p_Program->audio.music_loaded = false;
    AudioClock_Init(nullptr, nullptr, nullptr);
    p_Program->audio.game = nullptr;
    SoundEffects_Init(nullptr);
    VSyncIrq_BeginApplication(&p_Program->clock);
    Arena_Init(&p_Program->assets.arena, "TH02 assets", 32 * 1024 * 1024);
    VFS *const files = Shell_GetFiles();
    MUST(ProgramAssets_Mount(files));
    MUST(HighScoresFile_Load(
        files, Shell_GetUserFiles(), &p_Program->scenes.scores));
    Result_Log(
        Settings_ImportOriginal(files, Shell_GetUserFiles(), &g_Th02Settings),
        LOG_LEVEL_WARNING);
    MUST(TextFont_Open(&p_Program->display.font));
    MUST(ProgramAssets_Load(files));
    READER effects;
    MUST(Vfs_Open(files, "huuma.efc", VFS_STREAM, &effects));
    PMD_EFFECTS *effect_bank;
    const RESULT effects_loaded = PmdData_LoadEffects(
        &effects, Arena_Allocator(&p_Program->assets.arena), &effect_bank);
    Reader_Close(&effects);
    MUST(effects_loaded);
    MUST(Pmd_Init(
        effect_bank, PMD_BOARD_86, 0, Arena_Allocator(&p_Program->assets.arena),
        &p_Program->audio.pmd));
    p_Program->host_origin = Shell_GetWallClock() * 1000;
    SoundEffects_Init(p_Program->audio.pmd);
    if (Audio_IsInitialized()) {
        Result_Log(
            GameAudio_Prepare(
                Arena_Allocator(&p_Program->assets.arena),
                &p_Program->audio.game),
            LOG_LEVEL_WARNING);
    }
    AudioClock_Init(
        p_Program->audio.pmd, p_Program->audio.game,
        &p_Program->audio.music_wait);
    ProgramAudio_InitSound(&p_Program->audio.sound);
    p_Program->display.saved_gaiji = p_Program->display.gaiji;
    p_Program->display.gaiji = p_Program->assets.opening.gaiji;
    p_Program->display.font_saved = true;
    p_Program->assets.opening.title.glyphs = &p_Program->display.gaiji;
    Video_Init(Arena_Allocator(&p_Program->assets.arena));
    Tram_Init();
    TramDisplay_Init(&p_Program->display.text, 0);
    TramConsole_Init(
        &p_Program->display.console, TRAM_CONSOLE_DEFAULT_ATTRIBUTE);
    p_Program->display.console.cursor_visible = false;
    p_Program->display.console.system_line_visible = false;
    Clip_Reset();
    Grcg_Init();
    Palette_Init();

    Session_Init(&p_Program->scenes.session);
    p_Program->scenes.session.reduce_effects = g_Th02Settings.reduce_effects;
    const OPENING_AUDIO opening_audio = ProgramAudio_Opening();
    Opening_Begin(
        &p_Program->assets.opening, &p_Program->scenes.session, &g_Th02Settings,
        &p_Program->scenes.scores, &p_Program->audio.sound,
        &p_Program->display.console, &opening_audio, 1, &g_MusicRoomRender,
        &g_HighScoreViewRender, &g_ShotSelectRender);
    MUST(Presenter_Init());
    p_Program->initialized = true;
    return OK;
}

static void M_Opening(const TH02_INPUT *const input)
{
    Palette_SetTone(0);
    TramConsole_Clear(&p_Program->display.console);
    Clip_Reset();
    p_Program->display.gaiji = p_Program->assets.opening.gaiji;
    p_Program->scenes.scene = TH02_SCENE_OPENING;
    if (p_Program->scenes.session.demo_num != 0) {
        Opening_BeginDemoReturn(GameInput_Sense(input));
    } else {
        Opening_BeginProgram();
        Opening_Update(false, GameInput_Sense(input), GameInput_LogoKey(input));
    }
}

static void M_UpdateGameOver(const TH02_INPUT *input);

static void M_UpdateRegistration(const TH02_INPUT *const input)
{
    const HIGH_SCORE_ENTRY_EVENTS event = HighScoreEntry_Update(input);
    if (event.save_section) {
        p_Program->scenes.save_scores = true;
    }
    if (event.result == HIGH_SCORE_ENTRY_COMPLETE) {
        if (p_Program->scenes.scene == TH02_SCENE_ENDING_REGISTRATION) {
            M_Opening(input);
        } else {
            GameOver_Resume();
            p_Program->scenes.scene = TH02_SCENE_GAME_OVER;
            M_UpdateGameOver(input);
        }
    }
}

static void M_Registration(const TH02_INPUT *const input, const bool ending)
{
    if (!ending) {
        Score_Set(p_Program->scenes.session.score);
    }
    Palette_SetTone(50);
    const uint32_t date = input->date != 0 ? input->date : 19000101;
    const SCORE_RUN run = {
        .score = p_Program->scenes.session.score,
        .stage = ending ? 127 : (uint8_t)(p_Program->scenes.session.stage + 1),
        .shot_type = p_Program->scenes.session.shot_type,
        .year = (int16_t)(date / 10000),
        .day = (uint8_t)(date % 100),
        .month = (uint8_t)(date / 100 % 100),
    };
    HighScoreEntry_Begin(
        &p_Program->scenes.scores.sections[p_Program->scenes.session.rank],
        &run, ending, input);
    p_Program->scenes.scene =
        ending ? TH02_SCENE_ENDING_REGISTRATION : TH02_SCENE_REGISTRATION;
    M_UpdateRegistration(input);
}

static void M_UpdateGameOver(const TH02_INPUT *const input)
{
    const GAME_OVER_EVENT event = GameOver_Update(GameInput_Sense(input));
    if (event == GAME_OVER_NAME) {
        M_Registration(input, false);
    } else if (event == GAME_OVER_RESUME || event == GAME_OVER_EXIT) {
        Gameplay_EndCredit(event == GAME_OVER_RESUME);
        if (event == GAME_OVER_RESUME) {
            p_Program->scenes.scene = TH02_SCENE_GAMEPLAY;
        } else {
            M_Opening(input);
        }
    }
}

static void M_EnterEnding(void)
{
    VSyncIrq_BeginProgram();
    Clip_Reset();
    p_Program->display.gaiji = p_Program->assets.ending.gaiji;
    HIGH_SCORE_CLEARS clears;
    HighScores_GetClears(&p_Program->scenes.scores, &clears);
    Ending_Begin(
        &p_Program->assets.ending, &p_Program->scenes.session,
        ProgramAudio_Story, nullptr, clears.extra_unlocked);
    p_Program->scenes.scene = TH02_SCENE_ENDING;
}

static void M_Ending(const bool extra)
{
    if (!extra) {
        Palette_SetTone(0);
        if (p_Program->scenes.session.continues_used == 0) {
            p_Program->scenes.save_scores =
                HighScores_RecordClear(
                    &p_Program->scenes.scores,
                    p_Program->scenes.session.shot_type, false,
                    p_Program->scenes.session.continues_used)
                >= 0;
        }
    } else {
        Score_Commit();
    }
    p_Program->scenes.session.score = HighScoreEntry_MainScore(
        g_Score->value, p_Program->scenes.session.continues_used);
    p_Program->scenes.session.highest_score =
        p_Program->scenes.session.highest_score > g_Score->value
        ? p_Program->scenes.session.highest_score
        : g_Score->value;
    p_Program->scenes.session.lives = g_Score->lives;
    p_Program->scenes.session.bombs = g_Score->bombs;
    if (p_Program->scenes.session.rank != SESSION_EXTRA_RANK) {
        p_Program->scenes.session.skill = Performance_Skill(
            p_Program->scenes.session.rank,
            p_Program->scenes.session.continues_used);
    }
    M_EnterEnding();
}

static void M_BeginBonus(const TH02_INPUT *const input)
{
    const STAGE_BONUS_RUN run = {
        .rank = p_Program->scenes.session.rank,
        .performance = g_Performance->value,
        .point_items = g_Items->point_count,
        .bombs_used = g_Bomb->stage_bombs,
        .misses = g_Miss->stage_misses,
        .start_bombs = p_Program->scenes.session.start_bombs,
        .start_lives = p_Program->scenes.session.start_lives,
        .sigma_frames = g_Stage->world.extra_boss_frames,
    };
    const STAGE_BONUS_RESULT result = StageBonus_Begin(
        &p_Program->assets.gameplay.bonus, &run,
        p_Program->scenes.session.stage == 5,
        p_Program->scenes.session.stage < 2
                || p_Program->scenes.session.stage == 5
            ? 2
            : 1,
        input);
    Stage_SetKeys(result.keys);
    p_Program->scenes.scene = TH02_SCENE_BONUS;
}

static void M_UpdateBonus(const TH02_INPUT *const input)
{
    if (!StageBonus_Update(input)) {
        return;
    }
    if (p_Program->scenes.session.stage == 5) {
        p_Program->scenes.save_scores =
            HighScores_RecordClear(
                &p_Program->scenes.scores, p_Program->scenes.session.shot_type,
                true, p_Program->scenes.session.continues_used)
            >= 0;
        Score_ClearDelta();
        p_Program->scenes.session.stage = 127;
        M_Ending(true);
        return;
    }
    Hud_BeginWipe(&p_Program->scenes.stage_leave, true);
    p_Program->scenes.scene = TH02_SCENE_STAGE_LEAVE;
}

static void M_UpdateStageLeave(void)
{
    if (!Hud_UpdateWipe(&p_Program->scenes.stage_leave)) {
        return;
    }
    Score_ClearDelta();
    const uint8_t completed_stage = p_Program->scenes.session.stage;
    p_Program->scenes.session.stage++;
    if (completed_stage < 2) {
        Sparks_SetDrift(0);
    }
    if (completed_stage == 0) {
        StageEffects_StopLightning();
    }
    ProgramAudio_Fade(40);
    Clip_Reset();
    Gameplay_BeginStage(
        &p_Program->assets.stages[p_Program->scenes.session.stage],
        &p_Program->assets.stage_patterns[p_Program->scenes.session.stage],
        &p_Program->assets.bombs[p_Program->scenes.session.shot_type]);
    p_Program->scenes.scene = TH02_SCENE_GAMEPLAY;
}

static bool M_Update(const void *const input)
{
    const TH02_INPUT *const raw = input;
    const bool edge = VSync_Advance(&p_Program->clock);
    if (edge) {
        TramDisplay_VSync(&p_Program->display.text);
        if (g_Th02Hooks.on_vsync != nullptr) {
            g_Th02Hooks.on_vsync(g_Th02Hooks.arg);
        }
        VSyncIrq_VSync();
    }
    for (size_t i = 0; i < raw->clock_count; i++) {
        if (raw->clocks[i].key == TH02_CLOCK_SCANOUT_KEY) {
            TramDisplay_Clock(
                &p_Program->display.text, raw->clocks[i].microseconds);
            break;
        }
    }
    bool text_visible = p_Program->display.text.visible;
    if (p_Program->scenes.scene == TH02_SCENE_BONUS) {
        M_UpdateBonus(raw);
    } else if (p_Program->scenes.scene == TH02_SCENE_STAGE_LEAVE) {
        M_UpdateStageLeave();
    } else if (p_Program->scenes.scene == TH02_SCENE_GAME_OVER) {
        M_UpdateGameOver(raw);
    } else if (
        p_Program->scenes.scene == TH02_SCENE_REGISTRATION
        || p_Program->scenes.scene == TH02_SCENE_ENDING_REGISTRATION) {
        M_UpdateRegistration(raw);
    } else if (p_Program->scenes.scene == TH02_SCENE_ENDING) {
        const bool music_active = p_Program->audio.sound.active;
        if (music_active) {
            AudioClock_Advance(TH02_CLOCK_MEASURE);
        }
        const uint16_t measure =
            music_active ? Pmd_Measure(p_Program->audio.pmd) : 0;
        if (Ending_Update(raw, music_active, measure)) {
            M_Registration(raw, true);
        }
    } else if (p_Program->scenes.scene == TH02_SCENE_GAMEPLAY) {
        const GAMEPLAY_RESULT result = Gameplay_Update(raw);
        if (result == GAMEPLAY_ENDING) {
            M_Ending(false);
        } else if (result == GAMEPLAY_DEMO_END) {
            M_Opening(raw);
        } else if (
            result == GAMEPLAY_CLEAR
            && (p_Program->scenes.session.stage < 4
                || p_Program->scenes.session.stage == 5)) {
            M_BeginBonus(raw);
        } else if (result != GAMEPLAY_RUNNING) {
            Score_Commit();
            p_Program->scenes.session.score = HighScoreEntry_MainScore(
                g_Score->value, p_Program->scenes.session.continues_used);
            p_Program->scenes.session.lives = g_Score->lives;
            p_Program->scenes.session.bombs = g_Score->bombs;
            p_Program->scenes.session.start_power = (int8_t)g_Player->power;
            GameOver_Begin(&p_Program->scenes.session);
            p_Program->scenes.scene = TH02_SCENE_GAME_OVER;
        }
    } else {
        const bool skip = GameInput_LogoKey(raw);
        const OPENING_EVENTS opening =
            Opening_Update(edge, GameInput_Sense(raw), skip);
        const MAIN_MENU_EVENTS events = opening.menu;
        text_visible &= !opening.hide_text;
        if (events.quit) {
            return false;
        }
        if (events.action == MAIN_MENU_START || events.action == MAIN_MENU_EXTRA
            || events.action == MAIN_MENU_DEMO) {
            const bool demo = events.action == MAIN_MENU_DEMO;
            if (demo) {
                Session_BeginDemo(
                    &p_Program->scenes.session, &g_Th02Settings, events.demo);
                Demo_ConfigureSession(&p_Program->scenes.session);
                p_Program->scenes.save_settings = true;
                Opening_DemoStarted(events.demo);
            }
            if (!demo) {
                ProgramAudio_Fade(15);
            }
            if (!demo && g_Th02Hooks.on_start != nullptr) {
                g_Th02Hooks.on_start(
                    g_Th02Hooks.arg, &p_Program->scenes.session);
            }
            if (p_Program->scenes.session.stage == 127) {
                M_EnterEnding();
            } else {
                const GAMEPLAY_MUSIC music = ProgramAudio_Gameplay(demo);
                VSyncIrq_BeginProgram();
                const TRAM_FONT font = TextFont_Tram(&p_Program->display.font);
                BombTail_Build(&font, demo, p_Program->assets.bomb_tail);
                p_Program->display.gaiji = p_Program->assets.gameplay.gaiji;
                Clip_Reset();
                Gameplay_Begin(
                    &p_Program->scenes.session, &p_Program->assets.gameplay,
                    &music,
                    demo ? &p_Program->assets.demos[events.demo - 1] : nullptr,
                    p_Program->scenes.scores
                        .sections[p_Program->scenes.session.rank]
                        .records[0]
                        .score,
                    g_Th02Settings.fix_bomb_transition_overread,
                    p_Program->assets.bomb_tail);
                Gameplay_BeginStage(
                    &p_Program->assets.stages[p_Program->scenes.session.stage],
                    &p_Program->assets
                         .stage_patterns[p_Program->scenes.session.stage],
                    &p_Program->assets
                         .bombs[p_Program->scenes.session.shot_type]);
                p_Program->scenes.scene = TH02_SCENE_GAMEPLAY;
            }
        } else if (events.action != MAIN_MENU_NONE) {
            p_Program->scenes.request = events.action;
            return false;
        }
    }
    AudioClock_End();
    if (AudioClock_Failed()) {
        return false;
    }
    const TRAM_FONT font = TextFont_Tram(&p_Program->display.font);
    Presenter_Draw(
        text_visible, &p_Program->display.gaiji, &font,
        p_Program->display.text.first_cell,
        TramDisplay_BlinkVisible(&p_Program->display.text));
    return true;
}

static bool M_FinishTick(void)
{
    if (!p_Program->initialized) {
        return true;
    }
    bool success = true;
    if (p_Program->scenes.save_scores) {
        p_Program->scenes.save_scores = false;
        success = Result_Log(
            HighScoresFile_Save(
                Shell_GetUserFiles(), &p_Program->scenes.scores),
            LOG_LEVEL_ERROR);
    }
    if (p_Program->scenes.save_settings) {
        p_Program->scenes.save_settings = false;
        Shell_RequestSaveSettings();
    }
    if (AudioClock_Failed()) {
        const RESULT error = AudioClock_TakeError();
        Result_Log(error, LOG_LEVEL_ERROR);
        success = false;
    }
    if (p_Program->scenes.request != MAIN_MENU_NONE) {
        static const char *const names[] = {
            "",        "gameplay",   "Extra",        "high scores",
            "options", "music room", "demo playback"
        };
        LOG_ERROR(
            "cannot open %s: its assets are not loaded",
            names[p_Program->scenes.request]);
        p_Program->scenes.request = MAIN_MENU_NONE;
        success = false;
    }
    p_Program->failed |= !success;
    return success;
}

bool Program_Tick(const void *const input)
{
    if (p_Program->failed) {
        return false;
    }
    TH02_INPUT raw = *(const TH02_INPUT *)input;
    M_PrepareInput(&raw);
    AudioClock_Begin(raw.clocks, raw.clock_count);
    const bool running = !AudioClock_Failed() && M_Update(&raw);
    const bool success = M_FinishTick();
    return running && success;
}

void Program_Shutdown(void)
{
    M_FinishTick();
    p_Program->initialized = false;
    if (p_Program->display.font_saved) {
        p_Program->display.gaiji = p_Program->display.saved_gaiji;
        p_Program->display.font_saved = false;
    }
    Presenter_Close();
    TextFont_Close(&p_Program->display.font);
    AudioClock_Init(nullptr, nullptr, nullptr);
    GameAudio_Free(p_Program->audio.game);
    p_Program->audio.game = nullptr;
    if (p_Program->audio.pmd != nullptr) {
        Pmd_Stop(p_Program->audio.pmd);
    }
    SoundEffects_Init(nullptr);
    p_Program->audio.pmd = nullptr;
    Arena_Free(&p_Program->assets.arena);
    Gameplay_Shutdown();
    Opening_Shutdown();
    StageBonus_Init();
    memset(
        &p_Program->scenes.stage_leave, 0,
        sizeof(p_Program->scenes.stage_leave));
    Ending_Init();
    HighScoreEntry_Init(&g_HighScoreEntryRender);
    GameOver_Init();
    p_Program->scenes.save_scores = p_Program->scenes.save_settings = false;
    memset(&p_Program->assets.gameplay, 0, sizeof(p_Program->assets.gameplay));
    memset(p_Program->assets.demos, 0, sizeof(p_Program->assets.demos));
    memset(&p_Program->assets.music, 0, sizeof(p_Program->assets.music));
    memset(
        p_Program->assets.stage_patterns, 0,
        sizeof(p_Program->assets.stage_patterns));
    memset(p_Program->assets.stages, 0, sizeof(p_Program->assets.stages));
    memset(p_Program->assets.bombs, 0, sizeof(p_Program->assets.bombs));
    memset(p_Program->assets.bomb_tail, 0, sizeof(p_Program->assets.bomb_tail));
    memset(&p_Program->assets.ending, 0, sizeof(p_Program->assets.ending));
    p_Program->scenes.scene = TH02_SCENE_OPENING;
}

void Th02_EnableLiveClock(void)
{
    p_Program->live_clock_enabled = true;
}

bool Th02_Failed(void)
{
    return p_Program->failed;
}

void Th02_View(TH02_VIEW *const out)
{
    *out = (TH02_VIEW) {
        .scene = p_Program->scenes.scene,
        .frame = p_Program->scenes.session.frame,
        .stage = p_Program->scenes.session.stage,
        .reduce_effects = p_Program->scenes.session.reduce_effects,
        .ending_program = p_Program->scenes.scene == TH02_SCENE_ENDING
            || p_Program->scenes.scene == TH02_SCENE_ENDING_REGISTRATION,
    };
}
