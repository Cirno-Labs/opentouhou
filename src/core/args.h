#pragma once

#include "core/base/result.h"
#include "core/base/utils.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Parses a command line against a schema: options that fill the fields of
// the caller's struct, and rules between options that the parse checks.
// Every usage error reads "<program>: <message> (see --help)".
//
// Example:
//
// typedef struct {
//     const char *game;
//     bool verbose;
//     int32_t count;
//     ARG_ITEMS seeds;
// } M_ARGS;
//
// static const ARG_OPTION m_Options[] = {
//     ARG_STRING_OF(M_ARGS, "--game", game, "<dir>", "the game",
//                   ARG_REQUIRED),
//     ARG_FLAG_OF(M_ARGS, "--verbose", verbose, "print more",
//                 ARG_SHORT("-v")),
//     ARG_INT_OF(M_ARGS, "--count", count, "<n>", "how many",
//                ARG_FALLBACK_INT(1)),
//     ARG_LIST_OF(M_ARGS, "--seed", seeds, "<file>", "copy a file in"),
// };
//
// static const ARG_RULE m_Rules[] = {
//     { ARG_REQUIRES, { "--seed", "--count" } },
// };
//
// static const ARG_SCHEMA m_Schema =
//     ARG_SCHEMA_OF("tool", "Runs the tool.", m_Options, m_Rules);
//
// M_ARGS args;
// bool help;
// MUST(Args_Parse(&m_Schema, argc, argv, &args, &help));

#define ARG_LIST_MAX 16

// Holds the values of an ARG_LIST option, borrowed from argv.
typedef struct {
    const char *items[ARG_LIST_MAX];
    size_t count;
} ARG_ITEMS;

typedef enum {
    ARG_FLAG,
    ARG_STRING,
    ARG_INT,
    ARG_LIST,
} ARG_TYPE;

typedef struct {
    // "--game", or "cases" for a positional argument.
    const char *name;
    // "-h", or nullptr.
    const char *short_name;
    ARG_TYPE type;
    // The field's position in the args struct, from offsetof.
    size_t offset;
    // What the field holds when the option is not given.
    union {
        const char *string;
        int32_t integer;
    } fallback;
    const char *metavar;
    const char *help;
    bool required;
} ARG_OPTION;

typedef enum {
    // Exactly one of the options.
    ARG_ONE_OF,
    // At most one of the options.
    ARG_EXCLUSIVE,
    // The first option only with each of the others.
    ARG_REQUIRES,
} ARG_RULE_TYPE;

typedef struct {
    ARG_RULE_TYPE type;
    // Option names, ended by nullptr or the array's end.
    const char *options[4];
} ARG_RULE;

typedef struct {
    const char *program;
    const char *summary;
    const ARG_OPTION *options;
    size_t option_count;
    const ARG_RULE *rules;
    size_t rule_count;
} ARG_SCHEMA;

// Builds the ARG_SCHEMA of a program from its ARG_OPTION table and,
// optionally, its ARG_RULE table.
#define ARG_SCHEMA_OF(program_, summary_, options_, ...)                       \
    { .program = (program_),                                                   \
      .summary = (summary_),                                                   \
      .options = (options_),                                                   \
      .option_count = ARRAY_SIZE(options_),                                    \
      __VA_OPT__(                                                              \
              .rules = (__VA_ARGS__),                                          \
              .rule_count = ARRAY_SIZE(__VA_ARGS__), ) }

// Fills *out from argv[1] on (fallbacks first) and checks the rules. Sets
// *help when --help or -h is given: every argument is still read, so the
// options given with it take effect, but the required options and the rules
// are not checked. Prints nothing: the RESULT carries the usage error.
RESULT Args_Parse(
    const ARG_SCHEMA *schema, int argc, char *const argv[], void *out,
    bool *help);

// Fails with the usage error format, for checks the rules cannot say.
PRINTF_FORMAT(2, 3)
RESULT Args_Fail(const ARG_SCHEMA *schema, const char *format, ...);

// Prints the usage line, the summary and one line per option to out.
void Args_PrintHelp(const ARG_SCHEMA *schema, FILE *out);

// The rest of this header is the implementation behind the ARG_ macros. Each
// macro takes extra designators last, such as ARG_REQUIRED, ARG_SHORT and
// ARG_FALLBACK.
// ARG_TYPE_CHECK fails to compile when the field's type is not c_type_.

#define ARG_REQUIRED .required = true
#define ARG_SHORT(short_name_) .short_name = (short_name_)
// Gives an ARG_STRING_OF option the value it holds when not given, which is
// nullptr otherwise.
#define ARG_FALLBACK(string_) .fallback = { .string = (string_) }
// Gives an ARG_INT_OF option the value it holds when not given, which is 0
// otherwise.
#define ARG_FALLBACK_INT(integer_) .fallback = { .integer = (integer_) }

#define ARG_TYPE_CHECK(type_, field_, c_type_)                                 \
    _Generic(&((type_ *)nullptr)->field_, c_type_ *: (size_t)0)

// Builds an ARG_OPTION for a bool field, which is true when it is given.
#define ARG_FLAG_OF(type_, name_, field_, help_, ...)                          \
    { .name = (name_),                                                         \
      .type = ARG_FLAG,                                                        \
      .offset = offsetof(type_, field_) + ARG_TYPE_CHECK(type_, field_, bool), \
      .help = (help_),                                                         \
      __VA_ARGS__ }

// Builds an ARG_OPTION for a const char * field.
#define ARG_STRING_OF(type_, name_, field_, metavar_, help_, ...)              \
    { .name = (name_),                                                         \
      .type = ARG_STRING,                                                      \
      .offset = offsetof(type_, field_)                                        \
          + ARG_TYPE_CHECK(type_, field_, const char *),                       \
      .metavar = (metavar_),                                                   \
      .help = (help_),                                                         \
      __VA_ARGS__ }

// Builds an ARG_OPTION for an int32_t field.
#define ARG_INT_OF(type_, name_, field_, metavar_, help_, ...)                 \
    { .name = (name_),                                                         \
      .type = ARG_INT,                                                         \
      .offset =                                                                \
          offsetof(type_, field_) + ARG_TYPE_CHECK(type_, field_, int32_t),    \
      .metavar = (metavar_),                                                   \
      .help = (help_),                                                         \
      __VA_ARGS__ }

// Builds an ARG_OPTION for an ARG_ITEMS field, which is empty when the
// option is not given.
#define ARG_LIST_OF(type_, name_, field_, metavar_, help_, ...)                \
    { .name = (name_),                                                         \
      .type = ARG_LIST,                                                        \
      .offset =                                                                \
          offsetof(type_, field_) + ARG_TYPE_CHECK(type_, field_, ARG_ITEMS),  \
      .metavar = (metavar_),                                                   \
      .help = (help_),                                                         \
      __VA_ARGS__ }
