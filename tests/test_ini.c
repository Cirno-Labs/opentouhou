#include "core/ini.h"

#include "core/base/string.h"
#include "core/io/writer.h"

#include "test.h"
#include "test_allocator.h"
#include "test_dir.h"

#include <SDL3/SDL.h>
#include <string.h>

static char *M_WriteToBuffer(const INI *const ini)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Ini_Write(ini, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    char *const text = Mem_Alloc(size + 1);
    memcpy(text, data, size);
    text[size] = '\0';
    Allocator_Free(&g_HeapAllocator, data, size);
    Result_Ignore(Writer_Close(&writer));
    return text;
}

static INI *M_Parse(const ALLOCATOR *const allocator, const char *const text)
{
    INI *ini;
    CHECK(Result_Log(
        Ini_Parse(text, strlen(text), allocator, &ini), LOG_LEVEL_ERROR));
    return ini;
}

TEST(round_trip_reproduces_the_input_byte_for_byte)
{
    const char *const text =
        "; a leading comment\n"
        "loose = before any section\n"
        "\n"
        "[Video]\n"
        "  width =   640  \n"
        "height=480\n"
        "# another comment\n"
        "width = 800\n"
        "\n"
        "[Audio]\n"
        "volume=1.0\n"
        "not a key or section\n";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, text);
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(round_trip_without_a_final_newline)
{
    const char *const text = "[a]\nk=v";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, text);
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(crlf_line_endings_are_kept)
{
    const char *const text = "[a]\r\nk=v\r\n\r\n[b]\r\nx=y\r\n";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, text);
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(a_leading_bom_is_skipped_and_not_written_back)
{
    const char *const text = "\xEF\xBB\xBF[a]\nk=v\n";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk=v\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(duplicate_keys_keep_every_line_and_get_returns_the_last)
{
    const char *const text = "[a]\nk=1\nk=2\nk=3\n";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    CHECK_EQ_STR(Ini_Get(ini, "a", "k"), "3");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, text);
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(section_and_key_names_compare_without_case)
{
    const char *const text = "[Video]\nWidth=640\n";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    CHECK_EQ_STR(Ini_Get(ini, "video", "width"), "640");
    CHECK_EQ_STR(Ini_Get(ini, "VIDEO", "WIDTH"), "640");
    CHECK(Ini_Get(ini, "audio", "width") == nullptr);
    Ini_Free(ini);
}

TEST(invalid_utf8_fails_to_parse)
{
    const char text[] = "[a]\nk=\xFF\n";
    INI *ini;
    const RESULT result =
        Ini_Parse(text, sizeof(text) - 1, &g_HeapAllocator, &ini);
    CHECK(!IS_OK(result));
    CHECK(ini == nullptr);
    Result_Ignore(result);
}

TEST(unknown_lines_are_kept_and_do_not_fail_parsing)
{
    const char *const text = "not valid at all\n[a]\nk=v\n???\n";
    INI *const ini = M_Parse(&g_HeapAllocator, text);
    CHECK_EQ_STR(Ini_Get(ini, "a", "k"), "v");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, text);
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_replaces_an_existing_key_in_place)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk = 1\nj = 2\n");
    Ini_Set(ini, "a", "k", "9");
    CHECK_EQ_STR(Ini_Get(ini, "a", "k"), "9");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk = 9\nj = 2\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_of_an_empty_value_leaves_no_trailing_space)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nx = 1\n");
    Ini_Set(ini, "a", "x", "");
    Ini_Set(ini, "a", "y", "");
    CHECK_EQ_STR(Ini_Get(ini, "a", "x"), "");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nx =\ny =\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_adds_a_new_key_after_the_last_key_of_its_section)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\n[b]\nx=1\n");
    Ini_Set(ini, "a", "new", "9");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk=1\nnew = 9\n[b]\nx=1\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_adds_a_new_section_at_the_end)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\n");
    Ini_Set(ini, "b", "x", "1");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk=1\n\n[b]\nx = 1\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_adds_a_new_section_without_an_extra_blank_line)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\n\n");
    Ini_Set(ini, "b", "x", "1");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk=1\n\n[b]\nx = 1\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_in_the_implicit_section_before_any_header)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\n");
    Ini_Set(ini, "", "loose", "v");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "loose = v\n[a]\nk=1\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(set_on_an_empty_ini_creates_a_section_with_no_leading_blank_line)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "");
    Ini_Set(ini, "a", "k", "1");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk = 1\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(remove_deletes_every_duplicate_but_keeps_the_section_header)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\nj=2\nk=3\n");
    Ini_Remove(ini, "a", "k");
    CHECK(Ini_Get(ini, "a", "k") == nullptr);
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nj=2\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(remove_of_a_missing_key_does_nothing)
{
    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\n");
    Ini_Remove(ini, "a", "missing");
    Ini_Remove(ini, "missing", "k");
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "[a]\nk=1\n");
    Mem_Free(written);
    Ini_Free(ini);
}

TEST(load_of_a_missing_file_gives_an_empty_ini)
{
    char *const directory = TestDir_Prepare("missing");
    char *const path = TestDir_Join(directory, "config.ini");
    INI *ini;
    CHECK(Result_Log(Ini_Load(path, &g_HeapAllocator, &ini), LOG_LEVEL_ERROR));
    CHECK(Ini_Get(ini, "a", "k") == nullptr);
    char *const written = M_WriteToBuffer(ini);
    CHECK_EQ_STR(written, "");
    Mem_Free(written);
    Ini_Free(ini);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(save_is_atomic_and_load_reads_it_back)
{
    char *const directory = TestDir_Prepare("save");
    char *const path = TestDir_Join(directory, "config.ini");
    char *const temp_path = Str_Format("%s.tmp", path);

    INI *const ini = M_Parse(&g_HeapAllocator, "[a]\nk=1\n");
    Ini_Set(ini, "a", "k", "2");
    CHECK(Result_Log(Ini_Save(ini, path), LOG_LEVEL_ERROR));
    CHECK(!SDL_GetPathInfo(temp_path, nullptr));
    Ini_Free(ini);

    INI *loaded;
    CHECK(
        Result_Log(Ini_Load(path, &g_HeapAllocator, &loaded), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Ini_Get(loaded, "a", "k"), "2");
    Ini_Free(loaded);

    Mem_Free(temp_path);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(no_leaks_through_parsing_setting_removing_and_freeing)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    INI *const ini = M_Parse(&test.allocator, "[a]\nk=1\nj=2\nk=3\n[b]\nx=1\n");
    Ini_Set(ini, "a", "k", "9");
    Ini_Set(ini, "a", "new", "0");
    Ini_Set(ini, "c", "y", "1");
    Ini_Remove(ini, "a", "k");
    Ini_Free(ini);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(merge_sets_only_the_changed_keys_over_the_file)
{
    INI *const base = M_Parse(
        &g_HeapAllocator,
        "[video]\nfullscreen = false\ninterpolation = true\n"
        "[patches]\nlanguage = en\n");
    INI *const changed = M_Parse(
        &g_HeapAllocator,
        "[video]\nfullscreen = true\ninterpolation = true\n"
        "[patches]\nlanguage = en\n");
    INI *const disk = M_Parse(
        &g_HeapAllocator,
        "; my settings\n"
        "[patches]\n"
        "language = de ; set by othpatch\n"
        "[video]\n"
        "interpolation = false\n");
    Ini_MergeChanges(disk, base, changed);
    char *const written = M_WriteToBuffer(disk);
    CHECK_EQ_STR(
        written,
        "; my settings\n"
        "[patches]\n"
        "language = de ; set by othpatch\n"
        "[video]\n"
        "interpolation = false\n"
        "fullscreen = true\n");
    Mem_Free(written);
    Ini_Free(disk);
    Ini_Free(changed);
    Ini_Free(base);
}

TEST(merge_adds_the_keys_that_the_file_lacks)
{
    INI *const base =
        M_Parse(&g_HeapAllocator, "[video]\nfullscreen = false\n");
    INI *const changed = M_Parse(
        &g_HeapAllocator, "[video]\nfullscreen = false\n[new]\nkey = 1\n");
    INI *const disk = M_Parse(&g_HeapAllocator, "");
    Ini_MergeChanges(disk, base, changed);
    char *const written = M_WriteToBuffer(disk);
    CHECK_EQ_STR(written, "[video]\nfullscreen = false\n\n[new]\nkey = 1\n");
    Mem_Free(written);
    Ini_Free(disk);
    Ini_Free(changed);
    Ini_Free(base);
}
