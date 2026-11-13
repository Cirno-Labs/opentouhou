#include "core/patch/fetch.h"

#include "core/base/memory.h"

void PatchFetch_Release(PATCH_FETCH *const fetch)
{
    Mem_Free(fetch->data);
    fetch->data = nullptr;
    fetch->size = 0;
    Result_Ignore(fetch->result);
    fetch->result = OK;
}
