// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/log.h"

#include "test.h"

static char m_Output[4096];

// Sends the standard error stream to a file, so that a test can read back
// what the log wrote.
static void M_Capture(void)
{
    if (freopen("test_log.txt", "w+", stderr) == nullptr) {
        abort();
    }
}

static const char *M_Captured(void)
{
    fflush(stderr);
    rewind(stderr);
    const size_t size = fread(m_Output, 1, sizeof(m_Output) - 1, stderr);
    m_Output[size] = '\0';
    return m_Output;
}

TEST(message_has_level_file_line_and_text)
{
    M_Capture();
    Log_Message(LOG_LEVEL_WARNING, "src/core/foo.c", 12, "x=%d", 5);
    CHECK_EQ_STR(M_Captured(), "WARNING foo.c:12: x=5\n");
}

TEST(windows_paths_are_shortened_too)
{
    M_Capture();
    Log_Message(LOG_LEVEL_ERROR, "src\\core\\bar.c", 3, "failed");
    CHECK_EQ_STR(M_Captured(), "ERROR bar.c:3: failed\n");
}

TEST(messages_below_the_minimum_level_are_dropped)
{
    M_Capture();
    Log_SetMinLevel(LOG_LEVEL_WARNING);
    LOG_INFO("dropped");
    LOG_ERROR("kept");
    Log_SetMinLevel(LOG_LEVEL_INFO);
    CHECK(strstr(M_Captured(), "dropped") == nullptr);
    CHECK(strstr(m_Output, "ERROR test_log.c:") == m_Output);
    CHECK(strstr(m_Output, "kept\n") != nullptr);
}

TEST(long_messages_are_not_truncated)
{
    char text[2001];
    memset(text, 'a', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';

    M_Capture();
    LOG_INFO("%s|", text);
    const char *const output = M_Captured();
    CHECK(strstr(output, text) != nullptr);
    CHECK(strstr(output, "a|\n") != nullptr);
}

TEST(lines_go_to_the_file_and_the_standard_error_stream)
{
    FILE *const file = fopen("test_log_file.txt", "w+");
    CHECK(file != nullptr);
    if (file == nullptr) {
        return;
    }
    M_Capture();
    Log_SetFile(file);
    Log_Message(LOG_LEVEL_INFO, "src/core/foo.c", 4, "into %s", "both");
    Log_SetFile(nullptr);
    Log_Message(LOG_LEVEL_INFO, "src/core/foo.c", 5, "stderr only");

    CHECK_EQ_STR(
        M_Captured(),
        "INFO foo.c:4: into both\n"
        "INFO foo.c:5: stderr only\n");
    char text[256];
    rewind(file);
    const size_t size = fread(text, 1, sizeof(text) - 1, file);
    text[size] = '\0';
    fclose(file);
    CHECK_EQ_STR(text, "INFO foo.c:4: into both\n");
}
