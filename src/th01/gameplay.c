#include "th01/gameplay_internal.h"

#include "th01/scale2x.h"

#include "core/base/assert.h"

#include <string.h>

static void M_CopyPage(GAMEPLAY *const g, const int32_t from, const int32_t to)
{
    memcpy(g->video->pages[to], g->video->pages[from], VIDEO_PAGE_SIZE);
}

static void M_Sense(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    if (GameInput_SenseGame(&g->input, raw, false)) {
        g->player.is_hit = true;
    }
}

static void M_Draw(
    GAMEPLAY *const g, const char *const text, const int32_t left,
    const int32_t top, const uint8_t color, const GAME_TEXT_WEIGHT weight,
    const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .clear_background = clear };
    ASSERT(Result_Ignore(GameText_Draw(
        &g->assets->menu_font, g->video, text, left, top, color, &style)));
}

static void M_Line(GAMEPLAY *const g, const char *const text)
{
    const GAME_TEXT_GLYPH *glyphs[5];
    int32_t count = 0;
    for (; text[count] != '\0'; count++) {
        ASSERT(count < 5);
        glyphs[count] =
            GameText_Find(&g->assets->menu_font, (uint8_t)text[count]);
    }
    Tram_PutLine16(&g->tram, glyphs, count, TRAM_RED, TRAM_BLACK);
    Tram_Fill(&g->tram, (GFX_RECT) { 0, 336, 640, 64 }, TRAM_BLACK);
}

static void M_TitleChar(GAMEPLAY *const g)
{
    const uint32_t chars[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    Gameplay_Effect(g, 14);
    Tram_PutKanji16Reverse(
        &g->tram,
        GameText_Find(&g->assets->menu_font, chars[g->animation_frame]));
}

static void M_StageBackground(GAMEPLAY *const g)
{
    const GAMEPLAY_ASSETS *const a = g->assets;
    const PI_IMAGE *background = &a->backgrounds[Session_Scene(g->session)];
    switch (g->boss) {
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
    Video_SelectDrawingPage(g->video, 0);
    if (g->first_stage) {
        memcpy(g->video->pages[0], background->pixels, VIDEO_PAGE_SIZE);
        Palette_Import(&g->stage_palette, background->palette);
        Palette_Set(&g->palette, g->video, &g->stage_palette);
        M_CopyPage(g, 0, 1);
    } else {
        M_CopyPage(g, 1, 0);
        Sprite_PtnPut(g->video, &a->miko, 0, -1, g->player.left, PLAYER_TOP);
    }
    StageObjects_InitPrepared(
        &g->objects, &g->scene.active->layouts, g->session->stage_id,
        g->first_stage, &g->storage, g->video, g->stage.sprites, &g->rng);
    g->phase = GP_OBJECT_ENTRANCE;
}

static void M_StageBegin(GAMEPLAY *const g)
{
    Pellets_EraseAndResetMoving(&g->pellets, g->video);
    Shots_EraseAndReset(&g->shots, g->shots_output);
    const int16_t stage = g->session->stage_id;
    g->boss_stage = stage % 5 == 4;
    g->boss = stage == 4 ? 1
        : stage == 9     ? (g->session->route == 0 ? 2 : 3)
        : stage == 14    ? (g->session->route == 0 ? 5 : 4)
        : stage == 19    ? (g->session->route == 0 ? 6 : 7)
                         : 0;
    Gameplay_Bind(g);
    g->player.invincible = false;
    g->player.invincibility_time = 0;
    g->hit.frame_rand = g->session->frame_rand;
    if (g->boss != 7) {
        Gameplay_BossSetup(g);
    }
    if (g->boss_stage) {
        g->first_stage = true;
        Items_Reset(&g->items);
    }
    if (g->boss == 3) {
        g->phase = GP_B10J_SETUP;
    } else if (g->boss == 6) {
        B20M_BeginEntrance(&g->b20m, &g->c20m);
        g->phase = GP_FINAL_ENTRANCE;
    } else if (g->boss == 7) {
        B20J_BeginEntrance(&g->b20j, &g->c20j);
        g->phase = GP_FINAL_ENTRANCE;
    } else {
        M_StageBackground(g);
    }
}

static void M_StageReady(GAMEPLAY *const g)
{
    Timer_Redraw(
        &g->timer, &g->timing, g->first_stage, g->timer_initialized,
        g->session->route);
    g->metrics.maximum_combo = 0;
    g->orb.in_portal = false;
    g->player.bomb_frame = 0;
    g->frame = 0;
    Orb_ResetFlight(&g->orb);
    if (g->session->stage_id % 5 == 0 || g->boss_stage) {
        g->player.left = 304;
    }
    g->player.deflecting = false;
    g->player.bomb_damaging = false;
    g->metrics.combo = 0;
    g->phase = GP_LIFE_BEGIN;
}

static void M_LifeBegin(GAMEPLAY *const g, const TH01_INPUT *const raw)
{
    Player_Reset(&g->player, &g->input, g->session->settings.difficulty);
    Gameplay_DefaultActors(g);
    g->input.horizontal = 0;
    g->input.shot = false;
    g->input.ok = false;
    g->input.paused = false;
    Hud_ScoreAndCombo(&g->hud, &g->display);
    g->input.bomb_frame = 60;
    Stage_UpdateObstacles(&g->stage, true);
    if (!g->bgm_started && g->boss < 6) {
        const size_t song = g->boss == 1   ? 7
            : g->boss == 2 || g->boss == 3 ? 8
            : g->boss == 4                 ? 9
            : g->boss == 5                 ? 10
                                           : Session_Scene(g->session);
        Gameplay_Play(g, song);
        g->bgm_started = true;
    }
    Gameplay_ResetInput(g);
    if (g->player.invincibility_time > 1) {
        g->player.invincible = true;
    }
    if (g->boss == 1 || g->boss == 3) {
        g->phase = GP_LIFE_BOSS;
    } else {
        if (g->boss == 4) {
            g->input.strike = false;
        }
        g->phase = GP_WAIT_SHOT;
    }
    if (g->phase == GP_LIFE_BOSS && Gameplay_BossTick(g, raw)) {
        g->phase = GP_WAIT_SHOT;
    }
}

static void M_StartFrames(GAMEPLAY *const g)
{
    g->wait_for_shot = false;
    g->input.shot = false;
    g->timer_initialized = true;
    g->rng = g->session->frame_rand;
    g->input.bomb_frame = GAME_INPUT_BOMB_WINDOW;
    g->first_stage = false;
    g->phase = GP_FRAME_BEGIN;
}

static void M_PlayerBegin(GAMEPLAY *const g, const GAMEPLAY_PHASE after)
{
    g->after_player = after;
    if (Player_BeginUpdate(
            &g->player_bomb, &g->player, &g->input, &g->orb, &g->shots, &g->rng,
            g->player_output)) {
        const BOMB_UPDATE result = Bomb_BeginUpdate(
            &g->bomb, &g->bombing, (int16_t)g->player.bomb_frame, &g->vsync);
        if (result == BOMB_UPDATE_WAIT) {
            g->phase = GP_PLAYER_BOMB;
            return;
        }
        Player_FinishBombUpdate(&g->player_bomb, result == BOMB_UPDATE_DONE);
    }
    g->phase = after;
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
    g->menu_selection = 0;
    GameInput_ResetMenu(&g->input);
    g->waiting = false;
}

static void M_PauseBegin(GAMEPLAY *const g)
{
    M_PauseDraw(g, false);
    PALETTE dark = g->palette.saved;
    for (int32_t i = 0; i < 16; i++) {
        if (i == 7) {
            continue;
        }
        for (int32_t c = 0; c < 3; c++) {
            dark.colors[i][c] = (int8_t)(dark.colors[i][c] * 40 / 100);
        }
    }
    Palette_Set(&g->palette, g->video, &dark);
    g->phase = GP_PAUSE;
}

static void M_ContinueUse(GAMEPLAY *const g)
{
    g->session->continues_total =
        (int32_t)((uint32_t)g->session->continues_total + 1);
    const int16_t scene = g->session->stage_id / 5;
    g->session->continues_per_scene[scene]++;
}

static void M_RequestOpening(GAMEPLAY *const g)
{
    g->request = GAMEPLAY_REQUEST_OPENING;
    g->phase = GP_TERMINAL;
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
        if (g->waiting && !Frame_DelayReady(&g->vsync, 1)) {
            return false;
        }
        g->waiting = false;
        if (g->input.paused) {
            M_Sense(g, raw);
            if (g->player.is_hit) {
                M_OpenMenu(g);
                return true;
            }
            if (g->input.horizontal == GAME_INPUT_LEFT
                || g->input.horizontal == GAME_INPUT_RIGHT) {
                const int32_t left = quit ? 224 : 256;
                const int32_t second = quit ? 336 : 320;
                const bool select_second =
                    g->input.horizontal == GAME_INPUT_RIGHT;
                Sprite_Restore16(
                    g->video, (int16_t)(select_second ? left : second), 144, 16,
                    16);
                M_Draw(
                    g, "●", select_second ? second : left, 144, 7,
                    GAME_TEXT_NORMAL, false);
                g->menu_selection = select_second;
            }
            if (!g->input.shot && !g->input.ok) {
                g->waiting = true;
                if (!Frame_DelayReady(&g->vsync, 1)) {
                    return false;
                }
                g->waiting = false;
                continue;
            }
        }
        if (quit) {
            if (g->menu_selection != 0) {
                M_ContinueUse(g);
                M_OpenMenu(g);
            } else {
                Palette_Set(&g->palette, g->video, &g->stage_palette);
                Gameplay_ResetInput(g);
                Sprite_Restore16(g->video, 232, 128, 176, 32);
                g->phase = GP_AFTER_ORB;
            }
        } else {
            Sprite_Restore16(g->video, 276, 128, 80, 16);
            Sprite_Restore16(g->video, 256, 144, 112, 16);
            g->phase = GP_PAUSE_DELAY;
        }
        return true;
    }
}

static void M_BeginGameover(GAMEPLAY *const g)
{
    Items_Reset(&g->items);
    Orb_RenderAction(
        g->video, g->stage.sprites,
        (ORB_ACTION) {
            .kind = ORB_ACTION_PUT,
            .left = g->orb.left,
            .top = g->orb.top,
            .image = 0,
        });
    const int16_t image = Math_Random(&g->rng) % 8 == 0 ? 23 : 22;
    Sprite_PtnPut(
        g->video, &g->assets->miko, image, -1, g->player.left, PLAYER_TOP);
    for (int32_t i = 0; i < 5; i++) {
        g->gameover_left[i] = g->player.left;
        g->gameover_top[i] = PLAYER_TOP;
        g->gameover_clipped[i] = false;
    }
    M_CopyPage(g, 0, 1);
    g->animation_frame = 0;
    Frame_BeginWait(&g->wait, &g->vsync);
    g->phase = GP_GAMEOVER;
}

static void M_AfterBonus(GAMEPLAY *const g)
{
    if (g->pellet_shared.speed < 0) {
        g->pellet_shared.speed = 0;
    }
    Gameplay_SyncSession(g);
    if (g->session->stage_id % 5 == 4 || g->boss != 0) {
        MdrvSynth_Fade(g->audio, 1);
        Gameplay_ResetWorld(g);
        Gameplay_Play(g, 11);
    } else {
        g->orb.in_portal = false;
        Video_SelectDrawingPage(g->video, 1);
        StageObjects_PutAllBackgrounds(&g->objects, g->video);
        Video_SelectDrawingPage(g->video, 0);
        g->phase = GP_STAGE_BEGIN;
    }
}

static void M_ContinueMenu(GAMEPLAY *const g)
{
    Scale2x_Copy(
        g->video, 288, 200, 0, g->menu_selection == 0 ? 16 : 48, 48, 16);
    Scale2x_Copy(
        g->video, 288, 232, 0, g->menu_selection == 0 ? 64 : 32, 48, 16);
}

void Gameplay_ResumeAfterScore(GAMEPLAY *const g)
{
    ASSERT(g->request == GAMEPLAY_REQUEST_SCORE);
    g->request = GAMEPLAY_REQUEST_NONE;
    Video_SelectDrawingPage(g->video, 1);
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
    Video_SelectDrawingPage(g->video, 0);
    Scale2x_Copy(g->video, 160, 64, 0, 0, 144, 16);
    g->menu_selection = 0;
    M_ContinueMenu(g);
    Gameplay_ResetInput(g);
    M_ContinueUse(g);
    if (g->session->highest_score < g->metrics.score) {
        g->session->highest_score = g->metrics.score;
    }
    g->session->score = 0;
    g->session->lives = (int16_t)(g->session->settings.lives_extra + 2);
    Pellets_SpeedLower(&g->pellet_shared, -2, -5);
    g->continue_frame = 0;
    g->phase = GP_CONTINUE;
}

static GAMEPLAY_REQUEST M_Run(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const uint64_t host_tick)
{
    for (;;) {
        switch (g->phase) {
        case GP_STAGE_BEGIN:
            M_StageBegin(g);
            break;
        case GP_B10J_SETUP:
            if (!B10J_ResumeSetup(&g->b10j, &g->c10j, &g->vsync, host_tick)) {
                return g->request;
            }
            M_StageBackground(g);
            break;
        case GP_FINAL_ENTRANCE:
            if (g->boss == 6
                    ? !B20M_ResumeEntrance(
                          &g->b20m, &g->c20m, &g->vsync, host_tick)
                    : !B20J_ResumeEntrance(&g->b20j, &g->c20j, &g->vsync)) {
                return g->request;
            }
            if (g->boss == 7) {
                Gameplay_BossSetup(g);
                g->stage_palette = g->b20j.stage_palette;
            } else {
                g->stage_palette = g->b20m.stage_palette;
            }
            M_StageReady(g);
            break;
        case GP_OBJECT_ENTRANCE:
            if (!StageObjects_ResumeEntrance(
                    &g->objects, g->video, g->stage.sprites, &g->vsync)) {
                return g->request;
            }
            if (g->first_stage) {
                M_CopyPage(g, 0, 1);
            } else {
                StageObjects_CopyToBackground(&g->objects, g->video);
                Sprite_PtnPut(
                    g->video, &g->assets->miko, 0, -1, g->player.left,
                    PLAYER_TOP);
                Items_Render(&g->items, g->video, g->stage.sprites);
            }
            if (g->boss == 4) {
                Video_Clear(g->video, 0);
            }
            g->animation_frame = 0;
            if (g->session->stage_id % 5 == 0 || g->boss_stage) {
                M_TitleChar(g);
                g->phase = GP_TITLE;
            } else {
                M_Line(g, "STAGE");
                g->phase = GP_STAGE_LABEL;
            }
            break;
        case GP_TITLE:
            if (!Frame_DelayReady(&g->vsync, 8)) {
                return g->request;
            }
            if (++g->animation_frame < 6) {
                M_TitleChar(g);
            } else {
                M_Line(g, "STAGE");
                g->phase = GP_STAGE_LABEL;
            }
            break;
        case GP_STAGE_LABEL:
            if (!Frame_DelayReady(&g->vsync, 35)) {
                return g->request;
            }
            {
                const uint32_t number = (uint32_t)(g->session->stage_id + 1);
                const GAME_TEXT_GLYPH *glyphs[] = {
                    GameText_Find(&g->assets->menu_font, '0' + number / 10),
                    GameText_Find(&g->assets->menu_font, '0' + number % 10),
                };
                Tram_PutLine16(&g->tram, glyphs, 2, TRAM_RED, TRAM_BLACK);
            }
            g->phase = GP_STAGE_NUMBER;
            break;
        case GP_STAGE_NUMBER:
            if (!Frame_DelayReady(&g->vsync, 35)) {
                return g->request;
            }
            Tram_Clear(&g->tram);
            M_StageReady(g);
            break;
        case GP_LIFE_BEGIN:
            M_LifeBegin(g, raw);
            break;
        case GP_LIFE_BOSS:
            if (!Gameplay_BossTick(g, raw)) {
                return g->request;
            }
            g->phase = GP_WAIT_SHOT;
            break;
        case GP_WAIT_SHOT:
            if (g->boss != 0 || !g->wait_for_shot || g->input.shot) {
                M_StartFrames(g);
            } else {
                M_Sense(g, raw);
                M_PlayerBegin(g, GP_WAIT_SHOT_DELAY);
            }
            break;
        case GP_WAIT_SHOT_DELAY:
            if (!Frame_DelayReady(&g->vsync, 1)) {
                return g->request;
            }
            g->player.bomb_frame++;
            g->phase = GP_WAIT_SHOT;
            break;
        case GP_FRAME_BEGIN:
            if (g->player.is_hit) {
                g->timer_initialized = false;
                Frame_BeginWait(&g->wait, &g->vsync);
                g->phase = GP_LIFE_EXIT;
                break;
            }
            g->session->frame_rand++;
            {
                const int32_t period = 1800 - g->session->lives * 200
                    - g->player.remaining_bombs * 50;
                if (period != 0
                    && g->session->frame_rand % (uint32_t)period == 0) {
                    Pellets_SpeedRaise(&g->pellet_shared, 1);
                }
            }
            M_Sense(g, raw);
            if (g->player.invincibility_time > 1) {
                g->player.invincibility_time--;
                g->player.invincible = true;
            } else if (g->player.invincibility_time == 1) {
                g->player.invincible = false;
                g->player.invincibility_time = 0;
            }
            M_PlayerBegin(g, GP_ITEMS);
            break;
        case GP_PLAYER_BOMB: {
            const BOMB_UPDATE result =
                Bomb_ResumeUpdate(&g->bomb, &g->bombing, &g->vsync);
            if (result == BOMB_UPDATE_WAIT) {
                return g->request;
            }
            Player_FinishBombUpdate(
                &g->player_bomb, result == BOMB_UPDATE_DONE);
            g->phase = g->after_player;
            break;
        }
        case GP_ITEMS:
            Items_UpdateAndRender(&g->items, &g->item);
            g->frame++;
            Orb_AdvanceForceFrame(&g->orb);
            g->player.bomb_frame++;
            GameInput_AdvanceBombFrame(&g->input);
            g->phase = (g->session->frame_rand & 3) == 0 ? GP_TIMER : GP_BOSS;
            break;
        case GP_TIMER:
            if (!Timer_TickAndPut(&g->timer, &g->timing)) {
                return g->request;
            }
            g->phase = GP_BOSS;
            break;
        case GP_BOSS:
            if (!g->waiting) {
                Invincibility_UpdateAndRender(
                    &g->invincibility, g->video, &g->assets->invincibility,
                    &g->player, &g->rng);
                g->hit.frame_rand = g->session->frame_rand;
                g->waiting = true;
            }
            if (!Gameplay_BossTick(g, raw)) {
                return g->request;
            }
            g->waiting = false;
            ShootoutLasers_UpdateAll(
                &g->lasers, g->video, &g->player, g->assets->lasers);
            g->phase = GP_ORB;
            if (!Stage_BeginOrbComposite(&g->composite, &g->stage, &g->vsync)) {
                return g->request;
            }
            break;
        case GP_ORB:
            if (!Stage_ResumeOrbComposite(
                    &g->composite, &g->stage, &g->vsync)) {
                return g->request;
            }
            if (g->input.paused) {
                M_PauseBegin(g);
            } else {
                g->phase = GP_AFTER_ORB;
            }
            break;
        case GP_PAUSE:
            if (!M_PausePoll(g, raw, false)) {
                return g->request;
            }
            break;
        case GP_PAUSE_DELAY:
            if (!Frame_DelayReady(&g->vsync, 20)) {
                return g->request;
            }
            GameInput_ResetMenu(&g->input);
            if (g->menu_selection != 0 && g->input.paused) {
                M_PauseDraw(g, true);
                g->phase = GP_QUIT_CONFIRM;
            } else {
                Palette_Set(&g->palette, g->video, &g->stage_palette);
                Gameplay_ResetInput(g);
                Sprite_Restore16(g->video, 232, 128, 176, 32);
                g->phase = GP_AFTER_ORB;
            }
            break;
        case GP_QUIT_CONFIRM:
            if (!M_PausePoll(g, raw, true)) {
                return g->request;
            }
            break;
        case GP_AFTER_ORB:
            Extend_UpdateAndRender(&g->extend);
            if (g->game_cleared) {
                Gameplay_SyncSession(g);
                g->session->ending = g->session->route == 0
                    ? SESSION_END_MAKAI
                    : SESSION_END_JIGOKU;
                if (g->metrics.score > g->session->highest_score) {
                    g->session->highest_score = g->metrics.score;
                }
                g->phase = GP_VICTORY_DELAY;
            } else {
                if (g->pellet_shared.destroy_score_delta != 0) {
                    g->metrics.score += g->pellet_shared.destroy_score_delta;
                    Hud_ScoreAndCombo(&g->hud, &g->display);
                    g->pellet_shared.destroy_score_delta = 0;
                }
                g->phase = GP_FRAME_BEGIN;
            }
            break;
        case GP_VICTORY_DELAY:
            if (!Frame_DelayReady(&g->vsync, 120)) {
                return g->request;
            }
            g->request = GAMEPLAY_REQUEST_ENDING;
            g->phase = GP_TERMINAL;
            break;
        case GP_LIFE_EXIT:
            if (!Frame_WaitReady(&g->wait, &g->vsync)) {
                return g->request;
            }
            Video_SetScroll(g->video, 0);
            g->session->exit_rand = g->session->frame_rand;
            g->test_damage = false;
            g->player.bomb_frame = 200;
            if (g->metrics.cleared) {
                g->metrics.cleared = false;
                g->player.is_hit = false;
                if (g->boss != 0) {
                    g->wait_for_shot = true;
                    g->first_stage = true;
                }
                g->session->stage_id++;
                const int16_t finished = g->session->stage_id;
                if (g->boss != 0) {
                    g->phase = GP_TOTAL_BONUS;
                    if (Bonus_BeginTotal(
                            &g->total, &g->total_context, finished, raw,
                            &g->vsync, host_tick)) {
                        M_AfterBonus(g);
                    }
                } else {
                    g->phase = GP_STAGE_BONUS;
                    if (Bonus_BeginStage(
                            &g->bonus, &g->bonus_context, finished, raw,
                            &g->vsync)) {
                        M_AfterBonus(g);
                    }
                }
            } else if (g->session->lives <= 0) {
                M_BeginGameover(g);
            } else {
                Gameplay_Effect(g, 5);
                g->session->lives--;
                g->retained_lives = (int8_t)g->session->lives;
                g->missing.remaining_lives = g->session->lives;
                Miss_Begin(&g->miss, &g->missing, &g->vsync);
                g->phase = GP_MISS;
            }
            break;
        case GP_MISS:
            if (!Miss_Resume(&g->miss, &g->missing, &g->vsync)) {
                return g->request;
            }
            g->player.is_hit = false;
            g->player.invincibility_time = 150;
            g->phase = GP_LIFE_BEGIN;
            break;
        case GP_STAGE_BONUS:
            if (!Bonus_Resume(&g->bonus, &g->bonus_context, raw, &g->vsync)) {
                return g->request;
            }
            M_AfterBonus(g);
            break;
        case GP_TOTAL_BONUS:
            if (!Bonus_ResumeTotal(
                    &g->total, &g->total_context, raw, &g->vsync, host_tick)) {
                return g->request;
            }
            M_AfterBonus(g);
            break;
        case GP_GAMEOVER:
            if (!Frame_WaitReady(&g->wait, &g->vsync)) {
                return g->request;
            }
            Video_SetScroll(
                g->video, (int16_t)(400 - g->animation_frame % 2 * 8));
            const int32_t frame = g->animation_frame;
            // Model the clipped-flag overread through the adjacent top and
            // left arrays. The caller's nonzero saved BP suppresses frame 15.
            const bool restore = frame < 5 ? !g->gameover_clipped[frame]
                : frame < 10               ? g->gameover_top[frame - 5] == 0
                : frame < 15               ? g->gameover_left[frame - 10] == 0
                                           : false;
            if (restore) {
                for (int32_t i = 0; i < 5; i++) {
                    Sprite_Restore16(
                        g->video, g->gameover_left[i], PLAYER_TOP, 32, 32);
                }
            }
            g->gameover_left[0] -= 8;
            g->gameover_left[1] -= 8;
            g->gameover_left[3] += 8;
            g->gameover_left[4] += 8;
            g->gameover_top[1] -= 8;
            g->gameover_top[2] -= 8;
            g->gameover_top[3] -= 8;
            for (int32_t i = 0; i < 5; i++) {
                if (g->gameover_left[i] >= 608 || g->gameover_left[i] < 0) {
                    g->gameover_clipped[i] = true;
                }
                if (!g->gameover_clipped[i]) {
                    Sprite_PtnPut(
                        g->video, &g->assets->miko, 20 + g->animation_frame % 2,
                        -1, g->gameover_left[i], g->gameover_top[i]);
                }
            }
            g->phase = GP_GAMEOVER_DELAY;
            break;
        case GP_GAMEOVER_DELAY:
            if (!Frame_DelayReady(&g->vsync, g->animation_frame / 6)) {
                return g->request;
            }
            g->animation_frame++;
            Frame_BeginWait(&g->wait, &g->vsync);
            g->phase = g->animation_frame < 16 ? GP_GAMEOVER : GP_GAMEOVER_END;
            break;
        case GP_GAMEOVER_END:
            if (!Frame_WaitReady(&g->wait, &g->vsync)) {
                return g->request;
            }
            Video_SetScroll(g->video, 0);
            g->player.remaining_bombs = 1;
            Shots_EraseAndReset(&g->shots, g->shots_output);
            Pellets_EraseAndResetMoving(&g->pellets, g->video);
            g->next_extend = 1;
            Gameplay_SyncSession(g);
            g->request = GAMEPLAY_REQUEST_SCORE;
            g->phase = GP_TERMINAL;
            break;
        case GP_CONTINUE:
            M_Sense(g, raw);
            g->continue_frame++;
            g->phase = GP_CONTINUE_DELAY;
            break;
        case GP_CONTINUE_DELAY:
            if (!Frame_DelayReady(&g->vsync, 1)) {
                return g->request;
            }
            if (g->input.ok || g->input.shot) {
                if (g->menu_selection == 0) {
                    g->session->point_value = 0;
                    g->session->pellet_speed = g->pellet_shared.speed;
                    Gameplay_ResetWorld(g);
                    g->bgm_started = true;
                } else {
                    g->player.is_hit = false;
                    g->input.paused = false;
                    MdrvSynth_Stop(g->audio);
                    M_RequestOpening(g);
                }
            } else if (g->continue_frame > 3000 || g->input.paused) {
                g->player.is_hit = false;
                g->input.paused = false;
                if (g->continue_frame > 3000) {
                    MdrvSynth_Stop(g->audio);
                }
                M_RequestOpening(g);
            } else {
                if (g->input.up) {
                    g->menu_selection = 0;
                    M_ContinueMenu(g);
                }
                if (g->input.down) {
                    g->menu_selection = 1;
                    M_ContinueMenu(g);
                }
                g->phase = GP_CONTINUE;
            }
            break;
        case GP_TERMINAL:
            return g->request;
        }
    }
}

void Gameplay_Begin(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const uint64_t host_tick)
{
    Gameplay_ResetWorld(g);
    Gameplay_Play(g, 11);
    g->host_tick = host_tick;
    M_Run(g, raw, host_tick);
}

GAMEPLAY_REQUEST Gameplay_Tick(
    GAMEPLAY *const g, const TH01_INPUT *const raw, const bool reference_edge,
    const uint64_t host_tick)
{
    g->host_tick = host_tick;
    Gameplay_AdvanceAudio(g);
    if (reference_edge) {
        Frame_VSync(&g->vsync);
    }
    return M_Run(g, raw, host_tick);
}
