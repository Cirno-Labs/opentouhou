#include "replay/harness/compare.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/image/decode.h"
#include "core/image/encode.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M_SHOT_SUFFIX ".png"

struct STATE_COMPARE {
    STATE_READER *expected;
    const HARNESS_GAME *game;
    ARENA *arena;
    uint32_t running;
    STATE_DIFF diff;
};

typedef ARRAY(uint32_t) M_FRAMES;

STATE_COMPARE *Compare_OpenState(
    STATE_READER *const expected, const HARNESS_GAME *const game,
    ARENA *const arena)
{
    STATE_COMPARE *const compare = Mem_AllocZ(sizeof(*compare));
    compare->expected = expected;
    compare->game = game;
    compare->arena = arena;
    return compare;
}

// Returns a copy of frame and row from the compare's arena, or nullptr
// without a row.
static const uint32_t *M_Keep(
    STATE_COMPARE *const compare, const uint32_t frame,
    const uint32_t *const row)
{
    if (row == nullptr) {
        return nullptr;
    }
    const size_t count = compare->game->column_count;
    uint32_t *const copy =
        Arena_AllocArr(compare->arena, count + 1, sizeof(uint32_t));
    copy[0] = frame;
    memcpy(copy + 1, row, count * sizeof(uint32_t));
    return copy;
}

// Returns whether the columns of a and b match, the HASH ones only with
// hashes.
static bool M_SameColumns(
    const HARNESS_GAME *const game, const uint32_t *const a,
    const uint32_t *const b, const bool hashes)
{
    for (size_t i = 0; i < game->column_count; i++) {
        if ((hashes || game->columns[i].kind != COLUMN_HASH) && a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void M_Compare(
    STATE_COMPARE *const compare, const STATE_ROW *const expected,
    const uint32_t frame, const uint32_t *const row)
{
    const HARNESS_GAME *const game = compare->game;
    STATE_DIFF *const diff = &compare->diff;
    const uint8_t hash = (uint8_t)(compare->running & 0xf);
    const bool both = expected != nullptr && row != nullptr;
    const bool check = both && expected->check;
    const bool same = both && expected->frame == frame
        && M_SameColumns(game, expected->values, row, false)
        && expected->hash == hash
        && (!check
            || (expected->running == compare->running
                && M_SameColumns(game, expected->values, row, true)));
    if (!same && !diff->differs) {
        diff->differs = true;
        diff->first_row = diff->rows;
        if (expected != nullptr) {
            diff->expected = (STATE_SIDE) {
                .values = M_Keep(compare, expected->frame, expected->values),
                .hash = expected->hash,
            };
        }
        if (row != nullptr) {
            diff->actual = (STATE_SIDE) {
                .values = M_Keep(compare, frame, row),
                .hash = hash,
            };
        }
    }
    if (check && diff->differs && diff->check_expected == nullptr) {
        diff->check_row = diff->rows;
        diff->check_expected =
            M_Keep(compare, expected->frame, expected->values);
        diff->check_actual = M_Keep(compare, frame, row);
    }
    diff->rows++;
}

void Compare_StateRow(
    STATE_COMPARE *const compare, const uint32_t frame,
    const uint32_t *const row)
{
    compare->running =
        State_RunningHash(compare->game, compare->running, frame, row);
    STATE_ROW expected;
    const bool read = State_ReadRow(compare->expected, &expected);
    M_Compare(compare, read ? &expected : nullptr, frame, row);
}

void Compare_CloseState(STATE_COMPARE *const compare, STATE_DIFF *const out)
{
    STATE_ROW expected;
    while (State_ReadRow(compare->expected, &expected)) {
        M_Compare(compare, &expected, 0, nullptr);
    }
    *out = compare->diff;
    Mem_Free(compare);
}

RESULT Compare_Images(
    const IMAGE *const a, const IMAGE *const b, const uint32_t frame,
    IMAGE *const xor_out, IMAGE *const delta_out, SHOT_DIFF *const out)
{
    FAIL_IF(
        a->width != b->width || a->height != b->height,
        "images of %dx%d and %dx%d", a->width, a->height, b->width, b->height);
    *xor_out = Image_Create(a->width, a->height, &g_HeapAllocator);
    *delta_out = Image_Create(a->width, a->height, &g_HeapAllocator);
    SHOT_DIFF diff = { .frame = frame };
    const size_t pixels = (size_t)a->width * (size_t)a->height;
    for (size_t i = 0; i < pixels; i++) {
        bool differs = false;
        for (size_t c = 0; c < 4; c++) {
            const uint8_t pa = a->pixels[i * 4 + c];
            const uint8_t pb = b->pixels[i * 4 + c];
            const uint8_t delta = pa > pb ? pa - pb : pb - pa;
            differs |= delta != 0;
            if (delta > diff.max_delta) {
                diff.max_delta = delta;
            }
            xor_out->pixels[i * 4 + c] = c == 3 ? 255 : pa ^ pb;
            delta_out->pixels[i * 4 + c] =
                c == 3 ? 255 : (uint8_t)(delta > 31 ? 255 : delta * 8);
        }
        diff.different_pixels += differs;
    }
    *out = diff;
    return OK;
}

char *Compare_ShotPath(
    const ALLOCATOR *const allocator, const char *const dir,
    const char *const kind, const uint32_t frame)
{
    char name[32];
    snprintf(name, sizeof(name), "%s/%06" PRIu32 M_SHOT_SUFFIX, kind, frame);
    return Path_Join(allocator, dir, name);
}

static bool M_ParseFrame(const char *const name, uint32_t *const frame)
{
    const size_t length = strlen(name);
    if (length != 6 + strlen(M_SHOT_SUFFIX)
        || !Str_EndsWith(name, M_SHOT_SUFFIX)) {
        return false;
    }
    char digits[7];
    memcpy(digits, name, 6);
    digits[6] = '\0';
    return Str_ParseU32(digits, frame);
}

static void M_AddFrames(const char *const dir, M_FRAMES *const frames)
{
    char *const shots = Path_Join(&g_HeapAllocator, dir, "shots");
    FILE_LIST files;
    if (Result_Ignore(File_ListDir(shots, &g_HeapAllocator, &files))) {
        for (int32_t i = 0; i < files.count; i++) {
            uint32_t frame;
            if (!M_ParseFrame(files.items[i], &frame)) {
                continue;
            }
            bool known = false;
            for (int32_t j = 0; j < frames->count; j++) {
                known |= frames->items[j] == frame;
            }
            if (!known) {
                Array_Append(frames, frame);
            }
        }
        File_FreeList(&files);
    }
    Mem_Free(shots);
}

static int M_CompareFrames(const void *const a, const void *const b)
{
    const uint32_t x = *(const uint32_t *)a;
    const uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static RESULT M_LoadImage(const char *const path, IMAGE *const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_SaveImage(
    const char *const out_dir, const char *const kind, const uint32_t frame,
    const IMAGE *const image)
{
    char *const path = Compare_ShotPath(&g_HeapAllocator, out_dir, kind, frame);
    WRITER writer;
    RESULT result = Writer_OpenFile(&writer, path);
    Mem_Free(path);
    MUST(result);
    Image_EncodePng(image, &writer);
    return Writer_Close(&writer);
}

static RESULT M_MakeDir(const char *const dir, const char *const name)
{
    char *const path = Path_Join(&g_HeapAllocator, dir, name);
    const RESULT result = File_MakeDir(path);
    Mem_Free(path);
    return result;
}

static RESULT M_ComparePair(
    const char *const run_dir, const char *const against_dir,
    const char *const out_dir, SHOT_DIFF *const diff)
{
    char *const run_path =
        Compare_ShotPath(&g_HeapAllocator, run_dir, "shots", diff->frame);
    char *const against_path =
        Compare_ShotPath(&g_HeapAllocator, against_dir, "shots", diff->frame);
    const bool both = File_Exists(run_path) && File_Exists(against_path);
    IMAGE a = { .allocator = &g_HeapAllocator };
    IMAGE b = a;
    IMAGE xor_image = a;
    IMAGE delta_image = a;
    RESULT result = OK;
    if (both) {
        result = M_LoadImage(run_path, &a);
        if (IS_OK(result)) {
            result = M_LoadImage(against_path, &b);
        }
    }
    Mem_Free(run_path);
    Mem_Free(against_path);
    if (!both) {
        diff->different_pixels = -1;
    } else if (IS_OK(result)) {
        result =
            Compare_Images(&a, &b, diff->frame, &xor_image, &delta_image, diff);
        if (IS_OK(result) && diff->different_pixels != 0) {
            result = M_SaveImage(out_dir, "xor", diff->frame, &xor_image);
            if (IS_OK(result)) {
                result =
                    M_SaveImage(out_dir, "delta", diff->frame, &delta_image);
            }
        }
    }
    Image_Free(&xor_image);
    Image_Free(&delta_image);
    Image_Free(&a);
    Image_Free(&b);
    return result;
}

RESULT Compare_Shots(
    const char *const run_dir, const char *const against_dir,
    const char *const out_dir, uint32_t *const different)
{
    *different = 0;
    M_FRAMES frames;
    Array_Init(&frames, &g_HeapAllocator);
    M_AddFrames(run_dir, &frames);
    M_AddFrames(against_dir, &frames);
    qsort(
        frames.items, (size_t)frames.count, sizeof(uint32_t), M_CompareFrames);
    RESULT result = M_MakeDir(out_dir, "xor");
    if (IS_OK(result)) {
        result = M_MakeDir(out_dir, "delta");
    }
    char *const tsv_path = Path_Join(&g_HeapAllocator, out_dir, "shots.tsv");
    WRITER tsv;
    if (IS_OK(result)) {
        result = Writer_OpenFile(&tsv, tsv_path);
    }
    Mem_Free(tsv_path);
    if (!IS_OK(result)) {
        Array_Free(&frames);
        return result;
    }
    const char *const head = "frame\tdifferent_pixels\tmax_delta\n";
    Writer_WriteBytes(&tsv, head, strlen(head));
    for (int32_t i = 0; IS_OK(result) && i < frames.count; i++) {
        SHOT_DIFF diff = { .frame = frames.items[i] };
        result = M_ComparePair(run_dir, against_dir, out_dir, &diff);
        *different += diff.different_pixels != 0;
        char line[64];
        const int length = snprintf(
            line, sizeof(line), "%" PRIu32 "\t%lld\t%u\n", diff.frame,
            (long long)diff.different_pixels, diff.max_delta);
        Writer_WriteBytes(&tsv, line, (size_t)length);
    }
    Array_Free(&frames);
    const RESULT closed = Writer_Close(&tsv);
    if (IS_OK(result)) {
        return closed;
    }
    Result_Ignore(closed);
    return result;
}
