#include "tests/th01/th01_grz_fixture.h"

#include "th01/grz_private.h"

#include "core/base/memory.h"

RESULT Grx_Load(
    READER *const reader, const ALLOCATOR *const allocator, GRX *const out)
{
    *out = (GRX) {};
    GRX_RECORD record = {};
    MUST(Grz_ReadHeader(reader, &record));
    const size_t size =
        record.rle_size + (size_t)record.stream_count * record.planar_size;
    uint8_t *const data = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    MUST(Reader_Result(reader));
    READER body;
    Reader_OpenBuffer(&body, data, size, nullptr);
    const RESULT result = Grz_ReadBody(&body, &record);
    if (IS_OK(result)) {
        Grz_Publish(&record, allocator, out);
    }
    Mem_Free(data);
    return result;
}
