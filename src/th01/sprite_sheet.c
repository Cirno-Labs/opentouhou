#include "th01/sprite_sheet.h"

#include "core/base/assert.h"
#include "core/image.h"

RESULT SpriteSheet_Load(
    VFS *const vfs, const char *const name, const int32_t cel_width,
    const int32_t cel_height, const ALLOCATOR *const allocator,
    uint8_t **const out, size_t *const size)
{
    ASSERT(cel_width > 0 && cel_width % 8 == 0 && cel_height > 0);
    *out = nullptr;
    *size = 0;
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    IMAGE image;
    const RESULT result = Image_Decode(&reader, &g_HeapAllocator, &image);
    Reader_Close(&reader);
    MUST(result, "%s", name);
    if (image.width % cel_width != 0 || image.height % cel_height != 0) {
        Image_Free(&image);
        return FAIL("%s is not a sheet of whole cels", name);
    }
    const int32_t row_bytes = cel_width / 8;
    const int32_t columns = image.width / cel_width;
    const int32_t rows = image.height / cel_height;
    *size = (size_t)(image.width / 8 * image.height);
    *out = Allocator_AllocZ(allocator, *size);
    uint8_t *byte = *out;
    for (int32_t cel = 0; cel < columns * rows; cel++) {
        const int32_t left = cel % columns * cel_width;
        const int32_t top = cel / columns * cel_height;
        for (int32_t y = top; y < top + cel_height; y++) {
            for (int32_t x = left; x < left + cel_width; x++) {
                const uint8_t *const pixel =
                    &image.pixels[((size_t)y * image.width + x) * 4];
                if (pixel[0] >= 128 && pixel[1] >= 128 && pixel[2] >= 128) {
                    byte[(x - left) / 8] |= (uint8_t)(0x80 >> ((x - left) % 8));
                }
            }
            byte += row_bytes;
        }
    }
    Image_Free(&image);
    return OK;
}
