#include "th01/main.h"

#include "th01/gameplay.h"

#include "th01/logo.h"
#include "th01/opening.h"
#include "th01/frame.h"
#include "th01/input.h"
#include "th01/settings.h"
#include "th01/score.h"
#include "th01/ending.h"

#include "core/base/arena.h"
#include "core/gfx.h"
#include "core/vfs.h"

#include "version.h"

#include <string.h>

#define M_ASSET_CAPACITY (64 * 1024 * 1024)

static_assert(SHELL_TICK_RATE == 60);

static const char *const m_DataFiles[] = { "TOUHOU.DAT" };
static ARENA m_Assets;
static BFNT m_Sprites;
static LOGO m_Logo;
static OPENING_ASSETS m_OpeningAssets;
static OPENING m_Opening;
static bool m_InOpening;
static bool m_InGameplay, m_InScore;
static SCORE_SAVES m_Scores;
static SCORE_ASSETS m_ScoreAssets;
static SCORE_SCENE m_Score;
static ENDING_ASSETS m_EndingAssets;
static ENDING *m_Ending;
static bool m_InEnding;
static GAMEPLAY_ASSETS m_GameplayAssets;
static GAMEPLAY *m_Gameplay;
static SESSION m_Session;
static uint64_t m_HostTick;
static GFX_TEXTURE *m_TextTexture;
static uint8_t *m_UploadedText, *m_ScrolledPage;
static VIDEO m_Video;
static FRAME_CLOCK m_Frame;
static GFX_TEXTURE *m_Pages[2];
static uint8_t *m_UploadedPages[2];
static GFX_COLOR m_UploadedPalette[16];
static bool m_IntroPressed;
static bool m_Initialized;

static RESULT M_LoadAssets(void)
{
    VFS *const vfs = Shell_GetFiles();
    RESULT result = OK;
    if (IS_OK(result)) {
        Result_Log(
            Settings_ImportOriginal(vfs, Shell_GetUserFiles(), &g_Settings),
            LOG_LEVEL_WARNING);
        READER reader;
        result = Vfs_Open(vfs, "TOUHOU.DAT", VFS_STREAM, &reader);
        if (IS_OK(result)) {
            result = Bfnt_Load(&reader, Arena_Allocator(&m_Assets), &m_Sprites);
            Reader_Close(&reader);
        }
    }
    if (IS_OK(result)) {
        result = Opening_MountArchive(vfs);
    }
    if (IS_OK(result)) {
        result = Opening_LoadAssets(
            vfs, Arena_Allocator(&m_Assets), &m_OpeningAssets);
    }
    if (IS_OK(result)) {
        result = GameplayAssets_Load(
            vfs, Arena_Allocator(&m_Assets), &m_GameplayAssets);
    }
    if (IS_OK(result)) {
        Result_Log(
            ScoreData_Load(vfs, Shell_GetUserFiles(), &m_Scores),
            LOG_LEVEL_WARNING);
        result = Score_LoadAssets(
            vfs, &m_Scores, Arena_Allocator(&m_Assets), &m_ScoreAssets);
    }
    if (IS_OK(result)) {
        result =
            Ending_LoadAssets(vfs, Arena_Allocator(&m_Assets), &m_EndingAssets);
    }
    return result;
}

static void M_EndingAudio(void *const context, const ENDING_AUDIO command)
{
    static const OPENING_EVENTS events[] = {
        { .audio = OPENING_AUDIO_PLAY, .track = 14 },
        { .audio = OPENING_AUDIO_FADE },
        { .audio = OPENING_AUDIO_STOP },
        { .audio = OPENING_AUDIO_PLAY, .track = 3 },
    };
    Gameplay_OpeningAudio(context, events[command]);
}

static RESULT M_Init(void)
{
    m_Initialized = false;
    Arena_Init(&m_Assets, "TH01 assets", M_ASSET_CAPACITY);
    MUST(M_LoadAssets());
    FAIL_IF(
        m_Sprites.width != 64 || m_Sprites.height != 16
            || m_Sprites.count != 16,
        "TOUHOU.DAT must contain sixteen 64 by 16 logo sprites");
    Video_Init(&m_Video, Arena_Allocator(&m_Assets));
    memset(&m_Frame, 0, sizeof(m_Frame));
    m_IntroPressed = false;
    m_InOpening = false;
    m_InGameplay = false;
    m_InScore = false;
    m_InEnding = false;
    m_HostTick = 0;
    memset(&m_Session, 0, sizeof(m_Session));
    MUST(Gameplay_Prepare(
        Arena_Allocator(&m_Assets), &m_GameplayAssets, &m_Video, &m_Session,
        &m_Gameplay));
    const GAMEPLAY_SCORE_CONTEXT ending_context =
        Gameplay_ScoreContext(m_Gameplay);
    Ending_Prepare(
        Arena_Allocator(&m_Assets), &m_EndingAssets, &m_Video,
        ending_context.vsync, ending_context.input, M_EndingAudio, m_Gameplay,
        &m_Ending);
    Logo_Init(&m_Logo, &m_Video);
    for (int32_t i = 0; i < 2; i++) {
        MUST(GfxTexture_CreateIndexed(LOGO_WIDTH, LOGO_HEIGHT, &m_Pages[i]));
        GfxTexture_UploadIndices(
            m_Pages[i], (GFX_RECT) { 0, 0, LOGO_WIDTH, LOGO_HEIGHT },
            m_Video.pages[i]);
        GfxTexture_UploadPalette(m_Pages[i], 0, 16, m_Video.palette);
        m_UploadedPages[i] =
            Allocator_Alloc(Arena_Allocator(&m_Assets), VIDEO_PAGE_SIZE);
        memcpy(m_UploadedPages[i], m_Video.pages[i], VIDEO_PAGE_SIZE);
    }
    memcpy(m_UploadedPalette, m_Video.palette, sizeof(m_UploadedPalette));
    MUST(GfxTexture_CreateIndexed(VIDEO_WIDTH, VIDEO_HEIGHT, &m_TextTexture));
    m_UploadedText =
        Allocator_AllocZ(Arena_Allocator(&m_Assets), VIDEO_PAGE_SIZE);
    m_ScrolledPage =
        Allocator_Alloc(Arena_Allocator(&m_Assets), VIDEO_PAGE_SIZE);
    GfxTexture_UploadIndices(
        m_TextTexture, (GFX_RECT) { 0, 0, 640, 400 }, m_UploadedText);
    GfxTexture_UploadPalette(m_TextTexture, 0, 9, g_TramPalette);
    m_Initialized = true;
    return OK;
}

static bool M_OpeningEvents(
    const OPENING_EVENTS events, const TH01_INPUT *const raw)
{
    if (events.clear_key_buffer) {
        m_IntroPressed = false;
    }
    if (events.action == OPENING_ACTION_QUIT) {
        return false;
    }
    Gameplay_OpeningAudio(m_Gameplay, events);
    if (events.action == OPENING_ACTION_START
        || events.action == OPENING_ACTION_CONTINUE) {
        Session_Begin(
            &m_Session,
            events.action == OPENING_ACTION_START ? SESSION_START
                                                  : SESSION_CONTINUE,
            &g_Settings, m_Opening.frame_rand);
        m_Session.table_high_score =
            (uint32_t)m_Scores.tables[g_Settings.difficulty].entries[0].score;
        Gameplay_Begin(m_Gameplay, raw, m_HostTick);
        m_InGameplay = true;
        m_InOpening = false;
    }
    return true;
}

static void M_BeginOpening(const bool switched, const TH01_INPUT *const raw)
{
    m_InOpening = true;
    m_InGameplay = false;
    Video_SetScroll(&m_Video, 0);
    M_OpeningEvents(
        Opening_Init(&m_Opening, &m_Video, &g_Settings, &m_OpeningAssets), raw);
    if (switched) {
        // Counts the VSync latched by the program switch (Frame_SwitchProgram).
        m_Opening.count1 = 1;
    }
}

static bool M_Tick(const void *const input)
{
    const TH01_INPUT *const raw = input;
    INPUT_BINDING key;
    const bool key_pressed = Shell_GetKeyPress(&key);
    GameInput_LatchIntro(&m_IntroPressed, raw, key_pressed ? &key : nullptr);
    m_HostTick++;
    const bool reference_edge = Frame_Advance(&m_Frame);
    if (m_InScore) {
        Gameplay_AdvanceAudio(m_Gameplay);
        if (Score_Tick(&m_Score, raw, reference_edge, m_HostTick)) {
            m_Session.table_high_score =
                (uint32_t)m_Scores.tables[m_Session.settings.difficulty]
                    .entries[0]
                    .score;
            m_InScore = false;
            if (m_Score.context == SCORE_ENDING) {
                Ending_Clear(&m_Session);
                M_BeginOpening(true, raw);
            } else {
                Gameplay_ResumeAfterScore(m_Gameplay);
            }
        }
    } else if (m_InEnding) {
        Gameplay_AdvanceAudio(m_Gameplay);
        if (Ending_Tick(m_Ending, raw, reference_edge)) {
            const END_RESULT *const result = Ending_Result(m_Ending);
            const GAMEPLAY_SCORE_CONTEXT context =
                Gameplay_ScoreContext(m_Gameplay);
            SCORE_ENTRY entry = { .stage = result->route == SESSION_END_MAKAI
                                      ? 50
                                      : 60,
                                  .route = "完" };
            memcpy(&entry.score, &result->score, sizeof(entry.score));
            Score_Begin(
                &m_Score, &m_ScoreAssets, &m_Video, &m_Scores, context.vsync,
                context.input, SCORE_ENDING, result->rank, &entry);
            m_InScore = true;
            m_InEnding = false;
        }
    } else if (m_InGameplay) {
        const GAMEPLAY_REQUEST request =
            Gameplay_Tick(m_Gameplay, raw, reference_edge, m_HostTick);
        if (request == GAMEPLAY_REQUEST_OPENING) {
            M_BeginOpening(true, raw);
        } else if (request == GAMEPLAY_REQUEST_SCORE) {
            const GAMEPLAY_SCORE_CONTEXT context =
                Gameplay_ScoreContext(m_Gameplay);
            SCORE_ENTRY entry = { .score = (int32_t)m_Session.score,
                                  .stage = (int16_t)(m_Session.stage_id + 1) };
            strcpy(
                entry.route,
                m_Session.stage_id < 5                       ? "社"
                    : m_Session.route == SESSION_ROUTE_MAKAI ? "魔"
                                                             : "地");
            Score_Begin(
                &m_Score, &m_ScoreAssets, &m_Video, &m_Scores, context.vsync,
                context.input, SCORE_GAMEOVER, m_Session.settings.difficulty,
                &entry);
            m_InScore = true;
        } else if (request == GAMEPLAY_REQUEST_ENDING) {
            Ending_Begin(m_Ending, &m_Session);
            Ending_Tick(m_Ending, raw, false);
            m_InGameplay = false;
            m_InEnding = true;
        }
    } else {
        Gameplay_AdvanceAudio(m_Gameplay);
        if (reference_edge) {
            if (!m_InOpening && m_IntroPressed) {
                M_BeginOpening(false, raw);
            } else if (m_InOpening) {
                if (!M_OpeningEvents(
                        Opening_Tick(&m_Opening, raw, m_IntroPressed), raw)) {
                    return false;
                }
            } else {
                int32_t changed;
                if (!Logo_Tick(&m_Logo, &m_Sprites, &changed)) {
                    M_BeginOpening(false, raw);
                } else {
                    GFX_COLOR palette[16];
                    Logo_Palette(&m_Logo, &m_Sprites, palette);
                    Video_SetPalette(&m_Video, palette);
                }
            }
        }
    }
    {
        for (int32_t i = 0; i < 2; i++) {
            const uint8_t *pixels = m_Video.pages[i];
            if (m_Video.scroll_top != 0) {
                for (int32_t y = 0; y < VIDEO_HEIGHT; y++) {
                    memcpy(
                        m_ScrolledPage + y * VIDEO_WIDTH,
                        pixels
                            + ((y + m_Video.scroll_top) % VIDEO_HEIGHT)
                                * VIDEO_WIDTH,
                        VIDEO_WIDTH);
                }
                pixels = m_ScrolledPage;
            }
            if (memcmp(m_UploadedPages[i], pixels, VIDEO_PAGE_SIZE) != 0) {
                GfxTexture_UploadIndices(
                    m_Pages[i], (GFX_RECT) { 0, 0, LOGO_WIDTH, LOGO_HEIGHT },
                    pixels);
                memcpy(m_UploadedPages[i], pixels, VIDEO_PAGE_SIZE);
            }
        }
        if (memcmp(
                m_UploadedPalette, m_Video.palette, sizeof(m_UploadedPalette))
            != 0) {
            for (int32_t i = 0; i < 2; i++) {
                GfxTexture_UploadPalette(m_Pages[i], 0, 16, m_Video.palette);
            }
            memcpy(
                m_UploadedPalette, m_Video.palette, sizeof(m_UploadedPalette));
        }
    }
    const TRAM *const text =
        m_InGameplay && !m_InScore ? Gameplay_TextLayer(m_Gameplay) : nullptr;
    if (text != nullptr
        && memcmp(m_UploadedText, text->pixels, VIDEO_PAGE_SIZE) != 0) {
        GfxTexture_UploadIndices(
            m_TextTexture, (GFX_RECT) { 0, 0, 640, 400 }, text->pixels);
        memcpy(m_UploadedText, text->pixels, VIDEO_PAGE_SIZE);
    }
    const GFX_COLOR white = { 255, 255, 255, 255 };
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0 }, white, { 0, 0 } },
        { { LOGO_WIDTH, 0, 0 }, white, { 1, 0 } },
        { { LOGO_WIDTH, LOGO_HEIGHT, 0 }, white, { 1, 1 } },
        { { 0, LOGO_HEIGHT, 0 }, white, { 0, 1 } },
    };
    const GFX_STATE state = {};
    Gfx_DrawQuad(&state, m_Pages[m_Video.visible_page], vertices, 0);
    if (text != nullptr && text->visible) {
        const GFX_STATE overlay = { .blend = GFX_BLEND_ALPHA };
        Gfx_DrawQuad(&overlay, m_TextTexture, vertices, 0);
    }
    return true;
}

static void M_Shutdown(void)
{
    Th01_FlushSaves();
    m_Initialized = false;
    for (int32_t i = 0; i < 2; i++) {
        GfxTexture_Free(m_Pages[i]);
        m_Pages[i] = nullptr;
        m_UploadedPages[i] = nullptr;
    }
    GfxTexture_Free(m_TextTexture);
    m_TextTexture = nullptr;
    m_UploadedText = nullptr;
    m_ScrolledPage = nullptr;
    m_Ending = nullptr;
    memset(&m_EndingAssets, 0, sizeof(m_EndingAssets));
    m_InEnding = false;
    m_Gameplay = nullptr;
    memset(&m_GameplayAssets, 0, sizeof(m_GameplayAssets));
    Arena_Free(&m_Assets);
    memset(&m_Sprites, 0, sizeof(m_Sprites));
    memset(&m_Video, 0, sizeof(m_Video));
    memset(&m_Logo, 0, sizeof(m_Logo));
    memset(&m_Opening, 0, sizeof(m_Opening));
    memset(&m_OpeningAssets, 0, sizeof(m_OpeningAssets));
}

void Th01_FlushSaves(void)
{
    if (m_Initialized) {
        Result_Log(ScoreData_WriteSaves(&m_Scores), LOG_LEVEL_WARNING);
    }
}

const GAME_DESC g_GameDesc = {
    .id = "th01",
    .title = "東方靈異伝 ~ Highly Responsive to Prayers",
    .version = GAME_VERSION,
    .logical_width = LOGO_WIDTH,
    .logical_height = LOGO_HEIGHT,
    .input_schema = &g_InputSchema,
    .settings_schema = &g_SettingsSchema,
    .settings = &g_Settings,
    .data_files = m_DataFiles,
    .data_file_count = 1,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
