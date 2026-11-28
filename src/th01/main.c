#include "th01/main.h"

#include "th01/ending.h"
#include "th01/gameplay.h"
#include "th01/input.h"
#include "th01/music.h"
#include "th01/opening.h"
#include "th01/present.h"
#include "th01/score.h"
#include "th01/settings.h"
#include "th01/sound.h"
#include "th01/version.h"
#include "th01/vsync_irq.h"

#include "core/audio.h"
#include "core/base/arena.h"
#include "core/base/assert.h"
#include "core/text/font_registry.h"
#include "core/text/font_rom.h"
#include "core/vfs.h"
#include "pc98/vsync.h"
#include "pc98/zun/bfnt.h"
#include "pc98/zun/logo1.h"

#include <string.h>

#define M_ASSET_CAPACITY (64 * 1024 * 1024)

static_assert(SHELL_TICK_RATE == 60);

static const char *const m_DataFiles[] = { "TOUHOU.DAT" };
// DOSBox-X's order, then the built-in Shinonome 16.
static const char *const m_Fonts[][2] = {
    { "FONT.ROM", nullptr },
    { "NEC.BMP", nullptr },
    { "ANEX86.BMP", nullptr },
    { "FREECG98.BMP", nullptr },
    { "shnmk16.bdf", "shnm8x16r.bdf" },
};
static ARENA m_Assets;
static BFNT m_Sprites;
static ZUN_LOGO1 m_Logo;
static OPENING_ASSETS m_OpeningAssets;
static SCORE_ASSETS m_ScoreAssets;
static ENDING_ASSETS m_EndingAssets;
static GAMEPLAY_ASSETS m_GameplayAssets;
static GAME_AUDIO *m_Audio;
static uint64_t m_HostTick;
static VSYNC_CLOCK m_Frame;
static bool m_IntroPressed;
static bool m_Initialized;
static TH01_SCENE m_Th01Scene;
const TH01_SCENE *const g_Th01Scene = &m_Th01Scene;

static RESULT M_OpenFont(FONT_ROM **const out)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Fonts); i++) {
        const void *data;
        size_t size;
        if (FontRegistry_FindFile(m_Fonts[i][0], &data, &size)
            && Result_Log(
                FontRom_Open(m_Fonts[i][0], m_Fonts[i][1], out),
                LOG_LEVEL_WARNING)) {
            LOG_INFO("Drawing text from %s", m_Fonts[i][0]);
            return OK;
        }
    }
    return FAIL(
        "No PC-98 font: put FONT.ROM, NEC.BMP, ANEX86.BMP or FREECG98.BMP, or "
        "shnmk16.bdf with shnm8x16r.bdf, in opentouhou/fonts/");
}

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
    FONT_ROM *rom = nullptr;
    if (IS_OK(result)) {
        result = M_OpenFont(&rom);
    }
    if (IS_OK(result)) {
        result = OpeningAssets_Load(
            vfs, rom, Arena_Allocator(&m_Assets), &m_OpeningAssets);
    }
    if (IS_OK(result)) {
        result = GameplayAssets_Load(
            vfs, rom, Arena_Allocator(&m_Assets), &m_GameplayAssets);
    }
    if (IS_OK(result)) {
        result = ScoreAssets_Load(
            vfs, Shell_GetUserFiles(), rom, Arena_Allocator(&m_Assets),
            &m_ScoreAssets);
    }
    if (IS_OK(result)) {
        result = EndingAssets_Load(
            vfs, rom, Arena_Allocator(&m_Assets), &m_EndingAssets);
    }
    FontRom_Free(rom);
    return result;
}

static void M_ShowLogoPages(void)
{
    Video_SelectDrawingPage(m_Logo.drawing_page);
    Video_SelectVisiblePage(m_Logo.visible_page);
}

static void M_OpeningMusic(const OPENING_EVENTS events)
{
    switch (events.audio) {
    case OPENING_AUDIO_NONE:
        break;
    case OPENING_AUDIO_STOP:
        Music_Stop();
        break;
    case OPENING_AUDIO_FADE:
        Music_Fade();
        break;
    case OPENING_AUDIO_PLAY: {
        static const uint8_t songs[OPENING_TRACK_COUNT] = {
            15, 16, 0, 1, 2, 3, 4, 5, 7, 8, 9, 13, 14, 12, 17,
        };
        ASSERT(events.track >= 0 && events.track < OPENING_TRACK_COUNT);
        Sound_Load(songs[events.track]);
        Music_Play(songs[events.track]);
        break;
    }
    }
}

static RESULT M_Init(void)
{
    m_Initialized = false;
    Arena_Init(&m_Assets, "TH01 assets", M_ASSET_CAPACITY);
    MUST(M_LoadAssets());
    FAIL_IF(
        m_Sprites.width != 64 || m_Sprites.height != 16 || m_Sprites.count != 16
            || !m_Sprites.has_palette,
        "TOUHOU.DAT must contain sixteen 64 by 16 logo sprites");
    Video_Init(Arena_Allocator(&m_Assets));
    memset(&m_Frame, 0, sizeof(m_Frame));
    m_IntroPressed = false;
    m_Th01Scene = TH01_SCENE_LOGO;
    m_HostTick = 0;
    Session_Init();
    MUST(Music_Init(
        Arena_Allocator(&m_Assets), m_GameplayAssets.songs, GAMEPLAY_SONG_COUNT,
        &m_GameplayAssets.effects));
    Gameplay_Init(Arena_Allocator(&m_Assets), &m_GameplayAssets);
    if (Audio_IsInitialized()) {
        Result_Log(
            GameAudio_Prepare(Arena_Allocator(&m_Assets), &m_Audio),
            LOG_LEVEL_WARNING);
        Music_BindOutput(m_Audio);
    }
    Ending_Init(&m_EndingAssets);
    ZunLogo1_Init(&m_Logo, g_Video->pages);
    M_ShowLogoPages();
    MUST(Present_Init(Arena_Allocator(&m_Assets)));
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
    M_OpeningMusic(events);
    if (events.action == OPENING_ACTION_START
        || events.action == OPENING_ACTION_CONTINUE) {
        Session_Begin(
            events.action == OPENING_ACTION_START ? SESSION_START
                                                  : SESSION_CONTINUE,
            &g_Settings, g_Opening->frame_rand);
        if (events.action == OPENING_ACTION_START) {
            Th01Hooks_CallOnStart();
        }
        Score_SyncSession();
        Gameplay_Begin(raw, m_HostTick);
        m_Th01Scene = TH01_SCENE_GAMEPLAY;
    }
    return true;
}

static void M_BeginOpening(const bool switched, const TH01_INPUT *const raw)
{
    m_Th01Scene = TH01_SCENE_OPENING;
    Video_SetScroll(0);
    M_OpeningEvents(Opening_Init(&m_OpeningAssets), raw);
    if (switched) {
        Opening_CountSwitchedVSync();
    }
}

static bool M_Step(const TH01_INPUT *const raw, const bool reference_edge)
{
    switch (m_Th01Scene) {
    case TH01_SCENE_SCORE:
    case TH01_SCENE_ENDING_SCORE:
        if (Score_Tick(raw, reference_edge)) {
            Score_SyncSession();
            if (m_Th01Scene == TH01_SCENE_ENDING_SCORE) {
                Ending_BeginSwitch();
                m_Th01Scene = TH01_SCENE_SWITCH;
            } else {
                Gameplay_ResumeAfterScore(raw, m_HostTick);
                m_Th01Scene = TH01_SCENE_GAMEPLAY;
            }
        }
        break;
    case TH01_SCENE_SWITCH:
        if (Ending_SwitchTick(reference_edge)) {
            Ending_Clear();
            M_BeginOpening(true, raw);
        }
        break;
    case TH01_SCENE_ENDING:
        if (Ending_Tick(raw, reference_edge)) {
            const END_RESULT *const result = g_EndResult;
            SCORE_ENTRY entry = { .stage = result->route == SESSION_END_MAKAI
                                      ? 50
                                      : 60,
                                  .route = "完" };
            memcpy(&entry.score, &result->score, sizeof(entry.score));
            Score_Begin(&m_ScoreAssets, SCORE_ENDING, result->rank, &entry);
            m_Th01Scene = TH01_SCENE_ENDING_SCORE;
        }
        break;
    case TH01_SCENE_GAMEPLAY: {
        const GAMEPLAY_REQUEST request =
            Gameplay_Tick(raw, reference_edge, m_HostTick);
        if (request == GAMEPLAY_REQUEST_OPENING) {
            M_BeginOpening(true, raw);
        } else if (request == GAMEPLAY_REQUEST_SCORE) {
            SCORE_ENTRY entry = { .score = (int32_t)g_Session->score,
                                  .stage = (int16_t)(g_Session->stage_id + 1) };
            strcpy(
                entry.route,
                g_Session->stage_id < 5                       ? "社"
                    : g_Session->route == SESSION_ROUTE_MAKAI ? "魔"
                                                              : "地");
            Score_Begin(
                &m_ScoreAssets, SCORE_GAMEOVER, g_Session->settings.difficulty,
                &entry);
            m_Th01Scene = TH01_SCENE_SCORE;
        } else if (request == GAMEPLAY_REQUEST_ENDING) {
            Ending_Begin();
            Ending_Tick(raw, false);
            m_Th01Scene = TH01_SCENE_ENDING;
        }
        break;
    }
    case TH01_SCENE_OPENING:
        if (reference_edge) {
            return M_OpeningEvents(Opening_Tick(raw, m_IntroPressed), raw);
        }
        break;
    case TH01_SCENE_LOGO:
        if (reference_edge) {
            if (m_IntroPressed) {
                M_BeginOpening(false, raw);
            } else {
                const bool running =
                    ZunLogo1_Tick(&m_Logo, g_Video->pages, m_Sprites.pixels);
                M_ShowLogoPages();
                if (!running) {
                    M_BeginOpening(false, raw);
                } else {
                    GFX_COLOR palette[16];
                    ZunLogo1_Palette(&m_Logo, m_Sprites.palette, palette);
                    Video_SetPalette(palette);
                }
            }
        }
        break;
    }
    return true;
}

static void M_Save(void)
{
    if (m_Initialized) {
        Result_Log(Score_FlushSaves(), LOG_LEVEL_WARNING);
    }
}

static bool M_Tick(const void *const input)
{
    const TH01_INPUT *const raw = input;
    INPUT_BINDING key;
    const bool key_pressed = Shell_GetKeyPress(&key);
    GameInput_LatchIntro(&m_IntroPressed, raw, key_pressed ? &key : nullptr);
    m_HostTick++;
    const bool reference_edge = VSync_Advance(&m_Frame);
    if (reference_edge) {
        Th01Hooks_CallOnVSync();
    }
    Music_Update();
    const bool running = M_Step(raw, reference_edge);
    if (running) {
        Present_Frame(m_Th01Scene == TH01_SCENE_GAMEPLAY);
    }
    M_Save();
    return running;
}

static void M_Shutdown(void)
{
    M_Save();
    m_Initialized = false;
    Music_Close();
    GameAudio_Free(m_Audio);
    m_Audio = nullptr;
    Present_Close();
    Ending_Close();
    memset(&m_EndingAssets, 0, sizeof(m_EndingAssets));
    m_Th01Scene = TH01_SCENE_LOGO;
    Gameplay_Close();
    memset(&m_GameplayAssets, 0, sizeof(m_GameplayAssets));
    Arena_Free(&m_Assets);
    memset(&m_Sprites, 0, sizeof(m_Sprites));
    Video_Close();
    memset(&m_Logo, 0, sizeof(m_Logo));
    Opening_Close();
    Score_Close();
    memset(&m_OpeningAssets, 0, sizeof(m_OpeningAssets));
}

const GAME_DESC g_GameDesc = {
    .id = "th01",
    .title = "東方靈異伝 ~ Highly Responsive to Prayers",
    .version = GAME_VERSION,
    .logical_width = VIDEO_WIDTH,
    .logical_height = VIDEO_HEIGHT,
    .input_schema = &g_InputSchema,
    .settings_schema = &g_SettingsSchema,
    .settings = &g_Settings,
    .data_files = m_DataFiles,
    .data_file_count = 1,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
