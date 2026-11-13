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

PRINTF_FORMAT(2, 3)
int Console_Fail(FILE *const out, const char *const fmt, ...)
{
    fputs("othpatch: ", out);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fputc('\n', out);
    return 1;
}

int Console_FailResult(
    FILE *const out, const char *const context, const RESULT result)
{
    Console_Fail(out, "%s: %s", context, result.message);
    Result_Ignore(result);
    return 1;
}

void Console_PrintFailure(
    FILE *const out, const char *const what, const RESULT result)
{
    fprintf(out, "%s: %s.\n", what, result.message);
    Result_Ignore(result);
}

PRINTF_FORMAT(2, 3)
void Console_StartSection(APP_CONTEXT *const run, const char *const fmt, ...)
{
    if (run->sections++ > 0) {
        fputc('\n', run->out);
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(run->out, fmt, args);
    va_end(args);
    fputc('\n', run->out);
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
static void M_DrawBar(
    CONSOLE_BAR *const bar, const double fraction, const char *const amount)
{
    if (!bar->run->terminal) {
        return;
    }
    char cells[M_BAR_CELLS + 1];
    const int32_t filled =
        (int32_t)(MIN(MAX(fraction, 0.0), 1.0) * M_BAR_CELLS);
    for (int32_t i = 0; i < M_BAR_CELLS; i++) {
        cells[i] = i < filled ? '#' : '.';
    }
    cells[M_BAR_CELLS] = '\0';
    const int32_t padding =
        MAX(bar->width - Console_CountCodePoints(bar->label), 0);
    char line[sizeof(bar->line)];
    snprintf(
        line, sizeof(line), "%s%*s [%s] %s", bar->label, (int)padding, "",
        cells, amount);
    if (strcmp(line, bar->line) == 0) {
        return;
    }
    snprintf(bar->line, sizeof(bar->line), "%s", line);
    fprintf(bar->run->out, "\r%s", line);
    fflush(bar->run->out);
}

// Ends the bar: on a terminal, draws it a last time and ends its line;
// elsewhere, prints the label and plain.
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

void Console_DrawFiles(CONSOLE_BAR *const bar)
{
    char amount[64];
    snprintf(
        amount, sizeof(amount), "%d/%d %s", bar->done, bar->total,
        M_Files(bar->total));
    M_DrawBar(bar, (double)bar->done / bar->total, amount);
}

void Console_EndFiles(CONSOLE_BAR *const bar)
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
