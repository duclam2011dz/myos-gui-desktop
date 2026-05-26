#include "diskfs.h"

#include "ata.h"
#include "util.h"

#define DISKFS_MAGIC 0x5346594D
#define DISKFS_MAX_FILES 8
#define DISKFS_SECTOR_SIZE 512
#define DISKFS_MAX_FILE_SIZE 2048
#define DISKFS_TOTAL_SECTORS 32

struct diskfs_entry {
    char name[32];
    uint32_t start_sector;
    uint32_t size;
    uint8_t reserved[8];
} __attribute__((packed));

static uint32_t fs_start_lba;
static bool available;
static uint32_t file_count;
static struct diskfs_entry entries[DISKFS_MAX_FILES];
static uint8_t file_buffer[DISKFS_MAX_FILE_SIZE];

static bool read_fs_sector(uint32_t relative_sector, void *buffer)
{
    return ata_read_sector(fs_start_lba + relative_sector, buffer);
}

static bool write_fs_sector(uint32_t relative_sector, const void *buffer)
{
    return ata_write_sector(fs_start_lba + relative_sector, buffer);
}

static const char *skip_leading_slash(const char *path)
{
    while (*path == '/') {
        path++;
    }
    return path;
}

static bool path_matches(const char *left, const char *right)
{
    if (kstrcmp(left, right) == 0) {
        return true;
    }

    return kstrcmp(skip_leading_slash(left), skip_leading_slash(right)) == 0;
}

static void copy_name(char dst[32], const char *path)
{
    uint32_t i = 0;
    for (; i < 31 && path[i] != '\0'; i++) {
        dst[i] = path[i];
    }
    dst[i++] = '\0';
    for (; i < 32; i++) {
        dst[i] = '\0';
    }
}

static uint32_t file_sector_count(uint32_t size)
{
    uint32_t sectors = (size + DISKFS_SECTOR_SIZE - 1) / DISKFS_SECTOR_SIZE;
    return sectors == 0 ? 1 : sectors;
}

static bool entry_name_is_valid(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        return false;
    }
    for (uint32_t i = 0; i < 32; i++) {
        if (name[i] == '\0') {
            return true;
        }
    }
    return false;
}

bool diskfs_validate(void)
{
    if (!available || file_count > DISKFS_MAX_FILES) {
        return false;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        if (!entry_name_is_valid(entries[i].name) || entries[i].start_sector == 0 ||
            entries[i].size > DISKFS_MAX_FILE_SIZE) {
            return false;
        }
        uint32_t start = entries[i].start_sector;
        uint32_t end = start + file_sector_count(entries[i].size);
        if (end > DISKFS_TOTAL_SECTORS) {
            return false;
        }
        for (uint32_t j = i + 1; j < file_count; j++) {
            uint32_t other_start = entries[j].start_sector;
            uint32_t other_end = other_start + file_sector_count(entries[j].size);
            if (start < other_end && other_start < end) {
                return false;
            }
        }
    }
    return true;
}

static uint32_t entry_capacity_sectors(uint32_t index)
{
    uint32_t start = entries[index].start_sector;
    uint32_t end = DISKFS_TOTAL_SECTORS;

    for (uint32_t i = 0; i < file_count; i++) {
        if (i != index && entries[i].start_sector > start && entries[i].start_sector < end) {
            end = entries[i].start_sector;
        }
    }

    return end - start;
}

static uint32_t next_free_sector(void)
{
    uint32_t end = 1;
    for (uint32_t i = 0; i < file_count; i++) {
        uint32_t entry_end = entries[i].start_sector + file_sector_count(entries[i].size);
        if (entry_end > end) {
            end = entry_end;
        }
    }
    return end;
}

static bool write_directory_sector(void)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    for (uint32_t i = 0; i < DISKFS_SECTOR_SIZE; i++) {
        sector[i] = 0;
    }

    *(uint32_t *) &sector[0] = DISKFS_MAGIC;
    *(uint32_t *) &sector[4] = file_count;
    struct diskfs_entry *disk_entries = (struct diskfs_entry *) &sector[16];
    for (uint32_t i = 0; i < file_count; i++) {
        disk_entries[i] = entries[i];
    }

    return write_fs_sector(0, sector);
}

void diskfs_initialize(uint32_t start_lba)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];

    fs_start_lba = start_lba;
    available = false;
    file_count = 0;

    if (!read_fs_sector(0, sector)) {
        return;
    }

    uint32_t magic = *(uint32_t *) &sector[0];
    if (magic != DISKFS_MAGIC) {
        return;
    }

    uint32_t count = *(uint32_t *) &sector[4];
    if (count > DISKFS_MAX_FILES) {
        count = DISKFS_MAX_FILES;
    }

    struct diskfs_entry *disk_entries = (struct diskfs_entry *) &sector[16];
    for (uint32_t i = 0; i < count; i++) {
        entries[i] = disk_entries[i];
    }

    file_count = count;
    available = true;
    if (!diskfs_validate()) {
        file_count = 0;
        available = false;
    }
}

bool diskfs_is_available(void)
{
    return available;
}

size_t diskfs_file_count(void)
{
    return file_count;
}

const char *diskfs_file_name(size_t index)
{
    if (index >= file_count) {
        return "";
    }

    return entries[index].name;
}

uint32_t diskfs_file_size(size_t index)
{
    if (index >= file_count) {
        return 0;
    }

    return entries[index].size;
}

int diskfs_find_file(const char *path)
{
    for (uint32_t i = 0; i < file_count; i++) {
        if (path_matches(path, entries[i].name)) {
            return (int) i;
        }
    }

    return -1;
}

bool diskfs_read_index(uint32_t index, uint32_t offset, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    uint8_t *out = (uint8_t *) buffer;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }

    if (index >= file_count || buffer == 0) {
        return false;
    }

    if (offset >= entries[index].size || size == 0) {
        return true;
    }

    uint32_t remaining = entries[index].size - offset;
    if (size > remaining) {
        size = remaining;
    }

    uint32_t done = 0;
    while (done < size) {
        uint32_t absolute = offset + done;
        uint32_t sector_index = absolute / DISKFS_SECTOR_SIZE;
        uint32_t sector_offset = absolute % DISKFS_SECTOR_SIZE;
        uint32_t chunk = DISKFS_SECTOR_SIZE - sector_offset;

        if (chunk > size - done) {
            chunk = size - done;
        }

        if (!read_fs_sector(entries[index].start_sector + sector_index, sector)) {
            return false;
        }

        for (uint32_t i = 0; i < chunk; i++) {
            out[done + i] = sector[sector_offset + i];
        }
        done += chunk;
    }

    if (bytes_read != 0) {
        *bytes_read = done;
    }
    return true;
}

bool diskfs_write_file(const char *path, const void *buffer, uint32_t size)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    const uint8_t *input = (const uint8_t *) buffer;
    int existing = diskfs_find_file(path);
    uint32_t index;
    uint32_t sectors = file_sector_count(size);

    if (!available || path == 0 || buffer == 0 || !entry_name_is_valid(path) || size > DISKFS_MAX_FILE_SIZE) {
        return false;
    }

    if (existing >= 0) {
        index = (uint32_t) existing;
        if (sectors > entry_capacity_sectors(index)) {
            return false;
        }
    } else {
        if (file_count >= DISKFS_MAX_FILES) {
            return false;
        }

        uint32_t start = next_free_sector();
        if (start + sectors > DISKFS_TOTAL_SECTORS) {
            return false;
        }

        index = file_count++;
        copy_name(entries[index].name, path);
        entries[index].start_sector = start;
        for (uint32_t i = 0; i < sizeof(entries[index].reserved); i++) {
            entries[index].reserved[i] = 0;
        }
    }

    for (uint32_t sector_index = 0; sector_index < sectors; sector_index++) {
        for (uint32_t i = 0; i < DISKFS_SECTOR_SIZE; i++) {
            uint32_t source = sector_index * DISKFS_SECTOR_SIZE + i;
            sector[i] = source < size ? input[source] : 0;
        }

        if (!write_fs_sector(entries[index].start_sector + sector_index, sector)) {
            return false;
        }
    }

    entries[index].size = size;
    return write_directory_sector();
}

const char *diskfs_read_file(const char *name, size_t *size)
{
    int index = diskfs_find_file(name);
    if (index < 0 || entries[index].size > DISKFS_MAX_FILE_SIZE) {
        if (size != 0) {
            *size = 0;
        }
        return 0;
    }

    uint32_t bytes_read;
    if (!diskfs_read_index((uint32_t) index, 0, file_buffer, entries[index].size, &bytes_read)) {
        if (size != 0) {
            *size = 0;
        }
        return 0;
    }

    if (size != 0) {
        *size = bytes_read;
    }
    return (const char *) file_buffer;
}

uint32_t diskfs_max_file_size(void)
{
    return DISKFS_MAX_FILE_SIZE;
}
