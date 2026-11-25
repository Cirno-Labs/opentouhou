#include "th01/gameplay_internal.h"
#include "th01/rng.h"
#include "th01/hooks.h"
#include "th01/scale2x.h"

#include "pc98/zun/master_rng.h"
#include "core/base/assert.h"

#include <string.h>

GAMEPLAY_WORLD g_Gameplay;

static const GAMEPLAY_ASSETS *m_Assets;
static uint64_t m_HostTick;

static const PTN *M_StageSprites(void)
{
    const GAMEPLAY_ASSETS *const a = m_Assets;
    return g_Gameplay.boss_stage ? &a->boss_stage : &a->stage;
}

static PLAYER_SPRITES M_PlayerSprites(void)
{
    const GAMEPLAY_ASSETS *const a = m_Assets;
    return (PLAYER_SPRITES) {
        .miko = &a->miko,
        .miko_ac = &a->miko_ac,
        .miko_ac2 = &a->miko_ac2,
    };
}

static void M_ResetInput(void)
{
    GameInput_ResetGame();
}

static void M_DefaultActors(void)
{
    Sprite_PtnPut(&m_Assets->miko, 0, -1, g_Player.left, PLAYER_TOP);
    Orb_Put(M_StageSprites(), g_Orb.left, g_Orb.top, 0);
}

static void M_SyncSession(void)
{
    g_Session.score = g_Stage.score;
    g_Session.bombs = g_Player.remaining_bombs;
    g_Session.pellet_speed = g_PelletShared.speed;
}

static void M_HookBossHp(void)
{
    int16_t *const hp[] = {
        [BOSS_NONE] = nullptr,           [BOSS_SINGYOKU] = &g_B05.hp,
        [BOSS_YUUGENMAGAN] = &g_B10M.hp, [BOSS_MIMA] = &g_B10J.hp,
        [BOSS_KIKURI] = &g_B15J.hp,      [BOSS_ELIS] = &g_B15M.hp,
        [BOSS_SARIEL] = &g_B20M.hp,      [BOSS_KONNGARA] = &g_B20J.hp,
    };
    Th01Hooks_CallOnBoss(hp[g_Gameplay.boss]);
}

static void M_BossSetup(void)
{
    switch (g_Gameplay.boss) {
    case BOSS_SINGYOKU:
        B05_Setup(&m_Assets->b05);
        break;
    case BOSS_YUUGENMAGAN:
        B10M_Setup(&m_Assets->b10m, &m_Assets->miko);
        break;
    case BOSS_MIMA:
        B10J_BeginSetup(&m_Assets->b10j, &m_Assets->miko);
        break;
    case BOSS_KIKURI:
        B15J_Setup(&m_Assets->b15j, &m_Assets->miko);
        break;
    case BOSS_ELIS:
        B15M_Setup(&m_Assets->b15m, &m_Assets->miko);
        break;
    case BOSS_SARIEL:
        B20M_Setup(&m_Assets->b20m);
        break;
    case BOSS_KONNGARA:
        B20J_Setup(&m_Assets->b20j);
        break;
    default:
        break;
    }
    if (g_Gameplay.boss != BOSS_NONE && g_Gameplay.boss != BOSS_MIMA) {
        M_HookBossHp();
    }
}

static bool M_BossTick(const TH01_INPUT *const raw)
{
    switch (g_Gameplay.boss) {
    case BOSS_SINGYOKU: {
        const GAMEPLAY_ASSETS *const a = m_Assets;
        const ROUTE_SELECT_ASSETS route = { .font = &a->route_font,
                                            .stage = M_StageSprites(),
                                            .makai = &a->scenes[1],
                                            .jigoku = &a->scenes[2] };
        return B05_UpdateAndRender(&a->miko, &route, raw);
    }
    case BOSS_YUUGENMAGAN:
        return B10M_UpdateAndRender(&m_Assets->b10m, &m_Assets->miko);
    case BOSS_MIMA:
        return B10J_UpdateAndRender(&m_Assets->b10j, &m_Assets->miko);
    case BOSS_KIKURI:
        return B15J_UpdateAndRender(
            &m_Assets->b15j, &m_Assets->miko, M_StageSprites());
    case BOSS_ELIS:
        return B15M_UpdateAndRender(
            &m_Assets->b15m, &m_Assets->miko, m_HostTick);
    case BOSS_SARIEL:
        return B20M_UpdateAndRender(&m_Assets->b20m, m_HostTick);
    case BOSS_KONNGARA:
        return B20J_UpdateAndRender(&m_Assets->b20j);
    default:
        return true;
    }
}

static void M_CopyPage(const int32_t from, const int32_t to)
{
    memcpy(g_Video.pages[to], g_Video.pages[from], VIDEO_PAGE_SIZE);
}

static void M_Sense(const TH01_INPUT *const raw)
{
    if (GameInput_SenseGame(raw, false)) {
        g_Player.is_hit = true;
    }
}

static void M_Draw(
    const char *const text, const int32_t left, const int32_t top,
    const uint8_t color, const GAME_TEXT_WEIGHT weight, const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .clear_background = clear };
    GameText_Draw(&m_Assets->menu_font, text, left, top, color, &style);
}

static void M_Line(const char *const text)
{
    const GAME_TEXT_GLYPH *glyphs[5];
    int32_t count = 0;
    for (; text[count] != '\0'; count++) {
        ASSERT(count < 5);
        glyphs[count] =
            GameText_Find(&m_Assets->menu_font, (uint8_t)text[count]);
    }
    Tram_PutLine16(glyphs, count, TRAM_RED, TRAM_BLACK);
    Tram_Fill((GFX_RECT) { 0, 336, 640, 64 }, TRAM_BLACK);
}

static void M_TitleChar(void)
{
    const uint32_t chars[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    Sound_Effect(14);
    Tram_PutKanji16Reverse(
        GameText_Find(&m_Assets->menu_font, chars[g_Gameplay.animation_frame]));
}

static void M_StageBackground(void)
{
    const GAMEPLAY_ASSETS *const a = m_Assets;
    const PI_IMAGE *background = &a->backgrounds[Session_Scene()];
    switch (g_Gameplay.boss) {
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
    if (g_Stage.first_stage) {
        memcpy(g_Video.pages[0], background->pixels, VIDEO_PAGE_SIZE);
        Palette_Import(&g_StagePalette, background->palette);
        Palette_Set(&g_Palette, &g_StagePalette);
        M_CopyPage(0, 1);
    } else {
        M_CopyPage(1, 0);
        Sprite_PtnPut(&a->miko, 0, -1, g_Player.left, PLAYER_TOP);
    }
    Scene_BeginStage(g_Session.stage_id, g_Stage.first_stage, M_StageSprites());
    g_Gameplay.phase = GP_OBJECT_ENTRANCE;
}

static void M_StageBegin(void)
{
    Pellets_EraseAndResetMoving();
    Shots_EraseAndReset();
    const int16_t stage = g_Session.stage_id;
    g_Gameplay.boss_stage = stage % 5 == 4;
    g_Gameplay.boss = stage == 4 ? BOSS_SINGYOKU
        : stage == 9  ? (g_Session.route == 0 ? BOSS_YUUGENMAGAN : BOSS_MIMA)
        : stage == 14 ? (g_Session.route == 0 ? BOSS_ELIS : BOSS_KIKURI)
        : stage == 19 ? (g_Session.route == 0 ? BOSS_SARIEL : BOSS_KONNGARA)
                      : BOSS_NONE;
    Hud_Load(&m_Assets->hud, M_StageSprites(), &m_Assets->hud_font);
    g_Player.invincible = false;
    g_Player.invincibility_time = 0;
    M_BossSetup();
    if (g_Gameplay.boss_stage) {
        g_Stage.first_stage = true;
        Items_Reset();
    }
    if (g_Gameplay.boss == BOSS_MIMA) {
        g_Gameplay.phase = GP_B10J_SETUP;
    } else if (g_Gameplay.boss == BOSS_SARIEL) {
        B20M_BeginEntrance(&m_Assets->b20m);
        g_Gameplay.phase = GP_FINAL_ENTRANCE;
    } else if (g_Gameplay.boss == BOSS_KONNGARA) {
        B20J_BeginEntrance(&m_Assets->b20j);
        g_Gameplay.phase = GP_FINAL_ENTRANCE;
    } else {
        M_StageBackground();
    }
}

static void M_StageReady(void)
{
    StageTimer_Redraw(
        g_Stage.first_stage, g_Stage.timer_initialized, g_Session.route);
    g_Stage.maximum_combo = 0;
    g_Orb.in_portal = false;
    g_Player.bomb_frame = 0;
    g_Gameplay.frame = 0;
    Orb_ResetFlight();
    if (g_Session.stage_id % 5 == 0 || g_Gameplay.boss_stage) {
        g_Player.left = 304;
    }
    g_Player.deflecting = false;
    g_Player.bomb_damaging = false;
    g_Stage.combo = 0;
    g_Gameplay.phase = GP_LIFE_BEGIN;
}

static void M_LifeBegin(const TH01_INPUT *const raw)
{
    Player_Reset(g_Session.settings.difficulty);
    M_DefaultActors();
    g_GameInput.horizontal = 0;
    g_GameInput.shot = false;
    g_GameInput.ok = false;
    g_GameInput.paused = false;
    Hud_ScoreAndCombo();
    g_GameInput.bomb_frame = 60;
    Stage_UpdateObstacles(M_StageSprites(), true);
    if (!g_Gameplay.bgm_started && g_Gameplay.boss < BOSS_SARIEL) {
        const size_t song = g_Gameplay.boss == BOSS_SINGYOKU ? 7
            : g_Gameplay.boss == BOSS_YUUGENMAGAN
                || g_Gameplay.boss == BOSS_MIMA
            ? 8
            : g_Gameplay.boss == BOSS_KIKURI ? 9
            : g_Gameplay.boss == BOSS_ELIS   ? 10
                                             : Session_Scene();
        Sound_Load(song);
        Sound_Play();
        g_Gameplay.bgm_started = true;
    }
    M_ResetInput();
    if (g_Player.invincibility_time > 1) {
        g_Player.invincible = true;
    }
    if (g_Gameplay.boss == BOSS_SINGYOKU || g_Gameplay.boss == BOSS_MIMA) {
        g_Gameplay.phase = GP_LIFE_BOSS;
    } else {
        if (g_Gameplay.boss == BOSS_KIKURI) {
            g_GameInput.strike = false;
        }
        g_Gameplay.phase = GP_WAIT_SHOT;
    }
    if (g_Gameplay.phase == GP_LIFE_BOSS && M_BossTick(raw)) {
        g_Gameplay.phase = GP_WAIT_SHOT;
    }
}

static void M_StartFrames(void)
{
    g_Gameplay.wait_for_shot = false;
    g_GameInput.shot = false;
    g_Stage.timer_initialized = true;
    g_Rng = g_Session.frame_rand;
    g_GameInput.bomb_frame = GAME_INPUT_BOMB_WINDOW;
    g_Stage.first_stage = false;
    g_Gameplay.phase = GP_FRAME_BEGIN;
}

static void M_PlayerBegin(const GAMEPLAY_PHASE after)
{
    g_Gameplay.after_player = after;
    const PLAYER_SPRITES sprites = M_PlayerSprites();
    if (Player_BeginUpdate(&g_Gameplay.player_bomb, &sprites)) {
        const BOMB_UPDATE result = Bomb_BeginUpdate(
            &m_Assets->miko, &m_Assets->bomb, (int16_t)g_Player.bomb_frame);
        if (result == BOMB_UPDATE_WAIT) {
            g_Gameplay.phase = GP_PLAYER_BOMB;
            return;
        }
        Player_FinishBombUpdate(
            &g_Gameplay.player_bomb, &sprites, result == BOMB_UPDATE_DONE);
    }
    g_Gameplay.phase = after;
}

static void M_PauseDraw(const bool quit)
{
    M_Draw(
        quit ? "本当に終了しちゃうの？" : "ＰＡＵＳＥ", quit ? 224 : 272, 128,
        7, GAME_TEXT_BLACK, false);
    M_Draw(
        quit ? "　うそですぅ　　はいっ" : "　再開　　終了", quit ? 224 : 256,
        144, 7, GAME_TEXT_BLACK, false);
    M_Draw("●", quit ? 224 : 256, 144, 7, GAME_TEXT_NORMAL, false);
    g_Gameplay.menu_selection = 0;
    GameInput_ResetMenu();
    g_Gameplay.waiting = false;
}

static void M_PauseBegin(void)
{
    M_PauseDraw(false);
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
    g_Gameplay.phase = GP_PAUSE;
}

static void M_ContinueUse(void)
{
    g_Session.continues_total =
        (int32_t)((uint32_t)g_Session.continues_total + 1);
    const int16_t scene = g_Session.stage_id / 5;
    g_Session.continues_per_scene[scene]++;
}

static void M_BeginSwitch(const GAMEPLAY_SWITCH to)
{
    g_Gameplay.switch_to = to;
    Palette_BeginFade(
        &g_Gameplay.switch_fade, &g_Palette, PALETTE_FADE_BLACK_OUT);
    Frame_BeginWait(&g_Gameplay.wait);
    g_Gameplay.phase = GP_SWITCH;
}

static void M_Switch(void)
{
    switch (g_Gameplay.switch_to) {
    case GAMEPLAY_SWITCH_STAGE:
        Gameplay_Reset();
        Sound_Load(11);
        Sound_Play();
        return;
    case GAMEPLAY_SWITCH_CONTINUE:
        g_Session.point_value = 0;
        g_Session.pellet_speed = g_PelletShared.speed;
        g_Session.lives = (int16_t)Extend_ResidentLives();
        Gameplay_Reset();
        g_Gameplay.bgm_started = true;
        return;
    case GAMEPLAY_SWITCH_OPENING:
        g_Gameplay.request = GAMEPLAY_REQUEST_OPENING;
        g_Gameplay.phase = GP_TERMINAL;
        return;
    case GAMEPLAY_SWITCH_ENDING:
        g_Gameplay.request = GAMEPLAY_REQUEST_ENDING;
        g_Gameplay.phase = GP_TERMINAL;
        return;
    }
}

static void M_RequestOpening(void)
{
    M_BeginSwitch(GAMEPLAY_SWITCH_OPENING);
}

static void M_OpenMenu(void)
{
    M_SyncSession();
    M_RequestOpening();
}

static bool M_PausePoll(const TH01_INPUT *const raw, const bool quit)
{
    for (;;) {
        if (g_Gameplay.waiting && !Frame_DelayReady(1)) {
            return false;
        }
        g_Gameplay.waiting = false;
        if (g_GameInput.paused) {
            M_Sense(raw);
            if (g_Player.is_hit) {
                M_OpenMenu();
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
                    "●", select_second ? second : left, 144, 7,
                    GAME_TEXT_NORMAL, false);
                g_Gameplay.menu_selection = select_second;
            }
            if (!g_GameInput.shot && !g_GameInput.ok) {
                g_Gameplay.waiting = true;
                if (!Frame_DelayReady(1)) {
                    return false;
                }
                g_Gameplay.waiting = false;
                continue;
            }
        }
        if (quit) {
            if (g_Gameplay.menu_selection != 0) {
                M_ContinueUse();
                M_OpenMenu();
            } else {
                Palette_Set(&g_Palette, &g_StagePalette);
                M_ResetInput();
                Sprite_Restore16(232, 128, 176, 32);
                g_Gameplay.phase = GP_AFTER_ORB;
            }
        } else {
            Sprite_Restore16(276, 128, 80, 16);
            Sprite_Restore16(256, 144, 112, 16);
            g_Gameplay.phase = GP_PAUSE_DELAY;
        }
        return true;
    }
}

static void M_BeginGameover(void)
{
    Items_Reset();
    Orb_Put(M_StageSprites(), g_Orb.left, g_Orb.top, 0);
    const int16_t image = MasterRng_Next(&g_Rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(&m_Assets->miko, image, -1, g_Player.left, PLAYER_TOP);
    for (int32_t i = 0; i < 5; i++) {
        g_Gameplay.gameover_left[i] = g_Player.left;
        g_Gameplay.gameover_top[i] = PLAYER_TOP;
        g_Gameplay.gameover_clipped[i] = false;
    }
    M_CopyPage(0, 1);
    g_Gameplay.animation_frame = 0;
    Frame_BeginWait(&g_Gameplay.wait);
    g_Gameplay.phase = GP_GAMEOVER;
}

static void M_AfterBonus(void)
{
    if (g_PelletShared.speed < 0) {
        g_PelletShared.speed = 0;
    }
    M_SyncSession();
    if (g_Session.stage_id % 5 == 4 || g_Gameplay.boss != BOSS_NONE) {
        Sound_Fade();
        M_BeginSwitch(GAMEPLAY_SWITCH_STAGE);
    } else {
        g_Orb.in_portal = false;
        Video_SelectDrawingPage(1);
        StageObjects_PutAllBackgrounds();
        Video_SelectDrawingPage(0);
        g_Gameplay.phase = GP_STAGE_BEGIN;
    }
}

static void M_ContinueMenu(void)
{
    Scale2x_Copy(288, 200, 0, g_Gameplay.menu_selection == 0 ? 16 : 48, 48, 16);
    Scale2x_Copy(288, 232, 0, g_Gameplay.menu_selection == 0 ? 64 : 32, 48, 16);
}

static GAMEPLAY_REQUEST M_Run(
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    for (;;) {
        switch (g_Gameplay.phase) {
        case GP_STAGE_BEGIN:
            M_StageBegin();
            break;
        case GP_B10J_SETUP:
            if (!B10J_ResumeSetup(&m_Assets->b10j, &m_Assets->miko)) {
                return g_Gameplay.request;
            }
            M_HookBossHp();
            M_StageBackground();
            break;
        case GP_FINAL_ENTRANCE:
            if (g_Gameplay.boss == BOSS_SARIEL
                    ? !B20M_ResumeEntrance(&m_Assets->b20m, host_tick)
                    : !B20J_ResumeEntrance(&m_Assets->b20j)) {
                return g_Gameplay.request;
            }
            g_StagePalette = g_Gameplay.boss == BOSS_KONNGARA
                ? g_B20J.stage_palette
                : g_B20M.stage_palette;
            M_StageReady();
            break;
        case GP_OBJECT_ENTRANCE:
            if (!StageObjects_ResumeEntrance(M_StageSprites())) {
                return g_Gameplay.request;
            }
            if (g_Stage.first_stage) {
                M_CopyPage(0, 1);
            } else {
                StageObjects_CopyToBackground();
                Sprite_PtnPut(
                    &m_Assets->miko, 0, -1, g_Player.left, PLAYER_TOP);
                Items_Render(M_StageSprites());
            }
            if (g_Gameplay.boss == BOSS_KIKURI) {
                Video_Clear(0);
            }
            g_Gameplay.animation_frame = 0;
            if (g_Session.stage_id % 5 == 0 || g_Gameplay.boss_stage) {
                M_TitleChar();
                g_Gameplay.phase = GP_TITLE;
            } else {
                M_Line("STAGE");
                g_Gameplay.phase = GP_STAGE_LABEL;
            }
            break;
        case GP_TITLE:
            if (!Frame_DelayReady(8)) {
                return g_Gameplay.request;
            }
            if (++g_Gameplay.animation_frame < 6) {
                M_TitleChar();
            } else {
                M_Line("STAGE");
                g_Gameplay.phase = GP_STAGE_LABEL;
            }
            break;
        case GP_STAGE_LABEL:
            if (!Frame_DelayReady(35)) {
                return g_Gameplay.request;
            }
            {
                const uint32_t number = (uint32_t)(g_Session.stage_id + 1);
                const GAME_TEXT_GLYPH *glyphs[] = {
                    GameText_Find(&m_Assets->menu_font, '0' + number / 10),
                    GameText_Find(&m_Assets->menu_font, '0' + number % 10),
                };
                Tram_PutLine16(glyphs, 2, TRAM_RED, TRAM_BLACK);
            }
            g_Gameplay.phase = GP_STAGE_NUMBER;
            break;
        case GP_STAGE_NUMBER:
            if (!Frame_DelayReady(35)) {
                return g_Gameplay.request;
            }
            Tram_Clear();
            M_StageReady();
            break;
        case GP_LIFE_BEGIN:
            M_LifeBegin(raw);
            break;
        case GP_LIFE_BOSS:
            if (!M_BossTick(raw)) {
                return g_Gameplay.request;
            }
            g_Gameplay.phase = GP_WAIT_SHOT;
            break;
        case GP_WAIT_SHOT:
            if (g_Gameplay.boss != BOSS_NONE || !g_Gameplay.wait_for_shot
                || g_GameInput.shot) {
                M_StartFrames();
            } else {
                M_Sense(raw);
                M_PlayerBegin(GP_WAIT_SHOT_DELAY);
            }
            break;
        case GP_WAIT_SHOT_DELAY:
            if (!Frame_DelayReady(1)) {
                return g_Gameplay.request;
            }
            g_Player.bomb_frame++;
            g_Gameplay.phase = GP_WAIT_SHOT;
            break;
        case GP_FRAME_BEGIN:
            if (g_Player.is_hit) {
                g_Stage.timer_initialized = false;
                Frame_BeginWait(&g_Gameplay.wait);
                g_Gameplay.phase = GP_LIFE_EXIT;
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
            M_Sense(raw);
            if (g_Player.invincibility_time > 1) {
                g_Player.invincibility_time--;
                g_Player.invincible = true;
            } else if (g_Player.invincibility_time == 1) {
                g_Player.invincible = false;
                g_Player.invincibility_time = 0;
            }
            M_PlayerBegin(GP_ITEMS);
            break;
        case GP_PLAYER_BOMB: {
            const BOMB_UPDATE result = Bomb_ResumeUpdate();
            if (result == BOMB_UPDATE_WAIT) {
                return g_Gameplay.request;
            }
            const PLAYER_SPRITES sprites = M_PlayerSprites();
            Player_FinishBombUpdate(
                &g_Gameplay.player_bomb, &sprites, result == BOMB_UPDATE_DONE);
            g_Gameplay.phase = g_Gameplay.after_player;
            break;
        }
        case GP_ITEMS:
            Items_UpdateAndRender(M_StageSprites(), &m_Assets->item_font);
            g_Gameplay.frame++;
            Orb_AdvanceForceFrame();
            g_Player.bomb_frame++;
            GameInput_AdvanceBombFrame();
            g_Gameplay.phase =
                (g_Session.frame_rand & 3) == 0 ? GP_TIMER : GP_BOSS;
            break;
        case GP_TIMER:
            if (!StageTimer_TickAndPut()) {
                return g_Gameplay.request;
            }
            g_Gameplay.phase = GP_BOSS;
            break;
        case GP_BOSS:
            if (!g_Gameplay.waiting) {
                Invincibility_UpdateAndRender(
                    &g_Gameplay.invincibility, &m_Assets->invincibility);
                g_Gameplay.waiting = true;
            }
            if (!M_BossTick(raw)) {
                return g_Gameplay.request;
            }
            g_Gameplay.waiting = false;
            ShootoutLasers_UpdateAll(m_Assets->lasers);
            g_Gameplay.phase = GP_ORB;
            if (!Stage_BeginOrbComposite(
                    M_StageSprites(), &m_Assets->hud_font, m_Assets->pellets)) {
                return g_Gameplay.request;
            }
            break;
        case GP_ORB:
            if (!Stage_ResumeOrbComposite(
                    M_StageSprites(), &m_Assets->hud_font, m_Assets->pellets)) {
                return g_Gameplay.request;
            }
            if (g_GameInput.paused) {
                M_PauseBegin();
            } else {
                g_Gameplay.phase = GP_AFTER_ORB;
            }
            break;
        case GP_PAUSE:
            if (!M_PausePoll(raw, false)) {
                return g_Gameplay.request;
            }
            break;
        case GP_PAUSE_DELAY:
            if (!Frame_DelayReady(20)) {
                return g_Gameplay.request;
            }
            GameInput_ResetMenu();
            if (g_Gameplay.menu_selection != 0 && g_GameInput.paused) {
                M_PauseDraw(true);
                g_Gameplay.phase = GP_QUIT_CONFIRM;
            } else {
                Palette_Set(&g_Palette, &g_StagePalette);
                M_ResetInput();
                Sprite_Restore16(232, 128, 176, 32);
                g_Gameplay.phase = GP_AFTER_ORB;
            }
            break;
        case GP_QUIT_CONFIRM:
            if (!M_PausePoll(raw, true)) {
                return g_Gameplay.request;
            }
            break;
        case GP_AFTER_ORB:
            Extend_UpdateAndRender();
            if (g_Stage.game_cleared) {
                M_SyncSession();
                g_Session.ending = g_Session.route == 0 ? SESSION_END_MAKAI
                                                        : SESSION_END_JIGOKU;
                if (g_Stage.score > g_Session.highest_score) {
                    g_Session.highest_score = g_Stage.score;
                }
                g_Gameplay.phase = GP_VICTORY_DELAY;
            } else {
                if (g_PelletShared.destroy_score_delta != 0) {
                    g_Stage.score += g_PelletShared.destroy_score_delta;
                    Hud_ScoreAndCombo();
                    g_PelletShared.destroy_score_delta = 0;
                }
                g_Gameplay.phase = GP_FRAME_BEGIN;
            }
            break;
        case GP_VICTORY_DELAY:
            if (!Frame_DelayReady(120)) {
                return g_Gameplay.request;
            }
            M_BeginSwitch(GAMEPLAY_SWITCH_ENDING);
            break;
        case GP_LIFE_EXIT:
            if (!Frame_WaitReady(&g_Gameplay.wait)) {
                return g_Gameplay.request;
            }
            Video_SetScroll(0);
            g_Session.exit_rand = g_Session.frame_rand;
            g_TestDamage = false;
            g_Player.bomb_frame = 200;
            if (g_Stage.cleared) {
                g_Stage.cleared = false;
                g_Player.is_hit = false;
                if (g_Gameplay.boss != BOSS_NONE) {
                    g_Gameplay.wait_for_shot = true;
                    g_Stage.first_stage = true;
                }
                g_Session.stage_id++;
                const int16_t finished = g_Session.stage_id;
                if (g_Gameplay.boss != BOSS_NONE) {
                    g_Gameplay.phase = GP_TOTAL_BONUS;
                    if (Bonus_BeginTotal(
                            &m_Assets->total, finished, raw, host_tick)) {
                        M_AfterBonus();
                    }
                } else {
                    g_Gameplay.phase = GP_STAGE_BONUS;
                    if (Bonus_BeginStage(
                            m_Assets->bonus, &m_Assets->bonus_font, finished,
                            raw)) {
                        M_AfterBonus();
                    }
                }
            } else if (g_Session.lives <= 0) {
                M_BeginGameover();
            } else {
                Sound_Effect(5);
                g_Session.lives--;
                Extend_SyncResidentLives(g_Session.lives);
                Miss_Begin(&m_Assets->miko, M_StageSprites());
                g_Gameplay.phase = GP_MISS;
            }
            break;
        case GP_MISS:
            if (!Miss_Resume(&m_Assets->miko, M_StageSprites(), 1)) {
                return g_Gameplay.request;
            }
            g_Player.is_hit = false;
            g_Player.invincibility_time = 150;
            g_Gameplay.phase = GP_LIFE_BEGIN;
            break;
        case GP_STAGE_BONUS:
            if (!Bonus_Resume(m_Assets->bonus, &m_Assets->bonus_font, raw)) {
                return g_Gameplay.request;
            }
            M_AfterBonus();
            break;
        case GP_TOTAL_BONUS:
            if (!Bonus_ResumeTotal(&m_Assets->total, raw, host_tick)) {
                return g_Gameplay.request;
            }
            M_AfterBonus();
            break;
        case GP_GAMEOVER:
            if (!Frame_WaitReady(&g_Gameplay.wait)) {
                return g_Gameplay.request;
            }
            Video_SetScroll(
                (int16_t)(400 - g_Gameplay.animation_frame % 2 * 8));
            const int32_t frame = g_Gameplay.animation_frame;
            // Model the clipped-flag overread through the adjacent top and
            // left arrays. The caller's nonzero saved BP suppresses frame 15.
            const bool restore = frame < 5 ? !g_Gameplay.gameover_clipped[frame]
                : frame < 10 ? g_Gameplay.gameover_top[frame - 5] == 0
                : frame < 15 ? g_Gameplay.gameover_left[frame - 10] == 0
                             : false;
            if (restore) {
                for (int32_t i = 0; i < 5; i++) {
                    Sprite_Restore16(
                        g_Gameplay.gameover_left[i], PLAYER_TOP, 32, 32);
                }
            }
            g_Gameplay.gameover_left[0] -= 8;
            g_Gameplay.gameover_left[1] -= 8;
            g_Gameplay.gameover_left[3] += 8;
            g_Gameplay.gameover_left[4] += 8;
            g_Gameplay.gameover_top[1] -= 8;
            g_Gameplay.gameover_top[2] -= 8;
            g_Gameplay.gameover_top[3] -= 8;
            for (int32_t i = 0; i < 5; i++) {
                if (g_Gameplay.gameover_left[i] >= 608
                    || g_Gameplay.gameover_left[i] < 0) {
                    g_Gameplay.gameover_clipped[i] = true;
                }
                if (!g_Gameplay.gameover_clipped[i]) {
                    Sprite_PtnPut(
                        &m_Assets->miko, 20 + g_Gameplay.animation_frame % 2,
                        -1, g_Gameplay.gameover_left[i],
                        g_Gameplay.gameover_top[i]);
                }
            }
            g_Gameplay.phase = GP_GAMEOVER_DELAY;
            break;
        case GP_GAMEOVER_DELAY:
            if (!Frame_DelayReady(g_Gameplay.animation_frame / 6)) {
                return g_Gameplay.request;
            }
            g_Gameplay.animation_frame++;
            Frame_BeginWait(&g_Gameplay.wait);
            g_Gameplay.phase =
                g_Gameplay.animation_frame < 16 ? GP_GAMEOVER : GP_GAMEOVER_END;
            break;
        case GP_GAMEOVER_END:
            if (!Frame_WaitReady(&g_Gameplay.wait)) {
                return g_Gameplay.request;
            }
            Video_SetScroll(0);
            g_Player.remaining_bombs = 1;
            Shots_EraseAndReset();
            Pellets_EraseAndResetMoving();
            Extend_Reset();
            M_SyncSession();
            g_Gameplay.request = GAMEPLAY_REQUEST_SCORE;
            g_Gameplay.phase = GP_TERMINAL;
            break;
        case GP_CONTINUE:
            M_Sense(raw);
            g_Gameplay.continue_frame++;
            g_Gameplay.phase = GP_CONTINUE_DELAY;
            break;
        case GP_CONTINUE_DELAY:
            if (!Frame_DelayReady(1)) {
                return g_Gameplay.request;
            }
            const bool confirm = g_GameInput.ok || g_GameInput.shot;
            g_Gameplay.continue_locked &= confirm;
            if (confirm && !g_Gameplay.continue_locked) {
                if (g_Gameplay.menu_selection == 0) {
                    M_BeginSwitch(GAMEPLAY_SWITCH_CONTINUE);
                } else {
                    g_Player.is_hit = false;
                    g_GameInput.paused = false;
                    Sound_Stop();
                    M_RequestOpening();
                }
            } else if (g_Gameplay.continue_frame > 3000 || g_GameInput.paused) {
                g_Player.is_hit = false;
                g_GameInput.paused = false;
                if (g_Gameplay.continue_frame > 3000) {
                    Sound_Stop();
                }
                M_RequestOpening();
            } else {
                if (g_GameInput.up) {
                    g_Gameplay.menu_selection = 0;
                    M_ContinueMenu();
                }
                if (g_GameInput.down) {
                    g_Gameplay.menu_selection = 1;
                    M_ContinueMenu();
                }
                g_Gameplay.phase = GP_CONTINUE;
            }
            break;
        case GP_SWITCH:
            if (!Frame_WaitReady(&g_Gameplay.wait)) {
                return g_Gameplay.request;
            }
            Palette_FadeStep(&g_Gameplay.switch_fade, &g_Palette);
            if (g_Gameplay.switch_fade.remaining > 0) {
                Frame_BeginWait(&g_Gameplay.wait);
            } else {
                M_Switch();
            }
            break;
        case GP_TERMINAL:
            return g_Gameplay.request;
        }
    }
}

void Gameplay_Init(
    const ALLOCATOR *const allocator, const GAMEPLAY_ASSETS *const assets)
{
    m_Assets = assets;
    m_HostTick = 0;
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    g_Gameplay.wait_for_shot = true;
    g_Gameplay.phase = GP_STAGE_BEGIN;
    StageObjects_Prepare(allocator);
    Hud_Init(allocator);
    HudHp_Init(allocator);
    Tram_Init(allocator);
    Palette_Init(&g_Palette);
    Hud_Load(&assets->hud, M_StageSprites(), &assets->hud_font);
}

void Gameplay_Reset(void)
{
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    g_Gameplay.wait_for_shot = true;
    g_Gameplay.phase = GP_STAGE_BEGIN;
    Palette_Init(&g_Palette);
    memset(&g_StagePalette, 0, sizeof(g_StagePalette));
    g_TestDamage = false;
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    memset(&g_StageObjects, 0, sizeof(g_StageObjects));
    memset(&g_Stage, 0, sizeof(g_Stage));
    g_Stage.first_stage = true;
    StageOrb_Init();
    Items_Init();
    Missiles_Init();
    ShootoutLasers_Init();
    Particles_Reset();
    Bomb_Init();
    Miss_Init();
    Scene_Init();
    Bonus_Init();
    Hud_Reset();
    HudHp_Reset();
    Frame_Reset();
    GameInput_Init();
    StageTimer_Init();
    Frame_SwitchProgram();
    Tram_Clear();
    Tram_SetVisible(true);
    BossEntity_Init();
    B05_Init();
    B10J_Init();
    B10M_Init();
    B15J_Init();
    B15M_Init();
    B20J_Init();
    B20M_Init();
    g_Session.high_score = g_Session.table_high_score;
    g_Stage.score = g_Session.score;
    g_Player.remaining_bombs = g_Session.bombs;
    g_Player.left = 304;
    g_PelletShared.speed = g_Session.pellet_speed;
    Extend_SyncResidentLives(g_Session.lives);
    Extend_ResetNext(g_Session.score);
    g_Rng = g_Session.frame_rand;
    g_Stage.flip_cycle = (uint8_t)(MasterRng_Next(&g_Rng) % 60);
    g_Gameplay.boss_stage = g_Session.stage_id % 5 == 4;
    Scene_Activate(&m_Assets->scenes[Session_Scene()]);
    Video_SelectDrawingPage(0);
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    Video_Clear(0);
    Hud_Load(&m_Assets->hud, M_StageSprites(), &m_Assets->hud_font);
}

void Gameplay_ResumeAfterScore(
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    ASSERT(g_Gameplay.request == GAMEPLAY_REQUEST_SCORE);
    g_Gameplay.request = GAMEPLAY_REQUEST_NONE;
    Video_SelectDrawingPage(1);
    M_CopyPage(1, 0);
    const char *const lines[] = {
        "ｃｏｎｔｉｎｕｅ？　　　  ",
        "Ｙｅｓ　　  　",
        "Ｎｏ　　  　 ",
        "Ｙｅｓ　　　  ",
        "Ｎｏ　　　   ",
    };
    for (int32_t i = 0; i < 5; i++) {
        M_Draw(lines[i], 0, i * 16, i < 3 ? 7 : 1, GAME_TEXT_BOLD, true);
    }
    Video_SelectDrawingPage(0);
    Scale2x_Copy(160, 64, 0, 0, 144, 16);
    g_Gameplay.menu_selection = 0;
    M_ContinueMenu();
    M_ResetInput();
    M_ContinueUse();
    if (g_Session.highest_score < g_Stage.score) {
        g_Session.highest_score = g_Stage.score;
    }
    g_Session.score = 0;
    // Restores the resident's lives; the live count stays until the re-exec.
    Extend_SyncResidentLives(g_Session.settings.lives_extra + 2);
    Pellets_SpeedLower(-2, -5);
    g_Gameplay.continue_frame = 0;
    g_Gameplay.continue_locked = g_Session.settings.fix_debouncing;
    g_Gameplay.phase = GP_CONTINUE;
    m_HostTick = host_tick;
    M_Run(raw, host_tick);
}

void Gameplay_Begin(const TH01_INPUT *const raw, const uint64_t host_tick)
{
    Gameplay_Reset();
    Sound_Load(11);
    Sound_Play();
    m_HostTick = host_tick;
    M_Run(raw, host_tick);
}

GAMEPLAY_REQUEST Gameplay_Tick(
    const TH01_INPUT *const raw, const bool reference_edge,
    const uint64_t host_tick)
{
    m_HostTick = host_tick;
    if (reference_edge && g_Gameplay.phase == GP_SWITCH) {
        Frame_PollVSync();
    } else if (reference_edge) {
        Frame_VSync();
    }
    return M_Run(raw, host_tick);
}

void Gameplay_Close(void)
{
    memset(&g_Gameplay, 0, sizeof(g_Gameplay));
    m_Assets = nullptr;
    m_HostTick = 0;
}
