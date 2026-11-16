#include "th07/gui.h"
#include "th07/gui/private.h"

#include "th07/bullet.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screen_effect.h"
#include "th07/sound.h"
#include "th07/stage.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/assert.h"
#include "core/base/color.h"
#include "core/base/log.h"
#include "core/base/utils.h"
#include "core/gfx.h"
#include "core/io/reader.h"
#include "core/math/x87.h"

#include "stg/chain.h"
#include "stg/msg_diff.h"
#include "stg/note.h"
#include "stg/translation.h"

#include "core/base/arena.h"
#include "core/json.h"

#include <stdio.h>
#include <string.h>

#define M_PATH_SIZE 32
#define M_DIFF_ARENA_CAPACITY (16 * 1024 * 1024)

// Tells how a frame of the dialogue script ended.
typedef enum {
    M_RUN_ENDED,
    M_RUN_WAITING, // the next instruction is not due yet
    M_RUN_HALTED, // an instruction holds the script and its timer
} M_RUN;

typedef struct {
    const char *face;
    const char *loading;
} M_CHARACTER_FILES;

GUI g_Gui;

static const M_CHARACTER_FILES m_CharacterFiles[] = {
    { "data/face_rm00.anm", "data/loading.anm" },
    { "data/face_mr00.anm", "data/loading2.anm" },
    { "data/face_sk00.anm", "data/loading3.anm" },
};

static const uint32_t m_TextColors[GUI_MSG_SIDE_COUNT] = {
    COLOR_ARGB(0x00, 0xe8, 0xf0, 0xff),
    COLOR_ARGB(0x00, 0xff, 0xe8, 0xf0),
};

static GUI_MSG_FILE m_Msg;
static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;

static int32_t M_Wrap(const int64_t value)
{
    return (int32_t)(uint32_t)(uint64_t)value;
}

static bool M_KeepsInterface(void)
{
    const SCENE scene = Scene_GetRequested();
    return scene == SCENE_NEXT_STAGE || scene == SCENE_RESTART_STAGE
        || scene == SCENE_NEXT_STAGE_AGAIN;
}

static GFX_RECT M_SpriteRect(const int32_t sprite)
{
    const ANM_SPRITE *const data = Anm_GetSpriteEntry(sprite);
    const int32_t left = (int32_t)data->x0;
    const int32_t top = (int32_t)data->y0;
    return (GFX_RECT) { left, top, (int32_t)data->x1 - left,
                        (int32_t)data->y1 - top };
}

// Copies the art of sprite in the second texture of front.anm to where
// sprite 0x609 shows it.
static void M_ShowArt(const int32_t sprite)
{
    Result_Log(
        Anm_CopyTextureRect(
            M_FRONT_SLOT, M_SpriteRect(M_BOSS_ART_TARGET_SPRITE),
            M_FRONT_TEXTURE_SLOT, M_SpriteRect(sprite)),
        LOG_LEVEL_WARNING);
}

// Requests the copy of the play area into the snapshot's sprite.
static void M_RequestSnapshot(void)
{
    if (!Gfx_IsStarted()) {
        return;
    }
    const ANM_SPRITE *const sprite = g_Gui.loading.snapshot.sprite_data;
    ASSERT(sprite != nullptr);
    Anm_SetPendingCopy(
        M_SNAPSHOT_TEXTURE_SLOT, (GFX_RECT) { 32, 16, 384, 448 },
        (GFX_RECT) { (int32_t)sprite->x0, (int32_t)sprite->y0,
                     (int32_t)sprite->width, (int32_t)sprite->height });
}

static void M_PlaySong(const int32_t song)
{
    Sound_PlayMusic(g_Stage.data.song_paths[song]);
}

static bool M_SkipHeld(void)
{
    return g_Gui.msg.skippable && Buttons_FromWord(g_Buttons.play_word).skip;
}

static bool M_ShootPressed(void)
{
    return Buttons_FromWord(g_Buttons.play_word).shoot
        && !Buttons_FromWord(g_Buttons.previous_play_word).shoot;
}

static void M_UpdateBossBar(void)
{
    ANM_VM *const bar = &g_Gui.hud.sprites[M_BOSS_BAR];
    if (g_Gui.boss.present) {
        if (g_Gui.hud.boss_bar == GUI_BOSS_BAR_HIDDEN) {
            bar->pending_interrupt = 1;
            g_Gui.hud.boss_bar = GUI_BOSS_BAR_APPEARING;
            g_Gui.hud.boss_bar_alpha = 0;
        } else {
            if (bar->stopped) {
                g_Gui.hud.boss_bar = GUI_BOSS_BAR_SHOWN;
            }
            if (g_Gui.hud.boss_bar_alpha < M_BOSS_BAR_ALPHA_RISE_END) {
                g_Gui.hud.boss_bar_alpha += M_BOSS_BAR_ALPHA_STEP;
            } else {
                g_Gui.hud.boss_bar_alpha = M_BOSS_BAR_ALPHA_MAX;
            }
        }
    } else if (g_Gui.hud.boss_bar != GUI_BOSS_BAR_HIDDEN) {
        if (g_Gui.hud.boss_bar <= GUI_BOSS_BAR_SHOWN) {
            bar->pending_interrupt = 2;
            g_Gui.hud.boss_bar = GUI_BOSS_BAR_LEAVING;
        }
        // From 0xff, the alpha passes 3 and wraps below 0.
        if (g_Gui.hud.boss_bar_alpha > 0) {
            g_Gui.hud.boss_bar_alpha -= M_BOSS_BAR_ALPHA_STEP;
        } else {
            g_Gui.hud.boss_bar_alpha = 0;
        }
        // The bar still waits at the stop of its shown state until the
        // interrupt runs, so a shown bar hides on the frame it leaves.
        if (bar->stopped) {
            g_Gui.hud.boss_bar = GUI_BOSS_BAR_HIDDEN;
            g_Gui.hud.boss_health_shown = 0.0f;
            g_Gui.hud.boss_bar_alpha = 0;
        }
    }
    if (g_Gui.hud.boss_bar >= GUI_BOSS_BAR_SHOWN) {
        const float health = g_Gui.boss.health;
        float *const shown = &g_Gui.hud.boss_health_shown;
        if (health > *shown) {
            *shown += M_BOSS_HEALTH_RISE;
            if (health < *shown) {
                *shown = health;
            }
        } else if (health < *shown) {
            *shown -= M_BOSS_HEALTH_FALL;
            if (health > *shown) {
                *shown = health;
            }
        }
    }
}

static void M_UpdatePopup(
    GUI_POPUP *const popup, const int32_t frames, const bool slide)
{
    if (!popup->shown) {
        return;
    }
    if (slide) {
        popup->pos.x = popup->timer.current < 30
            ? Timer_AsFloat(&popup->timer) * -312.0f / 30.0f + 416.0f
            : 104.0f;
    }
    if (popup->timer.current >= frames) {
        popup->shown = false;
    }
    Timer_TickAt(&popup->timer, g_GameSpeed);
}

bool GuiStageClear_CountsLivesAndBombs(void)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    return gm->run.stage >= 7
        || (gm->run.stage == 6 && !gm->mode.in_practice
            && (!gm->mode.in_replay || Replay_HasStage(5)));
}

static void M_AddStageClearBonus(void)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    int32_t bonus = 0;
    bonus += gm->run.stage * 100000;
    bonus += g_Gui.stage_clear.graze * 50;
    bonus += g_Gui.stage_clear.point_items * 5000;
    bonus += g_Gui.stage_clear.cherry;
    if (GuiStageClear_CountsLivesAndBombs()) {
        bonus += X87_Ftol(gm->player.lives) * 2000000;
        bonus += X87_Ftol(gm->player.bombs) * 400000;
    }
    switch (gm->run.difficulty) {
    case 0:
        bonus /= 2;
        break;
    case 2:
        bonus = M_Wrap((int64_t)bonus * 12) / 10;
        break;
    case 3:
        bonus = M_Wrap((int64_t)bonus * 15) / 10;
        break;
    case 4:
    case 5:
        bonus = M_Wrap((int64_t)bonus * 2);
        break;
    default:
        break;
    }
    // The penalty for starting with more lives applies on every
    // difficulty and in practice, though the stage clear text shows it
    // only below Extra and outside practice.
    switch (gm->run.start_lives) {
    case 3:
        bonus = M_Wrap((int64_t)bonus * 5) / 10;
        break;
    case 4:
        bonus = M_Wrap((int64_t)bonus * 2) / 10;
        break;
    default:
        break;
    }
    g_Gui.stage_clear.total = bonus;
    // The original adds the bonus in ten calls, each dropping its last
    // digit.
    for (int32_t i = 0; i < 10; i++) {
        GameManager_AddScore(bonus);
    }
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_SHOWN;
}

static void M_UpdateInterface(void)
{
    if (g_Gui.msg.script < 0) {
        M_UpdateBossBar();
    }
    Anm_StepVms(g_Gui.hud.sprites, GUI_SPRITE_COUNT);
    Anm_StepVms(g_Gui.banners.titles, GUI_TITLE_COUNT);
    Anm_Step(&g_Gui.banners.bomb_portrait);
    Anm_Step(&g_Gui.banners.bomb_glow);
    Anm_Step(&g_Gui.banners.bomb_frame);
    Anm_Step(&g_Gui.banners.bomb_name);
    Anm_Step(&g_Gui.banners.spell_portrait);
    Anm_Step(&g_Gui.banners.spell_glow);
    Anm_Step(&g_Gui.banners.spell_frame);
    Anm_Step(&g_Gui.banners.spell_name);
    Anm_Step(&g_Gui.banners.bomb_bar);
    Anm_Step(&g_Gui.banners.spell_bar);
    Anm_Step(&g_Gui.banners.spell_score_bar);
    if (g_Gui.loading.icon.sprite >= 0) {
        if (Anm_Step(&g_Gui.loading.icon)) {
            g_Gui.loading.icon.sprite = -1;
        }
        if (Anm_Step(&g_Gui.loading.snapshot)) {
            g_Gui.loading.snapshot.sprite = -1;
        }
    }
    if (g_Gui.loading.tiles_running != 0) {
        int32_t running = GUI_TILE_COUNT;
        for (int32_t i = 0; i < GUI_TILE_COUNT; i++) {
            if (Anm_Step(&g_Gui.loading.tiles[i])) {
                running--;
            }
        }
        g_Gui.loading.tiles_running = running;
    }
    M_UpdatePopup(&g_Gui.popups.bonus, M_BONUS_POPUP_FRAMES, true);
    M_UpdatePopup(&g_Gui.popups.notice, M_NOTICE_POPUP_FRAMES, true);
    M_UpdatePopup(&g_Gui.popups.spell_bonus, M_SPELL_BONUS_POPUP_FRAMES, false);
    if (g_Gui.stage_clear.state == GUI_STAGE_CLEAR_PENDING) {
        M_AddStageClearBonus();
    }
}

static void M_NextScene(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    gm->score.shown = gm->score.value;
    if (gm->mode.in_practice) {
        Scene_Request(SCENE_GAME_RESULT);
    } else if (gm->run.stage < 6) {
        if (gm->mode.in_replay && !Replay_HasStage(gm->run.stage + 1)) {
            Scene_Request(SCENE_REPLAY_END);
            return;
        }
        Anm_InitVm(&g_Gui.loading.caption);
        Anm_SetSprite(&g_Gui.loading.caption, M_CAPTION_SPRITE);
        g_Gui.next_stage_requested = true;
        g_Gui.msg.script = GUI_MSG_TO_NEXT_STAGE;
    } else if (!gm->mode.in_replay) {
        if (gm->run.difficulty == 4) {
            gm->records.clears[gm->run.shot_index].no_continues[4] = 99;
        }
        if (gm->run.difficulty >= 4) {
            gm->records.play_stats.counts[gm->run.difficulty].clears++;
        }
        gm->mode.game_cleared = true;
        Scene_Request(
            gm->run.difficulty >= 4 ? SCENE_GAME_RESULT : SCENE_ENDING);
    } else {
        // Only a Phantasm replay is saved again, as in the original.
        if (gm->run.stage == 8) {
            Replay_ResaveIfScoreChanged();
        }
        Scene_Request(SCENE_REPLAY_END);
    }
}

// Starts the portrait script or sprite of instr. Retail reads the width of
// the new sprite at once, so a script or sprite without one crashes it.
static void M_ShowPortrait(const MSG_INSTR *const instr)
{
    ANM_VM *const portrait = &g_Gui.msg.portraits[instr->who];
    if (instr->opcode == GUI_MSG_OP_PORTRAIT_SCRIPT) {
        Anm_StartScript(
            portrait,
            instr->index
                + (instr->who != 0 ? M_BOSS_PORTRAIT_SCRIPT
                                   : M_PLAYER_PORTRAIT));
        ASSERT(portrait->sprite_data != nullptr);
        portrait->pos_offset.x =
            portrait->sprite_data->width > 128.0f ? -112.0f : 0.0f;
        return;
    }
    Anm_SetSprite(
        portrait,
        instr->index
            + (instr->who != 0 ? M_BOSS_PORTRAIT_SPRITE : M_PLAYER_PORTRAIT));
    ASSERT(portrait->sprite_data != nullptr);
    if (portrait->sprite_data->width > 256.0f) {
        portrait->pos_offset.x = -208.0f;
        portrait->pos_offset.y = -50.0f;
    } else if (portrait->sprite_data->width > 128.0f) {
        portrait->pos_offset.x = -80.0f;
    } else {
        portrait->pos_offset.x = 0.0f;
    }
}

static void M_ShowLine(const MSG_INSTR *const instr)
{
    if (instr->index == 0 && g_Gui.msg.lines[1].script_index >= 0) {
        Result_Log(
            Anm_RenderText(
                &g_Gui.msg.lines[1], m_TextColors[instr->who], 0, "%s", " "),
            LOG_LEVEL_WARNING);
    }
    ANM_VM *const line = &g_Gui.msg.lines[instr->index];
    Anm_StartScript(line, instr->index + M_LINE_SCRIPT);
    line->font_height = M_LINE_FONT_SIZE;
    line->font_width = line->font_height;
    if (instr->removes_note) {
        Note_Remove();
    }
    Result_Log(
        Anm_RenderText(line, m_TextColors[instr->who], 0, "%s", instr->text),
        LOG_LEVEL_WARNING);
    g_Gui.msg.wait_frames = 0;
}

// Returns whether the wait of instr holds the script this frame. The
// shoot button ends it only after 12 frames, and only on the frame it is
// pressed.
static bool M_Waits(const MSG_INSTR *const instr)
{
    if (M_SkipHeld()
        || (M_ShootPressed() && g_Gui.msg.wait_frames >= M_SHOOT_WAIT_FRAMES)) {
        return false;
    }
    if (g_Gui.msg.wait_frames >= instr->value) {
        return false;
    }
    g_Gui.msg.wait_frames++;
    return true;
}

static void M_ShowSongTitle(const int32_t song)
{
    ANM_VM *const title = &g_Gui.banners.titles[0];
    Anm_StartScript(
        title,
        g_GameManager.run.stage != 6 ? M_SONG_TITLE_SCRIPT
                                     : M_STAGE_6_SONG_TITLE_SCRIPT);
    Anm_SetSprite(title, song + M_SONG_TITLE_SPRITE);
    M_PlaySong(song);
}

static void M_ClearStage(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    g_Gui.stage_clear.point_items = gm->player.point_items;
    g_Gui.stage_clear.cherry = gm->cherry.max - gm->cherry.offset;
    g_Gui.stage_clear.graze = gm->player.graze;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
    if (gm->run.stage < 6) {
        Anm_StartScript(&g_Gui.loading.icon, M_LOADING_SCRIPT);
        Anm_StartScript(&g_Gui.loading.snapshot, M_SNAPSHOT_SCRIPT);
        M_RequestSnapshot();
    } else {
        gm->player.extends = -1;
    }
}

static M_RUN M_RunInstrs(void)
{
    const MSG_SCRIPT *const script = &m_Msg.scripts[g_Gui.msg.script];
    for (;;) {
        const MSG_INSTR *const instr = &script->instrs[g_Gui.msg.pc];
        if (g_Gui.msg.timer.current < instr->time) {
            return M_RUN_WAITING;
        }
        switch (instr->opcode) {
        case GUI_MSG_OP_END:
            g_Gui.msg.script = GUI_MSG_NONE;
            return M_RUN_ENDED;
        case GUI_MSG_OP_PORTRAIT_SCRIPT:
        case GUI_MSG_OP_PORTRAIT_SPRITE:
            M_ShowPortrait(instr);
            break;
        case GUI_MSG_OP_LINE:
            M_ShowLine(instr);
            break;
        case GUI_MSG_OP_WAIT:
            if (M_Waits(instr)) {
                return M_RUN_HALTED;
            }
            break;
        case GUI_MSG_OP_INTERRUPT:
            if (instr->who < GUI_MSG_SIDE_COUNT) {
                g_Gui.msg.portraits[instr->who].pending_interrupt =
                    instr->index;
            } else {
                g_Gui.msg.lines[instr->who - GUI_MSG_SIDE_COUNT]
                    .pending_interrupt = instr->index;
            }
            break;
        case GUI_MSG_OP_RELEASE_TIMELINE:
            g_Gui.msg.timeline_release++;
            break;
        case GUI_MSG_OP_SONG:
            M_ShowSongTitle(instr->value);
            break;
        case GUI_MSG_OP_NAMEPLATE: {
            ANM_VM *const nameplate = &g_Gui.msg.nameplates[instr->index];
            Anm_StartScript(nameplate, instr->index + M_NAMEPLATE_SCRIPT);
            if (instr->removes_note) {
                Note_Remove();
            }
            if (!GuiTextImage_Replaces(M_NAMEPLATE_SCRIPT)) {
                Result_Log(
                    Translation_HasBaseTsa()
                        ? Anm_RenderText(
                              nameplate, m_TextColors[instr->who], 0, "%s",
                              instr->text)
                        : Anm_RenderTextRightAligned(
                              nameplate, m_TextColors[instr->who], 0, "%s",
                              instr->text),
                    LOG_LEVEL_WARNING);
            }
            g_Gui.msg.wait_frames = 0;
            break;
        }
        case GUI_MSG_OP_STAGE_CLEAR:
            M_ClearStage();
            break;
        case GUI_MSG_OP_HALT:
            return M_RUN_HALTED;
        case GUI_MSG_OP_NEXT_SCENE:
            M_NextScene();
            return M_RUN_HALTED;
        case GUI_MSG_OP_FADE_OUT_MUSIC:
            Sound_FadeOutMusic(4.0f);
            break;
        case GUI_MSG_OP_SKIPPABLE:
            g_Gui.msg.skippable = instr->value != 0;
            break;
        case GUI_MSG_OP_FADE_OUT_SCREEN:
            ScreenEffect_FadeOutScreen(
                0x192, COLOR_ARGB(0x00, 0xff, 0xff, 0xff));
            break;
        default:
            break;
        }
        g_Gui.msg.pc++;
    }
}

static void M_RunMsg(void)
{
    if (g_Gui.msg.script < 0) {
        return;
    }
    if (g_Gui.msg.timeline_release > 0) {
        g_Gui.msg.timeline_release--;
    }
    if (M_SkipHeld()) {
        Timer_Set(
            &g_Gui.msg.timer,
            m_Msg.scripts[g_Gui.msg.script].instrs[g_Gui.msg.pc].time);
    }
    // Ends a pending Border too, which then awards its bonus.
    if (g_Player.border_state != PLAYER_BORDER_NONE) {
        Player_EndBorder();
    }
    if (g_Player.state.value != PLAYER_DYING) {
        Item_CollectAll();
    }
    switch (M_RunInstrs()) {
    case M_RUN_ENDED:
        return;
    case M_RUN_WAITING:
        Timer_TickAt(&g_Gui.msg.timer, g_GameSpeed);
        break;
    case M_RUN_HALTED:
        break;
    }
    for (int32_t i = 0; i < GUI_MSG_SIDE_COUNT; i++) {
        Anm_Step(&g_Gui.msg.portraits[i]);
    }
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_Step(&g_Gui.msg.lines[i]);
    }
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_Step(&g_Gui.msg.nameplates[i]);
    }
    if (g_Gui.msg.timer.current < M_BOX_OPEN_FRAMES && M_SkipHeld()) {
        Timer_Set(&g_Gui.msg.timer, M_BOX_OPEN_FRAMES);
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    if (g_Gui.next_stage_requested) {
        Scene_Request(SCENE_NEXT_STAGE);
        g_Gui.next_stage_requested = false;
    }
    M_UpdateInterface();
    M_RunMsg();
    g_Gui.frames++;
    if (g_GameManager.run.stage == 6 && g_Gui.frames == 300) {
        M_PlaySong(0);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    return GuiDraw_Frame();
}

static void M_HideInterface(void)
{
    g_Gui.loading.icon.sprite = -1;
    g_Gui.loading.caption.sprite = -1;
    g_Gui.loading.snapshot.sprite = -1;
    g_Gui.loading.tiles_running = 0;
}

static RESULT M_LoadInterface(void)
{
    MUST(Anm_Load(M_FRONT_SLOT, "data/front.anm", M_FRONT_SCRIPT));
    M_HideInterface();
    const uint8_t character = g_GameManager.run.character;
    if (character < ARRAY_SIZE(m_CharacterFiles)) {
        const M_CHARACTER_FILES *const files = &m_CharacterFiles[character];
        MUST(Anm_Load(M_FACE_SLOT, files->face, M_PLAYER_PORTRAIT));
        MUST(Anm_Load(M_LOADING_SLOT, files->loading, M_LOADING_SCRIPT));
    }
    return OK;
}

// Starts the transition from the last stage: the snapshot of its play
// area breaks up into tiles.
static void M_StartTransition(void)
{
    M_HideInterface();
    Anm_StartScript(&g_Gui.loading.snapshot, M_SNAPSHOT_SCRIPT);
    g_Gui.loading.snapshot.pending_interrupt = 1;
    M_RequestSnapshot();
    for (int32_t y = 0; y < GUI_TILE_ROWS; y++) {
        for (int32_t x = 0; x < GUI_TILE_COLUMNS; x++) {
            ANM_VM *const tile = &g_Gui.loading.tiles[y * GUI_TILE_COLUMNS + x];
            Anm_StartScript(tile, ((y + x) & 1) + M_TILE_SCRIPT);
            tile->int_vars[4] = y + x * 2;
            tile->pos.x = (float)x * 32.0f - 0.5f + 16.0f;
            tile->pos.y = (float)y * 32.0f - 0.5f + 16.0f;
            tile->pos.z = 0.0f;
            tile->uv_scroll_x = (float)x * 32.0f / 512.0f;
            tile->uv_scroll_y = (float)y * 32.0f / 512.0f;
        }
    }
    g_Gui.loading.tiles_running = GUI_TILE_COUNT;
}

static void M_PatchMsg(const char *const path)
{
    ARENA arena;
    Arena_Init(&arena, "dialogue jdiff", M_DIFF_ARENA_CAPACITY);
    const JSON_VALUE *const diff =
        Files_LoadDiff(path, Arena_Allocator(&arena));
    MsgDiff_Apply(
        m_Msg.scripts, m_Msg.count, diff,
        (MSG_DIFF_FORMAT) {
            .line = GUI_MSG_OP_LINE,
            .nameplate = GUI_MSG_OP_NAMEPLATE,
            .line_count = GUI_MSG_LINE_COUNT,
            .is_final = GuiMsg_IsFinal,
        });
    Arena_Free(&arena);
}

static RESULT M_LoadMsg(const char *const path)
{
    GuiMsg_Free(&m_Msg);
    READER reader;
    MUST(Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT result = GuiMsg_Read(&reader, &m_Msg);
    Reader_Close(&reader);
    MUST(result, "%s", path);
    M_PatchMsg(path);
    g_Gui.msg.script = GUI_MSG_NONE;
    g_Gui.msg.pc = 0;
    return OK;
}

static RESULT M_LoadStage(void)
{
    const int32_t stage = g_GameManager.run.stage;
    FAIL_IF(stage < 1 || stage > 8, "stage %d has no interface", stage);
    M_ShowArt(M_STAGE_ART_SPRITE + 2 * (stage - 1));
    char path[M_PATH_SIZE];
    snprintf(path, sizeof(path), "data/face_%02d_00.anm", stage);
    MUST(Anm_Load(M_STAGE_FACE_SLOT, path, M_BOSS_PORTRAIT_SPRITE));
    snprintf(path, sizeof(path), "data/std%dtxt.anm", stage);
    MUST(Anm_Load(M_STAGE_TEXT_SLOT, path, M_TITLE_SCRIPT));
    snprintf(path, sizeof(path), "data/msg%d.dat", stage);
    return M_LoadMsg(path);
}

static void M_StopBanner(ANM_VM *const vm)
{
    vm->script = nullptr;
    vm->shown = false;
}

static void M_StartBanners(void)
{
    g_Gui.boss.present = false;
    g_Gui.hud.boss_bar = GUI_BOSS_BAR_HIDDEN;
    g_Gui.boss.health = 0.0f;
    g_Gui.hud.boss_health_shown = 0.0f;
    GUI *const gui = &g_Gui;
    Anm_StartScript(&gui->banners.bomb_portrait, M_BOMB_PORTRAIT_SCRIPT);
    Anm_StartScript(&gui->banners.spell_portrait, M_SPELL_PORTRAIT_SCRIPT);
    Anm_StartScript(&gui->banners.bomb_name, M_BOMB_NAME_SCRIPT);
    Anm_StartScript(&gui->banners.spell_name, M_SPELL_NAME_SCRIPT);
    Anm_StartVmSeries(gui->banners.titles, M_TITLE_SCRIPT, GUI_TITLE_COUNT);
    Anm_StartScript(&gui->banners.bomb_bar, 1);
    Anm_StartScript(&gui->banners.spell_bar, 0);
    Anm_StartScript(&gui->banners.spell_score_bar, 2);
    Anm_StartScript(&gui->banners.spell_score_digit, 3);
    M_StopBanner(&gui->banners.bomb_portrait);
    M_StopBanner(&gui->banners.bomb_glow);
    M_StopBanner(&gui->banners.bomb_frame);
    M_StopBanner(&gui->banners.bomb_name);
    M_StopBanner(&gui->banners.spell_portrait);
    M_StopBanner(&gui->banners.spell_glow);
    M_StopBanner(&gui->banners.spell_frame);
    M_StopBanner(&gui->banners.spell_name);
    gui->banners.bomb_name.font_width = M_NAME_FONT_SIZE;
    gui->banners.bomb_name.font_height = M_NAME_FONT_SIZE;
    gui->banners.spell_name.font_width = M_NAME_FONT_SIZE;
    gui->banners.spell_name.font_height = M_NAME_FONT_SIZE;
    gui->msg.script = GUI_MSG_NONE;
    gui->stage_clear.state = GUI_STAGE_CLEAR_NONE;
    gui->popups.bonus.shown = false;
    gui->popups.notice.shown = false;
    gui->popups.spell_bonus.shown = false;
}

static void M_Release(void)
{
    Anm_Release(M_STAGE_TEXT_SLOT);
    for (int32_t slot = M_STAGE_FACE_SLOT; slot <= 0x1f; slot++) {
        Anm_Release(slot);
    }
    GuiMsg_Free(&m_Msg);
    GuiTextImage_Release();
    if (!M_KeepsInterface()) {
        Anm_Release(M_FRONT_SLOT);
        Anm_Release(M_LOADING_SLOT);
        Anm_Release(M_FACE_SLOT);
        Anm_Release(0x1a);
        Anm_Release(0x1b);
        Anm_Release(M_FRONT_TEXTURE_SLOT);
        // The original frees the dialogue state, so no dialogue runs.
        g_Gui.msg.script = GUI_MSG_NONE;
    }
}

static RESULT M_Load(void)
{
    g_Gui.frames = 0;
    if (!M_KeepsInterface()) {
        MUST(M_LoadInterface());
    } else {
        M_StartTransition();
    }
    MUST(M_LoadStage());
    if (!M_KeepsInterface()) {
        for (int32_t i = 0; i < GUI_SPRITE_COUNT; i++) {
            Anm_StartScript(&g_Gui.hud.sprites[i], i + M_FRONT_SCRIPT);
        }
    }
    M_StartBanners();
    GuiTextImage_Load();
    GuiTextImage_ShowStage(g_GameManager.run.stage);
    return OK;
}

static RESULT M_Added(void *const arg)
{
    const RESULT result = M_Load();
    if (!IS_OK(result)) {
        M_Release();
    }
    return result;
}

static void M_Deleted(void *const arg)
{
    M_Release();
}

static void M_ShowPopup(
    GUI_POPUP *const popup, const VEC3 pos, const int32_t value)
{
    popup->pos = pos;
    popup->shown = true;
    Timer_Set(&popup->timer, 0);
    popup->value = value;
}

static void M_ShowNotice(const GUI_NOTICE kind, const int32_t value)
{
    M_ShowPopup(&g_Gui.popups.notice, (VEC3) { 416.0f, 168.0f, 0.0f }, value);
    g_Gui.popups.notice_kind = kind;
}

RESULT Gui_Register(void)
{
    if (!M_KeepsInterface()) {
        memset(&g_Gui, 0, sizeof(g_Gui));
    }
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    return Chain_AddDraw(&g_Chain, &m_DrawElem, M_DRAW_PRIORITY);
}

void Gui_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
}

void Gui_StartMsg(const int32_t index)
{
    ASSERT(index >= 0);
    if (index >= m_Msg.count) {
        return;
    }
    memset(&g_Gui.msg, 0, sizeof(g_Gui.msg));
    g_Gui.msg.script = index;
    g_Gui.msg.pc = 0;
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        g_Gui.msg.lines[i].script_index = -1;
    }
    g_Gui.msg.skippable = true;
    Bullet_ClearAllToItems();
    Enemy_ClearForBonus(0, 0);
    Item_CollectAll();
    if (index % 10 != 0) {
        return;
    }
    const int32_t stage = g_GameManager.run.stage;
    if (stage >= 1 && stage <= 8) {
        M_ShowArt(M_STAGE_ART_SPRITE + 2 * (stage - 1) + 1);
    }
    switch (stage) {
    case 6:
    case 8:
        g_Stage.spell_background.offset = 2;
        g_Bullets.clear_item = ITEM_BOSS_STAR;
        break;
    case 7:
        g_Stage.spell_background.offset = 1;
        g_Stage.spell_background.count = 2;
        g_Bullets.clear_item = ITEM_BOSS_STAR;
        break;
    default:
        break;
    }
}

bool Gui_IsMsgRunning(void)
{
    return g_Gui.msg.script >= 0 || g_Gui.msg.script == GUI_MSG_TO_NEXT_STAGE;
}

bool Gui_MsgHoldsTimeline(void)
{
    if (g_Gui.msg.timeline_release > 0) {
        return false;
    }
    return g_Gui.msg.script >= 0;
}

bool Gui_IsMsgSkippable(void)
{
    return g_Gui.msg.skippable;
}

bool Gui_IsLoadingShown(void)
{
    return g_Gui.loading.icon.sprite >= 0 && g_Gui.loading.icon.stopped;
}

void Gui_ShowBombName(const int32_t sprite, const char *const name)
{
    GUI *const gui = &g_Gui;
    Anm_StartScript(&gui->banners.bomb_portrait, M_BOMB_PORTRAIT_SCRIPT);
    Anm_SetSprite(&gui->banners.bomb_portrait, sprite);
    Anm_StartScript(&gui->banners.bomb_glow, M_BOMB_GLOW_SCRIPT);
    Anm_SetSprite(&gui->banners.bomb_glow, M_GLOW_SPRITE);
    Anm_StartScript(&gui->banners.bomb_frame, M_BOMB_FRAME_SCRIPT);
    Anm_SetSprite(&gui->banners.bomb_frame, M_GLOW_SPRITE);
    Anm_StartScript(&gui->banners.bomb_name, M_BOMB_NAME_SCRIPT);
    Result_Log(
        Anm_RenderText(
            &gui->banners.bomb_name, COLOR_ARGB(0x00, 0xf0, 0xf0, 0xff), 0,
            "%s", name),
        LOG_LEVEL_WARNING);
    gui->banners.bomb_bar.pending_interrupt = 1;
    Sound_Queue(SOUND_CARD);
}

void Gui_EndBombName(void)
{
    g_Gui.banners.bomb_name.pending_interrupt = 1;
    g_Gui.banners.bomb_bar.pending_interrupt = 2;
}

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
    Note_Remove();
    GUI *const gui = &g_Gui;
    if (sprite >= 0) {
        ANM_VM *const portrait = &gui->banners.spell_portrait;
        Anm_StartScript(portrait, M_SPELL_PORTRAIT_SCRIPT);
        Anm_SetSprite(portrait, sprite + M_BOSS_PORTRAIT_SPRITE);
        // Retail reads the width at once and crashes without a sprite.
        ASSERT(portrait->sprite_data != nullptr);
        if (portrait->sprite_data->width > 256.0f) {
            portrait->pos_offset.x = -288.0f;
        } else if (portrait->sprite_data->width > 128.0f) {
            portrait->pos_offset.x = -112.0f;
        } else {
            portrait->pos_offset.x = 0.0f;
        }
    }
    Anm_StartScript(&gui->banners.spell_glow, M_SPELL_GLOW_SCRIPT);
    Anm_SetSprite(&gui->banners.spell_glow, M_GLOW_SPRITE);
    Anm_StartScript(&gui->banners.spell_frame, M_SPELL_FRAME_SCRIPT);
    Anm_SetSprite(&gui->banners.spell_frame, M_GLOW_SPRITE);
    Anm_StartScript(&gui->banners.spell_name, M_SPELL_NAME_SCRIPT);
    Result_Log(
        Anm_RenderTextRightAligned(
            &gui->banners.spell_name, COLOR_ARGB(0x00, 0xff, 0xf0, 0xf0), 0,
            "%s", name),
        LOG_LEVEL_WARNING);
    gui->banners.spell_bar.pending_interrupt = 1;
    gui->banners.spell_score_bar.pending_interrupt = 1;
    Sound_Queue(SOUND_CARD);
}

void Gui_EndSpellCardName(void)
{
    g_Gui.banners.spell_name.pending_interrupt = 1;
    g_Gui.banners.spell_bar.pending_interrupt = 2;
    g_Gui.banners.spell_score_bar.pending_interrupt = 2;
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
    M_ShowPopup(
        &g_Gui.popups.spell_bonus, (VEC3) { 224.0f, 16.0f, 0.0f }, bonus);
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
    M_ShowPopup(&g_Gui.popups.bonus, (VEC3) { 416.0f, 48.0f, 0.0f }, bonus);
}

void Gui_ShowFullPowerPopup(void)
{
    M_ShowNotice(GUI_NOTICE_FULL_POWER, 0);
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
    M_ShowNotice(GUI_NOTICE_CHERRY_MAX, cherry);
}

void Gui_ShowBorderStartPopup(void)
{
    M_ShowNotice(GUI_NOTICE_BORDER_START, 0);
}

void Gui_ShowBorderBonusPopup(const int32_t points)
{
    M_ShowNotice(GUI_NOTICE_BORDER_BONUS, points);
}
