#include "th02/map.h"

#include <string.h>

RESULT Map_Load(
    READER *const reader, const ALLOCATOR *const allocator, MAP *const out)
{
    *out = (MAP) {};
    MUST(Reader_Result(reader));
    FAIL_IF(
        Reader_Remaining(reader)
            < 10 + MAP_SECTION_COUNT * MAP_SECTION_ROWS * MAP_COLUMNS,
        "Truncated map data");
    Reader_Skip(reader, 10);
    MAP_SECTION sections[MAP_SECTION_COUNT];
    for (size_t section = 0; section < MAP_SECTION_COUNT; section++) {
        for (size_t row = 0; row < MAP_SECTION_ROWS; row++) {
            Reader_ReadBytes(reader, sections[section].tiles[row], MAP_COLUMNS);
        }
    }
    MUST(Reader_Result(reader));
    MAP_SECTION *const loaded = Allocator_Alloc(allocator, sizeof(sections));
    memcpy(loaded, sections, sizeof(sections));
    out->sections = loaded;
    return OK;
}
