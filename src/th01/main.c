#include "th01/main.h"

#include "th01/logo.h"
#include "th01/settings.h"

#include "core/base/arena.h"
#include "core/gfx.h"
#include "core/vfs.h"

#include "version.h"

#include <string.h>

#define M_ASSET_CAPACITY (64 * 1024 * 1024)

typedef struct {
    bool skip;
} LOGO_INPUT;

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(
        LOGO_INPUT, skip, "return, escape, space, z, x, q",
        "south, east, start", "joy_button_0, joy_button_1"),
};
static const INPUT_SCHEMA m_Input = INPUT_SCHEMA_OF(LOGO_INPUT, m_Actions, 1);
static const char *const m_DataFiles[] = { "TOUHOU.DAT" };
static ARENA m_Assets;
static BFNT m_Sprites;
static LOGO m_Logo;
static VIDEO m_Video;
static GFX_TEXTURE *m_Pages[2];

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
    Logo_Init(&m_Logo, &m_Video);
    for (int32_t i = 0; i < 2; i++) {
        MUST(GfxTexture_CreateIndexed(LOGO_WIDTH, LOGO_HEIGHT, &m_Pages[i]));
        GfxTexture_UploadIndices(
            m_Pages[i], (GFX_RECT) { 0, 0, LOGO_WIDTH, LOGO_HEIGHT },
            m_Video.pages[i]);
        GfxTexture_UploadPalette(m_Pages[i], 0, 16, m_Video.palette);
    }
    return OK;
}

static bool M_Tick(const void *const input)
{
    if (((const LOGO_INPUT *)input)->skip) {
        return false;
    }
    int32_t changed;
    if (!Logo_Tick(&m_Logo, &m_Sprites, &changed)) {
        return false;
    }
    if (changed >= 0) {
        GfxTexture_UploadIndices(
            m_Pages[changed], (GFX_RECT) { 0, 0, LOGO_WIDTH, LOGO_HEIGHT },
            m_Video.pages[changed]);
        GFX_COLOR palette[16];
        Logo_Palette(&m_Logo, &m_Sprites, palette);
        Video_SetPalette(&m_Video, palette);
        for (int32_t i = 0; i < 2; i++) {
            GfxTexture_UploadPalette(m_Pages[i], 0, 16, m_Video.palette);
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
    }
    Arena_Free(&m_Assets);
    memset(&m_Sprites, 0, sizeof(m_Sprites));
    memset(&m_Video, 0, sizeof(m_Video));
    memset(&m_Logo, 0, sizeof(m_Logo));
}

const GAME_DESC g_GameDesc = {
    .id = "th01",
    .title = "東方靈異伝 ~ Highly Responsive to Prayers",
    .version = GAME_VERSION,
    .logical_width = LOGO_WIDTH,
    .logical_height = LOGO_HEIGHT,
    .input_schema = &m_Input,
    .settings_schema = &g_SettingsSchema,
    .settings = &g_Settings,
    .data_files = m_DataFiles,
    .data_file_count = 1,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
