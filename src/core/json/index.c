#include "core/json.h"

void Json_BuildIndex(
    const JSON_VALUE *const object, const ALLOCATOR *const allocator,
    JSON_INDEX *const out)
{
    HashMap_Init(out, allocator);
    if (object == nullptr || object->type != JSON_OBJECT) {
        return;
    }
    for (int32_t i = 0; i < object->object.count; i++) {
        const JSON_MEMBER *const member = &object->object.members[i];
        HashMap_Put(out, member->key, member->value);
    }
}

const JSON_VALUE *Json_IndexGet(
    const JSON_INDEX *const index, const char *const key)
{
    if (index == nullptr || key == nullptr) {
        return nullptr;
    }
    const JSON_VALUE *const *const value = HashMap_Get(index, key);
    return value != nullptr ? *value : nullptr;
}
