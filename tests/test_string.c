#include "core/base/memory.h"
#include "core/base/string.h"

#include "test.h"

TEST(dup_copies_the_text)
{
    const char *const text = "東方";
    char *const copy = Str_Dup(text);
    CHECK(copy != text);
    CHECK_EQ_STR(copy, text);
    Mem_Free(copy);
}

TEST(format_returns_the_whole_text)
{
    char *const text = Str_Format("%s-%03d", "th", 6);
    CHECK_EQ_STR(text, "th-006");
    Mem_Free(text);

    char long_text[3001];
    memset(long_text, 'x', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    char *const formatted = Str_Format("<%s>", long_text);
    CHECK_EQ_INT(strlen(formatted), 3002);
    CHECK(formatted[3001] == '>');
    Mem_Free(formatted);
}

TEST(starts_with)
{
    CHECK(Str_StartsWith("th06.exe", "th06"));
    CHECK(Str_StartsWith("th06", ""));
    CHECK(!Str_StartsWith("th0", "th06"));
    CHECK(!Str_StartsWith("th07", "th06"));
}

TEST(ends_with)
{
    CHECK(Str_EndsWith("stage1.std", ".std"));
    CHECK(Str_EndsWith("stage1.std", ""));
    CHECK(!Str_EndsWith("std", ".std"));
    CHECK(!Str_EndsWith("stage1.ecl", ".std"));
}

TEST(equal_ignore_case_folds_ascii_only)
{
    CHECK(Str_EqualIgnoreCase("TITLE.ANM", "title.anm"));
    CHECK(Str_EqualIgnoreCase("", ""));
    CHECK(!Str_EqualIgnoreCase("title", "titles"));
    CHECK(!Str_EqualIgnoreCase("titles", "title"));
    CHECK(!Str_EqualIgnoreCase("Ä", "ä"));
    CHECK(!Str_EqualIgnoreCase("[", "{"));
}
