#include "th01/gameplay_internal.h"

#include "pc98/zun/master_rng.h"
#include "th01/scale2x.h"

#include "core/base/assert.h"

#include <string.h>

static void M_CopyPage(GAMEPLAY *const g, const int32_t from, const int32_t to)
{
    memcpy(
        g->storage.video->pages[to], g->storage.video->pages[from],
        VIDEO_PAGE_SIZE);
}

static void M_Sense(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    if (GameInput_SenseGame(&g->world.input, raw, false)) {
        g->world.player.is_hit = true;
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
        &g->storage.assets->menu_font, g->storage.video, text, left, top, color,
        &style);
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
    Tram_PutLine16(&g->world.tram, glyphs, count, TRAM_RED, TRAM_BLACK);
    Tram_Fill(&g->world.tram, (GFX_RECT) { 0, 336, 640, 64 }, TRAM_BLACK);
}

static void M_TitleChar(GAMEPLAY *const g)
{
    const uint32_t chars[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    Gameplay_Effect(g, 14);
    Tram_PutKanji16Reverse(
        &g->world.tram,
        GameText_Find(
            &g->storage.assets->menu_font, chars[g->world.animation_frame]));
}

static void M_StageBackground(GAMEPLAY *const g)
{
    const GAMEPLAY_ASSETS *const a = g->storage.assets;
    const PI_IMAGE *background =
        &a->backgrounds[Session_Scene(g->storage.session)];
    switch (g->world.boss) {
    case 1:
        background = &a->b05.background;
        break;
    case 2:
        background = &a->b10m.background;
        break;
    case 3:
        background = &a->b10j.background;
        break;
    case 4:
        background = &a->b15j.background;
        break;
    case 5:
        background = &a->b15m.background;
        break;
    default:
        break;
    }
    Video_SelectDrawingPage(g->storage.video, 0);
    if (g->world.first_stage) {
        memcpy(g->storage.video->pages[0], background->pixels, VIDEO_PAGE_SIZE);
        Palette_Import(&g->world.stage_palette, background->palette);
        Palette_Set(
            &g->world.palette, g->storage.video, &g->world.stage_palette);
        M_CopyPage(g, 0, 1);
    } else {
        M_CopyPage(g, 1, 0);
        Sprite_PtnPut(
            g->storage.video, &a->miko, 0, -1, g->world.player.left,
            PLAYER_TOP);
    }
    StageObjects_InitPrepared(
        &g->world.objects, &g->world.scene.active->layouts,
        g->storage.session->stage_id, g->world.first_stage, &g->storage.objects,
        g->storage.video, g->bindings.stage.sprites, &g->world.rng);
    g->world.phase = GP_OBJECT_ENTRANCE;
}

static void M_StageBegin(GAMEPLAY *const g)
{
    Pellets_EraseAndResetMoving(&g->world.pellets, g->storage.video);
    Shots_EraseAndReset(&g->world.shots, g->bindings.shots_output);
    const int16_t stage = g->storage.session->stage_id;
    g->world.boss_stage = stage % 5 == 4;
    g->world.boss = stage == 4 ? 1
        : stage == 9           ? (g->storage.session->route == 0 ? 2 : 3)
        : stage == 14          ? (g->storage.session->route == 0 ? 5 : 4)
        : stage == 19          ? (g->storage.session->route == 0 ? 6 : 7)
                               : 0;
    Gameplay_Bind(g);
    g->world.player.invincible = false;
    g->world.player.invincibility_time = 0;
    g->bindings.hit.frame_rand = g->storage.session->frame_rand;
    Gameplay_BossSetup(g);
    if (g->world.boss_stage) {
        g->world.first_stage = true;
        Items_Reset(&g->world.items);
    }
    if (g->world.boss == 3) {
        g->world.phase = GP_B10J_SETUP;
    } else if (g->world.boss == 6) {
        B20M_BeginEntrance(&g->world.b20m, &g->bindings.c20m);
        g->world.phase = GP_FINAL_ENTRANCE;
    } else if (g->world.boss == 7) {
        B20J_BeginEntrance(&g->world.b20j, &g->bindings.c20j);
        g->world.phase = GP_FINAL_ENTRANCE;
    } else {
        M_StageBackground(g);
    }
}

static void M_StageReady(GAMEPLAY *const g)
{
    Timer_Redraw(
        &g->world.timer, &g->bindings.timing, g->world.first_stage,
        g->world.timer_initialized, g->storage.session->route);
    g->world.metrics.maximum_combo = 0;
    g->world.orb.in_portal = false;
    g->world.player.bomb_frame = 0;
    g->world.frame = 0;
    Orb_ResetFlight(&g->world.orb);
    if (g->storage.session->stage_id % 5 == 0 || g->world.boss_stage) {
        g->world.player.left = 304;
    }
    g->world.player.deflecting = false;
    g->world.player.bomb_damaging = false;
    g->world.metrics.combo = 0;
    g->world.phase = GP_LIFE_BEGIN;
}

static void M_LifeBegin(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    Player_Reset(
        &g->world.player, &g->world.input,
        g->storage.session->settings.difficulty);
    Gameplay_DefaultActors(g);
    g->world.input.horizontal = 0;
    g->world.input.shot = false;
    g->world.input.ok = false;
    g->world.input.paused = false;
    Hud_ScoreAndCombo(&g->world.hud, &g->bindings.display);
    g->world.input.bomb_frame = 60;
    Stage_UpdateObstacles(&g->bindings.stage, true);
    if (!g->world.bgm_started && g->world.boss < 6) {
        const size_t song = g->world.boss == 1         ? 7
            : g->world.boss == 2 || g->world.boss == 3 ? 8
            : g->world.boss == 4                       ? 9
            : g->world.boss == 5                       ? 10
                                 : Session_Scene(g->storage.session);
        Gameplay_Play(g, song);
        g->world.bgm_started = true;
    }
    Gameplay_ResetInput(g);
    if (g->world.player.invincibility_time > 1) {
        g->world.player.invincible = true;
    }
    if (g->world.boss == 1 || g->world.boss == 3) {
        g->world.phase = GP_LIFE_BOSS;
    } else {
        if (g->world.boss == 4) {
            g->world.input.strike = false;
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
    g->world.input.shot = false;
    g->world.timer_initialized = true;
    g->world.rng = g->storage.session->frame_rand;
    g->world.input.bomb_frame = GAME_INPUT_BOMB_WINDOW;
    g->world.first_stage = false;
    g->world.phase = GP_FRAME_BEGIN;
}

static void M_PlayerBegin(GAMEPLAY *const g, const GAMEPLAY_PHASE after)
{
    g->world.after_player = after;
    if (Player_BeginUpdate(
            &g->world.player_bomb, &g->world.player, &g->world.input,
            &g->world.orb, &g->world.shots, &g->world.rng,
            g->bindings.player_output)) {
        const BOMB_UPDATE result = Bomb_BeginUpdate(
            &g->world.bomb, &g->bindings.bombing,
            (int16_t)g->world.player.bomb_frame, &g->world.vsync);
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
    GameInput_ResetMenu(&g->world.input);
    g->world.waiting = false;
}

static void M_PauseBegin(GAMEPLAY *const g)
{
    M_PauseDraw(g, false);
    PALETTE dark = g->world.palette.saved;
    for (int32_t i = 0; i < 16; i++) {
        if (i == 7) {
            continue;
        }
        for (int32_t c = 0; c < 3; c++) {
            dark.colors[i][c] = (int8_t)(dark.colors[i][c] * 40 / 100);
        }
    }
    Palette_Set(&g->world.palette, g->storage.video, &dark);
    g->world.phase = GP_PAUSE;
}

static void M_ContinueUse(GAMEPLAY *const g)
{
    g->storage.session->continues_total =
        (int32_t)((uint32_t)g->storage.session->continues_total + 1);
    const int16_t scene = g->storage.session->stage_id / 5;
    g->storage.session->continues_per_scene[scene]++;
}

// Starts game_switch_binary: its vsync_exit stops the counters, and its
// z_palette_black_out steps the palette to black after each of sixteen
// VSyncs. The switch to then follows the last step at once.
static void M_BeginSwitch(GAMEPLAY *const g, const GAMEPLAY_SWITCH to)
{
    g->world.switch_to = to;
    Palette_BeginFade(
        &g->world.switch_fade, &g->world.palette, PALETTE_FADE_BLACK_OUT);
    Frame_BeginWait(&g->world.wait, &g->world.vsync);
    g->world.phase = GP_SWITCH;
}

static void M_Switch(GAMEPLAY *const g)
{
    switch (g->world.switch_to) {
    case GAMEPLAY_SWITCH_STAGE:
        Gameplay_ResetWorld(g);
        Gameplay_Play(g, 11);
        return;
    case GAMEPLAY_SWITCH_CONTINUE:
        g->storage.session->point_value = 0;
        g->storage.session->pellet_speed = g->world.pellet_shared.speed;
        g->storage.session->lives = g->world.retained_lives;
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
        if (g->world.waiting && !Frame_DelayReady(&g->world.vsync, 1)) {
            return false;
        }
        g->world.waiting = false;
        if (g->world.input.paused) {
            M_Sense(g, raw);
            if (g->world.player.is_hit) {
                M_OpenMenu(g);
                return true;
            }
            if (g->world.input.horizontal == GAME_INPUT_LEFT
                || g->world.input.horizontal == GAME_INPUT_RIGHT) {
                const int32_t left = quit ? 224 : 256;
                const int32_t second = quit ? 336 : 320;
                const bool select_second =
                    g->world.input.horizontal == GAME_INPUT_RIGHT;
                Sprite_Restore16(
                    g->storage.video, (int16_t)(select_second ? left : second),
                    144, 16, 16);
                M_Draw(
                    g, "●", select_second ? second : left, 144, 7,
                    GAME_TEXT_NORMAL, false);
                g->world.menu_selection = select_second;
            }
            if (!g->world.input.shot && !g->world.input.ok) {
                g->world.waiting = true;
                if (!Frame_DelayReady(&g->world.vsync, 1)) {
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
                Palette_Set(
                    &g->world.palette, g->storage.video,
                    &g->world.stage_palette);
                Gameplay_ResetInput(g);
                Sprite_Restore16(g->storage.video, 232, 128, 176, 32);
                g->world.phase = GP_AFTER_ORB;
            }
        } else {
            Sprite_Restore16(g->storage.video, 276, 128, 80, 16);
            Sprite_Restore16(g->storage.video, 256, 144, 112, 16);
            g->world.phase = GP_PAUSE_DELAY;
        }
        return true;
    }
}

static void M_BeginGameover(GAMEPLAY *const g)
{
    Items_Reset(&g->world.items);
    Orb_RenderAction(
        g->storage.video, g->bindings.stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g->world.orb.left,
            .top = g->world.orb.top,
            .image = 0,
        });
    const int16_t image = MasterRng_Next(&g->world.rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(
        g->storage.video, &g->storage.assets->miko, image, -1,
        g->world.player.left, PLAYER_TOP);
    for (int32_t i = 0; i < 5; i++) {
        g->world.gameover_left[i] = g->world.player.left;
        g->world.gameover_top[i] = PLAYER_TOP;
        g->world.gameover_clipped[i] = false;
    }
    M_CopyPage(g, 0, 1);
    g->world.animation_frame = 0;
    Frame_BeginWait(&g->world.wait, &g->world.vsync);
    g->world.phase = GP_GAMEOVER;
}

static void M_AfterBonus(GAMEPLAY *const g)
{
    if (g->world.pellet_shared.speed < 0) {
        g->world.pellet_shared.speed = 0;
    }
    Gameplay_SyncSession(g);
    if (g->storage.session->stage_id % 5 == 4 || g->world.boss != 0) {
        Gameplay_Fade(g);
        M_BeginSwitch(g, GAMEPLAY_SWITCH_STAGE);
    } else {
        g->world.orb.in_portal = false;
        Video_SelectDrawingPage(g->storage.video, 1);
        StageObjects_PutAllBackgrounds(&g->world.objects, g->storage.video);
        Video_SelectDrawingPage(g->storage.video, 0);
        g->world.phase = GP_STAGE_BEGIN;
    }
}

static void M_ContinueMenu(GAMEPLAY *const g)
{
    Scale2x_Copy(
        g->storage.video, 288, 200, 0, g->world.menu_selection == 0 ? 16 : 48,
        48, 16);
    Scale2x_Copy(
        g->storage.video, 288, 232, 0, g->world.menu_selection == 0 ? 64 : 32,
        48, 16);
}

static GAMEPLAY_REQUEST M_Run(
    GAMEPLAY *g, const TH01_INPUT *raw, uint64_t host_tick);

void Gameplay_ResumeAfterScore(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const uint64_t host_tick)
{
    ASSERT(g->world.request == GAMEPLAY_REQUEST_SCORE);
    g->world.request = GAMEPLAY_REQUEST_NONE;
    Video_SelectDrawingPage(g->storage.video, 1);
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
    Video_SelectDrawingPage(g->storage.video, 0);
    Scale2x_Copy(g->storage.video, 160, 64, 0, 0, 144, 16);
    g->world.menu_selection = 0;
    M_ContinueMenu(g);
    Gameplay_ResetInput(g);
    M_ContinueUse(g);
    if (g->storage.session->highest_score < g->world.metrics.score) {
        g->storage.session->highest_score = g->world.metrics.score;
    }
    g->storage.session->score = 0;
    // Restores the resident's lives; the live count stays until the re-exec.
    g->world.retained_lives =
        (int8_t)(g->storage.session->settings.lives_extra + 2);
    Pellets_SpeedLower(&g->world.pellet_shared, -2, -5);
    g->world.continue_frame = 0;
    g->world.continue_locked = g->storage.session->settings.fix_debouncing;
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
                    &g->world.b10j, &g->bindings.c10j, &g->world.vsync)) {
                return g->world.request;
            }
            Gameplay_HookBossHp(g);
            M_StageBackground(g);
            break;
        case GP_FINAL_ENTRANCE:
            if (g->world.boss == 6
                    ? !B20M_ResumeEntrance(
                          &g->world.b20m, &g->bindings.c20m, &g->world.vsync,
                          host_tick)
                    : !B20J_ResumeEntrance(
                          &g->world.b20j, &g->bindings.c20j, &g->world.vsync)) {
                return g->world.request;
            }
            g->world.stage_palette = g->world.boss == 7
                ? g->world.b20j.stage_palette
                : g->world.b20m.stage_palette;
            M_StageReady(g);
            break;
        case GP_OBJECT_ENTRANCE:
            if (!StageObjects_ResumeEntrance(
                    &g->world.objects, g->storage.video,
                    g->bindings.stage.sprites, &g->world.vsync)) {
                return g->world.request;
            }
            if (g->world.first_stage) {
                M_CopyPage(g, 0, 1);
            } else {
                StageObjects_CopyToBackground(
                    &g->world.objects, g->storage.video);
                Sprite_PtnPut(
                    g->storage.video, &g->storage.assets->miko, 0, -1,
                    g->world.player.left, PLAYER_TOP);
                Items_Render(
                    &g->world.items, g->storage.video,
                    g->bindings.stage.sprites);
            }
            if (g->world.boss == 4) {
                Video_Clear(g->storage.video, 0);
            }
            g->world.animation_frame = 0;
            if (g->storage.session->stage_id % 5 == 0 || g->world.boss_stage) {
                M_TitleChar(g);
                g->world.phase = GP_TITLE;
            } else {
                M_Line(g, "STAGE");
                g->world.phase = GP_STAGE_LABEL;
            }
            break;
        case GP_TITLE:
            if (!Frame_DelayReady(&g->world.vsync, 8)) {
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
            if (!Frame_DelayReady(&g->world.vsync, 35)) {
                return g->world.request;
            }
            {
                const uint32_t number =
                    (uint32_t)(g->storage.session->stage_id + 1);
                const GAME_TEXT_GLYPH *glyphs[] = {
                    GameText_Find(
                        &g->storage.assets->menu_font, '0' + number / 10),
                    GameText_Find(
                        &g->storage.assets->menu_font, '0' + number % 10),
                };
                Tram_PutLine16(&g->world.tram, glyphs, 2, TRAM_RED, TRAM_BLACK);
            }
            g->world.phase = GP_STAGE_NUMBER;
            break;
        case GP_STAGE_NUMBER:
            if (!Frame_DelayReady(&g->world.vsync, 35)) {
                return g->world.request;
            }
            Tram_Clear(&g->world.tram);
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
            if (g->world.boss != 0 || !g->world.wait_for_shot
                || g->world.input.shot) {
                M_StartFrames(g);
            } else {
                M_Sense(g, raw);
                M_PlayerBegin(g, GP_WAIT_SHOT_DELAY);
            }
            break;
        case GP_WAIT_SHOT_DELAY:
            if (!Frame_DelayReady(&g->world.vsync, 1)) {
                return g->world.request;
            }
            g->world.player.bomb_frame++;
            g->world.phase = GP_WAIT_SHOT;
            break;
        case GP_FRAME_BEGIN:
            if (g->world.player.is_hit) {
                g->world.timer_initialized = false;
                Frame_BeginWait(&g->world.wait, &g->world.vsync);
                g->world.phase = GP_LIFE_EXIT;
                break;
            }
            g->storage.session->frame_rand++;
            {
                const int32_t period = 1800 - g->storage.session->lives * 200
                    - g->world.player.remaining_bombs * 50;
                if (period != 0
                    && g->storage.session->frame_rand % (uint32_t)period == 0) {
                    Pellets_SpeedRaise(&g->world.pellet_shared, 1);
                }
            }
            M_Sense(g, raw);
            if (g->world.player.invincibility_time > 1) {
                g->world.player.invincibility_time--;
                g->world.player.invincible = true;
            } else if (g->world.player.invincibility_time == 1) {
                g->world.player.invincible = false;
                g->world.player.invincibility_time = 0;
            }
            M_PlayerBegin(g, GP_ITEMS);
            break;
        case GP_PLAYER_BOMB: {
            const BOMB_UPDATE result = Bomb_ResumeUpdate(
                &g->world.bomb, &g->bindings.bombing, &g->world.vsync);
            if (result == BOMB_UPDATE_WAIT) {
                return g->world.request;
            }
            Player_FinishBombUpdate(
                &g->world.player_bomb, result == BOMB_UPDATE_DONE);
            g->world.phase = g->world.after_player;
            break;
        }
        case GP_ITEMS:
            Items_UpdateAndRender(&g->world.items, &g->bindings.item);
            g->world.frame++;
            Orb_AdvanceForceFrame(&g->world.orb);
            g->world.player.bomb_frame++;
            GameInput_AdvanceBombFrame(&g->world.input);
            g->world.phase =
                (g->storage.session->frame_rand & 3) == 0 ? GP_TIMER : GP_BOSS;
            break;
        case GP_TIMER:
            if (!Timer_TickAndPut(&g->world.timer, &g->bindings.timing)) {
                return g->world.request;
            }
            g->world.phase = GP_BOSS;
            break;
        case GP_BOSS:
            if (!g->world.waiting) {
                Invincibility_UpdateAndRender(
                    &g->world.invincibility, g->storage.video,
                    &g->storage.assets->invincibility, &g->world.player,
                    &g->world.rng);
                g->bindings.hit.frame_rand = g->storage.session->frame_rand;
                g->world.waiting = true;
            }
            if (!Gameplay_BossTick(g, raw)) {
                return g->world.request;
            }
            g->world.waiting = false;
            ShootoutLasers_UpdateAll(
                &g->world.lasers, g->storage.video, &g->world.player,
                g->storage.assets->lasers);
            g->world.phase = GP_ORB;
            if (!Stage_BeginOrbComposite(
                    &g->world.composite, &g->bindings.stage, &g->world.vsync)) {
                return g->world.request;
            }
            break;
        case GP_ORB:
            if (!Stage_ResumeOrbComposite(
                    &g->world.composite, &g->bindings.stage, &g->world.vsync)) {
                return g->world.request;
            }
            if (g->world.input.paused) {
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
            if (!Frame_DelayReady(&g->world.vsync, 20)) {
                return g->world.request;
            }
            GameInput_ResetMenu(&g->world.input);
            if (g->world.menu_selection != 0 && g->world.input.paused) {
                M_PauseDraw(g, true);
                g->world.phase = GP_QUIT_CONFIRM;
            } else {
                Palette_Set(
                    &g->world.palette, g->storage.video,
                    &g->world.stage_palette);
                Gameplay_ResetInput(g);
                Sprite_Restore16(g->storage.video, 232, 128, 176, 32);
                g->world.phase = GP_AFTER_ORB;
            }
            break;
        case GP_QUIT_CONFIRM:
            if (!M_PausePoll(g, raw, true)) {
                return g->world.request;
            }
            break;
        case GP_AFTER_ORB:
            Extend_UpdateAndRender(&g->bindings.extend);
            if (g->world.game_cleared) {
                Gameplay_SyncSession(g);
                g->storage.session->ending = g->storage.session->route == 0
                    ? SESSION_END_MAKAI
                    : SESSION_END_JIGOKU;
                if (g->world.metrics.score
                    > g->storage.session->highest_score) {
                    g->storage.session->highest_score = g->world.metrics.score;
                }
                g->world.phase = GP_VICTORY_DELAY;
            } else {
                if (g->world.pellet_shared.destroy_score_delta != 0) {
                    g->world.metrics.score +=
                        g->world.pellet_shared.destroy_score_delta;
                    Hud_ScoreAndCombo(&g->world.hud, &g->bindings.display);
                    g->world.pellet_shared.destroy_score_delta = 0;
                }
                g->world.phase = GP_FRAME_BEGIN;
            }
            break;
        case GP_VICTORY_DELAY:
            if (!Frame_DelayReady(&g->world.vsync, 120)) {
                return g->world.request;
            }
            M_BeginSwitch(g, GAMEPLAY_SWITCH_ENDING);
            break;
        case GP_LIFE_EXIT:
            if (!Frame_WaitReady(&g->world.wait, &g->world.vsync)) {
                return g->world.request;
            }
            Video_SetScroll(g->storage.video, 0);
            g->storage.session->exit_rand = g->storage.session->frame_rand;
            g->world.test_damage = false;
            g->world.player.bomb_frame = 200;
            if (g->world.metrics.cleared) {
                g->world.metrics.cleared = false;
                g->world.player.is_hit = false;
                if (g->world.boss != 0) {
                    g->world.wait_for_shot = true;
                    g->world.first_stage = true;
                }
                g->storage.session->stage_id++;
                const int16_t finished = g->storage.session->stage_id;
                if (g->world.boss != 0) {
                    g->world.phase = GP_TOTAL_BONUS;
                    if (Bonus_BeginTotal(
                            &g->world.total, &g->bindings.total_context,
                            finished, raw, &g->world.vsync, host_tick)) {
                        M_AfterBonus(g);
                    }
                } else {
                    g->world.phase = GP_STAGE_BONUS;
                    if (Bonus_BeginStage(
                            &g->world.bonus, &g->bindings.bonus_context,
                            finished, raw, &g->world.vsync)) {
                        M_AfterBonus(g);
                    }
                }
            } else if (g->storage.session->lives <= 0) {
                M_BeginGameover(g);
            } else {
                Gameplay_Effect(g, 5);
                g->storage.session->lives--;
                g->world.retained_lives = (int8_t)g->storage.session->lives;
                g->bindings.missing.remaining_lives = g->storage.session->lives;
                Miss_Begin(
                    &g->world.miss, &g->bindings.missing, &g->world.vsync);
                g->world.phase = GP_MISS;
            }
            break;
        case GP_MISS:
            if (!Miss_Resume(
                    &g->world.miss, &g->bindings.missing, &g->world.vsync)) {
                return g->world.request;
            }
            g->world.player.is_hit = false;
            g->world.player.invincibility_time = 150;
            g->world.phase = GP_LIFE_BEGIN;
            break;
        case GP_STAGE_BONUS:
            if (!Bonus_Resume(
                    &g->world.bonus, &g->bindings.bonus_context, raw,
                    &g->world.vsync)) {
                return g->world.request;
            }
            M_AfterBonus(g);
            break;
        case GP_TOTAL_BONUS:
            if (!Bonus_ResumeTotal(
                    &g->world.total, &g->bindings.total_context, raw,
                    &g->world.vsync, host_tick)) {
                return g->world.request;
            }
            M_AfterBonus(g);
            break;
        case GP_GAMEOVER:
            if (!Frame_WaitReady(&g->world.wait, &g->world.vsync)) {
                return g->world.request;
            }
            Video_SetScroll(
                g->storage.video,
                (int16_t)(400 - g->world.animation_frame % 2 * 8));
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
                        g->storage.video, g->world.gameover_left[i], PLAYER_TOP,
                        32, 32);
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
                        g->storage.video, &g->storage.assets->miko,
                        20 + g->world.animation_frame % 2, -1,
                        g->world.gameover_left[i], g->world.gameover_top[i]);
                }
            }
            g->world.phase = GP_GAMEOVER_DELAY;
            break;
        case GP_GAMEOVER_DELAY:
            if (!Frame_DelayReady(
                    &g->world.vsync, g->world.animation_frame / 6)) {
                return g->world.request;
            }
            g->world.animation_frame++;
            Frame_BeginWait(&g->world.wait, &g->world.vsync);
            g->world.phase =
                g->world.animation_frame < 16 ? GP_GAMEOVER : GP_GAMEOVER_END;
            break;
        case GP_GAMEOVER_END:
            if (!Frame_WaitReady(&g->world.wait, &g->world.vsync)) {
                return g->world.request;
            }
            Video_SetScroll(g->storage.video, 0);
            g->world.player.remaining_bombs = 1;
            Shots_EraseAndReset(&g->world.shots, g->bindings.shots_output);
            Pellets_EraseAndResetMoving(&g->world.pellets, g->storage.video);
            g->world.next_extend = 1;
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
            if (!Frame_DelayReady(&g->world.vsync, 1)) {
                return g->world.request;
            }
            const bool confirm = g->world.input.ok || g->world.input.shot;
            g->world.continue_locked &= confirm;
            if (confirm && !g->world.continue_locked) {
                if (g->world.menu_selection == 0) {
                    M_BeginSwitch(g, GAMEPLAY_SWITCH_CONTINUE);
                } else {
                    g->world.player.is_hit = false;
                    g->world.input.paused = false;
                    Gameplay_Stop(g);
                    M_RequestOpening(g);
                }
            } else if (
                g->world.continue_frame > 3000 || g->world.input.paused) {
                g->world.player.is_hit = false;
                g->world.input.paused = false;
                if (g->world.continue_frame > 3000) {
                    Gameplay_Stop(g);
                }
                M_RequestOpening(g);
            } else {
                if (g->world.input.up) {
                    g->world.menu_selection = 0;
                    M_ContinueMenu(g);
                }
                if (g->world.input.down) {
                    g->world.menu_selection = 1;
                    M_ContinueMenu(g);
                }
                g->world.phase = GP_CONTINUE;
            }
            break;
        case GP_SWITCH:
            if (!Frame_WaitReady(&g->world.wait, &g->world.vsync)) {
                return g->world.request;
            }
            Palette_FadeStep(
                &g->world.switch_fade, &g->world.palette, g->storage.video);
            if (g->world.switch_fade.remaining > 0) {
                Frame_BeginWait(&g->world.wait, &g->world.vsync);
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
    Gameplay_Play(g, 11);
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
        Frame_PollVSync(&g->world.vsync);
    } else if (reference_edge) {
        Frame_VSync(&g->world.vsync);
    }
    return M_Run(g, raw, host_tick);
}
