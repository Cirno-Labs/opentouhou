#include "th01/reiiden_image.h"

#include "core/hash/sha256.h"

#include <string.h>

RESULT ReiidenImage_Open(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    READER *const image)
{
    FAIL_IF(
        Reader_Size(reader) != REIIDEN_IMAGE_SIZE,
        "Unsupported REIIDEN.EXE length");
    FAIL_IF(
        scratch == nullptr || scratch_size < REIIDEN_IMAGE_SIZE,
        "Missing executable scratch storage");
    Reader_Seek(reader, 0);
    Reader_ReadBytes(reader, scratch, REIIDEN_IMAGE_SIZE);
    MUST(Reader_Result(reader));
    char digest[SHA256_HEX_SIZE];
    Sha256_ComputeHex(scratch, REIIDEN_IMAGE_SIZE, digest);
    FAIL_IF(
        strcmp(
            digest,
            "76a5d241dead706e768bd1516d6aa539b589445e5941877073e8cb9e9ae8f62a")
            != 0,
        "Unsupported REIIDEN.EXE version");
    Reader_OpenBuffer(image, scratch, REIIDEN_IMAGE_SIZE, nullptr);
    FAIL_IF(Reader_ReadU16(image) != 0x5a4d, "Invalid MZ signature");
    Reader_Seek(image, 8);
    FAIL_IF(Reader_ReadU16(image) != 896, "Unsupported MZ header");
    return Reader_Result(image);
}
