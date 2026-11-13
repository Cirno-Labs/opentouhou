// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *output;
    bool break_on_diff;
    bool has_break_at;
    uint64_t break_at;
} RUNNER_OPTIONS;

typedef struct {
    uint64_t rows;
    uint64_t first_diff;
    uint32_t differences;
    int32_t start_columns;
    int32_t calc_columns;
    int32_t skipped_columns;
    bool different;
    bool complete_schema;
    bool complete_replay;
} RUNNER_REPORT;
