#include "tests/test.h"

#include "core/base/arena.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/json.h"
#include "stg/end_diff.h"

#include <string.h>

// Returns script with diff applied, as a new string, or nullptr when
// diff does not parse.
static char *M_ApplyDiff(
    const char *const script, const size_t size, const char *const diff)
{
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    JSON_VALUE *tree = nullptr;
    if (!Result_Log(
            Json_Parse(diff, strlen(diff), Arena_Allocator(&arena), &tree),
            LOG_LEVEL_ERROR)) {
        Arena_Free(&arena);
        return nullptr;
    }
    size_t patched_size;
    uint8_t *const patched =
        EndDiff_Apply((const uint8_t *)script, size, tree, &patched_size);
    char *const text = Mem_AllocZ(patched_size + 1);
    memcpy(text, patched, patched_size);
    Mem_Free(patched);
    Arena_Free(&arena);
    return text;
}

static void M_CheckDiff(
    const char *const script, const size_t size, const char *const diff,
    const char *const expected, const size_t expected_size)
{
    char *const text = M_ApplyDiff(script, size, diff);
    CHECK(text != nullptr && memcmp(text, expected, expected_size) == 0);
    CHECK(text != nullptr && text[expected_size] == '\0');
    Mem_Free(text);
}

#define M_CHECK_DIFF(script_, diff_, expected_)                                \
    M_CheckDiff(                                                               \
        script_, sizeof(script_) - 1, diff_, expected_, sizeof(expected_) - 1)

#define M_DIFF_SCRIPT                                                          \
    "@c255\0\r\n"                                                              \
    "A1\0\r\n"                                                                 \
    "A2\0\r\n"                                                                 \
    "@w10\0"                                                                   \
    "0\0\r\n"                                                                  \
    "B1\0\r\n"                                                                 \
    "@z\r\n"

TEST(a_jdiff_replaces_the_text_block_its_line_starts)
{
    M_CHECK_DIFF(
        M_DIFF_SCRIPT, "{'1': {lines: ['One', 'Two', 'Three']}}",
        "@c255\0\r\n"
        "One\r\nTwo\r\nThree\r\n"
        "@w10\0"
        "0\0\r\n"
        "B1\0\r\n"
        "@z\r\n");
}

TEST(a_jdiff_counts_the_command_lines)
{
    M_CHECK_DIFF(
        M_DIFF_SCRIPT, "{'4': {lines: ['Bee']}}",
        "@c255\0\r\n"
        "A1\0\r\n"
        "A2\0\r\n"
        "@w10\0"
        "0\0\r\n"
        "Bee\r\n"
        "@z\r\n");
}

TEST(a_jdiff_key_inside_a_block_does_nothing)
{
    M_CHECK_DIFF(
        M_DIFF_SCRIPT, "{'2': {lines: ['x']}, '#1': {lines: ['y']}}",
        M_DIFF_SCRIPT);
}

TEST(an_empty_jdiff_block_removes_the_text)
{
    M_CHECK_DIFF(
        M_DIFF_SCRIPT, "{'4': {lines: []}}",
        "@c255\0\r\n"
        "A1\0\r\n"
        "A2\0\r\n"
        "@w10\0"
        "0\0\r\n"
        "@z\r\n");
}

TEST(jdiff_lines_keep_their_markup)
{
    M_CHECK_DIFF(
        M_DIFF_SCRIPT, "{'4': {lines: ['<c$Reimu:$12345678> \"Hi.\"']}}",
        "@c255\0\r\n"
        "A1\0\r\n"
        "A2\0\r\n"
        "@w10\0"
        "0\0\r\n"
        "<c$Reimu:$12345678> \"Hi.\"\r\n"
        "@z\r\n");
}
