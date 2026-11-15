#include "core/args.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define M_WIDTH 79
#define M_HELP_COLUMN_MAX 28
#define M_OPTION_MAX 32

typedef struct {
    FILE *out;
    size_t column;
    size_t indent;
    // Tells that the next word starts the text, with no space before it.
    bool fresh;
} M_LINE;

typedef struct {
    const ARG_SCHEMA *schema;
    char *out;
    bool given[M_OPTION_MAX];
} M_PARSE;

static bool M_IsPositional(const ARG_OPTION *const option)
{
    return option->name[0] != '-';
}

static bool M_TakesValue(const ARG_OPTION *const option)
{
    return option->type != ARG_FLAG;
}

static const char *M_Metavar(const ARG_OPTION *const option)
{
    return option->metavar != nullptr ? option->metavar : "<value>";
}

// Returns the name that messages use: the option's name, or the metavar of
// a positional argument.
static const char *M_Label(const ARG_OPTION *const option)
{
    if (M_IsPositional(option) && option->metavar != nullptr) {
        return option->metavar;
    }
    return option->name;
}

static bool M_ParseInt(const char *const text, int32_t *const out)
{
    if (text[0] == '\0') {
        return false;
    }
    char *end;
    errno = 0;
    const long long value = strtoll(text, &end, 10);
    if (*end != '\0' || errno != 0 || value < INT32_MIN || value > INT32_MAX) {
        return false;
    }
    *out = (int32_t)value;
    return true;
}

static void M_SetFallbacks(const ARG_SCHEMA *const schema, char *const out)
{
    for (size_t i = 0; i < schema->option_count; i++) {
        const ARG_OPTION *const option = &schema->options[i];
        void *const field = out + option->offset;
        switch (option->type) {
        case ARG_FLAG:
            *(bool *)field = false;
            break;
        case ARG_STRING:
            *(const char **)field = option->fallback.string;
            break;
        case ARG_INT:
            *(int32_t *)field = option->fallback.integer;
            break;
        case ARG_LIST:
            *(ARG_ITEMS *)field = (ARG_ITEMS) {};
            break;
        }
    }
}

static RESULT M_SetValue(
    M_PARSE *const parse, const size_t index, const char *const text)
{
    const ARG_OPTION *const option = &parse->schema->options[index];
    void *const field = parse->out + option->offset;
    if (parse->given[index]
        && (option->type == ARG_STRING || option->type == ARG_INT)) {
        return Args_Fail(parse->schema, "%s given twice", M_Label(option));
    }
    switch (option->type) {
    case ARG_FLAG:
        *(bool *)field = true;
        break;
    case ARG_STRING:
        *(const char **)field = text;
        break;
    case ARG_INT:
        if (!M_ParseInt(text, field)) {
            return Args_Fail(
                parse->schema, "%s needs an integer, not %s", M_Label(option),
                text);
        }
        break;
    case ARG_LIST: {
        ARG_ITEMS *const items = field;
        if (items->count == ARG_LIST_MAX) {
            return Args_Fail(
                parse->schema, "%s given more than %d times", M_Label(option),
                ARG_LIST_MAX);
        }
        items->items[items->count++] = text;
        break;
    }
    }
    parse->given[index] = true;
    return OK;
}

static int M_FindOption(const ARG_SCHEMA *const schema, const char *const arg)
{
    for (size_t i = 0; i < schema->option_count; i++) {
        const ARG_OPTION *const option = &schema->options[i];
        if (M_IsPositional(option)) {
            continue;
        }
        if (strcmp(arg, option->name) == 0
            || (option->short_name != nullptr
                && strcmp(arg, option->short_name) == 0)) {
            return (int)i;
        }
    }
    return -1;
}

// Returns the first positional argument that takes another value: one not
// given yet, or a list.
static int M_FindPositional(const M_PARSE *const parse)
{
    for (size_t i = 0; i < parse->schema->option_count; i++) {
        const ARG_OPTION *const option = &parse->schema->options[i];
        if (M_IsPositional(option)
            && (!parse->given[i] || option->type == ARG_LIST)) {
            return (int)i;
        }
    }
    return -1;
}

static size_t M_FindNamed(
    const ARG_SCHEMA *const schema, const char *const name)
{
    for (size_t i = 0; i < schema->option_count; i++) {
        if (strcmp(schema->options[i].name, name) == 0) {
            return i;
        }
    }
    LOG_FATAL("args: rule names %s, which %s lacks", name, schema->program);
}

static size_t M_RuleSize(const ARG_RULE *const rule)
{
    size_t count = 0;
    while (count < ARRAY_SIZE(rule->options)
           && rule->options[count] != nullptr) {
        count++;
    }
    return count;
}

static bool M_Given(const M_PARSE *const parse, const char *const name)
{
    return parse->given[M_FindNamed(parse->schema, name)];
}

static const char *M_RuleLabel(
    const ARG_SCHEMA *const schema, const char *const name)
{
    return M_Label(&schema->options[M_FindNamed(schema, name)]);
}

// Fails when more than one option of rule was given.
static RESULT M_CheckExclusive(
    const M_PARSE *const parse, const ARG_RULE *const rule, const size_t count)
{
    const char *first = nullptr;
    for (size_t i = 0; i < count; i++) {
        if (!M_Given(parse, rule->options[i])) {
            continue;
        }
        if (first != nullptr) {
            return Args_Fail(
                parse->schema, "%s cannot go with %s",
                M_RuleLabel(parse->schema, first),
                M_RuleLabel(parse->schema, rule->options[i]));
        }
        first = rule->options[i];
    }
    return OK;
}

static RESULT M_CheckOneOf(
    const M_PARSE *const parse, const ARG_RULE *const rule, const size_t count)
{
    MUST(M_CheckExclusive(parse, rule, count));
    for (size_t i = 0; i < count; i++) {
        if (M_Given(parse, rule->options[i])) {
            return OK;
        }
    }
    char names[256] = "";
    for (size_t i = 0; i < count; i++) {
        const size_t length = strlen(names);
        snprintf(
            names + length, sizeof(names) - length, "%s%s",
            i == 0               ? ""
                : i + 1 == count ? " or "
                                 : ", ",
            M_RuleLabel(parse->schema, rule->options[i]));
    }
    return Args_Fail(parse->schema, "give %s", names);
}

static RESULT M_CheckRequires(
    const M_PARSE *const parse, const ARG_RULE *const rule, const size_t count)
{
    for (size_t i = 1; i < count; i++) {
        if (M_Given(parse, rule->options[0])
            && !M_Given(parse, rule->options[i])) {
            return Args_Fail(
                parse->schema, "%s needs %s",
                M_RuleLabel(parse->schema, rule->options[0]),
                M_RuleLabel(parse->schema, rule->options[i]));
        }
    }
    return OK;
}

// Checks the required options and the rules. Every name of every rule is
// looked up, also when the check passes, so that a wrong name is always
// fatal.
static RESULT M_Check(const M_PARSE *const parse)
{
    const ARG_SCHEMA *const schema = parse->schema;
    for (size_t i = 0; i < schema->rule_count; i++) {
        const ARG_RULE *const rule = &schema->rules[i];
        const size_t count = M_RuleSize(rule);
        for (size_t j = 0; j < count; j++) {
            M_FindNamed(schema, rule->options[j]);
        }
    }
    for (size_t i = 0; i < schema->option_count; i++) {
        if (schema->options[i].required && !parse->given[i]) {
            return Args_Fail(
                schema, "%s is required", M_Label(&schema->options[i]));
        }
    }
    for (size_t i = 0; i < schema->rule_count; i++) {
        const ARG_RULE *const rule = &schema->rules[i];
        const size_t count = M_RuleSize(rule);
        switch (rule->type) {
        case ARG_ONE_OF:
            MUST(M_CheckOneOf(parse, rule, count));
            break;
        case ARG_EXCLUSIVE:
            MUST(M_CheckExclusive(parse, rule, count));
            break;
        case ARG_REQUIRES:
            MUST(M_CheckRequires(parse, rule, count));
            break;
        }
    }
    return OK;
}

static void M_Word(M_LINE *const line, const char *const word, size_t length)
{
    if (line->column == 0) {
        fprintf(line->out, "%*s", (int)line->indent, "");
        line->column = line->indent;
    } else if (!line->fresh && line->column + 1 + length > M_WIDTH) {
        fprintf(line->out, "\n%*s", (int)line->indent, "");
        line->column = line->indent;
    } else if (!line->fresh) {
        fputc(' ', line->out);
        line->column++;
    }
    fwrite(word, 1, length, line->out);
    line->column += length;
    line->fresh = false;
}

// Prints text word by word, wrapped to M_WIDTH, and ends the line.
static void M_Text(M_LINE *const line, const char *text)
{
    while (*text != '\0') {
        if (*text == ' ') {
            text++;
            continue;
        }
        if (*text == '\n') {
            fputc('\n', line->out);
            line->column = 0;
            text++;
            continue;
        }
        const size_t length = strcspn(text, " \n");
        M_Word(line, text, length);
        text += length;
    }
    fputc('\n', line->out);
}

static void M_UsageWord(M_LINE *const line, const ARG_OPTION *const option)
{
    char *const body = M_IsPositional(option) ? Str_Dup(M_Metavar(option))
        : M_TakesValue(option)
        ? Str_Format("%s %s", option->name, M_Metavar(option))
        : Str_Dup(option->name);
    char *const word = Str_Format(
        "%s%s%s%s", option->required ? "" : "[", body,
        option->required ? "" : "]", option->type == ARG_LIST ? "..." : "");
    M_Word(line, word, strlen(word));
    Mem_Free(word);
    Mem_Free(body);
}

static void M_PrintUsage(const ARG_SCHEMA *const schema, FILE *const out)
{
    fprintf(out, "Usage: %s", schema->program);
    M_LINE line = {
        .out = out,
        .indent = strlen("Usage: ") + strlen(schema->program) + 1,
    };
    line.column = line.indent - 1;
    for (size_t i = 0; i < schema->option_count; i++) {
        if (!M_IsPositional(&schema->options[i])) {
            M_UsageWord(&line, &schema->options[i]);
        }
    }
    for (size_t i = 0; i < schema->option_count; i++) {
        if (M_IsPositional(&schema->options[i])) {
            M_UsageWord(&line, &schema->options[i]);
        }
    }
    fputc('\n', out);
}

static char *M_HelpLabel(const ARG_OPTION *const option)
{
    if (M_IsPositional(option)) {
        return Str_Dup(M_Metavar(option));
    }
    return Str_Format(
        "%s%s%s%s%s", option->short_name != nullptr ? option->short_name : "",
        option->short_name != nullptr ? ", " : "", option->name,
        M_TakesValue(option) ? " " : "",
        M_TakesValue(option) ? M_Metavar(option) : "");
}

static void M_PrintEntry(
    FILE *const out, const char *const label, const char *const help,
    const size_t column)
{
    fprintf(out, "  %s", label);
    M_LINE line = {
        .out = out,
        .column = 2 + strlen(label),
        .indent = column,
        .fresh = true,
    };
    if (line.column + 2 > column) {
        fprintf(out, "\n%*s", (int)column, "");
    } else {
        fprintf(out, "%*s", (int)(column - line.column), "");
    }
    line.column = column;
    M_Text(&line, help != nullptr ? help : "");
}

RESULT Args_Parse(
    const ARG_SCHEMA *const schema, const int argc, char *const argv[],
    void *const out, bool *const help)
{
    ASSERT(schema->option_count <= M_OPTION_MAX);
    *help = false;
    M_PARSE parse = { .schema = schema, .out = out };
    M_SetFallbacks(schema, parse.out);
    for (int i = 1; i < argc; i++) {
        const char *const arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            *help = true;
            return OK;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            const int index = M_FindOption(schema, arg);
            if (index < 0) {
                return Args_Fail(schema, "unknown option %s", arg);
            }
            const ARG_OPTION *const option = &schema->options[index];
            const char *text = nullptr;
            if (option->type != ARG_FLAG) {
                if (i + 1 >= argc) {
                    return Args_Fail(schema, "%s needs a value", option->name);
                }
                text = argv[++i];
            }
            MUST(M_SetValue(&parse, (size_t)index, text));
            continue;
        }
        const int index = M_FindPositional(&parse);
        if (index < 0) {
            return Args_Fail(schema, "unexpected argument %s", arg);
        }
        MUST(M_SetValue(&parse, (size_t)index, arg));
    }
    return M_Check(&parse);
}

RESULT Args_Fail(const ARG_SCHEMA *const schema, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    char *const message = Str_FormatV(format, args);
    va_end(args);
    const RESULT result = FAIL("%s: %s (see --help)", schema->program, message);
    Mem_Free(message);
    return result;
}

void Args_PrintHelp(const ARG_SCHEMA *const schema, FILE *const out)
{
    M_PrintUsage(schema, out);
    if (schema->summary != nullptr) {
        fputc('\n', out);
        M_LINE line = { .out = out };
        M_Text(&line, schema->summary);
    }
    static const ARG_OPTION help_option = {
        .name = "--help",
        .short_name = "-h",
        .help = "print this help",
    };
    size_t column = strlen("  -h, --help") + 2;
    for (size_t i = 0; i < schema->option_count; i++) {
        char *const label = M_HelpLabel(&schema->options[i]);
        const size_t width = 2 + strlen(label) + 2;
        if (width <= M_HELP_COLUMN_MAX) {
            column = MAX(column, width);
        }
        Mem_Free(label);
    }
    fputs("\nOptions:\n", out);
    for (size_t i = 0; i <= schema->option_count; i++) {
        const ARG_OPTION *const option =
            i < schema->option_count ? &schema->options[i] : &help_option;
        char *const label = M_HelpLabel(option);
        M_PrintEntry(out, label, option->help, column);
        Mem_Free(label);
    }
}
