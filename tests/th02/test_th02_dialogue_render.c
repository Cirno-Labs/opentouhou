#include "tests/test.h"
#include "tests/test_allocator.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/dialogue/private.h"
#include "th02/grcg.c"
#include "th02/stage.h"
#include "th02/stage/private.h"

typedef struct {
    VFS_SOURCE base;
    TEST_ALLOCATOR *readers;
    uint8_t text[65 * 80];
    uint8_t portraits[54 + 191 * 128];
    size_t text_size;
    size_t portrait_size;
    uint8_t stage;
    int32_t fail;
} SOURCE;

static bool M_Contains(const VFS_SOURCE *const base, const char *const path)
{
    const SOURCE *const source = (const SOURCE *)base;
    char name[] = "stage0.txt";
    name[5] += source->stage;
    return strcmp(path, name) == 0 || strcmp(path, "miko_k.mpn") == 0;
}

static RESULT M_Open(
    VFS_SOURCE *const base, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    SOURCE *const source = (SOURCE *)base;
    const bool portrait = strcmp(path, "miko_k.mpn") == 0;
    FAIL_IF(source->fail == (portrait ? 1 : 0), "Synthetic resource failure");
    const size_t size = portrait ? source->portrait_size : source->text_size;
    uint8_t *const bytes = Allocator_Alloc(&source->readers->allocator, size);
    memcpy(bytes, portrait ? source->portraits : source->text, size);
    Reader_OpenOwnedBuffer(
        out, bytes, size, &source->readers->allocator, nullptr);
    return OK;
}

static void M_Close(VFS_SOURCE *const source)
{
    Mem_Free(source);
}

static const VFS_SOURCE_FUNCS m_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .close = M_Close,
};

static SOURCE *M_Source(TEST_ALLOCATOR *const readers, const uint8_t stage)
{
    SOURCE *const source = Mem_AllocZ(sizeof(*source));
    source->base.funcs = &m_Funcs;
    source->base.name = "synthetic dialogue";
    source->readers = readers;
    source->stage = stage;
    source->fail = -1;
    source->text_size = 160;
    source->portrait_size = sizeof(source->portraits);
    memset(source->text, ' ', sizeof(source->text));
    for (size_t offset = 0; offset < source->text_size; offset += 40) {
        source->text[offset] = (uint8_t)('A' + offset / 40);
        source->text[offset + 5] = 0;
        memset(source->text + offset + 36, 0xfd, 4);
    }
    source->text[0] = 0x82;
    source->text[1] = 0xa0;
    memcpy(source->portraits, "MPTN", 4);
    source->portraits[4] = 190;
    memset(source->portraits + 6, 0x30, 48);
    for (size_t tile = 0; tile < 191; tile++) {
        const size_t color = (tile / 16 + tile % 16) & 15;
        for (size_t plane = 0; plane < 4; plane++) {
            memset(
                source->portraits + 54 + tile * 128 + plane * 32,
                (color & (1u << plane)) != 0 ? 0xff : 0, 32);
        }
    }
    return source;
}

static void M_Empty(const DIALOGUE_ASSETS *const assets)
{
    const uint8_t *const bytes = (const uint8_t *)assets;
    for (size_t i = 0; i < sizeof(*assets); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

TEST(cache_owns_all_stage_text_and_portraits_after_namespace_close)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    for (uint8_t stage = 0; stage < 6; stage++) {
        SOURCE *const source = M_Source(&readers, stage);
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(files, "", &source->base, 0);
        DIALOGUE_ASSETS assets;
        CHECK(Result_Log(
            DialogueAssets_Load(files, stage, &data.allocator, &assets),
            LOG_LEVEL_ERROR));
        CHECK_EQ_INT(readers.live_blocks, 0);
        Vfs_Destroy(files);
        CHECK_EQ_INT(assets.count, 2);
        CHECK_EQ_INT(assets.faces.count, 191);
        CHECK_EQ_INT(assets.faces.pixels[190 * 256], 9);
        CHECK_EQ_INT(assets.faces.palette[15].red, 48);
        CHECK_EQ_INT(assets.blank.count, 18);
        CHECK_EQ_INT(assets.blank.source_size, 36);
        CHECK_EQ_INT(assets.boxes[0].lines[0].source_size, 5);
        CHECK_EQ_INT(assets.boxes[0].lines[1].characters[0].code, 'B');

        memset(&m_Tram, 0xa5, sizeof(m_Tram));
        DIALOGUE_RENDER render = { .assets = &assets };
        const DIALOGUE_VIEW view = DialogueRender_View(&render);
        data.allocations_left = 0;
        view.wipe(view.context);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 14], 0x2101);
        CHECK_EQ_INT(m_Tram.codes[22 * 80 + 49], 0x2181);
        view.text(view.context, 0, 0, 1);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 14], 0x6102);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 15], 0x6182);
        view.text(view.context, 0, 0, 2);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 14], 0x2204);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 16], 0x2101);
        CHECK_EQ_INT(m_Tram.attributes[21 * 80 + 14], 0xe1);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 13], 0xa5a5);
        CHECK_EQ_INT(m_Tram.codes[22 * 80 + 50], 0xa5a5);
        CHECK_EQ_INT(m_Tram.codes[4095], 0xa5a5);
        DialogueAssets_Free(&assets);
        DialogueAssets_Free(&assets);
        M_Empty(&assets);
        CHECK_EQ_INT(data.live_blocks, 0);
        CHECK_EQ_INT(data.live_bytes, 0);
        data.allocations_left = -1;
    }
    CHECK_EQ_INT(readers.size_errors + data.size_errors, 0);
}

TEST(failed_open_length_encoding_and_portrait_load_release_every_allocation)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    for (int32_t failure = 0; failure < 9; failure++) {
        SOURCE *const source = M_Source(&readers, 2);
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(files, "", &source->base, 0);
        if (failure < 2) {
            source->fail = failure;
        } else if (failure == 2) {
            source->text_size = 79;
        } else if (failure == 3) {
            source->text_size = sizeof(source->text);
        } else if (failure == 4) {
            source->text[120] = 0xfd;
        } else if (failure == 5) {
            memset(source->text + 80, 'A', 36);
            source->text[115] = 0x82;
        } else if (failure == 6) {
            source->portrait_size = 10;
        } else if (failure == 7) {
            source->portraits[4] = 189;
        }
        DIALOGUE_ASSETS assets;
        CHECK(!Result_Ignore(DialogueAssets_Load(
            files, failure == 8 ? 6 : 2, &data.allocator, &assets)));
        M_Empty(&assets);
        CHECK_EQ_INT(readers.live_blocks, 0);
        CHECK_EQ_INT(data.live_blocks, 0);
        CHECK_EQ_INT(data.live_bytes, 0);
        Vfs_Destroy(files);
    }
    CHECK_EQ_INT(readers.size_errors + data.size_errors, 0);
}

TEST(portraits_ignore_clip_and_keep_tile_row_gaps_hidden_rows_and_plane_aliases)
{
    DIALOGUE_ASSETS assets = {};
    assets.faces.count = 191;
    assets.faces.pixels = Mem_Alloc(191 * 256);
    for (size_t tile = 0; tile < 191; tile++) {
        memset(
            assets.faces.pixels + tile * 256, (int)(tile / 16 + tile % 16) & 15,
            256);
    }
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Video_ShowPage(1);
    Video_SetSplit(123, 456, 789);
    TestClip_Set(100, 100, 101, 101);
    Grcg_Init();
    memcpy(
        m_Grcg.tiles, (const uint8_t[4]) { 1, 2, 3, 4 }, sizeof(m_Grcg.tiles));

    memset(&p_Stage->scroll, 0, sizeof(p_Stage->scroll));
    DIALOGUE_RENDER render = {
        .assets = &assets,
    };
    const DIALOGUE_VIEW view = DialogueRender_View(&render);
    view.face(view.context, 0);
    CHECK_EQ_INT(g_Video.pages[0][328 * 640 + 40], 0);
    CHECK_EQ_INT(g_Video.pages[0][328 * 640 + 56], 1);
    CHECK_EQ_INT(g_Video.pages[0][344 * 640 + 40], 1);
    CHECK_EQ_INT(g_Video.pages[0][360 * 640 + 40], 2);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    p_Stage->scroll.line = 71;
    view.face(view.context, 0);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 40], 0);
    CHECK_EQ_INT(g_Video.pages[0][400 * 640 + 40], 0);
    CHECK_EQ_INT(g_Video.pages[0][40], 15);
    CHECK_EQ_INT(g_Video.pages[0][15 * 640 + 40], 1);
    CHECK_EQ_INT(g_Video.pages[0][31 * 640 + 40], 2);
    CHECK_EQ_INT(g_Video.pages[0][296], 9);
    CHECK_EQ_INT(g_Video.pages[0][312], 11);
    CHECK_EQ_INT(g_Video.pages[0][328], 13);
    CHECK_EQ_INT(m_Grcg.mode, 0);
    CHECK_EQ_INT(m_Grcg.tiles[0], 1);
    CHECK_EQ_INT(m_Grcg.tiles[3], 4);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[1][i], 6);
    }
    Mem_Free(g_Video.pages[0]);
    Mem_Free(assets.faces.pixels);
}

TEST(black_portrait_clips_without_rolling_and_does_not_touch_hidden_rows)
{
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);

    Clip_Reset();
    Grcg_Init();
    memcpy(
        m_Grcg.tiles, (const uint8_t[4]) { 1, 2, 3, 4 }, sizeof(m_Grcg.tiles));

    memset(&p_Stage->scroll, 0, sizeof(p_Stage->scroll));

    p_Stage->scroll.line = 71;
    DIALOGUE_RENDER render = {};
    const DIALOGUE_VIEW view = DialogueRender_View(&render);
    view.face(view.context, 255);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 40], 0);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 87], 0);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 88], 15);
    CHECK_EQ_INT(g_Video.pages[0][400 * 640 + 40], 15);
    CHECK_EQ_INT(g_Video.pages[0][40], 15);
    CHECK_EQ_INT(m_Grcg.mode, 0);
    for (size_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(m_Grcg.tiles[i], 0);
    }
    Mem_Free(g_Video.pages[0]);
}

TEST(continue_digits_preserve_source_reveal_boundaries_and_immutable_text)
{
    TRAM_CHARACTER characters[8];
    TRAM_CHARACTER digits[10];
    for (size_t i = 0; i < ARRAY_SIZE(characters); i++) {
        characters[i] = (TRAM_CHARACTER) {
            .code = (uint16_t)(0x2101 + i),
            .partial_code = (uint16_t)(0x6101 + i),
            .single_code = (uint16_t)(0x3101 + i),
            .source_end = (uint16_t)(i * 2 + 2),
            .wide = true,
        };
    }
    for (size_t i = 0; i < ARRAY_SIZE(digits); i++) {
        digits[i] = (TRAM_CHARACTER) {
            .code = (uint16_t)(0x2301 + i),
            .partial_code = (uint16_t)(0x6301 + i),
            .source_end = 2,
            .wide = true,
        };
    }
    const TRAM_TEXT text = { "synthetic", characters, 8, 16 };
    DIALOGUE_BOX boxes[2] = { { { text, text } }, { { text, text } } };
    const DIALOGUE_ASSETS assets = { .boxes = boxes, .count = 2 };
    const unsigned counts[] = { 1, 9, 10, 99 };
    for (size_t c = 0; c < ARRAY_SIZE(counts); c++) {
        DIALOGUE_RENDER render = { .assets = &assets };
        DialogueRender_Continues(&render, 0, digits, (uint16_t)counts[c]);
        const DIALOGUE_VIEW view = DialogueRender_View(&render);
        for (uint16_t bytes = 0; bytes <= 16; bytes++) {
            memset(&m_Tram, 0xa5, sizeof(m_Tram));
            view.text(view.context, 0, 0, bytes);
            if (bytes > 8) {
                const TRAM_CHARACTER *const tens =
                    counts[c] >= 10 ? &digits[counts[c] / 10] : &characters[4];
                const uint16_t code =
                    bytes == 9 ? tens->partial_code : tens->code;
                CHECK_EQ_INT(m_Tram.codes[21 * 80 + 22], code);
                CHECK_EQ_INT(m_Tram.codes[21 * 80 + 23], code | 0x80);
            } else {
                CHECK_EQ_INT(m_Tram.codes[21 * 80 + 22], 0xa5a5);
            }
            if (bytes > 10) {
                const TRAM_CHARACTER *const ones = &digits[counts[c] % 10];
                const uint16_t code =
                    bytes == 11 ? ones->partial_code : ones->code;
                CHECK_EQ_INT(m_Tram.codes[21 * 80 + 24], code);
                CHECK_EQ_INT(m_Tram.codes[21 * 80 + 25], code | 0x80);
            } else {
                CHECK_EQ_INT(m_Tram.codes[21 * 80 + 24], 0xa5a5);
            }
            CHECK_EQ_INT(m_Tram.codes[21 * 80 + 13], 0xa5a5);
        }
        CHECK(boxes[0].lines[0].characters == characters);
        CHECK_EQ_INT(characters[4].code, 0x2105);
        CHECK_EQ_INT(characters[5].code, 0x2106);
        view.text(view.context, 0, 1, 16);
        CHECK_EQ_INT(m_Tram.codes[22 * 80 + 22], characters[4].code);
        view.text(view.context, 1, 0, 16);
        CHECK_EQ_INT(m_Tram.codes[21 * 80 + 24], characters[5].code);
    }
}
