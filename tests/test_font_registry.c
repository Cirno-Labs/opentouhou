#include "core/text.h"
#include "core/text/private.h"

#include "test.h"
#include "test_fonts.h"

#include <string.h>

// Stands in for the system's fonts: m_System.data, when it is not
// nullptr, is the system's font of every family.
static struct {
    const uint8_t *data;
    size_t size;
    int32_t asked;
} m_System;

RESULT FontRegistry_AddSystemFont(const char *const family)
{
    m_System.asked++;
    FAIL_IF(m_System.data == nullptr, "The system has no font %s", family);
    return FontRegistry_AddSystemData(
        "system", Mem_Dup(m_System.data, m_System.size), m_System.size, family);
}

// Returns the table of the font in data with tag.
static uint8_t *M_Table(uint8_t *const data, const char *const tag)
{
    const int32_t count = data[4] << 8 | data[5];
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const record = data + 12 + i * 16;
        if (memcmp(record, tag, 4) == 0) {
            return data + M_TestFontsReadBe(record + 8);
        }
    }
    CHECK(false);
    return nullptr;
}

// Shrinks the cell of the font in data, which then draws its glyphs larger.
static void M_ShrinkCell(uint8_t *const data)
{
    uint8_t *const table = M_Table(data, "OS/2");
    table[76] = 0;
    table[77] = 0;
}

// Returns the width of "霊夢" in font at the cell height 30.
static int32_t M_Width(FONT *const font)
{
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height;
    CHECK(IS_OK(Text_Measure(font, "霊夢", &style, &width, &height)));
    return width;
}

// Returns the width that family opens with, or 0 when it does not open.
static int32_t M_FamilyWidth(const char *const family)
{
    FONT *font = nullptr;
    if (!Result_Ignore(FontRegistry_Open(family, 30, &font))) {
        return 0;
    }
    const int32_t width = M_Width(font);
    Font_Free(font);
    return width;
}

// Renders text in font at the cell height 30.
static IMAGE M_Render(FONT *const font, const char *const text)
{
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image;
    CHECK(IS_OK(Text_Render(font, text, &style, &g_HeapAllocator, &image)));
    return image;
}

static bool M_SameImage(const IMAGE *const a, const IMAGE *const b)
{
    return a->width == b->width && a->height == b->height
        && memcmp(
               a->pixels, b->pixels, (size_t)a->width * (size_t)a->height * 4)
        == 0;
}

// Returns whether family opens and draws text as reference does.
static bool M_DrawsAs(const char *const family, const char *const reference)
{
    FONT *font = nullptr;
    FONT *expected = nullptr;
    CHECK(IS_OK(FontRegistry_Open(family, 30, &font)));
    CHECK(IS_OK(FontRegistry_Open(reference, 30, &expected)));
    if (font == nullptr || expected == nullptr) {
        Font_Free(font);
        Font_Free(expected);
        return false;
    }
    IMAGE image = M_Render(font, "Reimu 霊夢");
    IMAGE other = M_Render(expected, "Reimu 霊夢");
    const bool same = M_SameImage(&image, &other);
    Image_Free(&other);
    Image_Free(&image);
    Font_Free(expected);
    Font_Free(font);
    return same;
}

TEST(names_match_in_every_language_ignoring_ascii_case)
{
    CHECK(IS_OK(Text_Init()));
    TestFonts_UseBuiltin();
    CHECK(M_DrawsAs("ipagothic", "IPAGothic"));
    CHECK(M_DrawsAs("IPAゴシック", "IPAGothic"));
    CHECK(M_DrawsAs("IPA Pゴシック", "IPAPGothic"));
    CHECK(!M_DrawsAs("IPAPGothic", "IPAGothic"));
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(a_collection_opens_the_face_with_the_name)
{
    CHECK(IS_OK(Text_Init()));
    size_t gothic_size, ui_size, size;
    uint8_t *const gothic = TestFonts_Read(TEST_FONT_GOTHIC, &gothic_size);
    uint8_t *const ui = TestFonts_Read(TEST_FONT_UI_GOTHIC, &ui_size);
    uint8_t *const collection =
        TestFonts_MakeCollection(gothic, gothic_size, ui, ui_size, &size);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("both.ttc", collection, size)));
    FONT *font = nullptr;
    CHECK(IS_OK(FontRegistry_Open("IPAPGothic", 30, &font)));
    IMAGE image = M_Render(font, "Reimu 霊夢");
    Font_Free(font);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("ui.ttf", ui, ui_size)));
    CHECK(IS_OK(FontRegistry_Open("IPAPGothic", 30, &font)));
    IMAGE expected = M_Render(font, "Reimu 霊夢");
    CHECK(M_SameImage(&image, &expected));
    Image_Free(&expected);
    Image_Free(&image);
    Font_Free(font);
    Mem_Free(collection);
    Mem_Free(ui);
    Mem_Free(gothic);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(the_later_font_wins_a_name)
{
    CHECK(IS_OK(Text_Init()));
    size_t size;
    uint8_t *const data = TestFonts_Read(TEST_FONT_GOTHIC, &size);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("first", data, size)));
    const int32_t first = M_FamilyWidth("IPAGothic");
    M_ShrinkCell(data);
    CHECK(IS_OK(FontRegistry_Add("second", data, size)));
    CHECK(M_FamilyWidth("IPAGothic") > first);
    Mem_Free(data);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(a_family_that_no_font_has_fails)
{
    CHECK(IS_OK(Text_Init()));
    TestFonts_UseBuiltin();
    FONT *font = nullptr;
    CHECK(!Result_Ignore(FontRegistry_Open("ＭＳ ゴシック", 30, &font)));
    CHECK(font == nullptr);
    CHECK(!Result_Ignore(FontRegistry_Open(nullptr, 30, &font)));
    CHECK(font == nullptr);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(the_system_is_asked_once_for_each_family_it_lacks)
{
    CHECK(IS_OK(Text_Init()));
    memset(&m_System, 0, sizeof(m_System));
    FontRegistry_Reset(false);
    CHECK(M_FamilyWidth("Aroania Nowhere") == 0);
    CHECK_EQ_INT(m_System.asked, 0);
    FontRegistry_Reset(true);
    TestFonts_Add(TEST_FONT_GOTHIC);
    CHECK(M_FamilyWidth("IPAGothic") > 0);
    CHECK(M_FamilyWidth("Aroania Nowhere") == 0);
    CHECK(M_FamilyWidth("aroania nowhere") == 0);
    CHECK(M_FamilyWidth("Aroania Elsewhere") == 0);
    CHECK_EQ_INT(m_System.asked, 2);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(a_system_font_without_the_family_adds_nothing)
{
    CHECK(IS_OK(Text_Init()));
    memset(&m_System, 0, sizeof(m_System));
    uint8_t *const data = TestFonts_Read(TEST_FONT_GOTHIC, &m_System.size);
    m_System.data = data;
    FontRegistry_Reset(true);
    CHECK(M_FamilyWidth("Aroania Nowhere") == 0);
    CHECK_EQ_INT(m_System.asked, 1);
    m_System.data = nullptr;
    CHECK(M_FamilyWidth("IPAGothic") == 0);
    Mem_Free(data);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(a_system_font_leaves_the_added_fonts_their_names)
{
    CHECK(IS_OK(Text_Init()));
    memset(&m_System, 0, sizeof(m_System));
    size_t gothic_size, ui_size, size;
    uint8_t *const gothic = TestFonts_Read(TEST_FONT_GOTHIC, &gothic_size);
    uint8_t *const ui = TestFonts_Read(TEST_FONT_UI_GOTHIC, &ui_size);
    uint8_t *const collection =
        TestFonts_MakeCollection(gothic, gothic_size, ui, ui_size, &size);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("both", collection, size)));
    const int32_t system = M_FamilyWidth("IPAGothic");
    M_ShrinkCell(gothic);
    FontRegistry_Reset(true);
    CHECK(IS_OK(FontRegistry_Add("added", gothic, gothic_size)));
    const int32_t added = M_FamilyWidth("IPAGothic");
    CHECK(added > system);
    // The system's font for IPAPGothic has IPAGothic too.
    m_System.data = collection;
    m_System.size = size;
    CHECK(M_FamilyWidth("IPAPGothic") > 0);
    CHECK_EQ_INT(m_System.asked, 1);
    CHECK_EQ_INT(M_FamilyWidth("IPAGothic"), added);
    Mem_Free(collection);
    Mem_Free(ui);
    Mem_Free(gothic);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

// Renames the font in data to a face that is not Regular, with the full
// name JPAGothic.
static void M_MakeOtherFace(uint8_t *const data)
{
    uint8_t *const os2 = M_Table(data, "OS/2");
    os2[63] &= (uint8_t)~0x40;
    uint8_t *const name = M_Table(data, "name");
    const int32_t count = name[2] << 8 | name[3];
    const int32_t storage = name[4] << 8 | name[5];
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const entry = name + 6 + i * 12;
        const int32_t id = entry[6] << 8 | entry[7];
        uint8_t *const text = name + storage + (entry[10] << 8 | entry[11]);
        // Macintosh names have a byte for the first character, others two.
        uint8_t *const first = entry[1] == 1 ? text : text + 1;
        if (id == 2) {
            *first = 'X';
        } else if (id == 4 && *first == 'I') {
            *first = 'J';
        }
    }
}

TEST(a_family_opens_its_regular_face)
{
    CHECK(IS_OK(Text_Init()));
    size_t size;
    uint8_t *const data = TestFonts_Read(TEST_FONT_GOTHIC, &size);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("regular", data, size)));
    const int32_t regular = M_FamilyWidth("IPAGothic");
    M_ShrinkCell(data);
    M_MakeOtherFace(data);
    CHECK(IS_OK(FontRegistry_Add("other", data, size)));
    CHECK_EQ_INT(M_FamilyWidth("IPAGothic"), regular);
    CHECK(M_FamilyWidth("JPAGothic") > regular);
    Mem_Free(data);
    FontRegistry_Reset(false);
    Text_Shutdown();
}

TEST(data_that_is_not_a_font_is_refused)
{
    FontRegistry_Reset(false);
    CHECK(!Result_Ignore(FontRegistry_Add("text", "not a font file", 15)));
    const uint8_t empty[12] = { 0, 1, 0, 0 };
    CHECK(!Result_Ignore(FontRegistry_Add("empty", empty, sizeof(empty))));
    CHECK(!Result_Ignore(FontRegistry_Add("nothing", nullptr, 0)));
}

TEST(a_file_is_found_by_its_name_ignoring_ascii_case)
{
    FontRegistry_Reset(false);
    char data[] = "font rom";
    FontRegistry_AddFile("FONT.ROM", "first", data, sizeof(data));
    data[0] = 'F';
    const void *found = nullptr;
    size_t size = 0;
    CHECK(FontRegistry_FindFile("font.rom", &found, &size));
    CHECK(size == sizeof(data));
    CHECK(memcmp(found, "font rom", size) == 0);
    CHECK(!FontRegistry_FindFile("NEC.BMP", &found, &size));
    FontRegistry_Reset(false);
}

TEST(the_later_file_wins_a_name)
{
    FontRegistry_Reset(false);
    FontRegistry_AddFile("nec.bmp", "first", "first", 5);
    FontRegistry_AddFile("NEC.BMP", "second", "second!", 7);
    const void *found = nullptr;
    size_t size = 0;
    CHECK(FontRegistry_FindFile("Nec.Bmp", &found, &size));
    CHECK(size == 7);
    CHECK(memcmp(found, "second!", size) == 0);
    FontRegistry_Reset(false);
}

TEST(reset_forgets_the_files)
{
    FontRegistry_Reset(false);
    FontRegistry_AddFile("shnmk16.bdf", "test", "STARTFONT", 9);
    FontRegistry_Reset(false);
    const void *found = nullptr;
    size_t size = 0;
    CHECK(!FontRegistry_FindFile("shnmk16.bdf", &found, &size));
}

TEST(fonts_do_not_open_before_init)
{
    TestFonts_UseBuiltin();
    FONT *font = nullptr;
    CHECK(!Result_Ignore(FontRegistry_Open("IPAGothic", 30, &font)));
    CHECK(font == nullptr);
    FontRegistry_Reset(false);
}

TEST(a_fallback_draws_what_the_font_lacks)
{
    CHECK(IS_OK(Text_Init()));
    TestFonts_UseBuiltin();
    FONT *font = nullptr;
    FONT *fallback = nullptr;
    CHECK(IS_OK(FontRegistry_Open("IPAGothic", 30, &font)));
    CHECK(IS_OK(FontRegistry_Open("IPAPGothic", 30, &fallback)));
    Font_SetFallback(font, fallback);
    CHECK(IS_OK(Font_SetSize(font, 15)));
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height;
    CHECK(IS_OK(Text_Measure(font, "Reimu", &style, &width, &height)));
    CHECK(width > 0);
    Font_SetFallback(font, nullptr);
    Font_Free(font);
    FontRegistry_Reset(false);
    Text_Shutdown();
}
