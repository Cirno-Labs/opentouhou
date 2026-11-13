#include "core/json.h"

#include <string.h>

const JSON_VALUE *Json_Get(
    const JSON_VALUE *const object, const char *const key)
{
    if (object == nullptr || object->type != JSON_OBJECT || key == nullptr) {
        return nullptr;
    }
    for (int32_t i = object->object.count - 1; i >= 0; i--) {
        const JSON_MEMBER *const member = &object->object.members[i];
        if (strcmp(member->key, key) == 0) {
            return member->value;
        }
    }
    return nullptr;
}

const JSON_VALUE *Json_At(const JSON_VALUE *const array, const int32_t index)
{
    if (array == nullptr || array->type != JSON_ARRAY || index < 0
        || index >= array->array.count) {
        return nullptr;
    }
    return &array->array.items[index];
}

bool Json_GetBool(const JSON_VALUE *const value, const bool fallback)
{
    if (value == nullptr || value->type != JSON_BOOL) {
        return fallback;
    }
    return value->boolean;
}

int64_t Json_GetInt(const JSON_VALUE *const value, const int64_t fallback)
{
    if (value == nullptr || value->type != JSON_NUMBER
        || !value->number.is_integer) {
        return fallback;
    }
    return value->number.integer;
}

double Json_GetNumber(const JSON_VALUE *const value, const double fallback)
{
    if (value == nullptr || value->type != JSON_NUMBER) {
        return fallback;
    }
    return value->number.value;
}

const char *Json_GetString(
    const JSON_VALUE *const value, const char *const fallback)
{
    if (value == nullptr || value->type != JSON_STRING) {
        return fallback;
    }
    return value->string.data;
}
