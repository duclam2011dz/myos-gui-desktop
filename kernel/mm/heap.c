#include "heap.h"

#include "pmm.h"

#include <stdbool.h>
#include <stdint.h>

#define HEAP_MAGIC 0xC0DEF00D
#define HEAP_ALIGN_MAGIC 0xA11A11A1
#define HEAP_MIN_SPLIT 32

struct heap_block {
    uint32_t magic;
    size_t size;
    uint32_t page_count;
    bool free;
    struct heap_block *next;
    struct heap_block *prev;
};

struct aligned_header {
    uint32_t magic;
    void *raw;
};

static struct heap_block *heap_head;
static size_t heap_total;
static size_t heap_used;
static uint32_t invalid_free_count;
static uint32_t double_free_count;

static size_t align_up_size(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static void block_from_pages(void *page, uint32_t page_count)
{
    struct heap_block *block = (struct heap_block *) page;
    size_t region_size = (size_t) page_count * PMM_PAGE_SIZE;

    block->magic = HEAP_MAGIC;
    block->size = region_size - sizeof(struct heap_block);
    block->page_count = page_count;
    block->free = true;
    block->next = 0;
    block->prev = 0;

    if (heap_head == 0) {
        heap_head = block;
    } else {
        struct heap_block *tail = heap_head;
        while (tail->next != 0) {
            tail = tail->next;
        }
        tail->next = block;
        block->prev = tail;
    }

    heap_total += block->size;
}

static struct heap_block *find_free_block(size_t size)
{
    struct heap_block *block = heap_head;
    while (block != 0) {
        if (block->free && block->size >= size) {
            return block;
        }
        block = block->next;
    }
    return 0;
}

static void split_block(struct heap_block *block, size_t size)
{
    if (block->size < size + sizeof(struct heap_block) + HEAP_MIN_SPLIT) {
        return;
    }

    struct heap_block *next = (struct heap_block *) ((uint8_t *) (block + 1) + size);
    next->magic = HEAP_MAGIC;
    next->size = block->size - size - sizeof(struct heap_block);
    next->free = true;
    next->next = block->next;
    next->prev = block;

    if (next->next != 0) {
        next->next->prev = next;
    }

    block->next = next;
    block->size = size;
    heap_total -= sizeof(struct heap_block);
}

static void coalesce_next(struct heap_block *block)
{
    struct heap_block *next = block->next;
    if (next == 0 || !next->free) {
        return;
    }

    uint8_t *block_end = (uint8_t *) (block + 1) + block->size;
    if (block_end != (uint8_t *) next) {
        return;
    }

    block->size += sizeof(struct heap_block) + next->size;
    block->next = next->next;
    if (block->next != 0) {
        block->next->prev = block;
    }
    heap_total += sizeof(struct heap_block);
}

static void unlink_block(struct heap_block *block)
{
    if (block->prev != 0) {
        block->prev->next = block->next;
    } else {
        heap_head = block->next;
    }

    if (block->next != 0) {
        block->next->prev = block->prev;
    }
}

static void release_whole_region_if_possible(struct heap_block *block)
{
    if (!block->free || block->page_count == 0) {
        return;
    }

    size_t region_payload = (size_t) block->page_count * PMM_PAGE_SIZE - sizeof(struct heap_block);
    if (block->size != region_payload) {
        return;
    }

    unlink_block(block);
    heap_total -= block->size;
    pmm_free_pages_block(block, block->page_count);
}

void heap_initialize(void)
{
    heap_head = 0;
    heap_total = 0;
    heap_used = 0;
    invalid_free_count = 0;
    double_free_count = 0;

    void *page = pmm_alloc_page();
    if (page != 0) {
        block_from_pages(page, 1);
    }
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return 0;
    }

    size = align_up_size(size, 8);
    struct heap_block *block = find_free_block(size);

    while (block == 0) {
        uint32_t pages_needed = (uint32_t) ((size + sizeof(struct heap_block) + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE);
        void *pages = pmm_alloc_pages(pages_needed);
        if (pages == 0) {
            return 0;
        }
        block_from_pages(pages, pages_needed);
        block = find_free_block(size);
    }

    split_block(block, size);
    block->free = false;
    heap_used += block->size;
    return block + 1;
}

void *kmalloc_aligned(size_t size, size_t alignment)
{
    if (alignment < sizeof(void *)) {
        alignment = sizeof(void *);
    }

    size_t total = size + alignment + sizeof(struct aligned_header);
    uint8_t *raw = (uint8_t *) kmalloc(total);
    if (raw == 0) {
        return 0;
    }

    uintptr_t aligned = ((uintptr_t) raw + sizeof(struct aligned_header) + alignment - 1) & ~(alignment - 1);
    struct aligned_header *header = ((struct aligned_header *) aligned) - 1;
    header->magic = HEAP_ALIGN_MAGIC;
    header->raw = raw;
    return (void *) aligned;
}

void kfree(void *ptr)
{
    if (ptr == 0) {
        return;
    }

    struct aligned_header *aligned = ((struct aligned_header *) ptr) - 1;
    if (aligned->magic == HEAP_ALIGN_MAGIC) {
        aligned->magic = 0;
        kfree(aligned->raw);
        return;
    }

    struct heap_block *block = ((struct heap_block *) ptr) - 1;
    if (block->magic != HEAP_MAGIC) {
        invalid_free_count++;
        return;
    }
    if (block->free) {
        double_free_count++;
        return;
    }

    block->free = true;
    heap_used -= block->size;

    coalesce_next(block);
    if (block->prev != 0 && block->prev->free) {
        block = block->prev;
        coalesce_next(block);
    }
    release_whole_region_if_possible(block);
}

size_t heap_total_bytes(void)
{
    return heap_total;
}

size_t heap_used_bytes(void)
{
    return heap_used;
}

size_t heap_free_bytes(void)
{
    return heap_total - heap_used;
}

uint32_t heap_invalid_free_count(void)
{
    return invalid_free_count;
}

uint32_t heap_double_free_count(void)
{
    return double_free_count;
}
