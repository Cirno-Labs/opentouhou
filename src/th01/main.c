#include "th01/main.h"

#include "th01/logo.h"
#include "th01/opening.h"
#include "th01/frame.h"
#include "th01/input.h"
#include "th01/settings.h"

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
static bool m_GameplayReported;
static VIDEO m_Video;
static FRAME_CLOCK m_Frame;
static GFX_TEXTURE *m_Pages[2];
static uint8_t *m_UploadedPages[2];
static GFX_COLOR m_UploadedPalette[16];
static bool m_IntroPressed;

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
    return result;
}

static RESULT M_Init(void)
{
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
    m_GameplayReported = false;
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
    return OK;
}

static bool M_OpeningEvents(const OPENING_EVENTS events)
{
    if (events.clear_key_buffer) {
        m_IntroPressed = false;
    }
    if (events.action == OPENING_ACTION_QUIT) {
        return false;
    }
    if ((events.action == OPENING_ACTION_START
         || events.action == OPENING_ACTION_CONTINUE)
        && !m_GameplayReported) {
        LOG_WARNING(
            "%s requested: the gameplay scene is not implemented",
            events.action == OPENING_ACTION_START ? "Start" : "Continue");
        m_GameplayReported = true;
    }
    return true;
}

static void M_BeginOpening(void)
{
    m_InOpening = true;
    M_OpeningEvents(
        Opening_Init(&m_Opening, &m_Video, &g_Settings, &m_OpeningAssets));
}

static bool M_Tick(const void *const input)
{
    const TH01_INPUT *const raw = input;
    INPUT_BINDING key;
    const bool key_pressed = Shell_GetKeyPress(&key);
    GameInput_LatchIntro(&m_IntroPressed, raw, key_pressed ? &key : nullptr);
    if (!raw->ok && !raw->shot) {
        m_GameplayReported = false;
    }
    if (Frame_Advance(&m_Frame)) {
        if (!m_InOpening && m_IntroPressed) {
            M_BeginOpening();
        } else if (m_InOpening) {
            if (!M_OpeningEvents(
                    Opening_Tick(&m_Opening, raw, m_IntroPressed))) {
                return false;
            }
        } else {
            int32_t changed;
            if (!Logo_Tick(&m_Logo, &m_Sprites, &changed)) {
                M_BeginOpening();
            } else {
                GFX_COLOR palette[16];
                Logo_Palette(&m_Logo, &m_Sprites, palette);
                Video_SetPalette(&m_Video, palette);
            }
        }
        for (int32_t i = 0; i < 2; i++) {
            if (memcmp(m_UploadedPages[i], m_Video.pages[i], VIDEO_PAGE_SIZE)
                != 0) {
                GfxTexture_UploadIndices(
                    m_Pages[i], (GFX_RECT) { 0, 0, LOGO_WIDTH, LOGO_HEIGHT },
                    m_Video.pages[i]);
                memcpy(m_UploadedPages[i], m_Video.pages[i], VIDEO_PAGE_SIZE);
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
    const GFX_COLOR white = { 255, 255, 255, 255 };
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0 }, white, { 0, 0 } },
        { { LOGO_WIDTH, 0, 0 }, white, { 1, 0 } },
        { { LOGO_WIDTH, LOGO_HEIGHT, 0 }, white, { 1, 1 } },
        { { 0, LOGO_HEIGHT, 0 }, white, { 0, 1 } },
    };
    const GFX_STATE state = {};
    Gfx_DrawQuad(&state, m_Pages[m_Video.visible_page], vertices, 0);
    return true;
}

static void M_Shutdown(void)
{
    for (int32_t i = 0; i < 2; i++) {
        GfxTexture_Free(m_Pages[i]);
        m_Pages[i] = nullptr;
        m_UploadedPages[i] = nullptr;
    }
    Arena_Free(&m_Assets);
    memset(&m_Sprites, 0, sizeof(m_Sprites));
    memset(&m_Video, 0, sizeof(m_Video));
    memset(&m_Logo, 0, sizeof(m_Logo));
    memset(&m_Opening, 0, sizeof(m_Opening));
    memset(&m_OpeningAssets, 0, sizeof(m_OpeningAssets));
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
