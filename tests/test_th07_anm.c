#include "th07/anm.h"
#include "th07/anm/private.h"

#include "th07/files.h"
#include "th07/game.h"

#include "core/patch.h"

#include "patch_builder.h"
#include "test.h"
#include "gfx_recorder.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/writer.h"

#include "stg/rng.h"

#include <string.h>

#define M_NO_PATCH SIZE_MAX

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static RNG m_Rng;

static char *M_SetupFiles(
    const char *const case_name, const char *const name,
    const ANM_BUILDER *const builder)
{
    char *const directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(directory, &m_Version, 1);
    Th07Data_WriteMusic(directory, nullptr, 0);
    if (builder != nullptr) {
        AnmBuilder_WriteFile(builder, directory, name);
    }
    return directory;
}

static void M_Setup(const char *const directory)
{
    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    Rng_SetSeed(&m_Rng, 4);
    Anm_Init(&g_HeapAllocator, &m_Rng);
}

static void M_Teardown(char *const directory)
{
    Anm_Shutdown();
    Th07Data_Unmount();
    Mem_Free(directory);
}

static void M_AddEmptyScript(ANM_BUILDER_ANM *const anm, const int32_t id)
{
    AnmBuilder_End(anm, AnmBuilder_BeginScript(anm, id));
}

static bool M_Loads(
    const char *const case_name, const ANM_BUILDER *const builder,
    const int32_t slot, const int32_t first_sprite, char **const directory)
{
    *directory = M_SetupFiles(case_name, "test.anm", builder);
    M_Setup(*directory);
    return Result_Log(Anm_Load(slot, "test.anm", first_sprite), LOG_LEVEL_INFO);
}

static void M_WritePng(
    const char *const directory, const char *const name, const int32_t width,
    const int32_t height, const uint32_t rgba)
{
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int32_t i = 0; i < width * height; i++) {
        for (int32_t c = 0; c < 4; c++) {
            image.pixels[i * 4 + c] = (uint8_t)(rgba >> (24 - c * 8));
        }
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Image_EncodePng(&image, &writer);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    Th07Data_WriteFile(directory, name, bytes, size);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Image_Free(&image);
}

static uint32_t M_Pixel(const int32_t slot, const int32_t index)
{
    const uint8_t *const pixel = AnmTexture_GetImage(slot)->pixels + index * 4;
    return (uint32_t)pixel[0] << 24 | (uint32_t)pixel[1] << 16
        | (uint32_t)pixel[2] << 8 | pixel[3];
}

TEST(load_reads_sprites_and_uvs)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 256, 128, "@");
    AnmBuilder_AddSprite(anm, 0, 16.0f, 32.0f, 48.0f, 64.0f);
    AnmBuilder_AddSprite(anm, 3, 0.0f, 0.0f, 8.0f, 8.0f);
    char *directory;
    CHECK(M_Loads("sprites", &builder, 2, 100, &directory));

    const ANM_SPRITE *const sprite = Anm_GetSpriteEntry(100);
    CHECK_EQ_INT(sprite->slot, 2);
    CHECK(sprite->x0 == 16.0f);
    CHECK(sprite->y0 == 32.0f);
    CHECK(sprite->x1 == 64.0f);
    CHECK(sprite->y1 == 96.0f);
    CHECK(sprite->width == 48.0f);
    CHECK(sprite->height == 64.0f);
    CHECK(sprite->texture_width == 256.0f);
    CHECK(sprite->texture_height == 128.0f);
    CHECK(sprite->scale_x == 1.0f);
    CHECK(sprite->scale_y == 1.0f);
    CHECK(sprite->u0 == 16.0f / 256.0f);
    CHECK(sprite->v0 == 32.0f / 128.0f);
    CHECK(sprite->u1 == 64.0f / 256.0f);
    CHECK(sprite->v1 == 96.0f / 128.0f);
    CHECK_EQ_INT(Anm_GetSpriteEntry(103)->slot, 2);
    CHECK_EQ_INT(Anm_GetSpriteEntry(103)->id, sprite->id + 1);
    CHECK_EQ_INT(Anm_GetSpriteEntry(101)->slot, -1);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(2)), 256);
    CHECK_EQ_INT(GfxTexture_GetHeight(Anm_GetTexture(2)), 128);

    M_Teardown(directory);
}

TEST(load_reads_instructions_and_resolves_jumps)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    const int32_t script = AnmBuilder_BeginScript(anm, 5);
    const uint32_t sprite[] = { 7 };
    const int32_t first = AnmBuilder_Instr(anm, script, 3, 2, 0x5, sprite, 1);
    const int32_t second =
        AnmBuilder_Instr(anm, script, 37, 4, 0, (uint32_t[]) { 10000, 3 }, 2);
    AnmBuilder_Instr(
        anm, script, 4, 6, 0, (uint32_t[]) { (uint32_t)second, 1 }, 2);
    AnmBuilder_Instr(
        anm, script, 5, 6, 1, (uint32_t[]) { 10000, (uint32_t)first, 0 }, 3);
    for (int16_t opcode = 67; opcode <= 78; opcode++) {
        AnmBuilder_Instr(
            anm, script, opcode, 7, 0,
            (uint32_t[]) { 1, 2, (uint32_t)second, 9 }, 4);
    }
    AnmBuilder_Instr(
        anm, script, 66, 8, 0, (uint32_t[]) { (uint32_t)second }, 1);
    AnmBuilder_End(anm, script);
    char *directory;
    CHECK(M_Loads("instructions", &builder, 0, 200, &directory));

    const ANM_SCRIPT *const loaded = Anm_GetScript(205);
    CHECK(loaded != nullptr);
    CHECK_EQ_INT(loaded->count, 18);
    CHECK_EQ_INT(loaded->sprite_offset, 200);
    CHECK_EQ_INT(loaded->instrs[0].opcode, 3);
    CHECK_EQ_INT(loaded->instrs[0].time, 2);
    CHECK_EQ_INT(loaded->instrs[0].param_mask, 0x5);
    CHECK_EQ_INT(loaded->instrs[0].args[0].i, 7);
    CHECK_EQ_INT(loaded->instrs[0].args[1].i, 0);
    CHECK_EQ_INT(loaded->instrs[1].args[0].i, 10000);
    CHECK_EQ_INT(loaded->instrs[2].args[0].i, 1);
    CHECK_EQ_INT(loaded->instrs[2].args[1].i, 1);
    CHECK_EQ_INT(loaded->instrs[3].args[0].i, 10000);
    CHECK_EQ_INT(loaded->instrs[3].args[1].i, 0);
    for (int32_t i = 4; i < 16; i++) {
        CHECK_EQ_INT(loaded->instrs[i].opcode, 67 + i - 4);
        CHECK_EQ_INT(loaded->instrs[i].args[2].i, 1);
        CHECK_EQ_INT(loaded->instrs[i].args[3].i, 9);
    }
    CHECK_EQ_INT(loaded->instrs[15].args[0].i, 1);
    CHECK_EQ_INT(loaded->instrs[16].args[0].i, second);
    CHECK_EQ_INT(loaded->instrs[17].opcode, -1);
    CHECK(Anm_GetScript(204) == nullptr);

    M_Teardown(directory);
}

TEST(only_jumps_have_their_offsets_resolved)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    const int32_t script = AnmBuilder_BeginScript(anm, 0);
    AnmBuilder_Instr(anm, script, 66, 0, 0, (uint32_t[]) { 0, 0, 24 }, 3);
    AnmBuilder_Instr(anm, script, 79, 0, 0, (uint32_t[]) { 0, 0, 24 }, 3);
    AnmBuilder_End(anm, script);
    char *directory;
    CHECK(M_Loads("no_jump", &builder, 0, 0, &directory));
    CHECK_EQ_INT(Anm_GetScript(0)->instrs[0].args[2].i, 24);
    CHECK_EQ_INT(Anm_GetScript(0)->instrs[1].args[2].i, 24);
    M_Teardown(directory);
}

TEST(a_jump_between_instructions_fails)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    AnmBuilder_AddSprite(anm, 0, 0, 0, 8, 8);
    const int32_t script = AnmBuilder_BeginScript(anm, 0);
    AnmBuilder_Instr(anm, script, 4, 0, 0, (uint32_t[]) { 4, 0 }, 2);
    AnmBuilder_End(anm, script);
    char *directory;
    CHECK(!M_Loads("bad_jump", &builder, 0, 0, &directory));
    CHECK(Anm_GetScript(0) == nullptr);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0)->slot, -1);
    CHECK(Anm_GetTexture(0) == nullptr);
    M_Teardown(directory);
}

TEST(an_instruction_takes_up_to_eight_arguments)
{
    const uint32_t args[9] = {};
    for (int32_t count = 8; count <= 9; count++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder);
        ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
        const int32_t script = AnmBuilder_BeginScript(anm, 0);
        AnmBuilder_Instr(anm, script, 30, 0, 0, args, count);
        AnmBuilder_End(anm, script);
        char *directory;
        CHECK_EQ_INT(
            M_Loads(count == 8 ? "args8" : "args9", &builder, 0, 0, &directory),
            count == 8);
        M_Teardown(directory);
    }
}

static bool M_LoadsInstrSize(const char *const case_name, const uint16_t size)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    const int32_t script = AnmBuilder_BeginScript(anm, 0);
    AnmBuilder_Instr(anm, script, 30, 0, 0, (uint32_t[]) { 0, 0 }, 2);
    AnmBuilder_End(anm, script);
    anm->scripts[script].bytes[2] = (uint8_t)size;
    anm->scripts[script].bytes[3] = (uint8_t)(size >> 8);
    char *directory;
    const bool loads = M_Loads(case_name, &builder, 0, 0, &directory);
    M_Teardown(directory);
    return loads;
}

TEST(an_instruction_is_whole_words_of_at_least_8_bytes)
{
    CHECK(M_LoadsInstrSize("size16", 16));
    CHECK(!M_LoadsInstrSize("size4", 4));
    CHECK(!M_LoadsInstrSize("size0", 0));
    CHECK(!M_LoadsInstrSize("size14", 14));
}

// Loads builder cut to cut bytes, with the word at patch set to value.
static bool M_LoadsChanged(
    const char *const case_name, const ANM_BUILDER *const builder,
    const size_t cut, const size_t patch, const uint32_t value)
{
    char *const directory = M_SetupFiles(case_name, "test.anm", nullptr);
    size_t size;
    uint8_t *const bytes = AnmBuilder_Build(builder, &size);
    CHECK(cut <= size && (patch == M_NO_PATCH || patch + 4 <= size));
    if (patch != M_NO_PATCH) {
        for (int32_t i = 0; i < 4; i++) {
            bytes[patch + (size_t)i] = (uint8_t)(value >> (i * 8));
        }
    }
    Th07Data_WriteFile(directory, "test.anm", bytes, cut);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    M_Setup(directory);
    const bool loads = Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_INFO);
    CHECK(Anm_GetTexture(0) == nullptr || loads);
    M_Teardown(directory);
    return loads;
}

TEST(a_script_without_an_end_marker_fails)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
    const int32_t script = AnmBuilder_BeginScript(anm, 0);
    AnmBuilder_Instr(anm, script, 1, 0, 0, nullptr, 0);
    AnmBuilder_Instr(anm, script, 2, 0, 0, nullptr, 0);
    char *directory;
    CHECK(!M_Loads("no_end", &builder, 0, 0, &directory));
    M_Teardown(directory);
}

TEST(truncated_files_fail)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    const uint8_t pixels[8] = {};
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 2, 1, "x");
    AnmBuilder_AddSprite(anm, 0, 0, 0, 2, 1);
    M_AddEmptyScript(anm, 0);
    AnmBuilder_SetImage(anm, ANM_TEXTURE_RGBA8, 2, 1, pixels, 8);
    size_t size;
    uint8_t *const built = AnmBuilder_Build(&builder, &size);
    Allocator_Free(&g_HeapAllocator, built, size);
    const size_t sprite_offset = 0x40;
    const size_t script_offset = 0x40 + 4 + 4;
    const size_t image = 0x40 + 4 + 8 + 20 + 8 + 2;
    CHECK_EQ_INT((int32_t)size, (int32_t)(image + 16 + 8));
    CHECK(M_LoadsChanged("whole", &builder, size, M_NO_PATCH, 0));
    CHECK(!M_LoadsChanged("cut_pixel", &builder, size - 1, M_NO_PATCH, 0));
    CHECK(!M_LoadsChanged("cut_image", &builder, image + 15, M_NO_PATCH, 0));
    CHECK(!M_LoadsChanged("cut_header", &builder, 0x3f, M_NO_PATCH, 0));
    CHECK(!M_LoadsChanged("cut_empty", &builder, 0, M_NO_PATCH, 0));
    CHECK(!M_LoadsChanged(
        "sprite_past", &builder, size, sprite_offset, (uint32_t)size - 19));
    CHECK(!M_LoadsChanged(
        "script_past", &builder, size, script_offset, (uint32_t)size - 7));
    CHECK(!M_LoadsChanged("sprite_count", &builder, size, 0, 0x10000000));
    CHECK(!M_LoadsChanged("script_count", &builder, size, 4, 0x10000000));
}

TEST(load_fails_on_a_version_other_than_2)
{
    const int32_t versions[] = { 0, 1, 3, 2 };
    for (int32_t i = 0; i < 4; i++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder);
        ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 64, "@");
        AnmBuilder_AddSprite(anm, 0, 0, 0, 8, 8);
        anm->version = versions[i];
        char *const directory = TestDir_Prepare("version");
        Th07Data_WriteArchive(directory, &m_Version, 1);
        Th07Data_WriteMusic(directory, nullptr, 0);
        AnmBuilder_WriteFile(&builder, directory, "test.anm");
        M_Setup(directory);
        const bool loads =
            Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_INFO);
        CHECK_EQ_INT(loads, versions[i] == 2);
        CHECK_EQ_INT(Anm_GetSpriteEntry(0)->slot, loads ? 0 : -1);
        CHECK_EQ_INT(Anm_GetTexture(0) != nullptr, loads);
        M_Teardown(directory);
    }
}

static void M_LoadImage(
    const char *const case_name, const int16_t source, const uint8_t *pixels,
    const size_t size, const int32_t format, uint32_t *const out)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 2, 1, "stored.png");
    anm->format = format;
    AnmBuilder_SetImage(anm, source, 2, 1, pixels, size);
    char *directory;
    CHECK(M_Loads(case_name, &builder, 1, 0, &directory));
    CHECK_EQ_INT(AnmTexture_GetImage(1)->width, 2);
    CHECK_EQ_INT(AnmTexture_GetImage(1)->height, 1);
    out[0] = M_Pixel(1, 0);
    out[1] = M_Pixel(1, 1);
    M_Teardown(directory);
}

TEST(a_stored_a8r8g8b8_image_is_bgra_bytes)
{
    const uint8_t pixels[] = { 0x10, 0x20, 0x30, 0x40, 1, 2, 3, 4 };
    uint32_t out[2];
    M_LoadImage("image_rgba8", 1, pixels, sizeof(pixels), 0, out);
    CHECK_EQ_INT(out[0], 0x30201040);
    CHECK_EQ_INT(out[1], 0x03020104);
}

TEST(a_stored_r8g8b8_image_is_opaque_bgr_bytes)
{
    const uint8_t pixels[] = { 0x10, 0x20, 0x30, 1, 2, 3 };
    uint32_t out[2];
    M_LoadImage("image_rgb8", 4, pixels, sizeof(pixels), 0, out);
    CHECK_EQ_INT(out[0], 0x302010ff);
    CHECK_EQ_INT(out[1], 0x030201ff);
}

TEST(a_stored_a1r5g5b5_image_expands_its_bits)
{
    // 1 00001 10000 11111, and 0 11111 00000 00001.
    const uint8_t pixels[] = { 0x1f, 0x86, 0x01, 0x7c };
    uint32_t out[2];
    M_LoadImage("image_a1r5g5b5", 2, pixels, sizeof(pixels), 0, out);
    CHECK_EQ_INT(out[0], 0x0884ffff);
    CHECK_EQ_INT(out[1], 0xff000800);
}

TEST(a_stored_r5g6b5_image_expands_its_bits)
{
    // 00001 100000 11111, and 11111 000001 00000.
    const uint8_t pixels[] = { 0x1f, 0x0c, 0x20, 0xf8 };
    uint32_t out[2];
    M_LoadImage("image_r5g6b5", 3, pixels, sizeof(pixels), 0, out);
    CHECK_EQ_INT(out[0], 0x0882ffff);
    CHECK_EQ_INT(out[1], 0xff0400ff);
}

TEST(a_stored_a4r4g4b4_image_expands_its_bits)
{
    // 0001 0010 0011 0100, and 1111 1000 0000 1010.
    const uint8_t pixels[] = { 0x34, 0x12, 0x0a, 0xf8 };
    uint32_t out[2];
    M_LoadImage("image_a4r4g4b4", 5, pixels, sizeof(pixels), 0, out);
    CHECK_EQ_INT(out[0], 0x22334411);
    CHECK_EQ_INT(out[1], 0x8800aaff);
}

TEST(a_stored_image_is_quantized_to_the_anm_format)
{
    const uint8_t pixels[] = { 0x10, 0x20, 0x37, 0x80, 0, 0, 0, 0 };
    uint32_t out[2];
    M_LoadImage("image_to_4444", 1, pixels, sizeof(pixels), 5, out);
    CHECK_EQ_INT(out[0], 0x33221188);
    M_LoadImage("image_to_565", 1, pixels, sizeof(pixels), 3, out);
    CHECK_EQ_INT(out[0], 0x392010ff);
    M_LoadImage("image_to_888", 1, pixels, sizeof(pixels), 4, out);
    CHECK_EQ_INT(out[0], 0x372010ff);
}

TEST(a_stored_image_of_format_0_or_out_of_range_fails)
{
    const uint8_t pixels[16] = {};
    const int16_t formats[] = { 0, 6, -1 };
    for (int32_t i = 0; i < 3; i++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder);
        ANM_BUILDER_ANM *const anm =
            AnmBuilder_Add(&builder, 2, 1, "stored.png");
        AnmBuilder_SetImage(anm, formats[i], 2, 1, pixels, 16);
        char *directory;
        CHECK(!M_Loads("image_format", &builder, 0, 0, &directory));
        M_Teardown(directory);
    }
}

static void M_WritePatchedPng(
    const char *const patches, const char *const path, const int32_t width,
    const int32_t height, const uint32_t rgba)
{
    char *const dir = Str_Dup(path);
    char *const slash = strrchr(dir, '/');
    *slash = '\0';
    char *const full_dir = Str_Format("%s/%s", patches, dir);
    CHECK(Result_Log(File_MakeDir(full_dir), LOG_LEVEL_ERROR));
    Mem_Free(full_dir);
    Mem_Free(dir);
    M_WritePng(patches, path, width, height, rgba);
}

TEST(a_patch_blends_over_an_opaque_stored_image_by_its_name)
{
    const uint8_t stored[] = { 0, 0, 0, 0xff, 0, 0, 0, 0xff };
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 2, 1, "data/img.png");
    anm->format = ANM_TEXTURE_RGBA8;
    AnmBuilder_SetImage(anm, 1, 2, 1, stored, sizeof(stored));
    char *const directory =
        M_SetupFiles("stored_patched", "test.anm", &builder);

    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    M_WritePatchedPng(patches, "a/lang/th07/data/img.png", 2, 1, 0xff0000ff);
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);

    M_Setup(directory);
    Files_SetPatches(stack);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_Pixel(0, 0), 0xfe0000ff);

    Patch_CloseStack(stack);
    M_Teardown(directory);
}

TEST(empty_patch_sprites_keep_the_image_from_earlier_layers)
{
    const uint8_t stored[] = {
        0xff, 0, 0, 0xff, 0xff, 0, 0, 0xff, 0xff, 0, 0, 0xff, 0xff, 0, 0, 0xff,
    };
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 4, 1, "data/img.png");
    AnmBuilder_SetImage(anm, 1, 4, 1, stored, sizeof(stored));
    for (int32_t i = 0; i < 4; i++) {
        AnmBuilder_AddSprite(anm, i, (float)i, 0, 1, 1);
    }
    char *const directory = M_SetupFiles("patch_layers", "test.anm", &builder);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/base", "{}");
    PatchBuilder_AddPatch(patches, "a/lang", "{dependencies: ['base']}");
    M_WritePatchedPng(patches, "a/base/th07/data/img.png", 2, 1, 0xff0000ff);
    M_WritePatchedPng(patches, "a/lang/th07/data/img.png", 4, 1, 0);
    PATCH_STACK *stack = nullptr;
    CHECK(
        IS_OK(Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack)));
    M_Setup(directory);
    Files_SetPatches(stack);
    CHECK(IS_OK(Anm_Load(0, "test.anm", 0)));
    CHECK_EQ_INT(M_Pixel(0, 0), 0xfe0000ff);
    CHECK_EQ_INT(M_Pixel(0, 1), 0xfe0000ff);
    CHECK_EQ_INT(M_Pixel(0, 2), 0x0000ffff);
    CHECK_EQ_INT(M_Pixel(0, 3), 0x0000ffff);
    Patch_CloseStack(stack);
    Mem_Free(patches);
    M_Teardown(directory);
}

TEST(a_patch_overwrites_a_sprite_with_partial_original_alpha)
{
    const uint8_t stored[] = { 0xff, 0, 0, 0x80, 0xff, 0, 0, 0x80 };
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 2, 1, "data/img.png");
    AnmBuilder_SetImage(anm, 1, 2, 1, stored, sizeof(stored));
    AnmBuilder_AddSprite(anm, 0, 0, 0, 2, 1);
    char *const directory = M_SetupFiles("patch_alpha", "test.anm", &builder);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    M_WritePatchedPng(patches, "a/lang/th07/data/img.png", 2, 1, 0xff000040);
    PATCH_STACK *stack = nullptr;
    CHECK(
        IS_OK(Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack)));
    M_Setup(directory);
    Files_SetPatches(stack);
    CHECK(IS_OK(Anm_Load(0, "test.anm", 0)));
    CHECK_EQ_INT(M_Pixel(0, 0), 0xff000040);
    Patch_CloseStack(stack);
    Mem_Free(patches);
    M_Teardown(directory);
}

TEST(a_stored_image_with_no_patch_keeps_its_embedded_pixels)
{
    const uint8_t stored[] = { 0, 0, 0, 0xff, 0, 0, 0, 0xff };
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 2, 1, "data/img.png");
    anm->format = ANM_TEXTURE_RGBA8;
    AnmBuilder_SetImage(anm, 1, 2, 1, stored, sizeof(stored));
    char *directory;
    CHECK(M_Loads("stored_unpatched", &builder, 0, 0, &directory));
    CHECK_EQ_INT(M_Pixel(0, 0), 0x000000ff);
    M_Teardown(directory);
}

TEST(sprites_scale_to_a_texture_larger_than_the_header)
{
    uint8_t pixels[8 * 4 * 4] = {};
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 4, 1, "stored.png");
    AnmBuilder_SetImage(anm, 1, 8, 4, pixels, sizeof(pixels));
    AnmBuilder_AddSprite(anm, 0, 1.0f, 0.25f, 2.0f, 0.5f);
    char *directory;
    CHECK(M_Loads("scale", &builder, 0, 0, &directory));
    const ANM_SPRITE *const sprite = Anm_GetSpriteEntry(0);
    CHECK(sprite->scale_x == 2.0f);
    CHECK(sprite->scale_y == 4.0f);
    CHECK(sprite->x0 == 2.0f);
    CHECK(sprite->y0 == 1.0f);
    CHECK(sprite->x1 == 6.0f);
    CHECK(sprite->y1 == 3.0f);
    CHECK(sprite->width == 2.0f);
    CHECK(sprite->height == 0.5f);
    CHECK(sprite->texture_width == 8.0f);
    CHECK(sprite->texture_height == 4.0f);
    CHECK(sprite->u0 == 0.25f);
    CHECK(sprite->v1 == 0.75f);
    M_Teardown(directory);
}

TEST(a_scaled_sprite_rounds_as_the_original)
{
    // 3 / 7 is inexact, so the last bits show the original's operation order.
    uint8_t pixels[3 * 4] = {};
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 7, 1, "stored.png");
    AnmBuilder_SetImage(anm, 1, 3, 1, pixels, sizeof(pixels));
    AnmBuilder_AddSprite(anm, 0, 1.1f, 0.0f, 2.3f, 1.0f);
    char *directory;
    CHECK(M_Loads("scale_rounding", &builder, 0, 0, &directory));
    const ANM_SPRITE *const sprite = Anm_GetSpriteEntry(0);
    volatile float scale = 3.0f;
    scale /= 7.0f;
    volatile float x0 = scale * 1.1f;
    volatile float sum = 1.1f + 2.3f;
    volatile float x1 = sum * scale;
    volatile float span = x1 - x0;
    volatile float width = span / scale;
    volatile float u1 = x1 / 3.0f;
    CHECK(sprite->scale_x == scale);
    CHECK(sprite->x0 == x0);
    CHECK(sprite->x1 == x1);
    CHECK(sprite->width == width);
    CHECK(sprite->u1 == u1);
    M_Teardown(directory);
}

TEST(a_blank_texture_has_the_header_size)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 64, 32, "@");
    anm->format = ANM_TEXTURE_A1R5G5B5;
    char *directory;
    CHECK(M_Loads("blank", &builder, 3, 0, &directory));
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(3)), 64);
    CHECK_EQ_INT(GfxTexture_GetHeight(Anm_GetTexture(3)), 32);
    CHECK_EQ_INT(M_Pixel(3, 0), 0);
    M_Teardown(directory);
}

TEST(a_named_image_takes_the_alpha_of_its_mask)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 2, 2, "img.png");
    anm->alpha_name = "img_a.png";
    anm->format = ANM_TEXTURE_RGBA8;
    AnmBuilder_AddSprite(anm, 0, 0, 0, 2, 2);
    char *const directory = M_SetupFiles("mask", "test.anm", &builder);
    M_WritePng(directory, "img.png", 4, 4, 0xff0000ff);
    M_WritePng(directory, "img_a.png", 4, 4, 0x00004cff);
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(M_Pixel(0, 5), 0xff00004c);
    CHECK(Anm_GetSpriteEntry(0)->scale_x == 2.0f);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(0)), 4);
    M_Teardown(directory);
}

TEST(a_missing_named_image_or_mask_fails)
{
    for (int32_t i = 0; i < 2; i++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder);
        ANM_BUILDER_ANM *const anm =
            AnmBuilder_Add(&builder, 4, 4, i == 0 ? "gone.png" : "img.png");
        anm->alpha_name = "gone_a.png";
        char *const directory = M_SetupFiles("missing", "test.anm", &builder);
        M_WritePng(directory, "img.png", 4, 4, 0xff0000ff);
        M_Setup(directory);
        CHECK(!Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_INFO));
        CHECK(Anm_GetTexture(0) == nullptr);
        M_Teardown(directory);
    }
}

TEST(chained_anms_take_the_next_slots_and_numbers)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const first = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(first, 0, 0, 0, 1, 1);
    AnmBuilder_AddSprite(first, 2, 0, 0, 1, 1);
    M_AddEmptyScript(first, 5);
    ANM_BUILDER_ANM *const second = AnmBuilder_Add(&builder, 32, 16, "@");
    AnmBuilder_AddSprite(second, 1, 0, 0, 1, 1);
    M_AddEmptyScript(second, 0);
    ANM_BUILDER_ANM *const third = AnmBuilder_Add(&builder, 64, 16, "@");
    AnmBuilder_AddSprite(third, 0, 0, 0, 1, 1);
    char *directory;
    CHECK(M_Loads("chain", &builder, 4, 10, &directory));

    CHECK_EQ_INT(Anm_GetSpriteEntry(10)->slot, 4);
    CHECK_EQ_INT(Anm_GetSpriteEntry(12)->slot, 4);
    CHECK_EQ_INT(Anm_GetScript(15)->sprite_offset, 10);
    CHECK_EQ_INT(Anm_GetSpriteEntry(16)->slot, -1);
    CHECK_EQ_INT(Anm_GetSpriteEntry(17)->slot, 5);
    CHECK_EQ_INT(Anm_GetScript(16)->sprite_offset, 16);
    CHECK_EQ_INT(Anm_GetSpriteEntry(18)->slot, 6);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(4)), 16);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(5)), 32);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(6)), 64);
    CHECK(Anm_GetSpriteEntry(17)->texture_width == 32.0f);

    Anm_Release(5);
    CHECK(Anm_GetTexture(5) == nullptr);
    CHECK(Anm_GetScript(16) == nullptr);
    CHECK(Anm_GetTexture(6) != nullptr);
    Anm_Release(4);
    CHECK(Anm_GetTexture(4) == nullptr);
    CHECK(Anm_GetTexture(6) == nullptr);
    CHECK_EQ_INT(Anm_GetSpriteEntry(18)->slot, -1);
    CHECK(Anm_GetScript(15) == nullptr);
    M_Teardown(directory);
}

TEST(an_empty_anm_takes_one_number)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    AnmBuilder_Add(&builder, 16, 16, "@");
    ANM_BUILDER_ANM *const second = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(second, 0, 0, 0, 1, 1);
    char *directory;
    CHECK(M_Loads("chain_empty", &builder, 0, 0, &directory));
    CHECK_EQ_INT(Anm_GetSpriteEntry(0)->slot, -1);
    CHECK_EQ_INT(Anm_GetSpriteEntry(1)->slot, 1);
    M_Teardown(directory);
}

TEST(releasing_a_chain_releases_its_slots_loaded_again)
{
    ANM_BUILDER chain;
    AnmBuilder_Init(&chain);
    AnmBuilder_Add(&chain, 16, 16, "@");
    AnmBuilder_Add(&chain, 16, 16, "@");
    ANM_BUILDER single;
    AnmBuilder_Init(&single);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&single, 8, 8, "@");
    AnmBuilder_AddSprite(anm, 0, 0, 0, 1, 1);
    char *const directory = M_SetupFiles("chain_again", "test.anm", &chain);
    AnmBuilder_WriteFile(&single, directory, "single.anm");
    M_Setup(directory);
    CHECK(Result_Log(Anm_Load(0, "test.anm", 0), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Anm_Load(1, "single.anm", 100), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(1)), 8);
    Anm_Release(0);
    CHECK(Anm_GetTexture(1) == nullptr);
    CHECK_EQ_INT(Anm_GetSpriteEntry(100)->slot, -1);
    M_Teardown(directory);
}

TEST(a_chain_past_the_last_slot_fails_and_leaves_its_slots_empty)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(anm, 0, 0, 0, 1, 1);
    AnmBuilder_Add(&builder, 16, 16, "@");
    char *directory;
    CHECK(M_Loads(
        "chain_last_slot", &builder, ANM_SLOT_COUNT - 2, 0, &directory));
    CHECK(!Result_Log(
        Anm_Load(ANM_SLOT_COUNT - 1, "test.anm", 0), LOG_LEVEL_INFO));
    CHECK(Anm_GetTexture(ANM_SLOT_COUNT - 1) == nullptr);
    CHECK(Anm_GetTexture(ANM_SLOT_COUNT - 2) != nullptr);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0)->slot, -1);
    M_Teardown(directory);
}

TEST(a_failed_anm_of_a_chain_leaves_the_slots_before_it_empty)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(anm, 0, 0, 0, 1, 1);
    M_AddEmptyScript(anm, 0);
    AnmBuilder_Add(&builder, 16, 16, "@")->version = 0;
    char *directory;
    CHECK(!M_Loads("chain_fails", &builder, 2, 0, &directory));
    CHECK(Anm_GetTexture(2) == nullptr);
    CHECK(Anm_GetTexture(3) == nullptr);
    CHECK_EQ_INT(Anm_GetSpriteEntry(0)->slot, -1);
    CHECK(Anm_GetScript(0) == nullptr);
    M_Teardown(directory);
}

static bool M_LoadsNumbers(
    const char *const case_name, const int32_t sprite, const int32_t script,
    const int32_t first_sprite)
{
    ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *const anm = AnmBuilder_Add(&builder, 16, 16, "@");
    AnmBuilder_AddSprite(anm, sprite, 0, 0, 1, 1);
    M_AddEmptyScript(anm, script);
    char *directory;
    const bool loads =
        M_Loads(case_name, &builder, 0, first_sprite, &directory);
    M_Teardown(directory);
    return loads;
}

TEST(sprite_and_script_numbers_fit_the_tables)
{
    CHECK_EQ_INT(ANM_SPRITE_COUNT, 0xa00);
    CHECK_EQ_INT(ANM_SCRIPT_COUNT, 0xa00);
    CHECK_EQ_INT(ANM_SLOT_COUNT, 0x32);
    CHECK(M_LoadsNumbers("numbers_fit", 0x9fe, 0x9ff, 0));
    CHECK(M_LoadsNumbers("numbers_offset", 0x8fe, 0x8ff, 0x100));
    CHECK(!M_LoadsNumbers("sprite_over", 0xa00, 0, 0));
    CHECK(!M_LoadsNumbers("script_over", 0, 0x9ff, 1));
    CHECK(!M_LoadsNumbers("sprite_negative", -1, 0, 0));
    CHECK(!M_LoadsNumbers("script_negative", 0, -2, 1));
}

static void M_FillTexture(
    const int32_t slot, const int32_t width, const int32_t height,
    const uint32_t base)
{
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int32_t i = 0; i < width * height; i++) {
        const uint32_t rgba = base + (uint32_t)i * 0x01010100u;
        for (int32_t c = 0; c < 4; c++) {
            image.pixels[i * 4 + c] = (uint8_t)(rgba >> (24 - c * 8));
        }
    }
    CHECK(IS_OK(
        Anm_UpdateTexture(slot, (GFX_RECT) { 0, 0, width, height }, &image)));
    Image_Free(&image);
}

TEST(copy_texture_rect_copies_the_kept_pixels)
{
    CHECK(IS_OK(Anm_CreateTexture(200, 4, 4, ANM_TEXTURE_RGBA8)));
    CHECK(IS_OK(Anm_CreateTexture(201, 8, 4, ANM_TEXTURE_RGBA8)));
    M_FillTexture(200, 4, 4, 0x102030ffu);
    CHECK(IS_OK(Anm_CopyTextureRect(
        201, (GFX_RECT) { 5, 1, 2, 2 }, 200, (GFX_RECT) { 1, 2, 2, 2 })));
    CHECK_EQ_INT(M_Pixel(201, 1 * 8 + 5), M_Pixel(200, 2 * 4 + 1));
    CHECK_EQ_INT(M_Pixel(201, 1 * 8 + 6), M_Pixel(200, 2 * 4 + 2));
    CHECK_EQ_INT(M_Pixel(201, 2 * 8 + 5), M_Pixel(200, 3 * 4 + 1));
    CHECK_EQ_INT(M_Pixel(201, 2 * 8 + 6), M_Pixel(200, 3 * 4 + 2));
    CHECK_EQ_INT(M_Pixel(201, 1 * 8 + 4), 0);
    CHECK_EQ_INT(M_Pixel(201, 1 * 8 + 7), 0);
    CHECK_EQ_INT(M_Pixel(201, 0 * 8 + 5), 0);
    CHECK_EQ_INT(M_Pixel(201, 3 * 8 + 6), 0);
    CHECK_EQ_INT(GfxTexture_GetWidth(Anm_GetTexture(201)), 8);
    Anm_ReleaseTexture(200);
    Anm_ReleaseTexture(201);
}

TEST(copy_texture_rect_scales_to_dest_and_its_format)
{
    CHECK(IS_OK(Anm_CreateTexture(200, 2, 2, ANM_TEXTURE_RGBA8)));
    CHECK(IS_OK(Anm_CreateTexture(201, 8, 8, ANM_TEXTURE_A1R5G5B5)));
    IMAGE image = Image_Create(2, 2, &g_HeapAllocator);
    for (int32_t i = 0; i < 4; i++) {
        image.pixels[i * 4 + 0] = 0x47;
        image.pixels[i * 4 + 1] = 0x86;
        image.pixels[i * 4 + 2] = 0xc5;
        image.pixels[i * 4 + 3] = 0xff;
    }
    CHECK(IS_OK(Anm_UpdateTexture(200, (GFX_RECT) { 0, 0, 2, 2 }, &image)));
    Image_Quantize(&image, IMAGE_DEPTH_A1R5G5B5);
    const uint8_t *const pixel = image.pixels;
    const uint32_t expected = (uint32_t)pixel[0] << 24
        | (uint32_t)pixel[1] << 16 | (uint32_t)pixel[2] << 8 | pixel[3];
    Image_Free(&image);
    CHECK(expected != 0x4786c5ff);
    CHECK(IS_OK(Anm_CopyTextureRect(
        201, (GFX_RECT) { 2, 2, 4, 6 }, 200, (GFX_RECT) { 0, 0, 2, 2 })));
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const bool inside = x >= 2 && x < 6 && y >= 2;
            CHECK_EQ_INT(M_Pixel(201, y * 8 + x), inside ? expected : 0);
        }
    }
    Anm_ReleaseTexture(200);
    Anm_ReleaseTexture(201);
}

TEST(copy_texture_rect_skips_textures_not_loaded)
{
    CHECK(IS_OK(Anm_CreateTexture(200, 2, 2, ANM_TEXTURE_RGBA8)));
    M_FillTexture(200, 2, 2, 0x102030ffu);
    CHECK(IS_OK(Anm_CopyTextureRect(
        201, (GFX_RECT) { 0, 0, 2, 2 }, 200, (GFX_RECT) { 0, 0, 2, 2 })));
    CHECK(IS_OK(Anm_CopyTextureRect(
        200, (GFX_RECT) { 0, 0, 2, 2 }, 201, (GFX_RECT) { 0, 0, 2, 2 })));
    CHECK_EQ_INT(M_Pixel(200, 0), 0x102030ff);
    CHECK_EQ_INT(M_Pixel(200, 3), 0x132333ff);
    Anm_ReleaseTexture(200);
}

TEST(copy_texture_rect_outside_either_texture_fails)
{
    CHECK(IS_OK(Anm_CreateTexture(200, 4, 4, ANM_TEXTURE_RGBA8)));
    CHECK(IS_OK(Anm_CreateTexture(201, 4, 4, ANM_TEXTURE_RGBA8)));
    CHECK(!Result_Log(
        Anm_CopyTextureRect(
            201, (GFX_RECT) { 0, 0, 2, 2 }, 200, (GFX_RECT) { 3, 0, 2, 2 }),
        LOG_LEVEL_INFO));
    CHECK(!Result_Log(
        Anm_CopyTextureRect(
            201, (GFX_RECT) { 3, 3, 2, 2 }, 200, (GFX_RECT) { 0, 0, 2, 2 }),
        LOG_LEVEL_INFO));
    CHECK(!Result_Log(
        Anm_CopyTextureRect(
            201, (GFX_RECT) { 0, 0, 0, 2 }, 200, (GFX_RECT) { 0, 0, 2, 2 }),
        LOG_LEVEL_INFO));
    Anm_ReleaseTexture(200);
    Anm_ReleaseTexture(201);
}

// Sets the recorded screen to blue with source a gradient whose red and
// green are the position in it.
static void M_SetScreen(const GFX_RECT source)
{
    IMAGE screen = Image_Create(640, 480, &g_HeapAllocator);
    for (int32_t y = 0; y < 480; y++) {
        for (int32_t x = 0; x < 640; x++) {
            uint8_t *const pixel = &screen.pixels[(y * 640 + x) * 4];
            const bool inside = x >= source.x && x < source.x + source.width
                && y >= source.y && y < source.y + source.height;
            pixel[0] = inside ? (uint8_t)((x - source.x) * 40) : 0;
            pixel[1] = inside ? (uint8_t)((y - source.y) * 60) : 0;
            pixel[2] = inside ? 0x80 : 0xff;
            pixel[3] = 0xff;
        }
    }
    GfxRecorder_SetScreen(&screen);
    Image_Free(&screen);
}

static GFX_RECT m_CopySource = { 30, 20, 4, 3 };
static GFX_RECT m_CopyDest = { 2, 1, 8, 6 };

static void M_CheckCopied(const int32_t slot)
{
    IMAGE expected =
        Image_Create(m_CopySource.width, m_CopySource.height, &g_HeapAllocator);
    for (int32_t y = 0; y < m_CopySource.height; y++) {
        for (int32_t x = 0; x < m_CopySource.width; x++) {
            uint8_t *const pixel =
                &expected.pixels[(y * m_CopySource.width + x) * 4];
            pixel[0] = (uint8_t)(x * 40);
            pixel[1] = (uint8_t)(y * 60);
            pixel[2] = 0x80;
            pixel[3] = 0xff;
        }
    }
    Image_Resize(&expected, m_CopyDest.width, m_CopyDest.height);
    const IMAGE *const image = AnmTexture_GetImage(slot);
    for (int32_t y = 0; y < image->height; y++) {
        for (int32_t x = 0; x < image->width; x++) {
            const int32_t dx = x - m_CopyDest.x;
            const int32_t dy = y - m_CopyDest.y;
            const bool inside = dx >= 0 && dx < m_CopyDest.width && dy >= 0
                && dy < m_CopyDest.height;
            const uint8_t *const wanted = inside
                ? &expected.pixels[(dy * m_CopyDest.width + dx) * 4]
                : (const uint8_t[4]) {};
            CHECK(
                memcmp(&image->pixels[(y * image->width + x) * 4], wanted, 4)
                == 0);
        }
    }
    Image_Free(&expected);
}

TEST(a_pending_copy_refuses_another_until_it_is_made)
{
    CHECK(Anm_SetPendingCopy(200, m_CopySource, m_CopyDest));
    CHECK(!Anm_SetPendingCopy(201, m_CopySource, m_CopyDest));
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK(Anm_SetPendingCopy(201, m_CopySource, m_CopyDest));
    CHECK(IS_OK(Anm_DoPendingCopy()));
}

TEST(a_pending_copy_scales_the_screen_into_dest)
{
    GfxRecorder_Start();
    M_SetScreen(m_CopySource);
    CHECK(IS_OK(Anm_CreateTexture(200, 12, 8, ANM_TEXTURE_RGBA8)));
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 0);
    CHECK(Anm_SetPendingCopy(200, m_CopySource, m_CopyDest));
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 1);
    M_CheckCopied(200);
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 1);
    Anm_ReleaseTexture(200);
    GfxRecorder_Stop();
}

TEST(a_pending_copy_into_a_texture_not_loaded_does_nothing)
{
    GfxRecorder_Start();
    M_SetScreen(m_CopySource);
    CHECK(Anm_SetPendingCopy(200, m_CopySource, m_CopyDest));
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 0);
    CHECK(Anm_SetPendingCopy(200, m_CopySource, m_CopyDest));
    CHECK(IS_OK(Anm_DoPendingCopy()));
    GfxRecorder_Stop();
}

TEST(a_failed_pending_copy_is_dropped)
{
    GfxRecorder_Start();
    CHECK(IS_OK(Anm_CreateTexture(200, 12, 8, ANM_TEXTURE_RGBA8)));
    CHECK(Anm_SetPendingCopy(200, m_CopySource, m_CopyDest));
    CHECK(!Result_Log(Anm_DoPendingCopy(), LOG_LEVEL_INFO));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 1);
    M_SetScreen(m_CopySource);
    CHECK(IS_OK(Anm_DoPendingCopy()));
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 1);
    CHECK(Anm_SetPendingCopy(200, (GFX_RECT) { 638, 0, 4, 3 }, m_CopyDest));
    CHECK(!Result_Log(Anm_DoPendingCopy(), LOG_LEVEL_INFO));
    CHECK(Anm_SetPendingCopy(200, m_CopySource, (GFX_RECT) { 6, 4, 8, 6 }));
    CHECK(!Result_Log(Anm_DoPendingCopy(), LOG_LEVEL_INFO));
    CHECK_EQ_INT(AnmTexture_GetImage(200)->pixels[0], 0);
    Anm_ReleaseTexture(200);
    GfxRecorder_Stop();
}

static int32_t m_ReadsInCalc;

static CHAIN_STATUS M_RequestCopy(void *const arg)
{
    CHECK(Anm_SetPendingCopy(200, m_CopySource, m_CopyDest));
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_CountReads(void *const arg)
{
    m_ReadsInCalc = GfxRecorder_ReadCount();
    return CHAIN_CONTINUE;
}

TEST(the_game_makes_the_pending_copy_after_the_draw_and_update_chains)
{
    GfxRecorder_Start();
    M_SetScreen(m_CopySource);
    CHECK(IS_OK(Anm_CreateTexture(200, 12, 8, ANM_TEXTURE_RGBA8)));
    Chain_Init(&g_Chain);
    CHAIN_ELEM draw;
    CHAIN_ELEM calc;
    Chain_InitElem(&draw, M_RequestCopy, nullptr);
    Chain_InitElem(&calc, M_CountReads, nullptr);
    CHECK(IS_OK(Chain_AddDraw(&g_Chain, &draw, 1)));
    CHECK(IS_OK(Chain_AddCalc(&g_Chain, &calc, 1)));
    m_ReadsInCalc = -1;
    CHECK(Game_Tick(&(TH07_INPUT) {}));
    CHECK_EQ_INT(m_ReadsInCalc, 0);
    CHECK_EQ_INT(GfxRecorder_ReadCount(), 1);
    M_CheckCopied(200);
    Chain_Release(&g_Chain);
    Anm_ReleaseTexture(200);
    GfxRecorder_Stop();
}

TEST(packed_patch_channels_drop_their_low_bits)
{
    const ANM_TEXTURE_FORMAT formats[] = { ANM_TEXTURE_A4R4G4B4,
                                           ANM_TEXTURE_R5G6B5 };
    const uint32_t expected[] = { 0x11223344, 0x182c39ff };
    const uint8_t stored[] = { 0, 0x80, 0, 0x80 };
    for (int32_t i = 0; i < 2; i++) {
        ANM_BUILDER builder;
        AnmBuilder_Init(&builder);
        ANM_BUILDER_ANM *const anm =
            AnmBuilder_Add(&builder, 2, 1, "data/img.png");
        anm->format = formats[i];
        AnmBuilder_SetImage(anm, formats[i], 2, 1, stored, sizeof(stored));
        AnmBuilder_AddSprite(anm, 0, 0, 0, 2, 1);
        char name[40];
        snprintf(name, sizeof(name), "packed_patch_%d", i);
        char *const directory = M_SetupFiles(name, "test.anm", &builder);
        char *const patches = TestDir_Join(directory, "patches");
        PatchBuilder_AddPatch(patches, "a/lang", "{}");
        M_WritePatchedPng(
            patches, "a/lang/th07/data/img.png", 2, 1, 0x1f2f3f4f);
        PATCH_STACK *stack = nullptr;
        CHECK(IS_OK(
            Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack)));
        M_Setup(directory);
        Files_SetPatches(stack);
        CHECK(IS_OK(Anm_Load(0, "test.anm", 0)));
        CHECK_EQ_INT(M_Pixel(0, 0), expected[i]);
        Patch_CloseStack(stack);
        Mem_Free(patches);
        M_Teardown(directory);
    }
}
