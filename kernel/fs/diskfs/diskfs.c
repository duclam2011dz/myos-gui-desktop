#include "diskfs.h"

#include "ata.h"
#include "serial.h"
#include "util.h"

#define DISKFS_MAGIC 0x3246594D
#define DISKFS_JOURNAL_MAGIC 0x324E524A
#define DISKFS_SECTOR_SIZE 512
#define DISKFS_TOTAL_SECTORS 4096
#define DISKFS_MAX_INODES 64
#define DISKFS_INODE_SIZE 64
#define DISKFS_BITMAP_BYTES 512
#define DISKFS_NAME_MAX 31
#define DISKFS_PATH_MAX 64
#define DISKFS_ROOT_INODE 0
#define DISKFS_TYPE_FILE 1
#define DISKFS_TYPE_DIR 2
#define DISKFS_CACHE_SLOTS 8
#define DISKFS_READ_BUFFER_SIZE 16384
#define DISKFS_MAX_FILE_SIZE 1048576

struct diskfs_superblock {
    uint32_t magic;
    uint32_t total_sectors;
    uint32_t max_inodes;
    uint32_t inode_count;
    uint32_t bitmap_start;
    uint32_t bitmap_sectors;
    uint32_t inode_start;
    uint32_t inode_sectors;
    uint32_t data_start;
    uint32_t journal_state;
} __attribute__((packed));

struct diskfs_inode {
    uint8_t type;
    uint8_t reserved0;
    uint16_t flags;
    uint32_t size;
    uint32_t first_sector;
    uint32_t sector_count;
    uint32_t parent;
    char name[32];
    uint8_t reserved[12];
} __attribute__((packed));

struct diskfs_cache_entry {
    bool valid;
    bool dirty;
    uint32_t sector;
    uint8_t data[DISKFS_SECTOR_SIZE];
};

static uint32_t fs_start_lba;
static bool available;
static struct diskfs_superblock superblock;
static struct diskfs_inode inodes[DISKFS_MAX_INODES];
static uint8_t bitmap[DISKFS_BITMAP_BYTES];
static struct diskfs_cache_entry cache[DISKFS_CACHE_SLOTS];
static uint32_t cache_clock;
static uint32_t file_indices[DISKFS_MAX_INODES];
static char file_names[DISKFS_MAX_INODES][DISKFS_PATH_MAX];
static uint32_t visible_file_count;
static uint8_t file_buffer[DISKFS_READ_BUFFER_SIZE];

static void mem_zero(void *ptr, uint32_t size)
{
    uint8_t *out = (uint8_t *) ptr;
    for (uint32_t i = 0; i < size; i++) {
        out[i] = 0;
    }
}

static void mem_copy(void *dst, const void *src, uint32_t size)
{
    uint8_t *out = (uint8_t *) dst;
    const uint8_t *in = (const uint8_t *) src;
    for (uint32_t i = 0; i < size; i++) {
        out[i] = in[i];
    }
}

static bool raw_read_sector(uint32_t relative_sector, void *buffer)
{
    if (relative_sector >= DISKFS_TOTAL_SECTORS) {
        return false;
    }
    return ata_read_sector(fs_start_lba + relative_sector, buffer);
}

static bool raw_write_sector(uint32_t relative_sector, const void *buffer)
{
    if (relative_sector >= DISKFS_TOTAL_SECTORS) {
        return false;
    }
    return ata_write_sector(fs_start_lba + relative_sector, buffer);
}

static bool cache_writeback(struct diskfs_cache_entry *entry)
{
    if (!entry->valid || !entry->dirty) {
        return true;
    }
    if (!raw_write_sector(entry->sector, entry->data)) {
        return false;
    }
    entry->dirty = false;
    return true;
}

static struct diskfs_cache_entry *cache_get(uint32_t sector, bool for_write)
{
    struct diskfs_cache_entry *victim = &cache[cache_clock++ % DISKFS_CACHE_SLOTS];
    for (uint32_t i = 0; i < DISKFS_CACHE_SLOTS; i++) {
        if (cache[i].valid && cache[i].sector == sector) {
            if (for_write) {
                cache[i].dirty = true;
            }
            return &cache[i];
        }
    }

    if (!cache_writeback(victim)) {
        return 0;
    }
    if (!raw_read_sector(sector, victim->data)) {
        victim->valid = false;
        return 0;
    }
    victim->valid = true;
    victim->dirty = for_write;
    victim->sector = sector;
    return victim;
}

static bool read_sector(uint32_t relative_sector, void *buffer)
{
    struct diskfs_cache_entry *entry = cache_get(relative_sector, false);
    if (entry == 0) {
        return false;
    }
    mem_copy(buffer, entry->data, DISKFS_SECTOR_SIZE);
    return true;
}

static bool write_sector(uint32_t relative_sector, const void *buffer)
{
    struct diskfs_cache_entry *entry = cache_get(relative_sector, true);
    if (entry == 0) {
        return false;
    }
    mem_copy(entry->data, buffer, DISKFS_SECTOR_SIZE);
    return true;
}

bool diskfs_flush(void)
{
    bool ok = true;
    for (uint32_t i = 0; i < DISKFS_CACHE_SLOTS; i++) {
        if (!cache_writeback(&cache[i])) {
            ok = false;
        }
    }
    return ok;
}

static bool bitmap_get(uint32_t sector)
{
    return (bitmap[sector / 8] & (uint8_t) (1 << (sector % 8))) != 0;
}

static void bitmap_set(uint32_t sector, bool used)
{
    if (used) {
        bitmap[sector / 8] |= (uint8_t) (1 << (sector % 8));
    } else {
        bitmap[sector / 8] &= (uint8_t) ~(1 << (sector % 8));
    }
}

static bool write_bitmap(void)
{
    return write_sector(superblock.bitmap_start, bitmap) && diskfs_flush();
}

static bool write_inode(uint32_t index)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    uint32_t per_sector = DISKFS_SECTOR_SIZE / DISKFS_INODE_SIZE;
    uint32_t sector_index = superblock.inode_start + index / per_sector;
    uint32_t offset = (index % per_sector) * DISKFS_INODE_SIZE;
    if (!read_sector(sector_index, sector)) {
        return false;
    }
    mem_copy(sector + offset, &inodes[index], sizeof(struct diskfs_inode));
    return write_sector(sector_index, sector) && diskfs_flush();
}

static bool write_superblock(void)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    mem_zero(sector, sizeof(sector));
    mem_copy(sector, &superblock, sizeof(superblock));
    return write_sector(0, sector) && diskfs_flush();
}

static bool write_journal(uint32_t state)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    mem_zero(sector, sizeof(sector));
    *(uint32_t *) &sector[0] = DISKFS_JOURNAL_MAGIC;
    *(uint32_t *) &sector[4] = state;
    superblock.journal_state = state;
    return write_sector(1, sector) && write_superblock();
}

static bool begin_transaction(void)
{
    return write_journal(1);
}

static bool commit_transaction(void)
{
    return write_journal(0);
}

static bool inode_name_valid(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        return false;
    }
    for (uint32_t i = 0; i < sizeof(((struct diskfs_inode *) 0)->name); i++) {
        if (name[i] == '\0') {
            return true;
        }
    }
    return false;
}

static bool path_part(const char **cursor, char *part)
{
    const char *p = *cursor;
    uint32_t len = 0;
    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        return false;
    }
    while (*p != '\0' && *p != '/') {
        if (len + 1 >= DISKFS_NAME_MAX) {
            return false;
        }
        part[len++] = *p++;
    }
    part[len] = '\0';
    *cursor = p;
    return len > 0;
}

static int find_child(uint32_t parent, const char *name, uint8_t type_or_zero)
{
    for (uint32_t i = 0; i < superblock.inode_count && i < DISKFS_MAX_INODES; i++) {
        if (inodes[i].type != 0 && inodes[i].parent == parent &&
            (type_or_zero == 0 || inodes[i].type == type_or_zero) &&
            kstrcmp(inodes[i].name, name) == 0) {
            return (int) i;
        }
    }
    return -1;
}

static int find_path(const char *path)
{
    if (path == 0) {
        return -1;
    }
    const char *cursor = path;
    uint32_t current = DISKFS_ROOT_INODE;
    char part[DISKFS_NAME_MAX + 1];
    bool saw_part = false;
    while (path_part(&cursor, part)) {
        saw_part = true;
        int child = find_child(current, part, 0);
        if (child < 0) {
            return -1;
        }
        current = (uint32_t) child;
    }
    return saw_part ? (int) current : (int) DISKFS_ROOT_INODE;
}

static int ensure_parent_dir(const char *path, char *leaf)
{
    const char *cursor = path;
    uint32_t current = DISKFS_ROOT_INODE;
    char part[DISKFS_NAME_MAX + 1];
    char next[DISKFS_NAME_MAX + 1];
    if (!path_part(&cursor, part)) {
        return -1;
    }
    while (path_part(&cursor, next)) {
        int child = find_child(current, part, DISKFS_TYPE_DIR);
        if (child < 0) {
            return -1;
        }
        current = (uint32_t) child;
        mem_copy(part, next, DISKFS_NAME_MAX + 1);
    }
    mem_copy(leaf, part, DISKFS_NAME_MAX + 1);
    return (int) current;
}

static int alloc_inode(void)
{
    for (uint32_t i = 1; i < DISKFS_MAX_INODES; i++) {
        if (i >= superblock.inode_count) {
            superblock.inode_count = i + 1;
            return (int) i;
        }
        if (inodes[i].type == 0) {
            return (int) i;
        }
    }
    return -1;
}

static bool alloc_contiguous(uint32_t sectors, uint32_t *first)
{
    uint32_t run_start = superblock.data_start;
    uint32_t run_len = 0;
    for (uint32_t sector = superblock.data_start; sector < superblock.total_sectors; sector++) {
        if (!bitmap_get(sector)) {
            if (run_len == 0) {
                run_start = sector;
            }
            run_len++;
            if (run_len == sectors) {
                for (uint32_t i = 0; i < sectors; i++) {
                    bitmap_set(run_start + i, true);
                }
                *first = run_start;
                return true;
            }
        } else {
            run_len = 0;
        }
    }
    return false;
}

static void free_extent(uint32_t first, uint32_t sectors)
{
    for (uint32_t i = 0; i < sectors && first + i < superblock.total_sectors; i++) {
        bitmap_set(first + i, false);
    }
}

static uint32_t sectors_for_size(uint32_t size)
{
    uint32_t sectors = (size + DISKFS_SECTOR_SIZE - 1) / DISKFS_SECTOR_SIZE;
    return sectors == 0 ? 1 : sectors;
}

static bool write_file_data(uint32_t first_sector, const void *buffer, uint32_t size)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    const uint8_t *input = (const uint8_t *) buffer;
    uint32_t sectors = sectors_for_size(size);
    for (uint32_t s = 0; s < sectors; s++) {
        mem_zero(sector, sizeof(sector));
        uint32_t offset = s * DISKFS_SECTOR_SIZE;
        uint32_t chunk = size > offset ? size - offset : 0;
        if (chunk > DISKFS_SECTOR_SIZE) {
            chunk = DISKFS_SECTOR_SIZE;
        }
        if (chunk > 0) {
            mem_copy(sector, input + offset, chunk);
        }
        if (!write_sector(first_sector + s, sector)) {
            return false;
        }
    }
    return diskfs_flush();
}

static void rebuild_file_list(void)
{
    visible_file_count = 0;
    for (uint32_t i = 0; i < superblock.inode_count && i < DISKFS_MAX_INODES; i++) {
        if (inodes[i].type == DISKFS_TYPE_FILE) {
            file_indices[visible_file_count] = i;
            char *out = file_names[visible_file_count];
            uint32_t pos = 0;
            out[pos++] = '/';
            if (inodes[i].parent != DISKFS_ROOT_INODE && inodes[inodes[i].parent].type == DISKFS_TYPE_DIR) {
                const char *dir = inodes[inodes[i].parent].name;
                for (uint32_t j = 0; dir[j] != '\0' && pos + 1 < DISKFS_PATH_MAX; j++) {
                    out[pos++] = dir[j];
                }
                if (pos + 1 < DISKFS_PATH_MAX) {
                    out[pos++] = '/';
                }
            }
            for (uint32_t j = 0; inodes[i].name[j] != '\0' && pos + 1 < DISKFS_PATH_MAX; j++) {
                out[pos++] = inodes[i].name[j];
            }
            out[pos] = '\0';
            visible_file_count++;
        }
    }
}

bool diskfs_validate(void)
{
    if (!available || superblock.magic != DISKFS_MAGIC || superblock.total_sectors != DISKFS_TOTAL_SECTORS ||
        superblock.inode_count > DISKFS_MAX_INODES || inodes[DISKFS_ROOT_INODE].type != DISKFS_TYPE_DIR) {
        return false;
    }
    for (uint32_t i = 0; i < superblock.inode_count; i++) {
        if (inodes[i].type == 0) {
            continue;
        }
        if (i != DISKFS_ROOT_INODE && !inode_name_valid(inodes[i].name)) {
            return false;
        }
        if (i != DISKFS_ROOT_INODE && inodes[i].parent >= superblock.inode_count) {
            return false;
        }
        if (inodes[i].type == DISKFS_TYPE_FILE) {
            if (inodes[i].first_sector < superblock.data_start ||
                inodes[i].first_sector + inodes[i].sector_count > superblock.total_sectors ||
                inodes[i].sector_count != sectors_for_size(inodes[i].size)) {
                return false;
            }
        }
    }
    return true;
}

void diskfs_initialize(uint32_t start_lba)
{
    uint8_t sector[DISKFS_SECTOR_SIZE];
    fs_start_lba = start_lba;
    available = false;
    visible_file_count = 0;
    mem_zero(&superblock, sizeof(superblock));
    mem_zero(inodes, sizeof(inodes));
    mem_zero(bitmap, sizeof(bitmap));
    mem_zero(cache, sizeof(cache));

    if (!raw_read_sector(0, sector)) {
        return;
    }
    mem_copy(&superblock, sector, sizeof(superblock));
    if (superblock.magic != DISKFS_MAGIC || superblock.max_inodes > DISKFS_MAX_INODES ||
        superblock.bitmap_sectors != 1 || superblock.total_sectors != DISKFS_TOTAL_SECTORS) {
        return;
    }
    if (!raw_read_sector(superblock.bitmap_start, bitmap)) {
        return;
    }
    for (uint32_t s = 0; s < superblock.inode_sectors; s++) {
        if (!raw_read_sector(superblock.inode_start + s, sector)) {
            return;
        }
        uint32_t base = s * (DISKFS_SECTOR_SIZE / DISKFS_INODE_SIZE);
        for (uint32_t i = 0; i < DISKFS_SECTOR_SIZE / DISKFS_INODE_SIZE && base + i < DISKFS_MAX_INODES; i++) {
            mem_copy(&inodes[base + i], sector + i * DISKFS_INODE_SIZE, sizeof(struct diskfs_inode));
        }
    }

    available = true;
    if (!diskfs_validate()) {
        available = false;
        return;
    }
    if (superblock.journal_state != 0) {
        serial_writestring("MyOS diskfs: recovered from dirty metadata journal marker.\n");
        (void) write_journal(0);
    }
    rebuild_file_list();
    serial_writestring("MyOS diskfs: mounted v2 directory filesystem.\n");
}

bool diskfs_is_available(void)
{
    return available;
}

size_t diskfs_file_count(void)
{
    return visible_file_count;
}

const char *diskfs_file_name(size_t index)
{
    return index < visible_file_count ? file_names[index] : "";
}

uint32_t diskfs_file_size(size_t index)
{
    if (index >= visible_file_count) {
        return 0;
    }
    return inodes[file_indices[index]].size;
}

uint32_t diskfs_index_size(uint32_t index)
{
    if (index >= superblock.inode_count || inodes[index].type != DISKFS_TYPE_FILE) {
        return 0;
    }
    return inodes[index].size;
}

int diskfs_find_file(const char *path)
{
    int index = find_path(path);
    if (index >= 0 && inodes[index].type == DISKFS_TYPE_FILE) {
        return index;
    }
    if (path != 0 && path[0] != '/') {
        char full[DISKFS_PATH_MAX];
        full[0] = '/';
        uint32_t i = 0;
        for (; path[i] != '\0' && i + 2 < sizeof(full); i++) {
            full[i + 1] = path[i];
        }
        full[i + 1] = '\0';
        index = find_path(full);
        if (index >= 0 && inodes[index].type == DISKFS_TYPE_FILE) {
            return index;
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
    if (!available || index >= superblock.inode_count || inodes[index].type != DISKFS_TYPE_FILE || buffer == 0) {
        return false;
    }
    if (offset >= inodes[index].size || size == 0) {
        return true;
    }
    uint32_t remaining = inodes[index].size - offset;
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
        if (!read_sector(inodes[index].first_sector + sector_index, sector)) {
            return false;
        }
        mem_copy(out + done, sector + sector_offset, chunk);
        done += chunk;
    }
    if (bytes_read != 0) {
        *bytes_read = done;
    }
    return true;
}

bool diskfs_write_file(const char *path, const void *buffer, uint32_t size)
{
    char leaf[DISKFS_NAME_MAX + 1];
    if (!available || path == 0 || buffer == 0 || size > DISKFS_MAX_FILE_SIZE) {
        return false;
    }
    int parent = ensure_parent_dir(path, leaf);
    if (parent < 0 || !inode_name_valid(leaf)) {
        return false;
    }
    int existing = find_child((uint32_t) parent, leaf, DISKFS_TYPE_FILE);
    uint32_t sectors = sectors_for_size(size);
    uint32_t first = 0;

    if (!begin_transaction()) {
        return false;
    }
    if (existing >= 0) {
        uint32_t index = (uint32_t) existing;
        if (sectors > inodes[index].sector_count) {
            if (!alloc_contiguous(sectors, &first)) {
                (void) commit_transaction();
                return false;
            }
            free_extent(inodes[index].first_sector, inodes[index].sector_count);
            inodes[index].first_sector = first;
            inodes[index].sector_count = sectors;
        } else if (sectors < inodes[index].sector_count) {
            free_extent(inodes[index].first_sector + sectors, inodes[index].sector_count - sectors);
            inodes[index].sector_count = sectors;
        }
        inodes[index].size = size;
        if (!write_file_data(inodes[index].first_sector, buffer, size) || !write_inode(index) || !write_bitmap()) {
            return false;
        }
    } else {
        int index = alloc_inode();
        if (index < 0 || !alloc_contiguous(sectors, &first)) {
            (void) commit_transaction();
            return false;
        }
        mem_zero(&inodes[index], sizeof(struct diskfs_inode));
        inodes[index].type = DISKFS_TYPE_FILE;
        inodes[index].parent = (uint32_t) parent;
        inodes[index].size = size;
        inodes[index].first_sector = first;
        inodes[index].sector_count = sectors;
        for (uint32_t i = 0; leaf[i] != '\0' && i < DISKFS_NAME_MAX; i++) {
            inodes[index].name[i] = leaf[i];
        }
        if (!write_file_data(first, buffer, size) || !write_inode((uint32_t) index) ||
            !write_superblock() || !write_bitmap()) {
            return false;
        }
    }
    rebuild_file_list();
    return commit_transaction();
}

bool diskfs_delete_file(const char *path)
{
    int index = diskfs_find_file(path);
    if (index < 0 || !begin_transaction()) {
        return false;
    }
    free_extent(inodes[index].first_sector, inodes[index].sector_count);
    mem_zero(&inodes[index], sizeof(struct diskfs_inode));
    if (!write_inode((uint32_t) index) || !write_bitmap()) {
        return false;
    }
    rebuild_file_list();
    return commit_transaction();
}

bool diskfs_rename_file(const char *old_path, const char *new_path)
{
    char leaf[DISKFS_NAME_MAX + 1];
    int index = diskfs_find_file(old_path);
    int parent = ensure_parent_dir(new_path, leaf);
    if (index < 0 || parent < 0 || !inode_name_valid(leaf) ||
        find_child((uint32_t) parent, leaf, 0) >= 0 || !begin_transaction()) {
        return false;
    }
    inodes[index].parent = (uint32_t) parent;
    mem_zero(inodes[index].name, sizeof(inodes[index].name));
    for (uint32_t i = 0; leaf[i] != '\0' && i < DISKFS_NAME_MAX; i++) {
        inodes[index].name[i] = leaf[i];
    }
    if (!write_inode((uint32_t) index)) {
        return false;
    }
    rebuild_file_list();
    return commit_transaction();
}

bool diskfs_truncate_file(const char *path, uint32_t size)
{
    uint8_t zero = 0;
    if (size > DISKFS_MAX_FILE_SIZE) {
        return false;
    }
    int index = diskfs_find_file(path);
    if (index < 0) {
        return size == 0 ? diskfs_write_file(path, &zero, 0) : false;
    }
    if (size == inodes[index].size) {
        return true;
    }
    if (size == 0) {
        return diskfs_write_file(path, &zero, 0);
    }
    if (inodes[index].size > DISKFS_READ_BUFFER_SIZE || size > DISKFS_READ_BUFFER_SIZE) {
        return false;
    }
    uint32_t read = 0;
    mem_zero(file_buffer, size);
    (void) diskfs_read_index((uint32_t) index, 0, file_buffer, inodes[index].size, &read);
    return diskfs_write_file(path, file_buffer, size);
}

const char *diskfs_read_file(const char *name, size_t *size)
{
    int index = diskfs_find_file(name);
    if (index < 0 || inodes[index].size > DISKFS_READ_BUFFER_SIZE) {
        if (size != 0) {
            *size = 0;
        }
        return 0;
    }
    uint32_t bytes_read = 0;
    if (!diskfs_read_index((uint32_t) index, 0, file_buffer, inodes[index].size, &bytes_read)) {
        if (size != 0) {
            *size = 0;
        }
        return 0;
    }
    if (bytes_read < DISKFS_READ_BUFFER_SIZE) {
        file_buffer[bytes_read] = '\0';
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

static void append_report(char *report, size_t report_size, const char *text)
{
    if (report == 0 || report_size == 0) {
        return;
    }
    size_t pos = kstrlen(report);
    for (uint32_t i = 0; text[i] != '\0' && pos + 1 < report_size; i++) {
        report[pos++] = text[i];
    }
    report[pos] = '\0';
}

uint32_t diskfs_fsck(char *report, size_t report_size)
{
    uint8_t seen[DISKFS_BITMAP_BYTES];
    uint32_t errors = 0;
    if (report != 0 && report_size != 0) {
        report[0] = '\0';
    }
    mem_zero(seen, sizeof(seen));
    if (!available) {
        append_report(report, report_size, "diskfs unavailable\n");
        return 1;
    }
    for (uint32_t sector = 0; sector < superblock.data_start; sector++) {
        seen[sector / 8] |= (uint8_t) (1 << (sector % 8));
    }
    for (uint32_t i = 0; i < superblock.inode_count; i++) {
        if (inodes[i].type == DISKFS_TYPE_FILE) {
            if (inodes[i].first_sector < superblock.data_start ||
                inodes[i].first_sector + inodes[i].sector_count > superblock.total_sectors) {
                errors++;
                append_report(report, report_size, "file extent out of bounds\n");
                continue;
            }
            for (uint32_t s = 0; s < inodes[i].sector_count; s++) {
                uint32_t sector = inodes[i].first_sector + s;
                if ((seen[sector / 8] & (uint8_t) (1 << (sector % 8))) != 0) {
                    errors++;
                    append_report(report, report_size, "overlapping extent\n");
                }
                seen[sector / 8] |= (uint8_t) (1 << (sector % 8));
            }
        }
    }
    for (uint32_t sector = 0; sector < superblock.total_sectors; sector++) {
        bool expected = (seen[sector / 8] & (uint8_t) (1 << (sector % 8))) != 0;
        if (bitmap_get(sector) != expected) {
            errors++;
            append_report(report, report_size, "bitmap mismatch\n");
            break;
        }
    }
    if (errors == 0) {
        append_report(report, report_size, "diskfs fsck: clean\n");
    }
    return errors;
}
