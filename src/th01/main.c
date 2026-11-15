#include "th01/main.h"

#include "th01/logo.h"

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
static GFX_TEXTURE *m_Pages[2];

static RESULT M_LoadSprites(void)
{
    VFS *const vfs = Shell_GetFiles();
    RESULT result = OK;
    if (IS_OK(result)) {
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
    MUST(M_LoadSprites());
    FAIL_IF(
        m_Sprites.width != 64 || m_Sprites.height != 16
            || m_Sprites.count != 16,
        "TOUHOU.DAT must contain sixteen 64 by 16 logo sprites");
    Logo_Init(&m_Logo);
    for (int32_t i = 0; i < 2; i++) {
        MUST(GfxTexture_CreateIndexed(LOGO_WIDTH, LOGO_HEIGHT, &m_Pages[i]));
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
            m_Logo.pages[changed]);
        GFX_COLOR palette[16];
        Logo_Palette(&m_Logo, &m_Sprites, palette);
        for (int32_t i = 0; i < 2; i++) {
            GfxTexture_UploadPalette(m_Pages[i], 0, 16, palette);
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
    Gfx_DrawQuad(&state, m_Pages[m_Logo.visible_page], vertices, 0);
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
}

const GAME_DESC g_GameDesc = {
    .id = "th01",
    .title = "東方靈異伝 ~ Highly Responsive to Prayers",
    .version = GAME_VERSION,
    .logical_width = LOGO_WIDTH,
    .logical_height = LOGO_HEIGHT,
    .input_schema = &m_Input,
    .data_files = m_DataFiles,
    .data_file_count = 1,
    .init = M_Init,
    .tick = M_Tick,
    .shutdown = M_Shutdown,
};
