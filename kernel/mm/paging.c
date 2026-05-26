#include "paging.h"

#include "pmm.h"

#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER 0x4
#define PAGE_ENTRIES 1024
#define PAGE_TABLE_COUNT (PAGING_IDENTITY_LIMIT / (PAGE_SIZE * PAGE_ENTRIES))
#define FRAMEBUFFER_PAGE_TABLES 8

static uint32_t kernel_page_directory[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static uint32_t identity_page_tables[PAGE_TABLE_COUNT][PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static uint32_t framebuffer_page_tables[FRAMEBUFFER_PAGE_TABLES][PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static struct address_space kernel_space;
static struct address_space *current_space;
static uintptr_t boot_framebuffer_physical;
static uint32_t boot_framebuffer_size;

static void zero_page_table(uint32_t *table)
{
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        table[i] = 0;
    }
}

static void copy_page_table(uint32_t *dst, const uint32_t *src)
{
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        dst[i] = src[i];
    }
}

static void invalidate_page(uintptr_t virtual_address)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

static void load_cr3(uint32_t *page_directory)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory) : "memory");
}

void paging_initialize(void)
{
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        kernel_page_directory[i] = PAGE_WRITABLE;
        kernel_space.owned_tables[i] = 0;
    }

    for (uint32_t table = 0; table < PAGE_TABLE_COUNT; table++) {
        for (uint32_t entry = 0; entry < PAGE_ENTRIES; entry++) {
            uint32_t address = (table * PAGE_ENTRIES + entry) * PAGE_SIZE;
            identity_page_tables[table][entry] = address | PAGE_PRESENT | PAGE_WRITABLE;
        }
        kernel_page_directory[table] = ((uint32_t) identity_page_tables[table]) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    if (boot_framebuffer_physical != 0 && boot_framebuffer_size != 0) {
        uintptr_t start = boot_framebuffer_physical & ~(PAGE_SIZE - 1);
        uintptr_t end = (boot_framebuffer_physical + boot_framebuffer_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint32_t table_slots_used = 0;

        for (uintptr_t address = start; address < end;) {
            uint32_t directory_index = (uint32_t) (address >> 22);
            uint32_t table_index = (uint32_t) ((address >> 12) & 0x3FF);

            if ((kernel_page_directory[directory_index] & PAGE_PRESENT) == 0) {
                if (table_slots_used >= FRAMEBUFFER_PAGE_TABLES) {
                    break;
                }
                zero_page_table(framebuffer_page_tables[table_slots_used]);
                kernel_page_directory[directory_index] =
                    ((uint32_t) framebuffer_page_tables[table_slots_used]) | PAGE_PRESENT | PAGE_WRITABLE;
                table_slots_used++;
            }

            uint32_t *table = (uint32_t *) (kernel_page_directory[directory_index] & ~(PAGE_SIZE - 1));
            for (; table_index < PAGE_ENTRIES && address < end; table_index++, address += PAGE_SIZE) {
                table[table_index] = (uint32_t) address | PAGE_PRESENT | PAGE_WRITABLE;
            }
        }
    }

    kernel_space.page_directory = kernel_page_directory;
    current_space = &kernel_space;
    load_cr3(kernel_page_directory);

    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

void paging_set_boot_framebuffer(uintptr_t physical_address, uint32_t size)
{
    boot_framebuffer_physical = physical_address;
    boot_framebuffer_size = size;
}

bool paging_is_enabled(void)
{
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000) != 0;
}

struct address_space *paging_kernel_address_space(void)
{
    return &kernel_space;
}

struct address_space *paging_current_address_space(void)
{
    return current_space;
}

struct address_space *paging_create_address_space(void)
{
    struct address_space *space = (struct address_space *) pmm_alloc_page();
    if (space == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        space->owned_tables[i] = 0;
    }

    uint32_t *directory = (uint32_t *) pmm_alloc_page();
    if (directory == 0) {
        pmm_free_page(space);
        return 0;
    }

    copy_page_table(directory, kernel_page_directory);
    space->page_directory = directory;
    return space;
}

void paging_destroy_address_space(struct address_space *space)
{
    if (space == 0 || space == &kernel_space) {
        return;
    }

    if (current_space == space) {
        paging_switch_address_space(&kernel_space);
    }

    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        if (space->owned_tables[i] != 0) {
            pmm_free_page(space->owned_tables[i]);
            space->owned_tables[i] = 0;
        }
    }

    if (space->page_directory != 0) {
        pmm_free_page(space->page_directory);
    }
    pmm_free_page(space);
}

void paging_switch_address_space(struct address_space *space)
{
    if (space == 0 || space->page_directory == 0) {
        return;
    }

    current_space = space;
    load_cr3(space->page_directory);
}

static uint32_t *ensure_table(struct address_space *space, uint32_t directory_index, uint32_t flags)
{
    uint32_t *table;
    uint32_t *directory = space->page_directory;

    if ((directory[directory_index] & PAGE_PRESENT) == 0) {
        table = (uint32_t *) pmm_alloc_page();
        if (table == 0) {
            return 0;
        }
        zero_page_table(table);
        space->owned_tables[directory_index] = table;
        directory[directory_index] = ((uintptr_t) table) | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
        return table;
    }

    table = (uint32_t *) (directory[directory_index] & ~(PAGE_SIZE - 1));
    if (space != &kernel_space && space->owned_tables[directory_index] == 0) {
        uint32_t *private_table = (uint32_t *) pmm_alloc_page();
        if (private_table == 0) {
            return 0;
        }
        copy_page_table(private_table, table);
        table = private_table;
        space->owned_tables[directory_index] = table;
        directory[directory_index] = ((uintptr_t) table) | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else {
        directory[directory_index] |= flags & (PAGE_WRITABLE | PAGE_USER);
    }

    return table;
}

bool paging_map_page_in_space(struct address_space *space, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags)
{
    if (space == 0 || space->page_directory == 0) {
        return false;
    }

    virtual_address &= ~(PAGE_SIZE - 1);
    physical_address &= ~(PAGE_SIZE - 1);

    uint32_t directory_index = (uint32_t) (virtual_address >> 22);
    uint32_t table_index = (uint32_t) ((virtual_address >> 12) & 0x3FF);
    uint32_t *table = ensure_table(space, directory_index, flags);
    if (table == 0) {
        return false;
    }

    table[table_index] = physical_address | PAGE_PRESENT | flags;
    if (space == current_space) {
        invalidate_page(virtual_address);
    }
    return true;
}

bool paging_map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags)
{
    return paging_map_page_in_space(current_space, virtual_address, physical_address, flags);
}

void paging_unmap_page_in_space(struct address_space *space, uintptr_t virtual_address)
{
    if (space == 0 || space->page_directory == 0) {
        return;
    }

    uint32_t directory_index = (uint32_t) (virtual_address >> 22);
    uint32_t table_index = (uint32_t) ((virtual_address >> 12) & 0x3FF);

    if ((space->page_directory[directory_index] & PAGE_PRESENT) == 0) {
        return;
    }

    uint32_t *table = (uint32_t *) (space->page_directory[directory_index] & ~(PAGE_SIZE - 1));
    table[table_index] = 0;
    if (space == current_space) {
        invalidate_page(virtual_address);
    }
}

void paging_unmap_page(uintptr_t virtual_address)
{
    paging_unmap_page_in_space(current_space, virtual_address);
}

uintptr_t paging_get_physical_in_space(struct address_space *space, uintptr_t virtual_address)
{
    if (space == 0 || space->page_directory == 0) {
        return 0;
    }

    uint32_t directory_index = (uint32_t) (virtual_address >> 22);
    uint32_t table_index = (uint32_t) ((virtual_address >> 12) & 0x3FF);

    if ((space->page_directory[directory_index] & PAGE_PRESENT) == 0) {
        return 0;
    }

    uint32_t *table = (uint32_t *) (space->page_directory[directory_index] & ~(PAGE_SIZE - 1));
    if ((table[table_index] & PAGE_PRESENT) == 0) {
        return 0;
    }

    return (table[table_index] & ~(PAGE_SIZE - 1)) | (virtual_address & (PAGE_SIZE - 1));
}

uintptr_t paging_get_physical(uintptr_t virtual_address)
{
    return paging_get_physical_in_space(current_space, virtual_address);
}
