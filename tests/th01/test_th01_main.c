#include "tests/test.h"
#include "tests/th01/th01_assets.h"
#include "tests/th01/th01_world.h"
#include "th01/ending.h"
#include "th01/gameplay/end.c"
#include "th01/gameplay/exit.c"
#include "th01/gameplay/gameplay.c"
#include "th01/gameplay/life.c"
#include "th01/gameplay/pause.c"
#include "th01/gameplay/stage.c"
#include "th01/hud/hp_private.h"
#include "th01/hud/hud_private.h"
#include "th01/main.h"
#include "th01/music.c"
#include "th01/opening.h"
#include "th01/score.h"
#include "th01/sound.c"

#include "core/audio.h"
#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/gfx.h"
#include "core/io/reader.h"
#include "core/text/font_registry.h"
#include "core/vfs.h"
#include "pc98/vsync.h"
#include "pc98/zun/bfnt.h"

#include <string.h>

struct VFS {
    bool user;
};
struct GFX_TEXTURE {
    bool alive;
};

typedef struct {
    bool in_tick;
    bool finish_score;
    bool finish_ending;
    bool queue_on_quit;
    bool fail_save;
    int asset_step;
    int fail_asset;
    int texture_step;
    int fail_texture;
    int textures_live;
    int flush_calls;
    int written;
    int opening_begins;
    int opening_ticks;
    int score_begins, score_ticks, ending_begins, ending_ticks, ending_clears,
        switch_begins;
    OPENING_ACTION action;
    SCORE_CONTEXT score_context;
    SCORE_ENTRY score_entry;
    SCORE_SAVES *saves;
    const ARENA *arena;
    GFX_TEXTURE textures[3];
} MAIN_FIXTURE;

static MAIN_FIXTURE m_Fixture;

static OPENING m_Opening;
const OPENING *const g_Opening = &m_Opening;
static END_RESULT m_EndResult;
const END_RESULT *const g_EndResult = &m_EndResult;

static RESULT M_Asset(void)
{
    CHECK(!m_Fixture.in_tick);
    m_Fixture.asset_step++;
    FAIL_IF(
        m_Fixture.asset_step == m_Fixture.fail_asset, "authored load failure");
    return OK;
}

bool Vfs_Exists(const VFS *vfs, const char *path)
{
    CHECK(!m_Fixture.in_tick);
    return false;
}

RESULT Vfs_Open(VFS *vfs, const char *path, VFS_MODE mode, READER *out)
{
    CHECK(!m_Fixture.in_tick);
    Reader_OpenBuffer(out, nullptr, 0, nullptr);
    return OK;
}

VFS *Shell_GetFiles(void)
{
    static VFS files;
    CHECK(!m_Fixture.in_tick);
    return &files;
}

VFS *Shell_GetUserFiles(void)
{
    static VFS files = { .user = true };
    CHECK(!m_Fixture.in_tick);
    return &files;
}

bool Shell_GetKeyPress(INPUT_BINDING *key)
{
    return false;
}

RESULT Bfnt_Load(READER *reader, const ALLOCATOR *allocator, BFNT *out)
{
    MUST(M_Asset());
    *out = (BFNT) { .width = 64,
                    .height = 16,
                    .count = 16,
                    .bits_per_pixel = 4,
                    .has_palette = true,
                    .pixels = Allocator_AllocZ(allocator, 64 * 16 * 16) };
    return OK;
}

RESULT Opening_MountArchive(VFS *vfs)
{
    return M_Asset();
}

RESULT OpeningAssets_Load(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    OPENING_ASSETS *out)
{
    return M_Asset();
}

RESULT GameplayAssets_Load(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    GAMEPLAY_ASSETS *out)
{
    MUST(M_Asset());
    *out = *Th01TestAssets_Create(allocator);
    m_Fixture.arena = allocator->context;
    return OK;
}

static SCORE_SAVES m_Saves;

RESULT ScoreData_Load(VFS *original, VFS *user_files, SCORE_SAVES *out)
{
    CHECK(!m_Fixture.in_tick);
    *out = (SCORE_SAVES) {};
    m_Fixture.saves = out;
    return OK;
}

RESULT ScoreData_WriteSaves(SCORE_SAVES *saves)
{
    m_Fixture.flush_calls++;
    for (size_t rank = 0; rank < SCORE_RANKS; rank++) {
        if (saves->pending[rank]) {
            m_Fixture.written++;
            saves->pending[rank] = false;
        }
    }
    FAIL_IF(m_Fixture.fail_save, "authored save failure");
    return OK;
}

RESULT ScoreAssets_Load(
    VFS *vfs, VFS *user_files, const FONT_ROM *rom, const ALLOCATOR *allocator,
    SCORE_ASSETS *out)
{
    MUST(ScoreData_Load(vfs, user_files, &m_Saves));
    return M_Asset();
}

void Score_SyncSession(void)
{
    Session_SyncTableHighScore(
        (uint32_t)m_Saves.tables[g_Session->settings.difficulty]
            .entries[0]
            .score);
}

RESULT Score_FlushSaves(void)
{
    return ScoreData_WriteSaves(&m_Saves);
}

RESULT EndingAssets_Load(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    ENDING_ASSETS *out)
{
    return M_Asset();
}

OPENING_EVENTS Opening_Init(const OPENING_ASSETS *assets)
{
    memset(&m_Opening, 0, sizeof(m_Opening));
    m_Opening.frame_rand = 1;
    m_Fixture.opening_begins++;
    return (OPENING_EVENTS) { .audio = OPENING_AUDIO_PLAY,
                              .track = 0,
                              .clear_key_buffer = true };
}

OPENING_EVENTS Opening_Tick(const TH01_INPUT *raw, bool any_key)
{
    m_Fixture.opening_ticks++;
    const OPENING_ACTION action = m_Fixture.action;
    m_Fixture.action = OPENING_ACTION_NONE;
    if (action == OPENING_ACTION_QUIT && m_Fixture.queue_on_quit) {
        m_Fixture.saves->pending[0] = true;
    }
    return (OPENING_EVENTS) { .action = action };
}

void Opening_Close(void)
{
    memset(&m_Opening, 0, sizeof(m_Opening));
}

void Opening_CountSwitchedVSync(void)
{
    m_Opening.count1 = 1;
}

void Score_Close(void)
{
}

void Score_Begin(
    const SCORE_ASSETS *assets, SCORE_CONTEXT context, int32_t rank,
    const SCORE_ENTRY *entry)
{
    m_Fixture.score_context = context;
    m_Fixture.score_entry = *entry;
    m_Fixture.score_begins++;
}

bool Score_Tick(const TH01_INPUT *input, bool reference_edge)
{
    m_Fixture.score_ticks++;
    if (m_Fixture.finish_score) {
        m_Fixture.saves->pending[0] = true;
    }
    return m_Fixture.finish_score;
}

void Ending_Init(const ENDING_ASSETS *assets)
{
    memset(&m_EndResult, 0, sizeof(m_EndResult));
}

void Ending_Close(void)
{
    memset(&m_EndResult, 0, sizeof(m_EndResult));
}

void Ending_Begin(void)
{
    m_Fixture.ending_begins++;
    memset(&m_EndResult, 0, sizeof(m_EndResult));
    m_EndResult.route = SESSION_END_MAKAI;
    m_EndResult.score = 12345;
    m_EndResult.rank = 1;
}

bool Ending_Tick(const TH01_INPUT *input, bool reference_edge)
{
    m_Fixture.ending_ticks++;
    return m_Fixture.finish_ending;
}

void Ending_Clear(void)
{
    m_Fixture.ending_clears++;
}

void Ending_BeginSwitch(void)
{
    m_Fixture.switch_begins++;
}

bool Ending_SwitchTick(bool reference_edge)
{
    return true;
}

RESULT GfxTexture_CreateIndexed(
    int32_t width, int32_t height, GFX_TEXTURE **out)
{
    CHECK(!m_Fixture.in_tick);
    *out = nullptr;
    const int step = ++m_Fixture.texture_step;
    FAIL_IF(step == m_Fixture.fail_texture, "authored texture failure");
    CHECK(step >= 1 && step <= 3);
    *out = &m_Fixture.textures[step - 1];
    (*out)->alive = true;
    m_Fixture.textures_live++;
    return OK;
}

void GfxTexture_Free(GFX_TEXTURE *texture)
{
    if (texture != nullptr) {
        CHECK(texture->alive);
        texture->alive = false;
        m_Fixture.textures_live--;
    }
}

void GfxTexture_UploadIndices(
    GFX_TEXTURE *texture, GFX_RECT rect, const uint8_t *indices)
{
    CHECK(texture != nullptr && texture->alive);
}

void GfxTexture_UploadPalette(
    GFX_TEXTURE *texture, int32_t first, int32_t count, const GFX_COLOR *colors)
{
    CHECK(texture != nullptr && texture->alive);
}

void Gfx_DrawQuad(
    const GFX_STATE *state, GFX_TEXTURE *texture, const GFX_VERTEX vertices[4],
    uint64_t id)
{
    CHECK(texture != nullptr && texture->alive);
}

static void M_Reset(void)
{
    memset(&m_Fixture, 0, sizeof(m_Fixture));
    FontRegistry_Reset(false);
    void *const rom = Mem_AllocZ(288768);
    FontRegistry_AddFile("FONT.ROM", "test", rom, 288768);
    Mem_Free(rom);
    CHECK_EQ_INT(m_Fixture.flush_calls, 0);
}

static bool M_Tick(const TH01_INPUT raw)
{
    const size_t before = m_Fixture.arena->used;
    const int writes = m_Fixture.flush_calls;
    m_Fixture.in_tick = true;
    const bool running = g_GameDesc.tick(&raw);
    m_Fixture.in_tick = false;
    CHECK_EQ_INT(m_Fixture.arena->used, before);
    CHECK_EQ_INT(m_Fixture.flush_calls, writes + 1);
    return running;
}

static void M_Opening(void)
{
    for (int i = 0; i < 4 && m_Fixture.opening_begins == 0; i++) {
        CHECK(M_Tick((TH01_INPUT) { .shot = true }));
    }
    CHECK_EQ_INT(m_Fixture.opening_begins, 1);
}

static void M_Play(void)
{
    M_Opening();
    m_Fixture.action = OPENING_ACTION_START;
    for (int i = 0; i < 4 && (*g_Th01Scene) != TH01_SCENE_GAMEPLAY; i++) {
        CHECK(M_Tick((TH01_INPUT) {}));
    }
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_GAMEPLAY);
    CHECK(p_Hud->numbers != nullptr);
    CHECK(p_Health->backgrounds != nullptr);
}

static void M_Shutdown(void)
{
    g_GameDesc.shutdown();
    CHECK_EQ_INT(m_Fixture.textures_live, 0);
    Audio_Shutdown();
}

TEST(real_main_initializes_public_gameplay_headless_and_with_audio)
{
    for (int audio = 0; audio < 2; audio++) {
        M_Reset();
        if (audio != 0) {
            CHECK(Result_Ignore(Audio_Init()));
        }
        CHECK(Result_Log(g_GameDesc.init(), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(m_Fixture.textures_live, 3);
        M_Play();
        CHECK((m_Output != nullptr) == (audio != 0));
        M_Shutdown();
    }
}

TEST(real_main_partial_init_cleanup_never_writes_scores)
{
    for (int failure = 1; failure <= 9; failure++) {
        M_Reset();
        CHECK(Result_Ignore(Audio_Init()));
        if (failure <= 6) {
            m_Fixture.fail_asset = failure;
        } else {
            m_Fixture.fail_texture = failure - 6;
        }
        CHECK(!Result_Ignore(g_GameDesc.init()));
        if (m_Fixture.saves != nullptr) {
            m_Fixture.saves->pending[0] = true;
        }
        CHECK_EQ_INT(m_Fixture.flush_calls, 0);
        M_Shutdown();
        CHECK_EQ_INT(m_Fixture.flush_calls, 0);
    }
}

TEST(real_main_fails_at_startup_without_a_font)
{
    M_Reset();
    FontRegistry_Reset(false);
    CHECK(!Result_Ignore(g_GameDesc.init()));
    CHECK_EQ_INT(m_Fixture.asset_step, 2);
    M_Shutdown();
}

TEST(final_quit_tick_saves_before_returning_to_the_host)
{
    for (int failure = 0; failure < 2; failure++) {
        M_Reset();
        CHECK(Result_Ignore(g_GameDesc.init()));
        M_Opening();
        m_Fixture.action = OPENING_ACTION_QUIT;
        m_Fixture.queue_on_quit = true;
        m_Fixture.fail_save = failure != 0;
        bool running = true;
        for (int i = 0; i < 4 && running; i++) {
            running = M_Tick((TH01_INPUT) {});
        }
        CHECK(!running);
        CHECK(!m_Fixture.saves->pending[0]);
        CHECK_EQ_INT(m_Fixture.written, 1);
        M_Shutdown();
        CHECK_EQ_INT(m_Fixture.written, 1);
    }
}

TEST(gameover_registration_saves_before_returning_to_continue)
{
    M_Reset();
    CHECK(Result_Ignore(g_GameDesc.init()));
    M_Play();
    p_Gameplay->sequence = GAMEPLAY_SEQUENCE_TERMINAL;
    p_Gameplay->request = GAMEPLAY_REQUEST_SCORE;
    p_Session->score = 12345;
    p_Session->stage_id = 4;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_SCORE);
    CHECK_EQ_INT(m_Fixture.score_begins, 1);
    CHECK_EQ_INT(m_Fixture.score_context, SCORE_GAMEOVER);
    CHECK_EQ_INT(m_Fixture.score_entry.score, 12345);
    CHECK_EQ_INT(m_Fixture.score_entry.stage, 5);
    CHECK_EQ_STR(m_Fixture.score_entry.route, "社");
    m_Fixture.finish_score = true;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_Gameplay->request, GAMEPLAY_REQUEST_NONE);
    CHECK_EQ_INT(g_Gameplay->sequence, GAMEPLAY_SEQUENCE_END);
    CHECK_EQ_INT(m_EndPhase, GP_END_CONTINUE_DELAY);
    CHECK(!m_Fixture.saves->pending[0]);
    CHECK_EQ_INT(m_Fixture.written, 1);
    CHECK_EQ_INT(m_Fixture.switch_begins, 0);
    M_Shutdown();
    CHECK_EQ_INT(m_Fixture.written, 1);
}

TEST(ending_registration_saves_before_switching_back_to_opening)
{
    M_Reset();
    CHECK(Result_Ignore(g_GameDesc.init()));
    M_Play();
    p_Gameplay->sequence = GAMEPLAY_SEQUENCE_TERMINAL;
    p_Gameplay->request = GAMEPLAY_REQUEST_ENDING;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_ENDING);
    CHECK_EQ_INT(m_Fixture.ending_begins, 1);
    CHECK_EQ_INT(m_Fixture.ending_ticks, 1);
    m_Fixture.finish_ending = true;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_ENDING_SCORE);
    CHECK_EQ_INT(m_Fixture.score_context, SCORE_ENDING);
    CHECK_EQ_INT(m_Fixture.score_entry.score, 12345);
    CHECK_EQ_INT(m_Fixture.score_entry.stage, 50);
    CHECK_EQ_STR(m_Fixture.score_entry.route, "完");
    m_Fixture.finish_score = true;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.switch_begins, 1);
    CHECK(!m_Fixture.saves->pending[0]);
    CHECK_EQ_INT(m_Fixture.written, 1);
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_OPENING);
    CHECK_EQ_INT(m_Fixture.ending_clears, 1);
    CHECK_EQ_INT(m_Fixture.opening_begins, 2);
    CHECK_EQ_INT(m_Fixture.written, 1);
    M_Shutdown();
}

typedef struct {
    int calls;
    TH01_SCENE scene;
} VSYNC_LOG;

static void M_OnVSync(void *const arg)
{
    VSYNC_LOG *const log = arg;
    log->calls++;
    log->scene = (*g_Th01Scene);
}

TEST(vsync_hook_runs_before_each_reference_vsync_and_scene_changes)
{
    M_Reset();
    CHECK(Result_Ignore(g_GameDesc.init()));
    VSYNC_LOG log = {};
    Th01Hooks_Set((TH01_HOOKS) { .on_vsync = M_OnVSync, .arg = &log });
    VSYNC_CLOCK clock = {};
    int edges = 0;
    for (int i = 0; i < 30; i++) {
        edges += VSync_Advance(&clock) ? 1 : 0;
        CHECK(M_Tick((TH01_INPUT) {}));
        CHECK_EQ_INT(log.calls, edges);
    }
    CHECK(edges > 0);
    CHECK_EQ_INT(log.scene, TH01_SCENE_LOGO);
    M_Opening();
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_OPENING);
    M_Play();
    CHECK_EQ_INT((*g_Th01Scene), TH01_SCENE_GAMEPLAY);
    CHECK(g_Stage->frame == 0);
    CHECK_EQ_INT(g_Opening->frame_rand, 1);
    Th01Hooks_Set((TH01_HOOKS) {});
    M_Shutdown();
}
