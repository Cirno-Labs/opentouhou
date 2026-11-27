#include "tests/patch_builder.h"
#include "tests/test.h"
#include "tests/test_dir.h"
#include "tests/test_fonts.h"
#include "tests/th07/th07_anm_builder.h"
#include "tests/th07/th07_data.h"
#include "th07/anm.h"
#include "th07/anm/private.h"
#include "th07/files.h"
#include "th07/gui/private.h"
#include "th07/strings.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/image.h"
#include "core/io/file.h"
#include "core/io/writer.h"
#include "core/patch.h"
#include "core/text/font_registry.h"
#include "stg/rng.h"
#include "stg/translation.h"

#include <SDL3/SDL.h>

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static RNG m_Rng;
static PATCH_STACK *m_Patches;

// Writes name's png of width x height directly under patches/a/lang/th07,
// a flat top-level file: base_tsa and lang_en both ship their nameplate
// images that way, not under a "data/" path.
static void M_WriteNameplate(
    const char *const patches, const char *const name, const int32_t width,
    const int32_t height)
{
    char *const dir = Str_Format("%s/a/lang/th07", patches);
    CHECK(Result_Log(File_MakeDir(dir), LOG_LEVEL_ERROR));
    Mem_Free(dir);

    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int32_t i = 0; i < width * height; i++) {
        image.pixels[i * 4] = 0xff;
        image.pixels[i * 4 + 3] = 0xff;
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    char *const path = Str_Format("%s/a/lang/th07/%s", patches, name);
    CHECK(SDL_SaveFile(path, bytes, size));
    Mem_Free(path);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

// Sets up a four-sprite text.anm at 0x700 (as data/text.anm does), a patch
// stack of nmlgc/base_tsa and a/lang (holding the nameplate images when
// with_images is set), and loads th07/gui/text_image.
static char *M_SetUp(const char *const case_name, const bool with_images)
{
    char *const directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(directory, &m_Version, 1);
    Th07Data_WriteMusic(directory, nullptr, 0);

    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 16, 16, "@");
    for (int32_t i = 0; i < 4; i++) {
        AnmBuilder_AddSprite(anm, i, 0.0f, 0.0f, 16.0f, 16.0f);
    }
    AnmBuilder_WriteFile(&builder, directory, "text.anm");

    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    if (with_images) {
        M_WriteNameplate(patches, "ti_bosstitle.png", 384, 64);
        M_WriteNameplate(patches, "ti_bossname.png", 384, 64);
    }
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &m_Patches),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);

    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(m_Patches);
    Translation_Init(m_Patches, &g_TranslationGame);
    Rng_SetSeed(&m_Rng, 4);
    Anm_Init(&g_HeapAllocator, &m_Rng);
    CHECK(Result_Log(Anm_Load(0, "text.anm", 0x700), LOG_LEVEL_ERROR));
    return directory;
}

static void M_TearDown(char *const directory)
{
    GuiTextImage_Release();
    Translation_Shutdown();
    Anm_Shutdown();
    Th07Data_Unmount();
    Patch_CloseStack(m_Patches);
    Mem_Free(directory);
}

#ifndef TEST_MS_GOTHIC
TEST(base_tsa_s_nameplate_images_replace_the_boss_title_and_name)
{
    char *const directory = M_SetUp("nameplate_images", /* with_images */ true);
    CHECK(Translation_HasBaseTsa());
    GuiTextImage_Load();
    GuiTextImage_ShowStage(1);
    CHECK(GuiTextImage_Replaces(M_NAMEPLATE_SCRIPT));
    CHECK(GuiTextImage_Replaces(M_NAMEPLATE_SCRIPT + 1));
    M_TearDown(directory);
}

TEST(base_tsa_without_nameplate_images_keeps_the_game_s_sprites)
{
    char *const directory =
        M_SetUp("no_nameplate_images", /* with_images */ false);
    CHECK(Translation_HasBaseTsa());
    GuiTextImage_Load();
    GuiTextImage_ShowStage(1);
    CHECK(!GuiTextImage_Replaces(M_NAMEPLATE_SCRIPT));
    CHECK(!GuiTextImage_Replaces(M_NAMEPLATE_SCRIPT + 1));
    M_TearDown(directory);
}
#endif

// Pins the registry to the font under test: the built-in IPA fonts, or MS
// Gothic from $OPENTOUHOU_MSGOTHIC.
static void M_UseFont(void)
{
#ifdef TEST_MS_GOTHIC
    size_t size;
    uint8_t *const data =
        TestFonts_ReadFile(TestFonts_RequireMsGothic(), &size);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("msgothic.ttc", data, size)));
    Mem_Free(data);
#else
    TestFonts_UseBuiltin();
#endif
}

static int32_t M_TextColumn(void)
{
    const IMAGE *const image = AnmTexture_GetImage(100);
    for (int32_t x = 0; x < image->width; x++) {
        for (int32_t y = 16; y < 31; y++) {
            if (image->pixels[((size_t)y * image->width + x) * 4 + 3] > 32) {
                return x;
            }
        }
    }
    return -1;
}

TEST(translated_text_uses_the_patch_s_alignment_padding)
{
    char *const directory = M_SetUp("alignment_padding", false);
    M_UseFont();
    CHECK(IS_OK(Anm_InitText(nullptr)));
    CHECK(IS_OK(Anm_CreateTexture(100, 512, 64, ANM_TEXTURE_RGBA8)));
    const ANM_SPRITE sprite = {
        .slot = 100,
        .y0 = 16,
        .width = 128,
        .texture_width = 512,
        .texture_height = 64,
        .scale_x = 1,
        .scale_y = 1,
    };
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15, .font_height = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, 0, "%s", "■")));
    const int32_t left = M_TextColumn();
    CHECK(left >= 0);
    CHECK(IS_OK(Anm_RenderTextRightAligned(&vm, 0xffffff, 0, "%s", "■")));
    CHECK_EQ_INT(M_TextColumn() - left, 110);
    CHECK(IS_OK(Anm_RenderTextCentered(&vm, 0xffffff, 0, "%s", "■")));
    CHECK_EQ_INT(M_TextColumn() - left, 53);
    Anm_ShutdownText();
    M_TearDown(directory);
}
