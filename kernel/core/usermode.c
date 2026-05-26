#include "usermode.h"

#include "diskfs.h"
#include "paging.h"
#include "pmm.h"

extern int usermode_enter(uint32_t entry, uint32_t user_stack);

#define USER_PAGE_FLAGS 0x6
#define USER_PROGRAM_BASE 0x00400000
#define USER_IMAGE_PAGES 16
#define USER_STACK_PAGES 1
#define USER_STACK_BASE (USER_PROGRAM_BASE + USER_IMAGE_PAGES * PAGE_SIZE)
#define USER_PROCESS_NAME_SIZE 32
#define USER_PROCESS_COUNT 8
#define USER_PROCESS_FD_COUNT 8

enum user_process_state {
    USER_PROCESS_UNUSED,
    USER_PROCESS_RUNNING,
    USER_PROCESS_EXITED,
};

struct user_exec_header {
    uint32_t magic;
    uint32_t entry_offset;
    uint32_t image_size;
    uint32_t reserved;
};

struct elf32_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf32_program_header {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed));

struct user_process {
    uint32_t pid;
    enum user_process_state state;
    char name[USER_PROCESS_NAME_SIZE];
    uint32_t entry;
    int exit_code;
    struct address_space *address_space;
    struct {
        bool open;
        uint32_t file_index;
        uint32_t offset;
    } fds[USER_PROCESS_FD_COUNT];
};

static uint32_t next_pid = 1;
static struct user_process processes[USER_PROCESS_COUNT];
static int32_t current_process_index = -1;
static uint8_t load_image[USER_IMAGE_PAGES * PAGE_SIZE];

static void copy_process_name(char *dst, const char *src)
{
    uint32_t i = 0;
    for (; i + 1 < USER_PROCESS_NAME_SIZE && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static struct user_process *current_process(void)
{
    if (current_process_index < 0 || current_process_index >= USER_PROCESS_COUNT) {
        return 0;
    }

    return &processes[current_process_index];
}

static const char *process_state_name(enum user_process_state state)
{
    switch (state) {
    case USER_PROCESS_RUNNING:
        return "running";
    case USER_PROCESS_EXITED:
        return "exited";
    default:
        return "unused";
    }
}

static struct user_process *allocate_process(const char *name, uint32_t entry)
{
    for (uint32_t i = 0; i < USER_PROCESS_COUNT; i++) {
        if (processes[i].state == USER_PROCESS_UNUSED) {
            processes[i].pid = next_pid++;
            processes[i].state = USER_PROCESS_RUNNING;
            copy_process_name(processes[i].name, name);
            processes[i].entry = entry;
            processes[i].exit_code = 0;
            processes[i].address_space = 0;
            for (uint32_t fd = 0; fd < USER_PROCESS_FD_COUNT; fd++) {
                processes[i].fds[fd].open = false;
                processes[i].fds[fd].file_index = 0;
                processes[i].fds[fd].offset = 0;
            }
            current_process_index = (int32_t) i;
            return &processes[i];
        }
    }

    for (uint32_t i = 0; i < USER_PROCESS_COUNT; i++) {
        if (processes[i].state == USER_PROCESS_EXITED) {
            processes[i].pid = next_pid++;
            processes[i].state = USER_PROCESS_RUNNING;
            copy_process_name(processes[i].name, name);
            processes[i].entry = entry;
            processes[i].exit_code = 0;
            processes[i].address_space = 0;
            for (uint32_t fd = 0; fd < USER_PROCESS_FD_COUNT; fd++) {
                processes[i].fds[fd].open = false;
                processes[i].fds[fd].file_index = 0;
                processes[i].fds[fd].offset = 0;
            }
            current_process_index = (int32_t) i;
            return &processes[i];
        }
    }

    return 0;
}

static int run_loaded_image(const char *name, uint32_t entry, const uint8_t *image, uint32_t image_size)
{
    if (entry < USER_PROGRAM_BASE || entry >= USER_PROGRAM_BASE + USER_IMAGE_PAGES * PAGE_SIZE ||
        image_size > USER_IMAGE_PAGES * PAGE_SIZE) {
        return -21;
    }

    uint8_t *image_pages[USER_IMAGE_PAGES];
    uint8_t *stack_page = (uint8_t *) pmm_alloc_page();
    struct address_space *space = paging_create_address_space();
    for (uint32_t i = 0; i < USER_IMAGE_PAGES; i++) {
        image_pages[i] = 0;
    }

    if (stack_page == 0 || space == 0) {
        if (space != 0) {
            paging_destroy_address_space(space);
        }
        if (stack_page != 0) {
            pmm_free_page(stack_page);
        }
        return -22;
    }

    for (uint32_t i = 0; i < USER_IMAGE_PAGES; i++) {
        image_pages[i] = (uint8_t *) pmm_alloc_page();
        if (image_pages[i] == 0) {
            for (uint32_t j = 0; j < i; j++) {
                pmm_free_page(image_pages[j]);
            }
            paging_destroy_address_space(space);
            pmm_free_page(stack_page);
            return -22;
        }
    }

    for (uint32_t i = 0; i < USER_IMAGE_PAGES; i++) {
        if (!paging_map_page_in_space(space, USER_PROGRAM_BASE + i * PAGE_SIZE, (uintptr_t) image_pages[i], USER_PAGE_FLAGS)) {
            for (uint32_t j = 0; j < i; j++) {
                paging_unmap_page_in_space(space, USER_PROGRAM_BASE + j * PAGE_SIZE);
            }
            for (uint32_t j = 0; j < USER_IMAGE_PAGES; j++) {
                pmm_free_page(image_pages[j]);
            }
            paging_destroy_address_space(space);
            pmm_free_page(stack_page);
            return -23;
        }
    }

    if (!paging_map_page_in_space(space, USER_STACK_BASE, (uintptr_t) stack_page, USER_PAGE_FLAGS)) {
        for (uint32_t i = 0; i < USER_IMAGE_PAGES; i++) {
            paging_unmap_page_in_space(space, USER_PROGRAM_BASE + i * PAGE_SIZE);
            pmm_free_page(image_pages[i]);
        }
        paging_destroy_address_space(space);
        pmm_free_page(stack_page);
        return -24;
    }

    for (uint32_t page = 0; page < USER_IMAGE_PAGES; page++) {
        for (uint32_t i = 0; i < PAGE_SIZE; i++) {
            image_pages[page][i] = 0;
        }
    }
    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
        stack_page[i] = 0;
    }

    for (uint32_t i = 0; i < image_size; i++) {
        image_pages[i / PAGE_SIZE][i % PAGE_SIZE] = image[i];
    }

    struct user_process *process = allocate_process(name, entry);
    if (process == 0) {
        paging_unmap_page_in_space(space, USER_STACK_BASE);
        for (uint32_t i = 0; i < USER_IMAGE_PAGES; i++) {
            paging_unmap_page_in_space(space, USER_PROGRAM_BASE + i * PAGE_SIZE);
            pmm_free_page(image_pages[i]);
        }
        paging_destroy_address_space(space);
        pmm_free_page(stack_page);
        return -25;
    }

    process->address_space = space;
    paging_switch_address_space(space);
    int exit_code = usermode_enter(entry, USER_STACK_BASE + PAGE_SIZE);
    paging_switch_address_space(paging_kernel_address_space());
    process->exit_code = exit_code;
    process->state = USER_PROCESS_EXITED;
    user_process_close_all();

    paging_unmap_page_in_space(space, USER_STACK_BASE);
    pmm_free_page(stack_page);

    for (uint32_t i = 0; i < USER_IMAGE_PAGES; i++) {
        paging_unmap_page_in_space(space, USER_PROGRAM_BASE + i * PAGE_SIZE);
        pmm_free_page(image_pages[i]);
    }
    paging_destroy_address_space(space);
    process->address_space = 0;

    current_process_index = -1;
    return exit_code;
}

static int run_mexe_program(const char *name, const uint8_t *file, size_t file_size)
{
    if (file_size < sizeof(struct user_exec_header)) {
        return -10;
    }

    const struct user_exec_header *header = (const struct user_exec_header *) file;
    if (header->magic != 0x4558454D || header->entry_offset >= header->image_size ||
        header->image_size > USER_IMAGE_PAGES * PAGE_SIZE) {
        return -11;
    }

    if (sizeof(struct user_exec_header) + header->image_size > file_size) {
        return -12;
    }

    return run_loaded_image(name, USER_PROGRAM_BASE + header->entry_offset, file + sizeof(struct user_exec_header), header->image_size);
}

static int run_elf_program(const char *name, const uint8_t *file, size_t file_size)
{
    if (file_size < sizeof(struct elf32_header)) {
        return -30;
    }

    const struct elf32_header *elf = (const struct elf32_header *) file;
    if (elf->ident[0] != 0x7F || elf->ident[1] != 'E' || elf->ident[2] != 'L' || elf->ident[3] != 'F') {
        return -31;
    }
    if (elf->ident[4] != 1 || elf->ident[5] != 1 || elf->type != 2 || elf->machine != 3 ||
        elf->version != 1 || elf->phentsize != sizeof(struct elf32_program_header)) {
        return -32;
    }
    if (elf->phnum == 0 || elf->phoff + elf->phnum * sizeof(struct elf32_program_header) > file_size) {
        return -33;
    }

    if (elf->entry < USER_PROGRAM_BASE || elf->entry >= USER_PROGRAM_BASE + USER_IMAGE_PAGES * PAGE_SIZE) {
        return -37;
    }

    for (uint32_t i = 0; i < USER_IMAGE_PAGES * PAGE_SIZE; i++) {
        load_image[i] = 0;
    }

    bool loaded = false;
    for (uint32_t i = 0; i < elf->phnum; i++) {
        const struct elf32_program_header *ph =
            (const struct elf32_program_header *) (file + elf->phoff + i * sizeof(struct elf32_program_header));
        if (ph->type != 1) {
            continue;
        }
        if (ph->vaddr < USER_PROGRAM_BASE || ph->vaddr + ph->memsz < ph->vaddr ||
            ph->vaddr + ph->memsz > USER_PROGRAM_BASE + USER_IMAGE_PAGES * PAGE_SIZE) {
            return -34;
        }
        if (ph->offset + ph->filesz > file_size || ph->filesz > ph->memsz) {
            return -35;
        }

        uint32_t destination = ph->vaddr - USER_PROGRAM_BASE;
        for (uint32_t j = 0; j < ph->filesz; j++) {
            load_image[destination + j] = file[ph->offset + j];
        }
        loaded = true;
    }

    if (!loaded) {
        return -36;
    }

    return run_loaded_image(name, elf->entry, load_image, USER_IMAGE_PAGES * PAGE_SIZE);
}

static void write_u32(uint8_t *buffer, uint32_t offset, uint32_t value)
{
    buffer[offset] = (uint8_t) (value & 0xFF);
    buffer[offset + 1] = (uint8_t) ((value >> 8) & 0xFF);
    buffer[offset + 2] = (uint8_t) ((value >> 16) & 0xFF);
    buffer[offset + 3] = (uint8_t) ((value >> 24) & 0xFF);
}

int usermode_run_test(void)
{
    uint8_t *code_page = (uint8_t *) pmm_alloc_page();
    uint8_t *stack_page = (uint8_t *) pmm_alloc_page();
    if (code_page == 0 || stack_page == 0) {
        return -1;
    }

    if (!paging_map_page((uintptr_t) code_page, (uintptr_t) code_page, USER_PAGE_FLAGS)) {
        return -2;
    }
    if (!paging_map_page((uintptr_t) stack_page, (uintptr_t) stack_page, USER_PAGE_FLAGS)) {
        return -3;
    }

    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
        code_page[i] = 0;
        stack_page[i] = 0;
    }

    uint32_t message_offset = 32;
    const char message[] = "Hello from ring 3 user mode via int 0x80.\n";
    for (uint32_t i = 0; i < sizeof(message) - 1; i++) {
        code_page[message_offset + i] = message[i];
    }

    uint32_t message_address = (uint32_t) (code_page + message_offset);
    uint32_t i = 0;
    code_page[i++] = 0xB8;
    write_u32(code_page, i, 1);
    i += 4;
    code_page[i++] = 0xBB;
    write_u32(code_page, i, message_address);
    i += 4;
    code_page[i++] = 0xB9;
    write_u32(code_page, i, sizeof(message) - 1);
    i += 4;
    code_page[i++] = 0xCD;
    code_page[i++] = 0x80;
    code_page[i++] = 0xB8;
    write_u32(code_page, i, 2);
    i += 4;
    code_page[i++] = 0xBB;
    write_u32(code_page, i, 42);
    i += 4;
    code_page[i++] = 0xCD;
    code_page[i++] = 0x80;
    code_page[i++] = 0xEB;
    code_page[i++] = 0xFE;

    int exit_code = usermode_enter((uint32_t) code_page, (uint32_t) (stack_page + PAGE_SIZE));

    paging_map_page((uintptr_t) code_page, (uintptr_t) code_page, 0x2);
    paging_map_page((uintptr_t) stack_page, (uintptr_t) stack_page, 0x2);
    pmm_free_page(code_page);
    pmm_free_page(stack_page);
    return exit_code;
}

int usermode_run_program(const char *name)
{
    size_t file_size;
    const uint8_t *file = (const uint8_t *) diskfs_read_file(name, &file_size);
    if (file == 0 || file_size < 4) {
        return -10;
    }

    if (file[0] == 0x7F && file[1] == 'E' && file[2] == 'L' && file[3] == 'F') {
        return run_elf_program(name, file, file_size);
    }

    return run_mexe_program(name, file, file_size);
}

uint32_t user_process_count(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < USER_PROCESS_COUNT; i++) {
        if (processes[i].state != USER_PROCESS_UNUSED) {
            count++;
        }
    }
    return count;
}

static struct user_process *process_by_visible_index(uint32_t index)
{
    uint32_t visible = 0;
    for (uint32_t i = 0; i < USER_PROCESS_COUNT; i++) {
        if (processes[i].state == USER_PROCESS_UNUSED) {
            continue;
        }
        if (visible == index) {
            return &processes[i];
        }
        visible++;
    }
    return 0;
}

uint32_t user_process_pid(uint32_t index)
{
    struct user_process *process = process_by_visible_index(index);
    return process == 0 ? 0 : process->pid;
}

const char *user_process_name(uint32_t index)
{
    struct user_process *process = process_by_visible_index(index);
    return process == 0 ? "" : process->name;
}

const char *user_process_state(uint32_t index)
{
    struct user_process *process = process_by_visible_index(index);
    return process == 0 ? "" : process_state_name(process->state);
}

int user_process_exit_code(uint32_t index)
{
    struct user_process *process = process_by_visible_index(index);
    return process == 0 ? 0 : process->exit_code;
}

int user_process_wait_pid(uint32_t pid)
{
    for (uint32_t i = 0; i < USER_PROCESS_COUNT; i++) {
        if (processes[i].state != USER_PROCESS_UNUSED && processes[i].pid == pid) {
            if (processes[i].state == USER_PROCESS_EXITED) {
                return processes[i].exit_code;
            }
            return -2;
        }
    }

    return -1;
}

uint32_t user_process_reap_exited(void)
{
    uint32_t reaped = 0;
    for (uint32_t i = 0; i < USER_PROCESS_COUNT; i++) {
        if (processes[i].state == USER_PROCESS_EXITED) {
            processes[i].state = USER_PROCESS_UNUSED;
            processes[i].pid = 0;
            processes[i].name[0] = '\0';
            processes[i].entry = 0;
            processes[i].exit_code = 0;
            processes[i].address_space = 0;
            reaped++;
        }
    }
    return reaped;
}

uint32_t user_process_current_pid(void)
{
    struct user_process *process = current_process();
    return process == 0 ? 0 : process->pid;
}

int user_process_fd_open(const char *path)
{
    struct user_process *process = current_process();
    if (process == 0) {
        return -1;
    }

    int file_index = diskfs_find_file(path);
    if (file_index < 0) {
        return -2;
    }

    for (uint32_t fd = 0; fd < USER_PROCESS_FD_COUNT; fd++) {
        if (!process->fds[fd].open) {
            process->fds[fd].open = true;
            process->fds[fd].file_index = (uint32_t) file_index;
            process->fds[fd].offset = 0;
            return (int) fd;
        }
    }

    return -3;
}

int user_process_fd_read(uint32_t fd, char *buffer, uint32_t size)
{
    struct user_process *process = current_process();
    if (process == 0 || fd >= USER_PROCESS_FD_COUNT || !process->fds[fd].open) {
        return -1;
    }

    uint32_t bytes_read;
    if (!diskfs_read_index(process->fds[fd].file_index, process->fds[fd].offset, buffer, size, &bytes_read)) {
        return -2;
    }

    process->fds[fd].offset += bytes_read;
    return (int) bytes_read;
}

int user_process_fd_close(uint32_t fd)
{
    struct user_process *process = current_process();
    if (process == 0 || fd >= USER_PROCESS_FD_COUNT || !process->fds[fd].open) {
        return -1;
    }

    process->fds[fd].open = false;
    return 0;
}

void user_process_close_all(void)
{
    struct user_process *process = current_process();
    if (process == 0) {
        return;
    }

    for (uint32_t fd = 0; fd < USER_PROCESS_FD_COUNT; fd++) {
        process->fds[fd].open = false;
    }
}
