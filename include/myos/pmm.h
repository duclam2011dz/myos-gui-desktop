#ifndef MYOS_PMM_H
#define MYOS_PMM_H

#include <stddef.h>
#include <stdint.h>

#define PMM_PAGE_SIZE 4096
#define E820_MEMORY_USABLE 1
#define E820_MAX_ENTRIES 32

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} __attribute__((packed));

void pmm_initialize(const struct e820_entry *entries, uint32_t entry_count);
uint32_t pmm_e820_entries(void);
void *pmm_alloc_page(void);
void *pmm_alloc_pages(uint32_t count);
void pmm_free_page(void *page);
void pmm_free_pages_block(void *page, uint32_t count);
uint32_t pmm_total_pages(void);
uint32_t pmm_used_pages(void);
uint32_t pmm_free_pages(void);
uintptr_t pmm_managed_start(void);
uintptr_t pmm_managed_end(void);

#endif
