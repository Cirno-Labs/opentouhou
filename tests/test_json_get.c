#include "core/base/arena.h"
#include "core/json.h"

#include "test.h"
#include "test_json.h"

#include <math.h>

static ARENA m_Arena;

[[gnu::destructor]] static void M_FreeArena(void)
{
    Arena_Free(&m_Arena);
}

static const JSON_VALUE *M_Parse(const char *const text)
{
    if (m_Arena.base == nullptr) {
        Arena_Init(&m_Arena, "json test", 1 << 20);
    }
    return TestJson_Parse(Arena_Allocator(&m_Arena), text);
}

// A patch.js as thcrap's repositories serve it, with the comments and the
// trailing commas that hand-written patch files have.
static const char m_PatchJs[] =
    "{\n"
    "\t// Identifies the patch in the repository.\n"
    "\t\"id\": \"lang_en\",\n"
    "\t\"title\": \"English translation\",\n"
    "\t\"dependencies\": [\n"
    "\t\t\"nmlgc/base_tsa\",\n"
    "\t\t\"lang_en_base\",\n"
    "\t],\n"
    "\t\"update\": {\n"
    "\t\t\"servers\": [\n"
    "\t\t\t\"https://srv.thpatch.net/lang_en/\",\n"
    "\t\t\t\"https://thcrap.thpatch.net/lang_en/\",\n"
    "\t\t],\n"
    "\t\t\"interval\": 86400,\n"
    "\t},\n"
    "\t/* Overrides of the games' fonts. */\n"
    "\t\"fonts\": {\n"
    "\t\t\"Noto Sans JP\": true,\n"
    "\t},\n"
    "\t\"motd\": \"Translations by the \\\"Touhou Patch Center\\\" "
    "community\",\n"
    "\t\"motd_type\": 0x40,\n"
    "\t\"version\": 1.5,\n"
    "}\n";

TEST(get_finds_members_by_key)
{
    const JSON_VALUE *const root = M_Parse("{\"a\": 1, b: \"two\", c: null}");
    CHECK_EQ_INT(Json_GetInt(Json_Get(root, "a"), -1), 1);
    CHECK_EQ_STR(Json_GetString(Json_Get(root, "b"), ""), "two");
    const JSON_VALUE *const c = Json_Get(root, "c");
    CHECK(c != nullptr && c->type == JSON_NULL);
    CHECK(Json_Get(root, "d") == nullptr);
    CHECK(Json_Get(root, "") == nullptr);
    CHECK(Json_Get(root, nullptr) == nullptr);
}

TEST(get_returns_the_last_of_duplicate_keys)
{
    const JSON_VALUE *const root = M_Parse("{a: 1, b: 2, a: 3}");
    CHECK_EQ_INT(Json_GetInt(Json_Get(root, "a"), -1), 3);
    CHECK_EQ_INT(Json_GetInt(Json_Get(root, "b"), -1), 2);
}

TEST(get_returns_nullptr_for_non_objects)
{
    CHECK(Json_Get(nullptr, "a") == nullptr);
    CHECK(Json_Get(M_Parse("[\"a\"]"), "a") == nullptr);
    CHECK(Json_Get(M_Parse("\"a\""), "a") == nullptr);
    CHECK(Json_Get(M_Parse("null"), "a") == nullptr);
}

TEST(at_returns_items_in_range)
{
    const JSON_VALUE *const root = M_Parse("[10, 20, 30]");
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 0), -1), 10);
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 2), -1), 30);
    CHECK(Json_At(root, 3) == nullptr);
    CHECK(Json_At(root, -1) == nullptr);
    CHECK(Json_At(root, INT32_MAX) == nullptr);
    CHECK(Json_At(M_Parse("[]"), 0) == nullptr);
    CHECK(Json_At(M_Parse("{\"0\": 1}"), 0) == nullptr);
    CHECK(Json_At(nullptr, 0) == nullptr);
}

TEST(getters_return_values_of_their_type)
{
    const JSON_VALUE *const root = M_Parse(
        "[true, false, 42, -9223372036854775808, 2.5, \"text\", "
        "9007199254740993]");
    CHECK(Json_GetBool(Json_At(root, 0), false));
    CHECK(!Json_GetBool(Json_At(root, 1), true));
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 2), -1), 42);
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 3), -1), INT64_MIN);
    CHECK(Json_GetNumber(Json_At(root, 2), -1.0) == 42.0);
    CHECK(Json_GetNumber(Json_At(root, 4), -1.0) == 2.5);
    CHECK_EQ_STR(Json_GetString(Json_At(root, 5), nullptr), "text");
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 6), -1), 9007199254740993);
    CHECK(Json_GetNumber(Json_At(root, 6), -1.0) == 9007199254740992.0);
}

TEST(getters_return_the_fallback_for_other_types)
{
    const JSON_VALUE *const root =
        M_Parse("[null, true, 1, 1.5, \"1\", [1], {\"a\": 1}, NaN]");
    for (int32_t i = 0; i < 8; i++) {
        const JSON_VALUE *const value = Json_At(root, i);
        if (value->type != JSON_BOOL) {
            CHECK(Json_GetBool(value, true));
            CHECK(!Json_GetBool(value, false));
        }
        if (value->type != JSON_NUMBER) {
            CHECK_EQ_INT(Json_GetInt(value, -7), -7);
            CHECK(Json_GetNumber(value, -7.5) == -7.5);
        }
        if (value->type != JSON_STRING) {
            CHECK_EQ_STR(Json_GetString(value, "fallback"), "fallback");
            CHECK(Json_GetString(value, nullptr) == nullptr);
        }
    }
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 3), -7), -7);
    CHECK_EQ_INT(Json_GetInt(Json_At(root, 7), -7), -7);
    CHECK(isnan(Json_GetNumber(Json_At(root, 7), 0.0)));
}

TEST(getters_accept_nullptr)
{
    CHECK(Json_GetBool(nullptr, true));
    CHECK_EQ_INT(Json_GetInt(nullptr, 5), 5);
    CHECK(Json_GetNumber(nullptr, 0.5) == 0.5);
    CHECK_EQ_STR(Json_GetString(nullptr, "none"), "none");
    CHECK(Json_GetString(nullptr, nullptr) == nullptr);
}

TEST(lookups_chain_without_checks)
{
    const JSON_VALUE *const patch = M_Parse(m_PatchJs);
    CHECK_EQ_STR(
        Json_GetString(
            Json_At(Json_Get(Json_Get(patch, "update"), "servers"), 0),
            nullptr),
        "https://srv.thpatch.net/lang_en/");
    CHECK(
        Json_GetString(
            Json_At(Json_Get(Json_Get(patch, "updates"), "servers"), 0),
            nullptr)
        == nullptr);
    CHECK(
        Json_GetString(
            Json_At(Json_Get(Json_Get(patch, "update"), "servers"), 2), nullptr)
        == nullptr);
    CHECK_EQ_INT(
        Json_GetInt(Json_Get(Json_Get(patch, "title"), "x"), 123), 123);
}

TEST(reads_a_patch_js)
{
    const JSON_VALUE *const patch = M_Parse(m_PatchJs);
    CHECK_EQ_STR(Json_GetString(Json_Get(patch, "id"), ""), "lang_en");
    CHECK_EQ_STR(
        Json_GetString(Json_Get(patch, "title"), ""), "English translation");

    const JSON_VALUE *const dependencies = Json_Get(patch, "dependencies");
    CHECK(dependencies != nullptr && dependencies->array.count == 2);
    CHECK_EQ_STR(
        Json_GetString(Json_At(dependencies, 0), ""), "nmlgc/base_tsa");
    CHECK_EQ_STR(Json_GetString(Json_At(dependencies, 1), ""), "lang_en_base");

    const JSON_VALUE *const update = Json_Get(patch, "update");
    const JSON_VALUE *const servers = Json_Get(update, "servers");
    CHECK(servers != nullptr && servers->array.count == 2);
    CHECK_EQ_STR(
        Json_GetString(Json_At(servers, 1), ""),
        "https://thcrap.thpatch.net/lang_en/");
    CHECK_EQ_INT(Json_GetInt(Json_Get(update, "interval"), 0), 86400);

    CHECK(Json_GetBool(
        Json_Get(Json_Get(patch, "fonts"), "Noto Sans JP"), false));
    CHECK_EQ_STR(
        Json_GetString(Json_Get(patch, "motd"), ""),
        "Translations by the \"Touhou Patch Center\" community");
    CHECK_EQ_INT(Json_GetInt(Json_Get(patch, "motd_type"), 0), 0x40);
    CHECK(Json_GetNumber(Json_Get(patch, "version"), 0.0) == 1.5);
}
