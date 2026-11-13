// SPDX-License-Identifier: GPL-3.0-only

#include "core/input.h"

#include "input_backend_fake.h"

#include <stdlib.h>

typedef struct {
    bool shoot;
} FIRST;

typedef struct {
    bool shoot;
} SECOND;

static const INPUT_ACTION m_First[] = {
    INPUT_BUTTON(FIRST, shoot, "z", nullptr, nullptr),
};
static const INPUT_ACTION m_Second[] = {
    INPUT_BUTTON(SECOND, shoot, "x", nullptr, nullptr),
};
static const INPUT_SCHEMA m_FirstSchema = INPUT_SCHEMA_OF(FIRST, m_First, 1);
static const INPUT_SCHEMA m_SecondSchema = INPUT_SCHEMA_OF(SECOND, m_Second, 1);

int main(void)
{
    INPUT_STATE *const input = Input_Create(&m_FirstSchema, &g_HeapAllocator);
    Input_AddSchema(input, &m_SecondSchema);
    Input_Destroy(input);
    return EXIT_SUCCESS;
}
