#ifndef MYOS_PAGING_H
#define MYOS_PAGING_H

#include <stdbool.h>
#include <stdint.h>

#define PAGING_IDENTITY_LIMIT 0x08000000
#define PAGE_SIZE 4096

struct address_space {
    uint32_t *page_directory;
    uint32_t *owned_tables[1024];
};

void paging_initialize(void);
void paging_set_boot_framebuffer(uintptr_t physical_address, uint32_t size);
bool paging_is_enabled(void);
struct address_space *paging_kernel_address_space(void);
struct address_space *paging_create_address_space(void);
void paging_destroy_address_space(struct address_space *space);
void paging_switch_address_space(struct address_space *space);
struct address_space *paging_current_address_space(void);
bool paging_map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
bool paging_map_page_in_space(struct address_space *space, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
void paging_unmap_page(uintptr_t virtual_address);
void paging_unmap_page_in_space(struct address_space *space, uintptr_t virtual_address);
uintptr_t paging_get_physical(uintptr_t virtual_address);
uintptr_t paging_get_physical_in_space(struct address_space *space, uintptr_t virtual_address);

#endif
