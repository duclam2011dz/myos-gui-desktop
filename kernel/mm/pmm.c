#include "pmm.h"

#include "paging.h"

#include <stdbool.h>

extern uint8_t _kernel_end;

#define PMM_MANAGED_LIMIT PAGING_IDENTITY_LIMIT
#define PMM_MAX_PAGES (PMM_MANAGED_LIMIT / PMM_PAGE_SIZE)

static uint32_t page_bitmap[PMM_MAX_PAGES / 32];
static uintptr_t managed_start;
static uintptr_t managed_end;
static uint32_t total_pages_count;
static uint32_t used_pages_count;
static uint32_t e820_entry_count;

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static void bitmap_set(uint32_t page_index)
{
    page_bitmap[page_index / 32] |= 1u << (page_index % 32);
}

static void bitmap_clear(uint32_t page_index)
{
    page_bitmap[page_index / 32] &= ~(1u << (page_index % 32));
}

static bool bitmap_test(uint32_t page_index)
{
    return (page_bitmap[page_index / 32] & (1u << (page_index % 32))) != 0;
}

static void mark_usable_region(uintptr_t start, uintptr_t end)
{
    start = align_up(start, PMM_PAGE_SIZE);
    end &= ~(PMM_PAGE_SIZE - 1);

    if (start < managed_start) {
        start = managed_start;
    }
    if (end > PMM_MANAGED_LIMIT) {
        end = PMM_MANAGED_LIMIT;
    }
    if (end <= start) {
        return;
    }

    if (end > managed_end) {
        managed_end = end;
    }

    uint32_t first_page = (uint32_t) (start / PMM_PAGE_SIZE);
    uint32_t last_page = (uint32_t) (end / PMM_PAGE_SIZE);

    for (uint32_t page = first_page; page < last_page; page++) {
        if (bitmap_test(page)) {
            bitmap_clear(page);
            total_pages_count++;
        }
    }
}

void pmm_initialize(const struct e820_entry *entries, uint32_t entry_count)
{
    for (uint32_t i = 0; i < PMM_MAX_PAGES / 32; i++) {
        page_bitmap[i] = 0xFFFFFFFFu;
    }

    managed_start = align_up((uintptr_t) &_kernel_end, PMM_PAGE_SIZE);
    managed_end = managed_start;
    total_pages_count = 0;
    used_pages_count = 0;
    e820_entry_count = entry_count;

    for (uint32_t i = 0; i < entry_count && i < E820_MAX_ENTRIES; i++) {
        if (entries[i].type != E820_MEMORY_USABLE || entries[i].length == 0) {
            continue;
        }
        if (entries[i].base >= PMM_MANAGED_LIMIT) {
            continue;
        }
        uint64_t end64 = entries[i].base + entries[i].length;
        uintptr_t start = (uintptr_t) entries[i].base;
        uintptr_t end = end64 > PMM_MANAGED_LIMIT ? PMM_MANAGED_LIMIT : (uintptr_t) end64;
        mark_usable_region(start, end);
    }

    if (total_pages_count == 0) {
        mark_usable_region(managed_start, 0x00400000);
    }
}

uint32_t pmm_e820_entries(void)
{
    return e820_entry_count;
}

void *pmm_alloc_page(void)
{
    return pmm_alloc_pages(1);
}

void *pmm_alloc_pages(uint32_t count)
{
    uint32_t first_page = (uint32_t) (managed_start / PMM_PAGE_SIZE);
    uint32_t last_page = (uint32_t) (managed_end / PMM_PAGE_SIZE);

    if (count == 0 || count > total_pages_count) {
        return 0;
    }

    for (uint32_t page = first_page; page < last_page; page++) {
        uint32_t run = 0;
        while (page + run < last_page && !bitmap_test(page + run) && run < count) {
            run++;
        }

        if (run == count) {
            for (uint32_t i = 0; i < count; i++) {
                bitmap_set(page + i);
            }
            used_pages_count += count;
            return (void *) (page * PMM_PAGE_SIZE);
        }

        if (run > 0) {
            page += run;
        }
    }

    return 0;
}

void pmm_free_page(void *page)
{
    pmm_free_pages_block(page, 1);
}

void pmm_free_pages_block(void *page, uint32_t count)
{
    uintptr_t address = (uintptr_t) page;

    if (count == 0 || address < managed_start || address >= managed_end || (address % PMM_PAGE_SIZE) != 0) {
        return;
    }

    if (address + count * PMM_PAGE_SIZE > managed_end) {
        return;
    }

    uint32_t first_page = (uint32_t) (address / PMM_PAGE_SIZE);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t page_index = first_page + i;
        if (bitmap_test(page_index)) {
            bitmap_clear(page_index);
            used_pages_count--;
        }
    }
}

uint32_t pmm_total_pages(void)
{
    return total_pages_count;
}

uint32_t pmm_used_pages(void)
{
    return used_pages_count;
}

uint32_t pmm_free_pages(void)
{
    return total_pages_count - used_pages_count;
}

uintptr_t pmm_managed_start(void)
{
    return managed_start;
}

uintptr_t pmm_managed_end(void)
{
    return managed_end;
}
