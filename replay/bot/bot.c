#include "bot.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/ini.h"
#include "core/io/file.h"

#include <ctype.h>
#include <string.h>

static const char *const m_RoleNames[] = {
#define M_ROLE_NAME(name) #name,
    BOT_ROLE_NAMES(M_ROLE_NAME)
#undef M_ROLE_NAME
};

static RESULT M_ParseButtons(
    const INPUT_SCHEMA *const schema, const char *const role, const char *word,
    const size_t length, uint32_t *const out)
{
    *out = 0;
    if (length == 1 && word[0] == '.') {
        return OK;
    }
    const char *const end = word + length;
    while (true) {
        const char *plus = memchr(word, '+', (size_t)(end - word));
        if (plus == nullptr) {
            plus = end;
        }
        const size_t size = (size_t)(plus - word);
        size_t found = schema->count;
        for (size_t i = 0; i < schema->count; i++) {
            const char *const name = schema->actions[i].name;
            if (schema->actions[i].type == INPUT_ACTION_BUTTON
                && strlen(name) == size && memcmp(name, word, size) == 0) {
                found = i;
                break;
            }
        }
        FAIL_IF(
            found == schema->count, "role %s names no button %.*s", role,
            (int)size, word);
        *out |= 1u << found;
        if (plus == end) {
            return OK;
        }
        word = plus + 1;
    }
}

static RESULT M_ParseRole(
    const INPUT_SCHEMA *const schema, const char *const role,
    const char *const value, BOT_SEQUENCE *const out)
{
    memset(out, 0, sizeof(*out));
    const char *cursor = value;
    while (true) {
        while (isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *end = cursor;
        while (*end != '\0' && !isspace((unsigned char)*end)) {
            end++;
        }
        FAIL_IF(
            out->count == BOT_MAX_FRAMES, "role %s has more than %d frames",
            role, BOT_MAX_FRAMES);
        MUST(M_ParseButtons(
            schema, role, cursor, (size_t)(end - cursor),
            &out->frames[out->count]));
        out->count++;
        cursor = end;
    }
    FAIL_IF(out->count == 0, "role %s has no frames", role);
    return OK;
}

static RESULT M_LoadIni(
    BOT *const bot, const INI *const ini, const INPUT_SCHEMA *const schema)
{
    memset(bot, 0, sizeof(*bot));
    bot->schema = schema;
    const char *key;
    for (size_t i = 0; (key = Ini_GetKey(ini, "roles", i)) != nullptr; i++) {
        size_t role = BOT_ROLE_COUNT;
        for (size_t j = 0; j < BOT_ROLE_COUNT; j++) {
            if (Str_EqualIgnoreCase(m_RoleNames[j], key)) {
                role = j;
            }
        }
        FAIL_IF(role == BOT_ROLE_COUNT, "there is no role %s", key);
        MUST(M_ParseRole(
            schema, m_RoleNames[role], Ini_Get(ini, "roles", key),
            &bot->roles[role]));
    }
    return OK;
}

RESULT Bot_LoadText(
    BOT *const bot, const char *const text, const size_t size,
    const INPUT_SCHEMA *const schema)
{
    INI *ini;
    MUST(Ini_Parse(text, size, &g_HeapAllocator, &ini));
    const RESULT result = M_LoadIni(bot, ini, schema);
    Ini_Free(ini);
    return result;
}

RESULT Bot_Load(
    BOT *const bot, const char *const path, const INPUT_SCHEMA *const schema)
{
    FAIL_IF(!File_Exists(path), "%s does not exist", path);
    INI *ini;
    MUST(Ini_Load(path, &g_HeapAllocator, &ini));
    const RESULT result = M_LoadIni(bot, ini, schema);
    Ini_Free(ini);
    MUST(result, "%s", path);
    return OK;
}

static void M_Write(const BOT *const bot, const uint32_t buttons, void *out)
{
    uint8_t *const input = out;
    memset(input, 0, bot->schema->size);
    for (size_t i = 0; i < bot->schema->count; i++) {
        if ((buttons & 1u << i) != 0) {
            *(bool *)(input + bot->schema->actions[i].offset) = true;
        }
    }
}

uint32_t Bot_GetRoles(const BOT *const bot)
{
    uint32_t roles = 0;
    for (int32_t i = 0; i < BOT_ROLE_COUNT; i++) {
        const BOT_SEQUENCE *const sequence = &bot->roles[i];
        for (int32_t j = 0; j < sequence->count; j++) {
            if (sequence->frames[j] != 0) {
                roles |= 1u << i;
            }
        }
    }
    return roles;
}

int32_t Bot_GetLeadFrames(const BOT *const bot, const uint32_t roles)
{
    for (int32_t i = 0; i < BOT_ROLE_COUNT; i++) {
        if ((roles & 1u << i) != 0 && bot->roles[i].count > 1) {
            return bot->roles[i].count;
        }
    }
    return 0;
}

void Bot_WriteInput(
    const BOT *const bot, const uint32_t roles, const int32_t frame,
    void *const out)
{
    uint32_t buttons = 0;
    bool lead = true;
    for (int32_t i = 0; i < BOT_ROLE_COUNT; i++) {
        const BOT_SEQUENCE *const sequence = &bot->roles[i];
        if ((roles & 1u << i) == 0) {
            continue;
        }
        if (sequence->count == 1) {
            buttons |= sequence->frames[0];
        } else if (sequence->count > 1 && lead) {
            if (frame < sequence->count) {
                M_Write(bot, sequence->frames[frame], out);
                return;
            }
            lead = false;
        }
    }
    M_Write(bot, buttons, out);
}
