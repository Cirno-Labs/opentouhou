#include "othpatch/context.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"

#include <stdarg.h>
#include <string.h>

#define M_BAR_CELLS 20
#define M_MEGABYTE (1024.0 * 1024.0)

PRINTF_FORMAT(2, 3)
char *Console_Format(ARENA *const arena, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const heap = Str_FormatV(fmt, args);
    va_end(args);
    const size_t size = strlen(heap) + 1;
    char *const copy = Arena_Alloc(arena, size);
    memcpy(copy, heap, size);
    Mem_Free(heap);
    return copy;
}

PRINTF_FORMAT(3, 4)
void Console_Fail(
    FILE *const out, const RESULT result, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fputc('\n', out);
    Result_Ignore(result);
}

PRINTF_FORMAT(2, 3)
void Console_StartSection(APP_CONTEXT *const run, const char *const fmt, ...)
{
    if (run->sections > 0) {
        fputc('\n', run->out);
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(run->out, fmt, args);
    va_end(args);
    fputc('\n', run->out);
    if (run->sections++ == 0) {
        fprintf(run->out, "Patches folder: %s\n", run->dir);
    }
}

int32_t Console_CountCodePoints(const char *const text)
{
    int32_t count = 0;
    for (const char *c = text; *c != '\0'; c++) {
        count += ((unsigned char)*c & 0xC0) != 0x80;
    }
    return count;
}

// Draws the bar on a terminal, filled to fraction, with amount after it.
// Formats a bar line: label padded to width code points, the cells
// filled to fraction, and amount.
static void M_FormatLine(
    char *const line, const size_t size, const char *const label,
    const int32_t width, const double fraction, const char *const amount)
{
    char cells[M_BAR_CELLS + 1];
    const int32_t filled =
        (int32_t)(MIN(MAX(fraction, 0.0), 1.0) * M_BAR_CELLS);
    for (int32_t i = 0; i < M_BAR_CELLS; i++) {
        cells[i] = i < filled ? '#' : '.';
    }
    cells[M_BAR_CELLS] = '\0';
    const int32_t padding = MAX(width - Console_CountCodePoints(label), 0);
    snprintf(
        line, size, "%s%*s [%s] %s", label, (int)padding, "", cells, amount);
}

static void M_DrawBar(
    CONSOLE_BAR *const bar, const double fraction, const char *const amount)
{
    if (!bar->run->terminal) {
        return;
    }
    char line[sizeof(bar->line)];
    M_FormatLine(line, sizeof(line), bar->label, bar->width, fraction, amount);
    if (strcmp(line, bar->line) == 0) {
        return;
    }
    snprintf(bar->line, sizeof(bar->line), "%s", line);
    fprintf(bar->run->out, "\r%s", line);
    fflush(bar->run->out);
}

static void M_EndBar(
    CONSOLE_BAR *const bar, const double fraction, const char *const amount,
    const char *const plain)
{
    if (bar->run->terminal) {
        M_DrawBar(bar, fraction, amount);
        fputc('\n', bar->run->out);
    } else {
        fprintf(bar->run->out, "%s: %s\n", bar->label, plain);
    }
}

static const char *M_Files(const int32_t total)
{
    return total == 1 ? "file" : "files";
}

static void M_FormatFiles(const CONSOLE_BAR *const bar, char amount[64])
{
    snprintf(
        amount, 64, "%d/%d %s", bar->done, bar->total, M_Files(bar->total));
}

static void M_DrawFiles(CONSOLE_BAR *const bar)
{
    char amount[64];
    M_FormatFiles(bar, amount);
    M_DrawBar(bar, (double)bar->done / bar->total, amount);
}

static void M_EndFiles(CONSOLE_BAR *const bar)
{
    char amount[64];
    snprintf(
        amount, sizeof(amount), "%d/%d %s", bar->done, bar->total,
        M_Files(bar->total));
    char plain[64];
    if (bar->done == bar->total) {
        snprintf(plain, sizeof(plain), "%d %s", bar->done, M_Files(bar->total));
    } else {
        snprintf(plain, sizeof(plain), "%s", amount);
    }
    M_EndBar(bar, (double)bar->done / bar->total, amount, plain);
}

static void M_FormatBytes(
    const CONSOLE_BAR *const bar, char amount[64], double *const fraction)
{
    if (bar->expected == 0) {
        snprintf(amount, 64, "%.1f MB", (double)bar->received / M_MEGABYTE);
        *fraction = 0.0;
        return;
    }
    snprintf(
        amount, 64, "%.1f/%.1f MB", (double)bar->received / M_MEGABYTE,
        (double)bar->expected / M_MEGABYTE);
    *fraction = (double)bar->received / (double)bar->expected;
}

void Console_ReceiveBytes(
    void *const context, const PATCH_FETCH *const fetch, const size_t received,
    const size_t expected)
{
    CONSOLE_BAR *const bar = context;
    bar->received = received;
    bar->expected = expected;
    char amount[64];
    double fraction;
    M_FormatBytes(bar, amount, &fraction);
    M_DrawBar(bar, fraction, amount);
}

void Console_EndBytes(CONSOLE_BAR *const bar, const bool arrived)
{
    if (arrived) {
        bar->expected = bar->received;
    }
    char amount[64];
    double fraction;
    M_FormatBytes(bar, amount, &fraction);
    char plain[64];
    if (arrived) {
        snprintf(
            plain, sizeof(plain), "%.1f MB",
            (double)bar->received / M_MEGABYTE);
    } else {
        snprintf(plain, sizeof(plain), "%s", amount);
    }
    M_EndBar(bar, fraction, amount, plain);
}

// Formats the amount of a file line of the size when it is known: bytes
// below 1 KiB, kilobytes, or megabytes from 1 MiB on.
static void M_FormatFile(
    const CONSOLE_FILE *const file, char amount[64], double *const fraction)
{
    const size_t largest = MAX(file->received, file->size);
    *fraction =
        file->size != 0 ? (double)file->received / (double)file->size : 0.0;
    if (largest < 1024) {
        if (file->size == 0) {
            snprintf(amount, 64, "%zu B", file->received);
        } else {
            snprintf(amount, 64, "%zu/%zu B", file->received, file->size);
        }
        return;
    }
    const bool mega = largest >= (size_t)1024 * 1024;
    const double unit = mega ? M_MEGABYTE : 1024.0;
    const char *const name = mega ? "MB" : "KB";
    const char *const format = mega ? "%.1f" : "%.0f";
    char received[24];
    snprintf(received, sizeof(received), format, (double)file->received / unit);
    if (file->size == 0) {
        snprintf(amount, 64, "%s %s", received, name);
        return;
    }
    char size[24];
    snprintf(size, sizeof(size), format, (double)file->size / unit);
    snprintf(amount, 64, "%s/%s %s", received, size, name);
}

// Draws the block of a patch on an ANSI terminal: moves the cursor up over
// the block drawn before, and clears and draws each line. With collapse,
// the block is the line of the patch alone, which stays. Draws nothing
// when the block is the one on the terminal already.
static void M_DrawBlock(CONSOLE_PATCH *const patch, const bool collapse)
{
    CONSOLE_BAR *const bar = &patch->bar;
    char labels[CONSOLE_FILE_LINES][256];
    int32_t count = 0;
    for (int32_t i = 0; !collapse && i < CONSOLE_FILE_LINES; i++) {
        if (patch->files[i].file != nullptr) {
            snprintf(
                labels[i], sizeof(labels[i]), "  %s", patch->files[i].file);
            patch->width =
                MAX(patch->width, Console_CountCodePoints(labels[i]));
        }
    }
    char block[sizeof(patch->block)];
    char line[512];
    char amount[64];
    M_FormatFiles(bar, amount);
    M_FormatLine(
        line, sizeof(line), bar->label, patch->width,
        (double)bar->done / bar->total, amount);
    int32_t length = snprintf(block, sizeof(block), "\x1b[2K%s\n", line);
    count++;
    for (int32_t i = 0; !collapse && i < CONSOLE_FILE_LINES; i++) {
        const CONSOLE_FILE *const file = &patch->files[i];
        if (file->file == nullptr) {
            continue;
        }
        double fraction;
        M_FormatFile(file, amount, &fraction);
        M_FormatLine(
            line, sizeof(line), labels[i], patch->width, fraction, amount);
        length += snprintf(
            block + length, sizeof(block) - (size_t)length, "\x1b[2K%s\n",
            line);
        count++;
    }
    if (strcmp(block, patch->block) == 0) {
        patch->lines = collapse ? 0 : patch->lines;
        return;
    }
    snprintf(patch->block, sizeof(patch->block), "%s", block);
    FILE *const out = bar->run->out;
    if (patch->lines > 0) {
        fprintf(out, "\x1b[%dA", (int)patch->lines);
    }
    fputs(block, out);
    for (int32_t i = count; i < patch->lines; i++) {
        fputs("\x1b[2K\n", out);
    }
    if (patch->lines > count) {
        fprintf(out, "\x1b[%dA", (int)(patch->lines - count));
    }
    patch->lines = collapse ? 0 : count;
    fflush(out);
}

// Returns the line of file, or a free line when file is nullptr.
static CONSOLE_FILE *M_FindFile(
    CONSOLE_PATCH *const patch, const char *const file)
{
    for (int32_t i = 0; i < CONSOLE_FILE_LINES; i++) {
        const char *const held = patch->files[i].file;
        if (file == nullptr ? held == nullptr
                            : held != nullptr && strcmp(held, file) == 0) {
            return &patch->files[i];
        }
    }
    return nullptr;
}

void Console_StartPatch(
    CONSOLE_PATCH *const patch, APP_CONTEXT *const run, const char *const label,
    const int32_t width, const int32_t files)
{
    *patch = (CONSOLE_PATCH) {
        .bar = {
            .run = run,
            .label = label,
            .width = width,
            .total = files,
        },
        .width = width,
    };
    if (run->ansi) {
        M_DrawBlock(patch, false);
    } else {
        M_DrawFiles(&patch->bar);
    }
}

void Console_ReceiveFile(
    CONSOLE_PATCH *const patch, const char *const file, const size_t received,
    const size_t size)
{
    if (!patch->bar.run->ansi) {
        return;
    }
    CONSOLE_FILE *line = M_FindFile(patch, file);
    if (line == nullptr) {
        line = M_FindFile(patch, nullptr);
    }
    if (line != nullptr) {
        *line = (CONSOLE_FILE) {
            .file = file,
            .received = received,
            .size = size,
        };
        M_DrawBlock(patch, false);
    }
}

void Console_EndFile(CONSOLE_PATCH *const patch, const char *const file)
{
    patch->bar.done++;
    if (!patch->bar.run->ansi) {
        M_DrawFiles(&patch->bar);
        return;
    }
    CONSOLE_FILE *const line = M_FindFile(patch, file);
    if (line != nullptr) {
        *line = (CONSOLE_FILE) {};
    }
    M_DrawBlock(patch, false);
}

void Console_EndPatch(CONSOLE_PATCH *const patch)
{
    if (patch->bar.run->ansi) {
        M_DrawBlock(patch, true);
    } else {
        M_EndFiles(&patch->bar);
    }
}
