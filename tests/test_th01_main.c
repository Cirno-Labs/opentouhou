#include "th01/main.h"
#include "th01/gameplay_internal.h"
#include "th01/logo.h"
#include "th01/score.h"
#include "th01/ending.h"
#include "th01_assets.h"

#include "core/audio.h"
#include "core/base/arena.h"
#include "core/gfx.h"
#include "core/text/font_registry.h"
#include "core/vfs.h"

#include "test.h"

struct VFS {
    bool user;
};
struct GFX_TEXTURE {
    bool alive;
};
struct ENDING {
    END_RESULT result;
};

typedef struct {
    bool in_tick, finish_score, finish_ending, queue_on_quit;
    int asset_step, fail_asset, texture_step, fail_texture;
    int textures_live, flush_calls, written;
    int opening_begins, opening_ticks, gameplay_ticks, resumes;
    int score_begins, score_ticks, ending_begins, ending_ticks, ending_clears,
        switch_begins;
    OPENING_ACTION action;
    GAMEPLAY_REQUEST request;
    SCORE_CONTEXT score_context;
    SCORE_SAVES *saves;
    GAMEPLAY *game;
    const ARENA *arena;
    GFX_TEXTURE textures[3];
} MAIN_FIXTURE;

static MAIN_FIXTURE m_Fixture;

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
                    .pixels = Allocator_AllocZ(allocator, 64 * 16 * 16) };
    return OK;
}

RESULT Opening_MountArchive(VFS *vfs)
{
    return M_Asset();
}

RESULT Opening_LoadAssets(
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

RESULT ScoreData_Load(VFS *original, VFS *user_files, SCORE_SAVES *out)
{
    CHECK(!m_Fixture.in_tick);
    *out = (SCORE_SAVES) {};
    m_Fixture.saves = out;
    return OK;
}

RESULT ScoreData_WriteSaves(SCORE_SAVES *saves)
{
    CHECK(!m_Fixture.in_tick);
    m_Fixture.flush_calls++;
    for (size_t rank = 0; rank < SCORE_RANKS; rank++) {
        if (saves->pending[rank]) {
            m_Fixture.written++;
            saves->pending[rank] = false;
        }
    }
    return OK;
}

RESULT Score_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const SCORE_SAVES *saves,
    const ALLOCATOR *allocator, SCORE_ASSETS *out)
{
    return M_Asset();
}

RESULT Ending_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    ENDING_ASSETS *out)
{
    return M_Asset();
}

OPENING_EVENTS Opening_Init(
    OPENING *opening, VIDEO *video, TH01_SETTINGS *settings,
    const OPENING_ASSETS *assets)
{
    opening->frame_rand = 1;
    m_Fixture.opening_begins++;
    return (OPENING_EVENTS) { .audio = OPENING_AUDIO_PLAY,
                              .track = 0,
                              .clear_key_buffer = true };
}

OPENING_EVENTS Opening_Tick(
    OPENING *opening, const TH01_INPUT *raw, bool any_key)
{
    m_Fixture.opening_ticks++;
    const OPENING_ACTION action = m_Fixture.action;
    m_Fixture.action = OPENING_ACTION_NONE;
    if (action == OPENING_ACTION_QUIT && m_Fixture.queue_on_quit) {
        m_Fixture.saves->pending[0] = true;
    }
    return (OPENING_EVENTS) { .action = action };
}

void Gameplay_Begin(GAMEPLAY *game, const TH01_INPUT *input, uint64_t host_tick)
{
    Gameplay_ResetWorld(game);
    Gameplay_Play(game, 11);
}

GAMEPLAY_REQUEST Gameplay_Tick(
    GAMEPLAY *game, const TH01_INPUT *input, bool reference_edge,
    uint64_t host_tick)
{
    m_Fixture.gameplay_ticks++;
    m_Fixture.game = game;
    Gameplay_AdvanceAudio(game);
    const GAMEPLAY_REQUEST request = m_Fixture.request;
    m_Fixture.request = GAMEPLAY_REQUEST_NONE;
    return request;
}

void Gameplay_ResumeAfterScore(
    GAMEPLAY *game, const TH01_INPUT *input, uint64_t host_tick)
{
    m_Fixture.resumes++;
}

void Score_Begin(
    SCORE_SCENE *scene, const SCORE_ASSETS *assets, VIDEO *video,
    SCORE_SAVES *saves, FRAME_VSYNC *vsync, GAME_INPUT *input,
    SCORE_CONTEXT context, int32_t rank, const SCORE_ENTRY *entry)
{
    scene->context = context;
    m_Fixture.score_context = context;
    m_Fixture.score_begins++;
}

bool Score_Tick(
    SCORE_SCENE *scene, const TH01_INPUT *input, bool reference_edge)
{
    m_Fixture.score_ticks++;
    if (m_Fixture.finish_score) {
        m_Fixture.saves->pending[0] = true;
    }
    return m_Fixture.finish_score;
}

void Ending_Prepare(
    const ALLOCATOR *allocator, const ENDING_ASSETS *assets, VIDEO *video,
    FRAME_VSYNC *vsync, GAME_INPUT *input, ENDING_AUDIO_CONSUMER audio,
    void *context, ENDING **out)
{
    *out = Allocator_AllocZ(allocator, sizeof(**out));
}

void Ending_Begin(ENDING *ending, SESSION *session)
{
    m_Fixture.ending_begins++;
    ending->result =
        (END_RESULT) { .route = SESSION_END_MAKAI, .score = 12345, .rank = 1 };
}

bool Ending_Tick(ENDING *ending, const TH01_INPUT *input, bool reference_edge)
{
    m_Fixture.ending_ticks++;
    return m_Fixture.finish_ending;
}

const END_RESULT *Ending_Result(const ENDING *ending)
{
    return &ending->result;
}

void Ending_Clear(SESSION *session)
{
    m_Fixture.ending_clears++;
}

void Ending_BeginSwitch(ENDING *ending)
{
    m_Fixture.switch_begins++;
}

bool Ending_SwitchTick(ENDING *ending, bool reference_edge)
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
    Th01_FlushSaves();
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
    CHECK_EQ_INT(m_Fixture.flush_calls, writes);
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
    for (int i = 0; i < 4 && m_Fixture.gameplay_ticks == 0; i++) {
        CHECK(M_Tick((TH01_INPUT) {}));
    }
    CHECK(m_Fixture.game != nullptr);
    CHECK(m_Fixture.game->storage.initial != nullptr);
}

static void M_Shutdown(void)
{
    g_GameDesc.shutdown();
    CHECK_EQ_INT(m_Fixture.textures_live, 0);
    const int flushes = m_Fixture.flush_calls;
    Th01_FlushSaves();
    CHECK_EQ_INT(m_Fixture.flush_calls, flushes);
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
        CHECK(
            (m_Fixture.game->storage.audio_output != nullptr) == (audio != 0));
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
        Th01_FlushSaves();
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

TEST(real_main_hands_score_and_ending_ownership_back_to_gameplay_and_opening)
{
    M_Reset();
    CHECK(Result_Ignore(g_GameDesc.init()));
    M_Play();
    m_Fixture.request = GAMEPLAY_REQUEST_SCORE;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.score_context, SCORE_GAMEOVER);
    m_Fixture.finish_score = true;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.resumes, 1);
    CHECK(m_Fixture.saves->pending[0]);
    CHECK_EQ_INT(m_Fixture.written, 0);
    Th01_FlushSaves();
    CHECK_EQ_INT(m_Fixture.written, 1);
    m_Fixture.request = GAMEPLAY_REQUEST_ENDING;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.ending_begins, 1);
    m_Fixture.finish_ending = true;
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.score_context, SCORE_ENDING);
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.switch_begins, 1);
    CHECK_EQ_INT(m_Fixture.ending_clears, 0);
    CHECK(M_Tick((TH01_INPUT) {}));
    CHECK_EQ_INT(m_Fixture.ending_clears, 1);
    CHECK_EQ_INT(m_Fixture.opening_begins, 2);
    CHECK_EQ_INT(m_Fixture.written, 1);
    M_Shutdown();
    CHECK_EQ_INT(m_Fixture.written, 2);
}

TEST(final_quit_tick_queues_a_save_for_the_host_and_shutdown)
{
    M_Reset();
    CHECK(Result_Ignore(g_GameDesc.init()));
    M_Opening();
    m_Fixture.action = OPENING_ACTION_QUIT;
    m_Fixture.queue_on_quit = true;
    bool running = true;
    for (int i = 0; i < 4 && running; i++) {
        running = M_Tick((TH01_INPUT) {});
    }
    CHECK(!running);
    CHECK(m_Fixture.saves->pending[0]);
    CHECK_EQ_INT(m_Fixture.written, 0);
    Th01_FlushSaves();
    CHECK_EQ_INT(m_Fixture.written, 1);
    M_Shutdown();
    CHECK_EQ_INT(m_Fixture.written, 1);
}

typedef struct {
    int calls;
    TH01_VIEW view;
} VSYNC_LOG;

static void M_OnVSync(void *const arg)
{
    VSYNC_LOG *const log = arg;
    log->calls++;
    Th01_View(&log->view);
}

TEST(vsync_hook_runs_before_each_reference_vsync_and_view_follows_scenes)
{
    M_Reset();
    CHECK(Result_Ignore(g_GameDesc.init()));
    VSYNC_LOG log = {};
    g_Th01Hooks = (TH01_HOOKS) { .on_vsync = M_OnVSync, .arg = &log };
    FRAME_CLOCK clock = {};
    int edges = 0;
    for (int i = 0; i < 30; i++) {
        edges += Frame_Advance(&clock) ? 1 : 0;
        CHECK(M_Tick((TH01_INPUT) {}));
        CHECK_EQ_INT(log.calls, edges);
    }
    CHECK(edges > 0);
    CHECK_EQ_INT(log.view.scene, TH01_SCENE_LOGO);
    M_Opening();
    TH01_VIEW view;
    Th01_View(&view);
    CHECK_EQ_INT(view.scene, TH01_SCENE_OPENING);
    M_Play();
    Th01_View(&view);
    CHECK_EQ_INT(view.scene, TH01_SCENE_GAMEPLAY);
    CHECK(view.gameplay == m_Fixture.game);
    CHECK(view.session != nullptr && view.opening != nullptr);
    m_Fixture.request = GAMEPLAY_REQUEST_SCORE;
    CHECK(M_Tick((TH01_INPUT) {}));
    Th01_View(&view);
    CHECK_EQ_INT(view.scene, TH01_SCENE_SCORE);
    CHECK_EQ_INT(view.score->context, SCORE_GAMEOVER);
    g_Th01Hooks = (TH01_HOOKS) {};
    M_Shutdown();
}
