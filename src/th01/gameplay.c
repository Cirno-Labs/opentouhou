#include "th01/gameplay_internal.h"
#include "th01/rng.h"

#include "pc98/zun/master_rng.h"
#include "th01/scale2x.h"

#include "core/base/assert.h"

#include <string.h>

static void M_CopyPage(GAMEPLAY *const g, const int32_t from, const int32_t to)
{
    memcpy(g_Video.pages[to], g_Video.pages[from], VIDEO_PAGE_SIZE);
}

static void M_Sense(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    if (GameInput_SenseGame(raw, false)) {
        g_Player.is_hit = true;
    }
}

static void M_Draw(
    GAMEPLAY *const g, const char *const text, const int32_t left,
    const int32_t top, const uint8_t color, const GAME_TEXT_WEIGHT weight,
    const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .clear_background = clear };
    GameText_Draw(
        &g->storage.assets->menu_font, text, left, top, color, &style);
}

static void M_Line(GAMEPLAY *const g, const char *const text)
{
    const GAME_TEXT_GLYPH *glyphs[5];
    int32_t count = 0;
    for (; text[count] != '\0'; count++) {
        ASSERT(count < 5);
        glyphs[count] =
            GameText_Find(&g->storage.assets->menu_font, (uint8_t)text[count]);
    }
    Tram_PutLine16(glyphs, count, TRAM_RED, TRAM_BLACK);
    Tram_Fill((GFX_RECT) { 0, 336, 640, 64 }, TRAM_BLACK);
}

static void M_TitleChar(GAMEPLAY *const g)
{
    const uint32_t chars[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    Sound_Effect(14);
    Tram_PutKanji16Reverse(GameText_Find(
        &g->storage.assets->menu_font, chars[g->world.animation_frame]));
}

static void M_StageBackground(GAMEPLAY *const g)
{
    const GAMEPLAY_ASSETS *const a = g->storage.assets;
    const PI_IMAGE *background = &a->backgrounds[Session_Scene()];
    switch (g->world.boss) {
    case BOSS_SINGYOKU:
        background = &a->b05.background;
        break;
    case BOSS_YUUGENMAGAN:
        background = &a->b10m.background;
        break;
    case BOSS_MIMA:
        background = &a->b10j.background;
        break;
    case BOSS_KIKURI:
        background = &a->b15j.background;
        break;
    case BOSS_ELIS:
        background = &a->b15m.background;
        break;
    default:
        break;
    }
    Video_SelectDrawingPage(0);
    if (Gameplay_FirstStage()) {
        memcpy(g_Video.pages[0], background->pixels, VIDEO_PAGE_SIZE);
        Palette_Import(&g_StagePalette, background->palette);
        Palette_Set(&g_Palette, &g_StagePalette);
        M_CopyPage(g, 0, 1);
    } else {
        M_CopyPage(g, 1, 0);
        Sprite_PtnPut(&a->miko, 0, -1, g_Player.left, PLAYER_TOP);
    }
    StageObjects_InitPrepared(
        &g_Scene.active->layouts, g_Session.stage_id, Gameplay_FirstStage(),
        g->bindings.stage.sprites);
    g->world.phase = GP_OBJECT_ENTRANCE;
}

static void M_StageBegin(GAMEPLAY *const g)
{
    Pellets_EraseAndResetMoving();
    Shots_EraseAndReset(g->bindings.shots_output);
    const int16_t stage = g_Session.stage_id;
    g->world.boss_stage = stage % 5 == 4;
    g->world.boss = stage == 4 ? BOSS_SINGYOKU
        : stage == 9  ? (g_Session.route == 0 ? BOSS_YUUGENMAGAN : BOSS_MIMA)
        : stage == 14 ? (g_Session.route == 0 ? BOSS_ELIS : BOSS_KIKURI)
        : stage == 19 ? (g_Session.route == 0 ? BOSS_SARIEL : BOSS_KONNGARA)
                      : BOSS_NONE;
    Gameplay_Bind(g);
    g_Player.invincible = false;
    g_Player.invincibility_time = 0;
    Gameplay_BossSetup(g);
    if (g->world.boss_stage) {
        Gameplay_SetFirstStage(true);
        Items_Reset();
    }
    if (g->world.boss == BOSS_MIMA) {
        g->world.phase = GP_B10J_SETUP;
    } else if (g->world.boss == BOSS_SARIEL) {
        B20M_BeginEntrance(&g->storage.assets->b20m, g->bindings.shots_output);
        g->world.phase = GP_FINAL_ENTRANCE;
    } else if (g->world.boss == BOSS_KONNGARA) {
        B20J_BeginEntrance(&g->storage.assets->b20j);
        g->world.phase = GP_FINAL_ENTRANCE;
    } else {
        M_StageBackground(g);
    }
}

static void M_StageReady(GAMEPLAY *const g)
{
    StageTimer_Redraw(
        Gameplay_FirstStage(), Gameplay_TimerInitialized(), g_Session.route);
    g_Stage.maximum_combo = 0;
    g_Orb.in_portal = false;
    g_Player.bomb_frame = 0;
    g->world.frame = 0;
    Orb_ResetFlight();
    if (g_Session.stage_id % 5 == 0 || g->world.boss_stage) {
        g_Player.left = 304;
    }
    g_Player.deflecting = false;
    g_Player.bomb_damaging = false;
    g_Stage.combo = 0;
    g->world.phase = GP_LIFE_BEGIN;
}

static void M_LifeBegin(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    Player_Reset(g_Session.settings.difficulty);
    Gameplay_DefaultActors(g);
    g_GameInput.horizontal = 0;
    g_GameInput.shot = false;
    g_GameInput.ok = false;
    g_GameInput.paused = false;
    Hud_ScoreAndCombo();
    g_GameInput.bomb_frame = 60;
    Stage_UpdateObstacles(&g->bindings.stage, true);
    if (!g->world.bgm_started && g->world.boss < BOSS_SARIEL) {
        const size_t song = g->world.boss == BOSS_SINGYOKU ? 7
            : g->world.boss == BOSS_YUUGENMAGAN || g->world.boss == BOSS_MIMA
            ? 8
            : g->world.boss == BOSS_KIKURI ? 9
            : g->world.boss == BOSS_ELIS   ? 10
                                           : Session_Scene();
        Sound_Load(song);
        Sound_Play();
        g->world.bgm_started = true;
    }
    Gameplay_ResetInput(g);
    if (g_Player.invincibility_time > 1) {
        g_Player.invincible = true;
    }
    if (g->world.boss == BOSS_SINGYOKU || g->world.boss == BOSS_MIMA) {
        g->world.phase = GP_LIFE_BOSS;
    } else {
        if (g->world.boss == BOSS_KIKURI) {
            g_GameInput.strike = false;
        }
        g->world.phase = GP_WAIT_SHOT;
    }
    if (g->world.phase == GP_LIFE_BOSS && Gameplay_BossTick(g, raw)) {
        g->world.phase = GP_WAIT_SHOT;
    }
}

static void M_StartFrames(GAMEPLAY *const g)
{
    g->world.wait_for_shot = false;
    g_GameInput.shot = false;
    Gameplay_SetTimerInitialized(true);
    g_Rng = g_Session.frame_rand;
    g_GameInput.bomb_frame = GAME_INPUT_BOMB_WINDOW;
    Gameplay_SetFirstStage(false);
    g->world.phase = GP_FRAME_BEGIN;
}

static void M_PlayerBegin(GAMEPLAY *const g, const GAMEPLAY_PHASE after)
{
    g->world.after_player = after;
    if (Player_BeginUpdate(&g->world.player_bomb, g->bindings.player_output)) {
        const BOMB_UPDATE result = Bomb_BeginUpdate(
            &g->storage.assets->miko, &g->storage.assets->bomb,
            (int16_t)g_Player.bomb_frame);
        if (result == BOMB_UPDATE_WAIT) {
            g->world.phase = GP_PLAYER_BOMB;
            return;
        }
        Player_FinishBombUpdate(
            &g->world.player_bomb, result == BOMB_UPDATE_DONE);
    }
    g->world.phase = after;
}

static void M_PauseDraw(GAMEPLAY *const g, const bool quit)
{
    M_Draw(
        g, quit ? "本当に終了しちゃうの？" : "ＰＡＵＳＥ", quit ? 224 : 272,
        128, 7, GAME_TEXT_BLACK, false);
    M_Draw(
        g, quit ? "　うそですぅ　　はいっ" : "　再開　　終了", quit ? 224 : 256,
        144, 7, GAME_TEXT_BLACK, false);
    M_Draw(g, "●", quit ? 224 : 256, 144, 7, GAME_TEXT_NORMAL, false);
    g->world.menu_selection = 0;
    GameInput_ResetMenu();
    g->world.waiting = false;
}

static void M_PauseBegin(GAMEPLAY *const g)
{
    M_PauseDraw(g, false);
    PALETTE dark = g_Palette.saved;
    for (int32_t i = 0; i < 16; i++) {
        if (i == 7) {
            continue;
        }
        for (int32_t c = 0; c < 3; c++) {
            dark.colors[i][c] = (int8_t)(dark.colors[i][c] * 40 / 100);
        }
    }
    Palette_Set(&g_Palette, &dark);
    g->world.phase = GP_PAUSE;
}

static void M_ContinueUse(GAMEPLAY *const g)
{
    g_Session.continues_total =
        (int32_t)((uint32_t)g_Session.continues_total + 1);
    const int16_t scene = g_Session.stage_id / 5;
    g_Session.continues_per_scene[scene]++;
}

// Starts game_switch_binary: its vsync_exit stops the counters, and its
// z_palette_black_out steps the palette to black after each of sixteen
// VSyncs. The switch to then follows the last step at once.
static void M_BeginSwitch(GAMEPLAY *const g, const GAMEPLAY_SWITCH to)
{
    g->world.switch_to = to;
    Palette_BeginFade(
        &g->world.switch_fade, &g_Palette, PALETTE_FADE_BLACK_OUT);
    Frame_BeginWait(&g->world.wait);
    g->world.phase = GP_SWITCH;
}

static void M_Switch(GAMEPLAY *const g)
{
    switch (g->world.switch_to) {
    case GAMEPLAY_SWITCH_STAGE:
        Gameplay_ResetWorld(g);
        Sound_Load(11);
        Sound_Play();
        return;
    case GAMEPLAY_SWITCH_CONTINUE:
        g_Session.point_value = 0;
        g_Session.pellet_speed = g_PelletShared.speed;
        g_Session.lives = g_Extend.resident_lives;
        Gameplay_ResetWorld(g);
        g->world.bgm_started = true;
        return;
    case GAMEPLAY_SWITCH_OPENING:
        g->world.request = GAMEPLAY_REQUEST_OPENING;
        g->world.phase = GP_TERMINAL;
        return;
    case GAMEPLAY_SWITCH_ENDING:
        g->world.request = GAMEPLAY_REQUEST_ENDING;
        g->world.phase = GP_TERMINAL;
        return;
    }
}

static void M_RequestOpening(GAMEPLAY *const g)
{
    M_BeginSwitch(g, GAMEPLAY_SWITCH_OPENING);
}

static void M_OpenMenu(GAMEPLAY *const g)
{
    Gameplay_SyncSession(g);
    M_RequestOpening(g);
}

static bool M_PausePoll(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const bool quit)
{
    for (;;) {
        if (g->world.waiting && !Frame_DelayReady(1)) {
            return false;
        }
        g->world.waiting = false;
        if (g_GameInput.paused) {
            M_Sense(g, raw);
            if (g_Player.is_hit) {
                M_OpenMenu(g);
                return true;
            }
            if (g_GameInput.horizontal == GAME_INPUT_LEFT
                || g_GameInput.horizontal == GAME_INPUT_RIGHT) {
                const int32_t left = quit ? 224 : 256;
                const int32_t second = quit ? 336 : 320;
                const bool select_second =
                    g_GameInput.horizontal == GAME_INPUT_RIGHT;
                Sprite_Restore16(
                    (int16_t)(select_second ? left : second), 144, 16, 16);
                M_Draw(
                    g, "●", select_second ? second : left, 144, 7,
                    GAME_TEXT_NORMAL, false);
                g->world.menu_selection = select_second;
            }
            if (!g_GameInput.shot && !g_GameInput.ok) {
                g->world.waiting = true;
                if (!Frame_DelayReady(1)) {
                    return false;
                }
                g->world.waiting = false;
                continue;
            }
        }
        if (quit) {
            if (g->world.menu_selection != 0) {
                M_ContinueUse(g);
                M_OpenMenu(g);
            } else {
                Palette_Set(&g_Palette, &g_StagePalette);
                Gameplay_ResetInput(g);
                Sprite_Restore16(232, 128, 176, 32);
                g->world.phase = GP_AFTER_ORB;
            }
        } else {
            Sprite_Restore16(276, 128, 80, 16);
            Sprite_Restore16(256, 144, 112, 16);
            g->world.phase = GP_PAUSE_DELAY;
        }
        return true;
    }
}

static void M_BeginGameover(GAMEPLAY *const g)
{
    Items_Reset();
    Orb_RenderAction(
        g->bindings.stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g_Orb.left,
            .top = g_Orb.top,
            .image = 0,
        });
    const int16_t image = MasterRng_Next(&g_Rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(
        &g->storage.assets->miko, image, -1, g_Player.left, PLAYER_TOP);
    for (int32_t i = 0; i < 5; i++) {
        g->world.gameover_left[i] = g_Player.left;
        g->world.gameover_top[i] = PLAYER_TOP;
        g->world.gameover_clipped[i] = false;
    }
    M_CopyPage(g, 0, 1);
    g->world.animation_frame = 0;
    Frame_BeginWait(&g->world.wait);
    g->world.phase = GP_GAMEOVER;
}

static void M_AfterBonus(GAMEPLAY *const g)
{
    if (g_PelletShared.speed < 0) {
        g_PelletShared.speed = 0;
    }
    Gameplay_SyncSession(g);
    if (g_Session.stage_id % 5 == 4 || g->world.boss != BOSS_NONE) {
        Sound_Fade();
        M_BeginSwitch(g, GAMEPLAY_SWITCH_STAGE);
    } else {
        g_Orb.in_portal = false;
        Video_SelectDrawingPage(1);
        StageObjects_PutAllBackgrounds();
        Video_SelectDrawingPage(0);
        g->world.phase = GP_STAGE_BEGIN;
    }
}

static void M_ContinueMenu(GAMEPLAY *const g)
{
    Scale2x_Copy(288, 200, 0, g->world.menu_selection == 0 ? 16 : 48, 48, 16);
    Scale2x_Copy(288, 232, 0, g->world.menu_selection == 0 ? 64 : 32, 48, 16);
}

static GAMEPLAY_REQUEST M_Run(
    GAMEPLAY *g, const TH01_INPUT *raw, uint64_t host_tick);

void Gameplay_ResumeAfterScore(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const uint64_t host_tick)
{
    ASSERT(g->world.request == GAMEPLAY_REQUEST_SCORE);
    g->world.request = GAMEPLAY_REQUEST_NONE;
    Video_SelectDrawingPage(1);
    M_CopyPage(g, 1, 0);
    const char *const lines[] = {
        "ｃｏｎｔｉｎｕｅ？　　　  ",
        "Ｙｅｓ　　  　",
        "Ｎｏ　　  　 ",
        "Ｙｅｓ　　　  ",
        "Ｎｏ　　　   ",
    };
    for (int32_t i = 0; i < 5; i++) {
        M_Draw(g, lines[i], 0, i * 16, i < 3 ? 7 : 1, GAME_TEXT_BOLD, true);
    }
    Video_SelectDrawingPage(0);
    Scale2x_Copy(160, 64, 0, 0, 144, 16);
    g->world.menu_selection = 0;
    M_ContinueMenu(g);
    Gameplay_ResetInput(g);
    M_ContinueUse(g);
    if (g_Session.highest_score < g_Stage.score) {
        g_Session.highest_score = g_Stage.score;
    }
    g_Session.score = 0;
    // Restores the resident's lives; the live count stays until the re-exec.
    g_Extend.resident_lives = (int8_t)(g_Session.settings.lives_extra + 2);
    Pellets_SpeedLower(-2, -5);
    g->world.continue_frame = 0;
    g->world.continue_locked = g_Session.settings.fix_debouncing;
    g->world.phase = GP_CONTINUE;
    g->storage.host_tick = host_tick;
    M_Run(g, raw, host_tick);
}

static GAMEPLAY_REQUEST M_Run(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const uint64_t host_tick)
{
    for (;;) {
        switch (g->world.phase) {
        case GP_STAGE_BEGIN:
            M_StageBegin(g);
            break;
        case GP_B10J_SETUP:
            if (!B10J_ResumeSetup(
                    &g->storage.assets->b10j, g->bindings.shots_output,
                    &g->storage.assets->miko)) {
                return g->world.request;
            }
            Gameplay_HookBossHp(g);
            M_StageBackground(g);
            break;
        case GP_FINAL_ENTRANCE:
            if (g->world.boss == BOSS_SARIEL
                    ? !B20M_ResumeEntrance(
                          &g->storage.assets->b20m, g->bindings.shots_output,
                          host_tick)
                    : !B20J_ResumeEntrance(&g->storage.assets->b20j)) {
                return g->world.request;
            }
            g_StagePalette = g->world.boss == BOSS_KONNGARA
                ? g_B20J.stage_palette
                : g_B20M.stage_palette;
            M_StageReady(g);
            break;
        case GP_OBJECT_ENTRANCE:
            if (!StageObjects_ResumeEntrance(g->bindings.stage.sprites)) {
                return g->world.request;
            }
            if (Gameplay_FirstStage()) {
                M_CopyPage(g, 0, 1);
            } else {
                StageObjects_CopyToBackground();
                Sprite_PtnPut(
                    &g->storage.assets->miko, 0, -1, g_Player.left, PLAYER_TOP);
                Items_Render(g->bindings.stage.sprites);
            }
            if (g->world.boss == BOSS_KIKURI) {
                Video_Clear(0);
            }
            g->world.animation_frame = 0;
            if (g_Session.stage_id % 5 == 0 || g->world.boss_stage) {
                M_TitleChar(g);
                g->world.phase = GP_TITLE;
            } else {
                M_Line(g, "STAGE");
                g->world.phase = GP_STAGE_LABEL;
            }
            break;
        case GP_TITLE:
            if (!Frame_DelayReady(8)) {
                return g->world.request;
            }
            if (++g->world.animation_frame < 6) {
                M_TitleChar(g);
            } else {
                M_Line(g, "STAGE");
                g->world.phase = GP_STAGE_LABEL;
            }
            break;
        case GP_STAGE_LABEL:
            if (!Frame_DelayReady(35)) {
                return g->world.request;
            }
            {
                const uint32_t number = (uint32_t)(g_Session.stage_id + 1);
                const GAME_TEXT_GLYPH *glyphs[] = {
                    GameText_Find(
                        &g->storage.assets->menu_font, '0' + number / 10),
                    GameText_Find(
                        &g->storage.assets->menu_font, '0' + number % 10),
                };
                Tram_PutLine16(glyphs, 2, TRAM_RED, TRAM_BLACK);
            }
            g->world.phase = GP_STAGE_NUMBER;
            break;
        case GP_STAGE_NUMBER:
            if (!Frame_DelayReady(35)) {
                return g->world.request;
            }
            Tram_Clear();
            M_StageReady(g);
            break;
        case GP_LIFE_BEGIN:
            M_LifeBegin(g, raw);
            break;
        case GP_LIFE_BOSS:
            if (!Gameplay_BossTick(g, raw)) {
                return g->world.request;
            }
            g->world.phase = GP_WAIT_SHOT;
            break;
        case GP_WAIT_SHOT:
            if (g->world.boss != BOSS_NONE || !g->world.wait_for_shot
                || g_GameInput.shot) {
                M_StartFrames(g);
            } else {
                M_Sense(g, raw);
                M_PlayerBegin(g, GP_WAIT_SHOT_DELAY);
            }
            break;
        case GP_WAIT_SHOT_DELAY:
            if (!Frame_DelayReady(1)) {
                return g->world.request;
            }
            g_Player.bomb_frame++;
            g->world.phase = GP_WAIT_SHOT;
            break;
        case GP_FRAME_BEGIN:
            if (g_Player.is_hit) {
                Gameplay_SetTimerInitialized(false);
                Frame_BeginWait(&g->world.wait);
                g->world.phase = GP_LIFE_EXIT;
                break;
            }
            g_Session.frame_rand++;
            {
                const int32_t period = 1800 - g_Session.lives * 200
                    - g_Player.remaining_bombs * 50;
                if (period != 0
                    && g_Session.frame_rand % (uint32_t)period == 0) {
                    Pellets_SpeedRaise(1);
                }
            }
            M_Sense(g, raw);
            if (g_Player.invincibility_time > 1) {
                g_Player.invincibility_time--;
                g_Player.invincible = true;
            } else if (g_Player.invincibility_time == 1) {
                g_Player.invincible = false;
                g_Player.invincibility_time = 0;
            }
            M_PlayerBegin(g, GP_ITEMS);
            break;
        case GP_PLAYER_BOMB: {
            const BOMB_UPDATE result = Bomb_ResumeUpdate();
            if (result == BOMB_UPDATE_WAIT) {
                return g->world.request;
            }
            Player_FinishBombUpdate(
                &g->world.player_bomb, result == BOMB_UPDATE_DONE);
            g->world.phase = g->world.after_player;
            break;
        }
        case GP_ITEMS:
            Items_UpdateAndRender(&g->bindings.item);
            g->world.frame++;
            Orb_AdvanceForceFrame();
            g_Player.bomb_frame++;
            GameInput_AdvanceBombFrame();
            g->world.phase =
                (g_Session.frame_rand & 3) == 0 ? GP_TIMER : GP_BOSS;
            break;
        case GP_TIMER:
            if (!StageTimer_TickAndPut()) {
                return g->world.request;
            }
            g->world.phase = GP_BOSS;
            break;
        case GP_BOSS:
            if (!g->world.waiting) {
                Invincibility_UpdateAndRender(
                    &g->world.invincibility, &g->storage.assets->invincibility);
                g->world.waiting = true;
            }
            if (!Gameplay_BossTick(g, raw)) {
                return g->world.request;
            }
            g->world.waiting = false;
            ShootoutLasers_UpdateAll(g->storage.assets->lasers);
            g->world.phase = GP_ORB;
            if (!Stage_BeginOrbComposite(&g->bindings.stage)) {
                return g->world.request;
            }
            break;
        case GP_ORB:
            if (!Stage_ResumeOrbComposite(&g->bindings.stage)) {
                return g->world.request;
            }
            if (g_GameInput.paused) {
                M_PauseBegin(g);
            } else {
                g->world.phase = GP_AFTER_ORB;
            }
            break;
        case GP_PAUSE:
            if (!M_PausePoll(g, raw, false)) {
                return g->world.request;
            }
            break;
        case GP_PAUSE_DELAY:
            if (!Frame_DelayReady(20)) {
                return g->world.request;
            }
            GameInput_ResetMenu();
            if (g->world.menu_selection != 0 && g_GameInput.paused) {
                M_PauseDraw(g, true);
                g->world.phase = GP_QUIT_CONFIRM;
            } else {
                Palette_Set(&g_Palette, &g_StagePalette);
                Gameplay_ResetInput(g);
                Sprite_Restore16(232, 128, 176, 32);
                g->world.phase = GP_AFTER_ORB;
            }
            break;
        case GP_QUIT_CONFIRM:
            if (!M_PausePoll(g, raw, true)) {
                return g->world.request;
            }
            break;
        case GP_AFTER_ORB:
            Extend_UpdateAndRender();
            if (Gameplay_GameCleared()) {
                Gameplay_SyncSession(g);
                g_Session.ending = g_Session.route == 0 ? SESSION_END_MAKAI
                                                        : SESSION_END_JIGOKU;
                if (g_Stage.score > g_Session.highest_score) {
                    g_Session.highest_score = g_Stage.score;
                }
                g->world.phase = GP_VICTORY_DELAY;
            } else {
                if (g_PelletShared.destroy_score_delta != 0) {
                    g_Stage.score += g_PelletShared.destroy_score_delta;
                    Hud_ScoreAndCombo();
                    g_PelletShared.destroy_score_delta = 0;
                }
                g->world.phase = GP_FRAME_BEGIN;
            }
            break;
        case GP_VICTORY_DELAY:
            if (!Frame_DelayReady(120)) {
                return g->world.request;
            }
            M_BeginSwitch(g, GAMEPLAY_SWITCH_ENDING);
            break;
        case GP_LIFE_EXIT:
            if (!Frame_WaitReady(&g->world.wait)) {
                return g->world.request;
            }
            Video_SetScroll(0);
            g_Session.exit_rand = g_Session.frame_rand;
            g_TestDamage = false;
            g_Player.bomb_frame = 200;
            if (g_Stage.cleared) {
                g_Stage.cleared = false;
                g_Player.is_hit = false;
                if (g->world.boss != BOSS_NONE) {
                    g->world.wait_for_shot = true;
                    Gameplay_SetFirstStage(true);
                }
                g_Session.stage_id++;
                const int16_t finished = g_Session.stage_id;
                if (g->world.boss != BOSS_NONE) {
                    g->world.phase = GP_TOTAL_BONUS;
                    if (Bonus_BeginTotal(
                            &g->bindings.total_context, finished, raw,
                            host_tick)) {
                        M_AfterBonus(g);
                    }
                } else {
                    g->world.phase = GP_STAGE_BONUS;
                    if (Bonus_BeginStage(
                            &g->bindings.bonus_context, finished, raw)) {
                        M_AfterBonus(g);
                    }
                }
            } else if (g_Session.lives <= 0) {
                M_BeginGameover(g);
            } else {
                Sound_Effect(5);
                g_Session.lives--;
                g_Extend.resident_lives = (int8_t)g_Session.lives;
                g->bindings.missing.remaining_lives = g_Session.lives;
                Miss_Begin(&g->bindings.missing);
                g->world.phase = GP_MISS;
            }
            break;
        case GP_MISS:
            if (!Miss_Resume(&g->bindings.missing)) {
                return g->world.request;
            }
            g_Player.is_hit = false;
            g_Player.invincibility_time = 150;
            g->world.phase = GP_LIFE_BEGIN;
            break;
        case GP_STAGE_BONUS:
            if (!Bonus_Resume(&g->bindings.bonus_context, raw)) {
                return g->world.request;
            }
            M_AfterBonus(g);
            break;
        case GP_TOTAL_BONUS:
            if (!Bonus_ResumeTotal(
                    &g->bindings.total_context, raw, host_tick)) {
                return g->world.request;
            }
            M_AfterBonus(g);
            break;
        case GP_GAMEOVER:
            if (!Frame_WaitReady(&g->world.wait)) {
                return g->world.request;
            }
            Video_SetScroll((int16_t)(400 - g->world.animation_frame % 2 * 8));
            const int32_t frame = g->world.animation_frame;
            // Model the clipped-flag overread through the adjacent top and
            // left arrays. The caller's nonzero saved BP suppresses frame 15.
            const bool restore = frame < 5 ? !g->world.gameover_clipped[frame]
                : frame < 10 ? g->world.gameover_top[frame - 5] == 0
                : frame < 15 ? g->world.gameover_left[frame - 10] == 0
                             : false;
            if (restore) {
                for (int32_t i = 0; i < 5; i++) {
                    Sprite_Restore16(
                        g->world.gameover_left[i], PLAYER_TOP, 32, 32);
                }
            }
            g->world.gameover_left[0] -= 8;
            g->world.gameover_left[1] -= 8;
            g->world.gameover_left[3] += 8;
            g->world.gameover_left[4] += 8;
            g->world.gameover_top[1] -= 8;
            g->world.gameover_top[2] -= 8;
            g->world.gameover_top[3] -= 8;
            for (int32_t i = 0; i < 5; i++) {
                if (g->world.gameover_left[i] >= 608
                    || g->world.gameover_left[i] < 0) {
                    g->world.gameover_clipped[i] = true;
                }
                if (!g->world.gameover_clipped[i]) {
                    Sprite_PtnPut(
                        &g->storage.assets->miko,
                        20 + g->world.animation_frame % 2, -1,
                        g->world.gameover_left[i], g->world.gameover_top[i]);
                }
            }
            g->world.phase = GP_GAMEOVER_DELAY;
            break;
        case GP_GAMEOVER_DELAY:
            if (!Frame_DelayReady(g->world.animation_frame / 6)) {
                return g->world.request;
            }
            g->world.animation_frame++;
            Frame_BeginWait(&g->world.wait);
            g->world.phase =
                g->world.animation_frame < 16 ? GP_GAMEOVER : GP_GAMEOVER_END;
            break;
        case GP_GAMEOVER_END:
            if (!Frame_WaitReady(&g->world.wait)) {
                return g->world.request;
            }
            Video_SetScroll(0);
            g_Player.remaining_bombs = 1;
            Shots_EraseAndReset(g->bindings.shots_output);
            Pellets_EraseAndResetMoving();
            g_Extend.next = 1;
            Gameplay_SyncSession(g);
            g->world.request = GAMEPLAY_REQUEST_SCORE;
            g->world.phase = GP_TERMINAL;
            break;
        case GP_CONTINUE:
            M_Sense(g, raw);
            g->world.continue_frame++;
            g->world.phase = GP_CONTINUE_DELAY;
            break;
        case GP_CONTINUE_DELAY:
            if (!Frame_DelayReady(1)) {
                return g->world.request;
            }
            const bool confirm = g_GameInput.ok || g_GameInput.shot;
            g->world.continue_locked &= confirm;
            if (confirm && !g->world.continue_locked) {
                if (g->world.menu_selection == 0) {
                    M_BeginSwitch(g, GAMEPLAY_SWITCH_CONTINUE);
                } else {
                    g_Player.is_hit = false;
                    g_GameInput.paused = false;
                    Sound_Stop();
                    M_RequestOpening(g);
                }
            } else if (g->world.continue_frame > 3000 || g_GameInput.paused) {
                g_Player.is_hit = false;
                g_GameInput.paused = false;
                if (g->world.continue_frame > 3000) {
                    Sound_Stop();
                }
                M_RequestOpening(g);
            } else {
                if (g_GameInput.up) {
                    g->world.menu_selection = 0;
                    M_ContinueMenu(g);
                }
                if (g_GameInput.down) {
                    g->world.menu_selection = 1;
                    M_ContinueMenu(g);
                }
                g->world.phase = GP_CONTINUE;
            }
            break;
        case GP_SWITCH:
            if (!Frame_WaitReady(&g->world.wait)) {
                return g->world.request;
            }
            Palette_FadeStep(&g->world.switch_fade, &g_Palette);
            if (g->world.switch_fade.remaining > 0) {
                Frame_BeginWait(&g->world.wait);
            } else {
                M_Switch(g);
            }
            break;
        case GP_TERMINAL:
            return g->world.request;
        }
    }
}

void Gameplay_Begin(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const uint64_t host_tick)
{
    Gameplay_ResetWorld(g);
    Sound_Load(11);
    Sound_Play();
    g->storage.host_tick = host_tick;
    M_Run(g, raw, host_tick);
}

GAMEPLAY_REQUEST Gameplay_Tick(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const bool reference_edge,
    const uint64_t host_tick)
{
    g->storage.host_tick = host_tick;
    Gameplay_AdvanceAudio(g);
    if (reference_edge && g->world.phase == GP_SWITCH) {
        Frame_PollVSync();
    } else if (reference_edge) {
        Frame_VSync();
    }
    return M_Run(g, raw, host_tick);
}
