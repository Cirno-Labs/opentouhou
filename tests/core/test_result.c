#include "core/base/result.h"

#include "test.h"

static int m_Line;

static RESULT M_ReadVersion(const int version)
{
    m_Line = __LINE__ + 1;
    FAIL_IF(version != 1, "unknown version %d", version);
    return OK;
}

static RESULT M_Load(const char *const path, const int version)
{
    MUST(M_ReadVersion(version), "%s", path);
    return OK;
}

static RESULT M_LoadWithoutContext(const int version)
{
    MUST(M_ReadVersion(version));
    return OK;
}

TEST(ok_is_a_success)
{
    const RESULT result = OK;
    CHECK(IS_OK(result));
    CHECK(Result_Ignore(result));
}

TEST(fail_records_the_site_and_the_message)
{
    const RESULT result = M_ReadVersion(7);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.site->file, "test_result.c") != nullptr);
    CHECK_EQ_INT(result.site->line, m_Line);
    CHECK_EQ_STR(result.message, "unknown version 7");
    CHECK(!Result_Ignore(result));
}

TEST(must_passes_a_success_through)
{
    CHECK(Result_Ignore(M_Load("stage1.ecl", 1)));
}

TEST(must_adds_context_and_keeps_the_site)
{
    const RESULT result = M_Load("stage1.ecl", 2);
    CHECK_EQ_INT(result.site->line, m_Line);
    CHECK_EQ_STR(result.message, "stage1.ecl: unknown version 2");
    CHECK(!Result_Ignore(result));
}

TEST(must_without_context_keeps_the_message)
{
    const RESULT result = M_LoadWithoutContext(3);
    CHECK_EQ_STR(result.message, "unknown version 3");
    CHECK(!Result_Ignore(result));
}

TEST(log_writes_a_failure_at_its_site)
{
    if (freopen("test_result.txt", "w+", stderr) == nullptr) {
        abort();
    }
    CHECK(Result_Log(OK, LOG_LEVEL_ERROR));
    CHECK(!Result_Log(M_Load("a.anm", 0), LOG_LEVEL_ERROR));
    fflush(stderr);
    rewind(stderr);

    char output[256] = {};
    char expected[256];
    snprintf(
        expected, sizeof(expected),
        "ERROR test_result.c:%d: a.anm: unknown version 0\n", m_Line);
    CHECK(fread(output, 1, sizeof(output) - 1, stderr) > 0);
    CHECK_EQ_STR(output, expected);
}
