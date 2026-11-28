#pragma once

// The types that more than one part of pc98/pmd shares, to avoid a
// circular include between them.

typedef struct PMD PMD;

typedef enum { PMD_BOARD_26, PMD_BOARD_86 } PMD_BOARD;
