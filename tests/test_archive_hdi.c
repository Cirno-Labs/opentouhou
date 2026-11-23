#include "core/archive/hdi.h"

#include "core/archive.h"
#include "hdi_builder.h"
#include "test.h"

#define M_TEXT "Every package holds the game and othpatch. "

static char m_Text[2048];

static const HDI_BUILDER_ENTRY m_Entries[] = {
    { "GAME/", "", 0, false },
    { "GAME/OLD.DAT", "x", 1, true },
    { "GAME/TOUHOU.DAT", m_Text, sizeof(m_Text) - 1, false },
    { "\x93\x8c\x95\xfb.TXT", "a", 1, false },
    { "EMPTY", "", 0, false },
};

static HDI_BUILDER_IMAGE M_Build(const uint32_t sector_size, const bool fat16)
{
    for (size_t i = 0; i + 1 < sizeof(m_Text); i++) {
        m_Text[i] = M_TEXT[i % strlen(M_TEXT)];
    }
    return HdiBuilder_Build(
        sector_size, fat16, m_Entries,
        (int)(sizeof(m_Entries) / sizeof(m_Entries[0])));
}

static RESULT M_OpenBytes(
    const uint8_t *const data, const size_t size, ARCHIVE **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    return Archive_Open(&g_HdiFormat, &reader, &g_HeapAllocator, out);
}

static void M_CheckRead(
    ARCHIVE *const archive, const char *const name, const char *const expected,
    const size_t expected_size)
{
    const ARCHIVE_ENTRY *const entry = Archive_Find(archive, name);
    CHECK(entry != nullptr);
    if (entry == nullptr) {
        return;
    }
    CHECK_EQ_INT(ArchiveEntry_GetSize(entry), expected_size);
    uint8_t *data;
    CHECK(Result_Log(Archive_Read(archive, entry, &data), LOG_LEVEL_ERROR));
    CHECK(data != nullptr && memcmp(data, expected, expected_size) == 0);
    Allocator_Free(&g_HeapAllocator, data, expected_size);
}

static void M_CheckLayout(const uint32_t sector_size, const bool fat16)
{
    HDI_BUILDER_IMAGE image = M_Build(sector_size, fat16);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_OpenBytes(image.data, image.size, &archive), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Archive_GetCount(archive), 4);
    CHECK_EQ_STR(ArchiveEntry_GetName(Archive_GetEntry(archive, 0)), "game/");
    CHECK_EQ_STR(
        ArchiveEntry_GetName(Archive_GetEntry(archive, 1)), "game/touhou.dat");
    CHECK_EQ_STR(
        ArchiveEntry_GetName(Archive_GetEntry(archive, 2)), "東方.txt");
    CHECK_EQ_STR(ArchiveEntry_GetName(Archive_GetEntry(archive, 3)), "empty");
    M_CheckRead(archive, "game/touhou.dat", m_Text, strlen(m_Text));
    M_CheckRead(archive, "東方.txt", "a", 1);
    M_CheckRead(archive, "empty", "", 0);
    Archive_Close(archive);
    Mem_Free(image.data);
}

static void M_CheckFails(
    const HDI_BUILDER_IMAGE *const image, const size_t size,
    const char *const expected)
{
    ARCHIVE *archive;
    const RESULT result = M_OpenBytes(image->data, size, &archive);
    CHECK(!IS_OK(result));
    if (!IS_OK(result)) {
        CHECK_EQ_STR(result.message, expected);
    }
    Result_Ignore(result);
}

TEST(fat12_with_512_byte_sectors_reads)
{
    M_CheckLayout(512, false);
}

TEST(fat12_with_256_byte_sectors_reads)
{
    M_CheckLayout(256, false);
}

TEST(fat16_with_512_byte_sectors_reads)
{
    M_CheckLayout(512, true);
}

TEST(fat16_with_256_byte_sectors_reads)
{
    M_CheckLayout(256, true);
}

TEST(a_damaged_image_fails_to_open)
{
    HDI_BUILDER_IMAGE image = M_Build(512, false);
    uint8_t *const fat = image.data + image.fat_offset;
    M_CheckFails(&image, 100, "the image has no Anex86 header");
    M_CheckFails(
        &image, image.size - 1, "the image is shorter than its header says");
    M_CheckFails(&image, 16, "read of 4 bytes at 0x10 past end 0x10");
    HdiBuilder_SetFat(fat, false, 4, 1000);
    M_CheckFails(
        &image, image.size,
        "game/touhou.dat: the cluster chain leaves the partition");
    HdiBuilder_SetFat(fat, false, 4, 6);
    HdiBuilder_SetFat(fat, false, 2, 2);
    M_CheckFails(
        &image, image.size, "game/: a directory's cluster chain is damaged");
    HdiBuilder_SetFat(fat, false, 2, 0xFFF);
    image.data[4096 + 512 + 33] = 0x01;
    M_CheckFails(
        &image, image.size, "the image has no active FAT12 or FAT16 partition");
    Mem_Free(image.data);
}

TEST(any_damaged_byte_of_the_file_system_fails_or_reads)
{
    HDI_BUILDER_IMAGE image = M_Build(512, false);
    const size_t start = image.fat_offset - 512;
    for (size_t at = 0; at < image.data_offset + 4 * 512 - start; at++) {
        uint8_t *const byte = image.data + start + at;
        const uint8_t kept = *byte;
        *byte = (uint8_t)(kept ^ 0xFF);
        ARCHIVE *archive;
        if (Result_Ignore(M_OpenBytes(image.data, image.size, &archive))) {
            for (int32_t i = 0; i < Archive_GetCount(archive); i++) {
                const ARCHIVE_ENTRY *const entry = Archive_GetEntry(archive, i);
                uint8_t *data;
                if (Result_Ignore(Archive_Read(archive, entry, &data))) {
                    Allocator_Free(
                        &g_HeapAllocator, data, ArchiveEntry_GetSize(entry));
                }
            }
            Archive_Close(archive);
        }
        *byte = kept;
    }
    Mem_Free(image.data);
}
