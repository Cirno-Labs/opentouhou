#pragma once

#include "core/base/types.h"
#include "core/base/utils.h"
#include "core/ini.h"

#include <stddef.h>

// Declares settings as fields of a plain struct and loads and saves them
// from an INI. Each part that owns settings, such as core's window and
// audio options or a game's own options, declares a struct, a table of
// CONFIG_OPTIONs describing its fields, and the schema built from it; the
// app loads the shared INI once and gives every part's schema a turn at it.
//
// A CONFIG_ENUM field is declared as int32_t, holding the index of the
// value's name in the option's names. Every other field has the C type its
// CONFIG_TYPE names: bool, int32_t, float.
//
// Example:
//
// typedef struct {
//     bool fullscreen;
//     int32_t width;
//     float volume;
//     int32_t filter;
// } VIDEO_SETTINGS;
//
// static const char *const m_Filters[] = { "nearest", "linear", nullptr };
//
// static const CONFIG_OPTION m_Options[] = {
//     CONFIG_BOOL(VIDEO_SETTINGS, "video", "fullscreen", fullscreen, false),
//     CONFIG_INT(VIDEO_SETTINGS, "video", "width", width, 640, 320, 7680),
//     CONFIG_FLOAT(VIDEO_SETTINGS, "audio", "volume", volume, 1.0f,
//     0.0f, 1.0f), CONFIG_ENUM(VIDEO_SETTINGS, "video", "filter", filter, 0,
//     m_Filters),
// };
//
// static const CONFIG_SCHEMA m_Schema =
//     CONFIG_SCHEMA_OF(VIDEO_SETTINGS, m_Options);
//
// INI *ini;
// MUST(Ini_Load(path, &g_HeapAllocator, &ini));
// VIDEO_SETTINGS settings;
// Config_Load(&m_Schema, ini, &settings);
// settings.width = 1920;
// Config_Save(&m_Schema, ini, &settings);
// MUST(Ini_Save(ini, path));

typedef enum {
    CONFIG_BOOL,
    CONFIG_INT,
    CONFIG_FLOAT,
    CONFIG_ENUM,
} CONFIG_TYPE;

typedef struct {
    const char *section;
    const char *key;
    CONFIG_TYPE type;
    // The field's position in the settings struct, from offsetof.
    size_t offset;
    union {
        bool boolean;
        // Also the default index into range.names for CONFIG_ENUM.
        int32_t integer;
        float real;
    } fallback;
    union {
        struct {
            int32_t min, max;
        } integer;
        struct {
            float min, max;
        } real;
        // A CONFIG_ENUM's names, in index order, ending with nullptr.
        const char *const *names;
    } range;
} CONFIG_OPTION;

typedef struct {
    const CONFIG_OPTION *options;
    int32_t count;
    // The size of the settings struct the options describe, checked
    // against nothing yet, but kept for a future bounds check.
    size_t size;
} CONFIG_SCHEMA;

// Builds the CONFIG_SCHEMA for a settings struct type and its CONFIG_OPTION
// table.
#define CONFIG_SCHEMA_OF(type_, options_)                                      \
    { (options_), ARRAY_SIZE(options_), sizeof(type_) }

// Writes the default of every option in schema into its field of settings.
// A field the schema does not describe is left untouched.
void Config_SetDefaults(const CONFIG_SCHEMA *schema, void *settings);

// Sets the defaults, then reads every option's key from ini and parses it
// into the matching field. Never fails: a value that fails to parse, or
// that parses outside its range, is a warning naming the section, the key
// and the value, and the field keeps its default.
void Config_Load(const CONFIG_SCHEMA *schema, const INI *ini, void *settings);

// Writes the current value of every option in schema into ini. A key the
// schema does not describe is left as it is.
void Config_Save(const CONFIG_SCHEMA *schema, INI *ini, const void *settings);

// The rest of this header is the implementation behind the CONFIG_ macros.
// CONFIG_TYPE_CHECK fails to compile when field's type in type_ is not a
// pointer-compatible match for c_type_, which is how the macros below catch
// an option declared with the wrong field.

#define CONFIG_TYPE_CHECK(type_, field_, c_type_)                              \
    _Generic(&((type_ *)nullptr)->field_, c_type_ *: (size_t)0)

// Builds a CONFIG_OPTION for a bool field.
#define CONFIG_BOOL(type_, section_, key_, field_, fallback_)                  \
    {                                                                          \
        .section = (section_),                                                 \
        .key = (key_),                                                         \
        .type = CONFIG_BOOL,                                                   \
        .offset =                                                              \
            offsetof(type_, field_) + CONFIG_TYPE_CHECK(type_, field_, bool),  \
        .fallback = { .boolean = (fallback_) },                                \
    }

// Builds a CONFIG_OPTION for an int32_t field, with an inclusive range.
#define CONFIG_INT(type_, section_, key_, field_, fallback_, min_, max_)       \
    {                                                                          \
        .section = (section_),                                                 \
        .key = (key_),                                                         \
        .type = CONFIG_INT,                                                    \
        .offset = offsetof(type_, field_)                                      \
            + CONFIG_TYPE_CHECK(type_, field_, int32_t),                       \
        .fallback = { .integer = (fallback_) },                                \
        .range = { .integer = { (min_), (max_) } },                            \
    }

// Builds a CONFIG_OPTION for a float field, with an inclusive range.
#define CONFIG_FLOAT(type_, section_, key_, field_, fallback_, min_, max_)     \
    {                                                                          \
        .section = (section_),                                                 \
        .key = (key_),                                                         \
        .type = CONFIG_FLOAT,                                                  \
        .offset =                                                              \
            offsetof(type_, field_) + CONFIG_TYPE_CHECK(type_, field_, float), \
        .fallback = { .real = (fallback_) },                                   \
        .range = { .real = { (min_), (max_) } },                               \
    }

// Builds a CONFIG_OPTION for an int32_t field read and written as one of
// names, a nullptr-terminated array of strings. fallback_ is the default
// index into names.
#define CONFIG_ENUM(type_, section_, key_, field_, fallback_, names_)          \
    {                                                                          \
        .section = (section_),                                                 \
        .key = (key_),                                                         \
        .type = CONFIG_ENUM,                                                   \
        .offset = offsetof(type_, field_)                                      \
            + CONFIG_TYPE_CHECK(type_, field_, int32_t),                       \
        .fallback = { .integer = (fallback_) },                                \
        .range = { .names = (names_) },                                        \
    }
