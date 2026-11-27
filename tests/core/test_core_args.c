#include "tests/test.h"

#include "core/args.h"
#include "core/base/memory.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct {
    const char *game;
    bool verbose;
    int32_t times;
    ARG_ITEMS seeds;
    bool fast;
    bool live;
    const char *replay;
    const char *case_path;
} M_ARGS;

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "--game", game, "<dir>", "the game folder", ARG_SHORT("-g"),
        ARG_REQUIRED),
    ARG_FLAG_OF(M_ARGS, "--verbose", verbose, "print more", ARG_SHORT("-v")),
    ARG_INT_OF(
        M_ARGS, "--times", times, "<n>", "how often", ARG_FALLBACK_INT(3)),
    ARG_LIST_OF(M_ARGS, "--seed", seeds, "<file>", "copy a file in"),
    ARG_FLAG_OF(M_ARGS, "--fast", fast, "run fast"),
    ARG_FLAG_OF(M_ARGS, "--live", live, "play live"),
    ARG_STRING_OF(M_ARGS, "--replay", replay, "<file>", "a replay"),
    ARG_STRING_OF(
        M_ARGS, "case", case_path, "<case>", "the case file",
        ARG_FALLBACK("default.othtc")),
};

static const ARG_SCHEMA m_Schema =
    ARG_SCHEMA_OF("tool", "Runs the tool.", m_Options);

static const ARG_RULE m_OneOf[] = {
    { ARG_ONE_OF, { "--replay", "--live" } },
};
static const ARG_SCHEMA m_OneOfSchema =
    ARG_SCHEMA_OF("tool", nullptr, m_Options, m_OneOf);

static const ARG_RULE m_OneOfThree[] = {
    { ARG_ONE_OF, { "--replay", "--live", "--fast" } },
};
static const ARG_SCHEMA m_OneOfThreeSchema =
    ARG_SCHEMA_OF("tool", nullptr, m_Options, m_OneOfThree);

static const ARG_RULE m_Exclusive[] = {
    { ARG_EXCLUSIVE, { "--fast", "--live", "--times" } },
};
static const ARG_SCHEMA m_ExclusiveSchema =
    ARG_SCHEMA_OF("tool", nullptr, m_Options, m_Exclusive);

static const ARG_RULE m_ExclusiveCase[] = {
    { ARG_EXCLUSIVE, { "case", "--live" } },
};
static const ARG_SCHEMA m_ExclusiveCaseSchema =
    ARG_SCHEMA_OF("tool", nullptr, m_Options, m_ExclusiveCase);

static const ARG_RULE m_Requires[] = {
    { ARG_REQUIRES, { "--fast", "--live" } },
};
static const ARG_SCHEMA m_RequiresSchema =
    ARG_SCHEMA_OF("tool", nullptr, m_Options, m_Requires);

// Parses the arguments after the program name, up to nullptr, into *args.
static RESULT M_Parse(
    const ARG_SCHEMA *const schema, M_ARGS *const args, bool *const help, ...)
{
    char *argv[32] = { "tool" };
    int argc = 1;
    va_list list;
    va_start(list, help);
    for (const char *arg; (arg = va_arg(list, const char *)) != nullptr;) {
        argv[argc++] = (char *)arg;
    }
    va_end(list);
    return Args_Parse(schema, argc, argv, args, help);
}

// Checks that result fails with message and releases it.
static void M_CheckFail(const RESULT result, const char *const message)
{
    CHECK(!IS_OK(result));
    if (!IS_OK(result)) {
        CHECK_EQ_STR(result.message, message);
    }
    Result_Ignore(result);
}

static char *M_Help(const ARG_SCHEMA *const schema)
{
    FILE *const out = tmpfile();
    Args_PrintHelp(schema, out);
    const long size = ftell(out);
    rewind(out);
    char *const text = Mem_AllocZ((size_t)size + 1);
    CHECK(fread(text, 1, (size_t)size, out) == (size_t)size);
    fclose(out);
    return text;
}

TEST(options_fill_their_fields)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(M_Parse(
        &m_Schema, &args, &help, "--game", "dir", "--verbose", "--times", "-12",
        "a.othtc", nullptr)));
    CHECK(!help);
    CHECK_EQ_STR(args.game, "dir");
    CHECK(args.verbose);
    CHECK_EQ_INT(args.times, -12);
    CHECK(!args.fast);
    CHECK_EQ_INT(args.seeds.count, 0);
    CHECK_EQ_STR(args.case_path, "a.othtc");
}

TEST(options_not_given_hold_their_fallbacks)
{
    M_ARGS args = { .verbose = true, .times = 99, .seeds.count = 2 };
    bool help;
    CHECK(Result_Ignore(M_Parse(&m_Schema, &args, &help, "-g", "d", nullptr)));
    CHECK(!args.verbose);
    CHECK_EQ_INT(args.times, 3);
    CHECK_EQ_INT(args.seeds.count, 0);
    CHECK(args.replay == nullptr);
    CHECK_EQ_STR(args.case_path, "default.othtc");
}

TEST(short_names_match)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(
        M_Parse(&m_Schema, &args, &help, "-g", "dir", "-v", nullptr)));
    CHECK_EQ_STR(args.game, "dir");
    CHECK(args.verbose);
}

TEST(a_value_may_start_with_a_dash)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(
        M_Parse(&m_Schema, &args, &help, "--game", "-h", nullptr)));
    CHECK(!help);
    CHECK_EQ_STR(args.game, "-h");
}

TEST(bad_int_is_a_usage_error)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "-g", "d", "--times", "3x", nullptr),
        "tool: --times needs an integer, not 3x (see --help)");
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "-g", "d", "--times", "", nullptr),
        "tool: --times needs an integer, not  (see --help)");
    M_CheckFail(
        M_Parse(
            &m_Schema, &args, &help, "-g", "d", "--times", "2147483648",
            nullptr),
        "tool: --times needs an integer, not 2147483648 (see --help)");
}

TEST(lists_repeat_up_to_the_cap)
{
    char *argv[3 + 2 * (ARG_LIST_MAX + 1)] = { "tool", "--game", "d" };
    int argc = 3;
    for (int i = 0; i < ARG_LIST_MAX; i++) {
        argv[argc++] = "--seed";
        argv[argc++] = i == 0 ? "first" : "more";
    }
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(Args_Parse(&m_Schema, argc, argv, &args, &help)));
    CHECK_EQ_INT(args.seeds.count, ARG_LIST_MAX);
    CHECK_EQ_STR(args.seeds.items[0], "first");
    argv[argc++] = "--seed";
    argv[argc++] = "one too many";
    M_CheckFail(
        Args_Parse(&m_Schema, argc, argv, &args, &help),
        "tool: --seed given more than 16 times (see --help)");
}

TEST(a_value_given_twice_fails)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "--game", "a", "--game", "b", nullptr),
        "tool: --game given twice (see --help)");
}

TEST(an_extra_positional_argument_fails)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "-g", "d", "a", "b", nullptr),
        "tool: unexpected argument b (see --help)");
}

TEST(a_required_option_must_be_given)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "--verbose", nullptr),
        "tool: --game is required (see --help)");
}

typedef struct {
    ARG_ITEMS cases;
} M_CASES;

static const ARG_OPTION m_CaseOptions[] = {
    ARG_LIST_OF(M_CASES, "cases", cases, "<case>", "the cases", ARG_REQUIRED),
};

TEST(a_positional_list_takes_every_argument)
{
    const ARG_SCHEMA schema = ARG_SCHEMA_OF("tool", nullptr, m_CaseOptions);
    char *argv[] = { "tool", "a", "b" };
    M_CASES cases;
    bool help;
    M_CheckFail(
        Args_Parse(&schema, 1, argv, &cases, &help),
        "tool: <case> is required (see --help)");
    CHECK(Result_Ignore(Args_Parse(&schema, 3, argv, &cases, &help)));
    CHECK_EQ_INT(cases.cases.count, 2);
    CHECK_EQ_STR(cases.cases.items[1], "b");
}

TEST(an_unknown_option_fails)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "-g", "d", "--bogus", nullptr),
        "tool: unknown option --bogus (see --help)");
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "-x", nullptr),
        "tool: unknown option -x (see --help)");
}

TEST(a_missing_value_fails)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "--game", nullptr),
        "tool: --game needs a value (see --help)");
}

TEST(help_skips_the_required_options_and_the_rules)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(
        M_Parse(&m_OneOfSchema, &args, &help, "--help", nullptr)));
    CHECK(help);
}

TEST(options_after_help_still_take_effect)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(
        M_Parse(&m_Schema, &args, &help, "-h", "--game", "dir", nullptr)));
    CHECK(help);
    CHECK_EQ_STR(args.game, "dir");
    M_CheckFail(
        M_Parse(&m_Schema, &args, &help, "-v", "-h", "--bogus", nullptr),
        "tool: unknown option --bogus (see --help)");
}

TEST(fail_uses_the_usage_error_format)
{
    M_CheckFail(
        Args_Fail(&m_Schema, "bad %s %d", "thing", 3),
        "tool: bad thing 3 (see --help)");
}

TEST(one_of_needs_exactly_one)
{
    M_ARGS args;
    bool help;
    M_CheckFail(
        M_Parse(&m_OneOfSchema, &args, &help, "-g", "d", nullptr),
        "tool: give --replay or --live (see --help)");
    CHECK(Result_Ignore(
        M_Parse(&m_OneOfSchema, &args, &help, "-g", "d", "--live", nullptr)));
    M_CheckFail(
        M_Parse(
            &m_OneOfSchema, &args, &help, "-g", "d", "--live", "--replay", "r",
            nullptr),
        "tool: --replay cannot go with --live (see --help)");
    M_CheckFail(
        M_Parse(&m_OneOfThreeSchema, &args, &help, "-g", "d", nullptr),
        "tool: give --replay, --live or --fast (see --help)");
}

TEST(exclusive_allows_none_or_one)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(
        M_Parse(&m_ExclusiveSchema, &args, &help, "-g", "d", nullptr)));
    CHECK(Result_Ignore(M_Parse(
        &m_ExclusiveSchema, &args, &help, "-g", "d", "--fast", nullptr)));
    M_CheckFail(
        M_Parse(
            &m_ExclusiveSchema, &args, &help, "-g", "d", "--times", "1",
            "--fast", nullptr),
        "tool: --fast cannot go with --times (see --help)");
    M_CheckFail(
        M_Parse(
            &m_ExclusiveCaseSchema, &args, &help, "-g", "d", "a.othtc",
            "--live", nullptr),
        "tool: <case> cannot go with --live (see --help)");
}

TEST(requires_needs_the_other_option)
{
    M_ARGS args;
    bool help;
    CHECK(Result_Ignore(
        M_Parse(&m_RequiresSchema, &args, &help, "-g", "d", nullptr)));
    M_CheckFail(
        M_Parse(&m_RequiresSchema, &args, &help, "-g", "d", "--fast", nullptr),
        "tool: --fast needs --live (see --help)");
    CHECK(Result_Ignore(M_Parse(
        &m_RequiresSchema, &args, &help, "-g", "d", "--fast", "--live",
        nullptr)));
}

TEST(help_lists_the_usage_and_every_option)
{
    char *const text = M_Help(&m_Schema);
    CHECK_EQ_STR(
        text,
        "Usage: tool --game <dir> [--verbose] [--times <n>] [--seed <file>]... "
        "[--fast]\n"
        "            [--live] [--replay <file>] [<case>]\n"
        "\n"
        "Runs the tool.\n"
        "\n"
        "Options:\n"
        "  -g, --game <dir>  the game folder\n"
        "  -v, --verbose     print more\n"
        "  --times <n>       how often\n"
        "  --seed <file>     copy a file in\n"
        "  --fast            run fast\n"
        "  --live            play live\n"
        "  --replay <file>   a replay\n"
        "  <case>            the case file\n"
        "  -h, --help        print this help\n");
    Mem_Free(text);
}

typedef struct {
    bool a;
    const char *b;
} M_LONG;

static const ARG_OPTION m_LongOptions[] = {
    ARG_FLAG_OF(
        M_LONG, "--a", a,
        "one two three four five six seven eight nine ten eleven twelve "
        "thirteen fourteen"),
    ARG_STRING_OF(M_LONG, "--a-very-long-option-name", b, "<value>", "short"),
};

TEST(help_wraps_long_text_under_its_column)
{
    const ARG_SCHEMA schema =
        ARG_SCHEMA_OF("t", "First line\nsecond line", m_LongOptions);
    char *const text = M_Help(&schema);
    CHECK_EQ_STR(
        text,
        "Usage: t [--a] [--a-very-long-option-name <value>]\n"
        "\n"
        "First line\n"
        "second line\n"
        "\n"
        "Options:\n"
        "  --a         one two three four five six seven eight nine ten "
        "eleven twelve\n"
        "              thirteen fourteen\n"
        "  --a-very-long-option-name <value>\n"
        "              short\n"
        "  -h, --help  print this help\n");
    Mem_Free(text);
}
