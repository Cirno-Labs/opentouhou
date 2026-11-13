// SPDX-License-Identifier: GPL-3.0-only

#include "core/config.h"

#include "core/base/log.h"

#include "test.h"

#include <float.h>
#include <string.h>

typedef struct {
    bool fullscreen;
    int32_t width;
    float volume;
    int32_t filter;
    char name[CONFIG_STRING_SIZE];
    bool untouched;
} SETTINGS;

static const char *const m_Filters[] = { "nearest", "linear", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_BOOL(SETTINGS, "video", "fullscreen", fullscreen, false),
    CONFIG_INT(SETTINGS, "video", "width", width, 640, 320, 7680),
    CONFIG_FLOAT(SETTINGS, "audio", "volume", volume, 1.0f, 0.0f, 1.0f),
    CONFIG_ENUM(SETTINGS, "video", "filter", filter, 0, m_Filters),
    CONFIG_STRING(SETTINGS, "user", "name", name, "nobody"),
};

static const CONFIG_SCHEMA m_Schema = CONFIG_SCHEMA_OF(SETTINGS, m_Options);

static INI *M_Parse(const char *const text)
{
    INI *ini;
    CHECK(Result_Log(
        Ini_Parse(text, strlen(text), &g_HeapAllocator, &ini),
        LOG_LEVEL_ERROR));
    return ini;
}

TEST(set_defaults_writes_every_default)
{
    SETTINGS settings = { .untouched = true };
    memset(&settings, 0xCC, sizeof(settings) - sizeof(settings.untouched));
    settings.untouched = true;
    Config_SetDefaults(&m_Schema, &settings);
    CHECK(!settings.fullscreen);
    CHECK_EQ_INT(settings.width, 640);
    CHECK_EQ_INT((int)(settings.volume * 1000), 1000);
    CHECK_EQ_INT(settings.filter, 0);
    CHECK_EQ_STR(settings.name, "nobody");
    CHECK(settings.untouched);
}

TEST(load_reads_every_type)
{
    INI *const ini = M_Parse(
        "[video]\n"
        "fullscreen = true\n"
        "width = 1920\n"
        "filter = Linear\n"
        "[audio]\n"
        "volume = 0.5\n"
        "[user]\n"
        "name = Reimu Hakurei\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_STR(settings.name, "Reimu Hakurei");
    CHECK(settings.fullscreen);
    CHECK_EQ_INT(settings.width, 1920);
    CHECK_EQ_INT((int)(settings.volume * 1000), 500);
    CHECK_EQ_INT(settings.filter, 1);
    Ini_Free(ini);
}

TEST(load_of_a_missing_key_keeps_the_default)
{
    INI *const ini = M_Parse("");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(settings.width, 640);
    Ini_Free(ini);
}

TEST(bad_bool_keeps_the_default)
{
    INI *const ini = M_Parse("[video]\nfullscreen = sideways\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK(!settings.fullscreen);
    Ini_Free(ini);
}

TEST(bad_int_keeps_the_default)
{
    INI *const ini = M_Parse("[video]\nwidth = many\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(settings.width, 640);
    Ini_Free(ini);
}

TEST(int_trailing_garbage_keeps_the_default)
{
    INI *const ini = M_Parse("[video]\nwidth = 640px\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(settings.width, 640);
    Ini_Free(ini);
}

TEST(int_out_of_range_keeps_the_default_without_clamping)
{
    INI *const ini = M_Parse("[video]\nwidth = 100000\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(settings.width, 640);
    Ini_Free(ini);
}

TEST(bad_float_keeps_the_default)
{
    INI *const ini = M_Parse("[audio]\nvolume = loud\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT((int)(settings.volume * 1000), 1000);
    Ini_Free(ini);
}

TEST(nan_and_inf_keep_the_default)
{
    INI *const ini = M_Parse("[audio]\nvolume = nan\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT((int)(settings.volume * 1000), 1000);
    Ini_Free(ini);
    INI *const ini2 = M_Parse("[audio]\nvolume = inf\n");
    Config_Load(&m_Schema, ini2, &settings);
    CHECK_EQ_INT((int)(settings.volume * 1000), 1000);
    Ini_Free(ini2);
}

TEST(float_out_of_range_keeps_the_default)
{
    INI *const ini = M_Parse("[audio]\nvolume = 2.0\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT((int)(settings.volume * 1000), 1000);
    Ini_Free(ini);
}

TEST(enum_is_case_insensitive)
{
    INI *const ini = M_Parse("[video]\nfilter = LINEAR\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(settings.filter, 1);
    Ini_Free(ini);
}

TEST(bad_enum_keeps_the_default)
{
    INI *const ini = M_Parse("[video]\nfilter = bicubic\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(settings.filter, 0);
    Ini_Free(ini);
}

TEST(save_then_load_round_trips_every_field)
{
    SETTINGS settings = {
        .fullscreen = true,
        .width = 1280,
        .volume = 0.25f,
        .filter = 1,
        .name = "Marisa",
    };
    INI *const ini = M_Parse("");
    Config_Save(&m_Schema, ini, &settings);
    SETTINGS loaded;
    Config_Load(&m_Schema, ini, &loaded);
    CHECK_EQ_INT(loaded.fullscreen, settings.fullscreen);
    CHECK_EQ_INT(loaded.width, settings.width);
    CHECK_EQ_INT((int)(loaded.volume * 1000), (int)(settings.volume * 1000));
    CHECK_EQ_INT(loaded.filter, settings.filter);
    CHECK_EQ_STR(loaded.name, "Marisa");
    Ini_Free(ini);
}

static void M_CheckFloatRoundTrips(const float value)
{
    typedef struct {
        float number;
    } FLOAT_SETTINGS;
    static const CONFIG_OPTION options[] = {
        CONFIG_FLOAT(FLOAT_SETTINGS, "a", "k", number, 0.0f, -FLT_MAX, FLT_MAX),
    };
    static const CONFIG_SCHEMA schema =
        CONFIG_SCHEMA_OF(FLOAT_SETTINGS, options);
    FLOAT_SETTINGS settings = { .number = value };
    INI *const ini = M_Parse("");
    Config_Save(&schema, ini, &settings);
    FLOAT_SETTINGS loaded;
    Config_Load(&schema, ini, &loaded);
    uint32_t a, b;
    memcpy(&a, &settings.number, sizeof(a));
    memcpy(&b, &loaded.number, sizeof(b));
    if (a != b) {
        TEST_FAIL(
            "%.9g: round trip through \"%s\" gave %.9g", (double)value,
            Ini_Get(ini, "a", "k"), (double)loaded.number);
    }
    Ini_Free(ini);
}

TEST(float_round_trip_on_tricky_values)
{
    M_CheckFloatRoundTrips(0.1f);
    M_CheckFloatRoundTrips(1e-7f);
    M_CheckFloatRoundTrips(-1e-7f);
    M_CheckFloatRoundTrips(FLT_MAX);
    M_CheckFloatRoundTrips(-FLT_MAX);
    M_CheckFloatRoundTrips(FLT_MIN);
    M_CheckFloatRoundTrips(FLT_TRUE_MIN);
    M_CheckFloatRoundTrips(0.0f);
    M_CheckFloatRoundTrips(-0.0f);
}

TEST(save_keeps_keys_the_schema_does_not_know)
{
    INI *const ini = M_Parse("[video]\nunknown = kept\nwidth = 640\n");
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    Config_Save(&m_Schema, ini, &settings);
    CHECK_EQ_STR(Ini_Get(ini, "video", "unknown"), "kept");
    Ini_Free(ini);
}

TEST(too_long_string_keeps_the_default)
{
    char text[CONFIG_STRING_SIZE + 32];
    const int prefix = snprintf(text, sizeof(text), "[user]\nname = ");
    memset(text + prefix, 'a', CONFIG_STRING_SIZE);
    snprintf(text + prefix + CONFIG_STRING_SIZE, 2, "\n");
    INI *const ini = M_Parse(text);
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_STR(settings.name, "nobody");
    Ini_Free(ini);
}

TEST(longest_string_that_fits_loads)
{
    char text[CONFIG_STRING_SIZE + 32];
    const int prefix = snprintf(text, sizeof(text), "[user]\nname = ");
    memset(text + prefix, 'a', CONFIG_STRING_SIZE - 1);
    snprintf(text + prefix + CONFIG_STRING_SIZE - 1, 2, "\n");
    INI *const ini = M_Parse(text);
    SETTINGS settings;
    Config_Load(&m_Schema, ini, &settings);
    CHECK_EQ_INT(strlen(settings.name), CONFIG_STRING_SIZE - 1);
    Ini_Free(ini);
}
