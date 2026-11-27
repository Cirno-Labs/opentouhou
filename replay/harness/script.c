#include "replay/harness/script.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M_MAX_FIELDS 4
#define M_MAX_KEYS 32

// Splits text in place at runs of the characters in seps into up to
// capacity fields. Returns their number, or capacity + 1 when there are
// more.
static size_t M_Split(
    char *const text, const char *const seps, char **const fields,
    const size_t capacity)
{
    size_t count = 0;
    for (char *c = text + strspn(text, seps); *c != '\0';
         c += strspn(c, seps)) {
        if (count == capacity) {
            return capacity + 1;
        }
        fields[count++] = c;
        c += strcspn(c, seps);
        if (*c != '\0') {
            *c++ = '\0';
        }
    }
    return count;
}

static RESULT M_ReadKeys(
    const HARNESS_GAME *const game, char *const text, uint32_t *const out)
{
    *out = 0;
    if (strcmp(text, "none") == 0) {
        return OK;
    }
    char *names[M_MAX_KEYS];
    const size_t count = M_Split(text, "+", names, M_MAX_KEYS);
    FAIL_IF(count > M_MAX_KEYS, "too many keys");
    for (size_t i = 0; i < count; i++) {
        const BINDING *const binding = Game_FindBinding(game, names[i]);
        FAIL_IF(binding == nullptr, "unknown key %s", names[i]);
        *out |= binding->mask;
    }
    return OK;
}

static RESULT M_ReadFps(const char *const text, FPS *const out)
{
    if (strcmp(text, "none") == 0) {
        *out = (FPS) {};
        return OK;
    }
    char *end;
    const float rate = strtof(text, &end);
    FAIL_IF(*end != '\0' || !isfinite(rate) || rate < 0, "bad rate %s", text);
    *out = (FPS) { .available = true, .rate = rate };
    return OK;
}

static RESULT M_ReadDate(const char *const text, uint32_t *const out)
{
    if (strcmp(text, "none") == 0) {
        *out = 0;
        return OK;
    }
    FAIL_IF(
        strlen(text) != 10 || text[4] != '-' || text[7] != '-', "bad date %s",
        text);
    uint32_t date = 0;
    for (size_t i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue;
        }
        FAIL_IF(text[i] < '0' || text[i] > '9', "bad date %s", text);
        date = date * 10 + (uint32_t)(text[i] - '0');
    }
    const uint32_t year = date / 10000;
    const uint32_t month = date / 100 % 100;
    const uint32_t day = date % 100;
    FAIL_IF(year == 0 || month == 0 || month > 12, "bad date %s", text);
    static const uint8_t days[] = { 31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31 };
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const uint32_t last = days[month - 1] + (month == 2 && leap ? 1u : 0u);
    FAIL_IF(day == 0 || day > last, "bad date %s", text);
    *out = date;
    return OK;
}

static bool M_ReadClockValue(const char *const text, uint64_t *const out)
{
    uint64_t value = 0;
    if (*text == '\0') {
        return false;
    }
    for (const char *c = text; *c != '\0'; c++) {
        const uint64_t digit = (uint64_t)(*c - '0');
        if (*c < '0' || *c > '9' || value > (UINT64_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *out = value;
    return true;
}

static int32_t M_ClockIndex(
    const SCRIPT *const script, const uint32_t frame, const uint32_t key)
{
    int32_t low = 0;
    int32_t high = script->clocks.count;
    while (low < high) {
        const int32_t middle = low + (high - low) / 2;
        const SCRIPT_CLOCK *const at = &script->clocks.items[middle];
        if (at->frame < frame || (at->frame == frame && at->key < key)) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

static RESULT M_ReadLine(
    const HARNESS_GAME *const game, char *const line, SCRIPT *const script)
{
    char *fields[M_MAX_FIELDS];
    const size_t count = M_Split(line, " \t\r", fields, M_MAX_FIELDS);
    if (count == 0) {
        return OK;
    }
    uint32_t frame;
    if (strcmp(fields[0], "frames") == 0) {
        FAIL_IF(count != 2 || !Str_ParseU32(fields[1], &frame), "bad frames");
        script->frames = frame;
        return OK;
    }
    if (strcmp(fields[0], "start") == 0) {
        FAIL_IF(count != 3, "expected start NAME VALUE");
        const int32_t field = Game_FindStart(game, fields[1]);
        FAIL_IF(field < 0, "unknown start field %s", fields[1]);
        for (int32_t i = 0; i < script->starts.count; i++) {
            FAIL_IF(
                script->starts.items[i].field == (uint32_t)field,
                "start %s is set twice", fields[1]);
        }
        START_VALUE start = { .field = (uint32_t)field };
        FAIL_IF(
            !Game_ParseValue(game->starts[field].kind, fields[2], &start.value),
            "bad start value %s", fields[2]);
        Array_Append(&script->starts, start);
        return OK;
    }
    if (strcmp(fields[0], "clock") == 0) {
        SCRIPT_CLOCK clock = {};
        FAIL_IF(
            count != 4 || !Str_ParseU32(fields[1], &clock.frame)
                || !Str_ParseU32(fields[2], &clock.key)
                || !M_ReadClockValue(fields[3], &clock.microseconds),
            "expected clock FRAME KEY MICROSECONDS");
        if (script->clocks.count > 0) {
            const SCRIPT_CLOCK *const last =
                &script->clocks.items[script->clocks.count - 1];
            FAIL_IF(
                last->frame > clock.frame
                    || (last->frame == clock.frame && last->key >= clock.key),
                "clock frame/key pairs must increase");
        }
        Array_Append(&script->clocks, clock);
        return OK;
    }
    FAIL_IF(count != 3, "expected KIND FRAME VALUE");
    FAIL_IF(!Str_ParseU32(fields[1], &frame), "bad frame %s", fields[1]);
    if (strcmp(fields[0], "input") == 0) {
        FAIL_IF(
            script->inputs.count > 0
                && script->inputs.items[script->inputs.count - 1].frame
                    >= frame,
            "input frames must increase");
        SCRIPT_INPUT input = { .frame = frame };
        MUST(M_ReadKeys(game, fields[2], &input.buttons));
        Array_Append(&script->inputs, input);
        return OK;
    }
    if (strcmp(fields[0], "fps") == 0) {
        FAIL_IF(
            script->fps.count > 0
                && script->fps.items[script->fps.count - 1].frame >= frame,
            "fps frames must increase");
        SCRIPT_FPS fps = { .frame = frame };
        MUST(M_ReadFps(fields[2], &fps.fps));
        Array_Append(&script->fps, fps);
        return OK;
    }
    if (strcmp(fields[0], "date") == 0) {
        FAIL_IF(
            script->dates.count > 0
                && script->dates.items[script->dates.count - 1].frame >= frame,
            "date frames must increase");
        SCRIPT_DATE date = { .frame = frame };
        MUST(M_ReadDate(fields[2], &date.date));
        Array_Append(&script->dates, date);
        return OK;
    }
    return FAIL("unknown line kind %s", fields[0]);
}

// Returns the script's leading description block (the '#' lines, and any
// blank lines between them) duped from text, or nullptr when it has none.
static char *M_ReadDescription(const char *const text)
{
    size_t pos = 0;
    size_t block_end = 0;
    while (text[pos] != '\0') {
        const size_t line_start = pos;
        const char *const newline = strchr(text + pos, '\n');
        const size_t line_end =
            newline != nullptr ? (size_t)(newline - text) : strlen(text);
        const size_t next_pos = newline != nullptr ? line_end + 1 : line_end;
        const bool blank =
            strspn(text + line_start, " \t\r") >= line_end - line_start;
        if (text[line_start] == '#') {
            block_end = next_pos;
        } else if (!blank) {
            break;
        }
        pos = next_pos;
    }
    if (block_end == 0) {
        return nullptr;
    }
    char *const block = Mem_Alloc(block_end + 1);
    memcpy(block, text, block_end);
    block[block_end] = '\0';
    return block;
}

char *Script_NextLine(char **const text)
{
    char *const line = *text;
    if (line == nullptr) {
        return nullptr;
    }
    char *const end = strchr(line, '\n');
    if (end != nullptr) {
        *end = '\0';
    }
    *text = end != nullptr ? end + 1 : nullptr;
    line[strcspn(line, "#")] = '\0';
    return line;
}

RESULT Script_Read(
    READER *const in, const HARNESS_GAME *const game, SCRIPT *const out)
{
    size_t size;
    char *const text = Reader_ReadCString(in, &g_HeapAllocator, &size);
    if (text == nullptr) {
        return Reader_Result(in);
    }
    SCRIPT script = { .description = M_ReadDescription(text) };
    Array_Init(&script.inputs, &g_HeapAllocator);
    Array_Init(&script.fps, &g_HeapAllocator);
    Array_Init(&script.dates, &g_HeapAllocator);
    Array_Init(&script.clocks, &g_HeapAllocator);
    Array_Init(&script.starts, &g_HeapAllocator);
    RESULT result = OK;
    char *rest = text;
    char *line;
    for (int32_t number = 1;
         IS_OK(result) && (line = Script_NextLine(&rest)) != nullptr;
         number++) {
        result = Result_Prefix(
            M_ReadLine(game, line, &script), "%s:%d", Reader_GetName(in),
            number);
    }
    Allocator_Free(&g_HeapAllocator, text, size + 1);
    if (!IS_OK(result)) {
        Script_Free(&script);
        return result;
    }
    *out = script;
    return OK;
}

static void M_Write(WRITER *const out, const char *const text)
{
    Writer_WriteBytes(out, text, strlen(text));
}

static void M_WriteKeys(
    WRITER *const out, const HARNESS_GAME *const game, const uint32_t buttons)
{
    if (buttons == 0) {
        M_Write(out, "none");
        return;
    }
    const char *sep = "";
    for (size_t i = 0; i < game->binding_count; i++) {
        if ((buttons & game->bindings[i].mask) != 0) {
            M_Write(out, sep);
            M_Write(out, game->bindings[i].name);
            sep = "+";
        }
    }
}

void Script_Write(
    WRITER *const out, const HARNESS_GAME *const game,
    const SCRIPT *const script)
{
    char line[64];
    if (script->description != nullptr) {
        M_Write(out, script->description);
    }
    if (script->frames != 0) {
        snprintf(line, sizeof(line), "frames %" PRIu32 "\n", script->frames);
        M_Write(out, line);
    }
    for (int32_t i = 0; i < script->inputs.count; i++) {
        const SCRIPT_INPUT *const input = &script->inputs.items[i];
        snprintf(line, sizeof(line), "input %" PRIu32 " ", input->frame);
        M_Write(out, line);
        M_WriteKeys(out, game, input->buttons);
        M_Write(out, "\n");
    }
    for (int32_t i = 0; i < script->fps.count; i++) {
        const SCRIPT_FPS *const fps = &script->fps.items[i];
        if (fps->fps.available) {
            snprintf(
                line, sizeof(line), "fps %" PRIu32 " %.9g\n", fps->frame,
                (double)fps->fps.rate);
        } else {
            snprintf(line, sizeof(line), "fps %" PRIu32 " none\n", fps->frame);
        }
        M_Write(out, line);
    }
    for (int32_t i = 0; i < script->dates.count; i++) {
        const SCRIPT_DATE *const date = &script->dates.items[i];
        if (date->date == 0) {
            snprintf(
                line, sizeof(line), "date %" PRIu32 " none\n", date->frame);
        } else {
            snprintf(
                line, sizeof(line),
                "date %" PRIu32 " %04" PRIu32 "-%02" PRIu32 "-%02" PRIu32 "\n",
                date->frame, date->date / 10000, date->date / 100 % 100,
                date->date % 100);
        }
        M_Write(out, line);
    }
    for (int32_t i = 0; i < script->clocks.count; i++) {
        const SCRIPT_CLOCK *const clock = &script->clocks.items[i];
        snprintf(
            line, sizeof(line), "clock %" PRIu32 " %" PRIu32 " %" PRIu64 "\n",
            clock->frame, clock->key, clock->microseconds);
        M_Write(out, line);
    }
}

void Script_Free(SCRIPT *const script)
{
    Mem_Free(script->description);
    Array_Free(&script->inputs);
    Array_Free(&script->fps);
    Array_Free(&script->dates);
    Array_Free(&script->clocks);
    Array_Free(&script->starts);
    *script = (SCRIPT) {};
}

// Returns the index of the last of count entries of size bytes at items
// whose leading u32 frame is at most frame, or -1.
static int32_t M_Find(
    const void *const items, const int32_t count, const size_t size,
    const uint32_t frame)
{
    int32_t low = 0;
    int32_t high = count;
    while (low < high) {
        const int32_t middle = low + (high - low) / 2;
        uint32_t at;
        memcpy(&at, (const char *)items + (size_t)middle * size, sizeof(at));
        if (at <= frame) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low - 1;
}

uint32_t Script_Buttons(const SCRIPT *const script, const uint32_t frame)
{
    const int32_t i = M_Find(
        script->inputs.items, script->inputs.count, sizeof(SCRIPT_INPUT),
        frame);
    return i < 0 ? 0 : script->inputs.items[i].buttons;
}

FPS Script_Fps(const SCRIPT *const script, const uint32_t frame)
{
    const int32_t i =
        M_Find(script->fps.items, script->fps.count, sizeof(SCRIPT_FPS), frame);
    return i < 0 ? (FPS) {} : script->fps.items[i].fps;
}

uint32_t Script_Date(const SCRIPT *const script, const uint32_t frame)
{
    const int32_t i = M_Find(
        script->dates.items, script->dates.count, sizeof(SCRIPT_DATE), frame);
    return i < 0 ? 0 : script->dates.items[i].date;
}

bool Script_Clock(
    const SCRIPT *const script, const uint32_t frame, const uint32_t key,
    uint64_t *const out)
{
    const int32_t i = M_ClockIndex(script, frame, key);
    if (i == script->clocks.count || script->clocks.items[i].frame != frame
        || script->clocks.items[i].key != key) {
        return false;
    }
    *out = script->clocks.items[i].microseconds;
    return true;
}

const SCRIPT_CLOCK *Script_Clocks(
    const SCRIPT *const script, const uint32_t frame, size_t *const count)
{
    const int32_t first = M_ClockIndex(script, frame, 0);
    int32_t end = first;
    while (end < script->clocks.count
           && script->clocks.items[end].frame == frame) {
        end++;
    }
    *count = (size_t)(end - first);
    return end == first ? nullptr : &script->clocks.items[first];
}

void Script_RecordClock(
    SCRIPT *const script, const uint32_t frame, const uint32_t key,
    const uint64_t microseconds)
{
    const int32_t i = M_ClockIndex(script, frame, key);
    if (i < script->clocks.count && script->clocks.items[i].frame == frame
        && script->clocks.items[i].key == key) {
        script->clocks.items[i].microseconds = microseconds;
        return;
    }
    Array_Insert(
        &script->clocks, i,
        ((SCRIPT_CLOCK) {
            .frame = frame, .key = key, .microseconds = microseconds }));
}

void Script_RecordDate(
    SCRIPT *const script, const uint32_t frame, const uint32_t date)
{
    if (script->dates.count > 0
        && script->dates.items[script->dates.count - 1].frame == frame) {
        script->dates.count--;
    }
    if (Script_Date(script, frame) != date) {
        Array_Append(
            &script->dates, ((SCRIPT_DATE) { .frame = frame, .date = date }));
    }
}

void Script_RecordButtons(
    SCRIPT *const script, const uint32_t frame, const uint32_t buttons)
{
    if (script->inputs.count > 0
        && script->inputs.items[script->inputs.count - 1].frame == frame) {
        script->inputs.count--;
    }
    if (Script_Buttons(script, frame) != buttons) {
        Array_Append(
            &script->inputs,
            ((SCRIPT_INPUT) { .frame = frame, .buttons = buttons }));
    }
}

void Script_RecordFps(SCRIPT *const script, const uint32_t frame, const FPS fps)
{
    if (script->fps.count > 0
        && script->fps.items[script->fps.count - 1].frame == frame) {
        script->fps.count--;
    }
    const FPS shown = Script_Fps(script, frame);
    if (shown.available != fps.available
        || (fps.available && shown.rate != fps.rate)) {
        Array_Append(
            &script->fps, ((SCRIPT_FPS) { .frame = frame, .fps = fps }));
    }
}
