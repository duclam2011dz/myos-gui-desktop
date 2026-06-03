TARGET ?= i686-linux-gnu
CROSS_COMPILE ?= $(TARGET)-

AS := nasm
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
QEMU := qemu-system-i386
QEMU_DISPLAY ?= gtk,show-cursor=off
GDB ?= gdb
CROSS_CXX := $(shell command -v $(CROSS_COMPILE)g++ 2>/dev/null)
HOST_CXX := $(shell command -v g++ 2>/dev/null)
CXX := $(if $(CROSS_CXX),$(CROSS_COMPILE)g++,$(HOST_CXX))
CXX_TARGET_FLAGS := $(if $(CROSS_CXX),,-m32)
CROSS_GCC_INCLUDE := $(shell $(CC) -print-file-name=include)

BUILD_DIR := build
STAGE2_SECTORS := 4
KERNEL_SECTORS := 288
FS_SECTORS := 4096
FS_START_LBA := $(shell echo $$((1 + $(STAGE2_SECTORS) + $(KERNEL_SECTORS))))
BOOT_BIN := $(BUILD_DIR)/boot.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
FS_IMG := $(BUILD_DIR)/fs.img
OS_IMAGE := $(BUILD_DIR)/myos.img
ASSET_MANIFEST := assets/manifest.json
ASSET_FILES := $(BUILD_DIR)/assets/wallpaper.myimg $(BUILD_DIR)/assets/cursor_pointer.myimg $(BUILD_DIR)/assets/cursor_text.myimg

CFLAGS := -std=gnu11 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
	-nostdinc -isystem $(CROSS_GCC_INCLUDE) \
	-Wall -Wextra -Iinclude/myos -DFS_START_LBA=$(FS_START_LBA) -g
CXXFLAGS := -std=gnu++17 -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics \
	-fno-use-cxa-atexit -fno-pie -fno-pic -fno-stack-protector $(CXX_TARGET_FLAGS) \
	-nostdinc -isystem $(CROSS_GCC_INCLUDE) \
	-Wall -Wextra -Iinclude/myos -DFS_START_LBA=$(FS_START_LBA) -g
LDFLAGS := -nostdlib -T linker.ld

KERNEL_ASM := arch/x86/boot/entry.asm arch/x86/interrupts/isr.asm arch/x86/switch.asm arch/x86/gdt_flush.asm arch/x86/usermode.asm
KERNEL_C := kernel/core/kernel.c kernel/core/syscall.c kernel/core/usermode.c arch/x86/gdt.c arch/x86/interrupts/idt.c \
	kernel/drivers/video/vga.c kernel/graphics/graphics.c kernel/drivers/platform/serial.c kernel/drivers/input/keyboard.c kernel/drivers/input/mouse.c \
	kernel/drivers/platform/timer.c kernel/drivers/platform/rtc.c kernel/drivers/platform/pci.c kernel/drivers/platform/power.c kernel/drivers/storage/ata.c \
	kernel/mm/paging.c kernel/mm/pmm.c kernel/mm/heap.c \
	kernel/sched/scheduler.c kernel/fs/initrd.c kernel/fs/diskfs/diskfs.c \
	kernel/input/input.c kernel/assets/assets.c kernel/shell/shell.c kernel/lib/util.c
KERNEL_CPP := kernel/gui/core/gui.cpp kernel/gui/wm/window_manager.cpp kernel/gui/desktop/desktop.cpp \
	kernel/gui/apps/terminal.cpp kernel/gui/apps/utility_apps.cpp kernel/gui/apps/notepad.cpp
KERNEL_OBJS := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM)) \
	$(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C)) \
	$(patsubst %.cpp,$(BUILD_DIR)/%.o,$(KERNEL_CPP))

.PHONY: all clean run run-serial debug debug-gui check test test-shell test-gui check-size ci disasm symbols tools lint format-check compile-commands

all: $(OS_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BOOT_BIN): arch/x86/boot/boot.asm | $(BUILD_DIR)
	$(AS) -f bin $< -o $@

$(STAGE2_BIN): arch/x86/boot/stage2.asm | $(BUILD_DIR)
	$(AS) -f bin $< -o $@
	@test $$(stat -c%s $@) -eq $$(($(STAGE2_SECTORS) * 512)) || \
		(echo "Stage2 must be exactly $(STAGE2_SECTORS) sectors"; exit 1)

$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@test $$(stat -c%s $@) -le $$(($(KERNEL_SECTORS) * 512)) || \
		(echo "Kernel is larger than the $(KERNEL_SECTORS) sectors loaded by stage2"; exit 1)

$(ASSET_FILES): tools/build/png_asset_pack.py $(ASSET_MANIFEST) assets/source/wallpaper.png assets/source/cursor_atlas.png | $(BUILD_DIR)
	python3 tools/build/png_asset_pack.py $(ASSET_MANIFEST)

$(FS_IMG): tools/build/mkfs.py $(ASSET_FILES) | $(BUILD_DIR)
	python3 tools/build/mkfs.py $@

$(OS_IMAGE): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(FS_IMG)
	dd if=/dev/zero of=$@ bs=512 count=$$(($(KERNEL_SECTORS) + $(STAGE2_SECTORS) + $(FS_SECTORS) + 1)) status=none
	dd if=$(BOOT_BIN) of=$@ conv=notrunc status=none
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc status=none
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=$$(($(STAGE2_SECTORS) + 1)) conv=notrunc status=none
	dd if=$(FS_IMG) of=$@ bs=512 seek=$(FS_START_LBA) conv=notrunc status=none

run: $(OS_IMAGE)
	$(QEMU) -display $(QEMU_DISPLAY) -drive format=raw,file=$(OS_IMAGE)

run-serial: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) -display none -serial stdio -no-reboot

debug: $(OS_IMAGE)
	$(QEMU) -display $(QEMU_DISPLAY) -drive format=raw,file=$(OS_IMAGE) -S -s

debug-gui: $(OS_IMAGE)
	mkdir -p $(BUILD_DIR)
	$(QEMU) -display $(QEMU_DISPLAY) -drive format=raw,file=$(OS_IMAGE) -serial file:$(BUILD_DIR)/debug-serial.log -monitor unix:/tmp/myos-debug-monitor.sock,server,nowait -S -s

check: $(OS_IMAGE)
	sh tools/test/smoke-test.sh $(OS_IMAGE)

test: check-size check test-shell test-gui
	@echo "All tests passed"

test-shell: $(OS_IMAGE)
	python3 tools/test/shell-test.py $(OS_IMAGE)

test-gui: $(OS_IMAGE)
	python3 tools/test/gui-test.py $(OS_IMAGE)

check-size: $(STAGE2_BIN) $(KERNEL_BIN) $(FS_IMG)
	@test $$(stat -c%s $(STAGE2_BIN)) -eq $$(($(STAGE2_SECTORS) * 512))
	@test $$(stat -c%s $(KERNEL_BIN)) -le $$(($(KERNEL_SECTORS) * 512))
	@test $$(stat -c%s $(FS_IMG)) -eq $$(($(FS_SECTORS) * 512))
	@echo "Image sizes: ok"

ci: tools all check-size check lint
	python3 -B -m py_compile tools/build/mkfs.py tools/build/png_asset_pack.py tools/test/shell-test.py tools/test/gui-test.py tools/dev/gen-compile-commands.py
	@echo "CI checks passed"

compile-commands:
	python3 tools/dev/gen-compile-commands.py "$(CC)" '$(CFLAGS)' "$(CXX)" '$(CXXFLAGS)' $(KERNEL_C) $(KERNEL_CPP)

lint: compile-commands
	@if command -v clang-tidy >/dev/null; then \
		for src in $(KERNEL_C) $(KERNEL_CPP); do clang-tidy "$$src" -p $(BUILD_DIR) || exit $$?; done; \
	else \
		echo "clang-tidy: missing, skipping optional lint"; \
	fi

format-check:
	@if command -v clang-format >/dev/null; then \
		clang-format --dry-run --Werror $(KERNEL_C) $(KERNEL_CPP) \
			kernel/gui/core/gui_types.hpp kernel/gui/wm/window_manager.hpp kernel/gui/desktop/desktop.hpp \
			kernel/gui/apps/terminal.hpp kernel/gui/apps/utility_apps.hpp kernel/gui/apps/notepad.hpp \
			include/myos/*.h; \
	else \
		echo "clang-format: missing, skipping optional format check"; \
	fi

disasm: $(KERNEL_ELF)
	$(CROSS_COMPILE)objdump -d $(KERNEL_ELF) > $(BUILD_DIR)/kernel.disasm
	@echo "$(BUILD_DIR)/kernel.disasm"

symbols: $(KERNEL_ELF)
	$(CROSS_COMPILE)nm -n $(KERNEL_ELF) > $(BUILD_DIR)/kernel.symbols
	@echo "$(BUILD_DIR)/kernel.symbols"

tools:
	@command -v nasm >/dev/null && echo "nasm: ok" || echo "nasm: missing"
	@command -v $(CC) >/dev/null && echo "$(CC): ok" || echo "$(CC): missing"
	@command -v $(CXX) >/dev/null && echo "$(CXX): ok" || echo "$(CXX): missing"
	@if [ -z "$(CROSS_CXX)" ]; then echo "$(CROSS_COMPILE)g++: missing, using host g++ -m32 fallback"; fi
	@command -v $(LD) >/dev/null && echo "$(LD): ok" || echo "$(LD): missing"
	@command -v $(OBJCOPY) >/dev/null && echo "$(OBJCOPY): ok" || echo "$(OBJCOPY): missing"
	@command -v qemu-system-i386 >/dev/null && echo "qemu-system-i386: ok" || echo "qemu-system-i386: missing"
	@command -v $(GDB) >/dev/null && echo "$(GDB): ok" || echo "$(GDB): missing"
	@command -v clang-tidy >/dev/null && echo "clang-tidy: ok" || echo "clang-tidy: optional missing"
	@command -v clang-format >/dev/null && echo "clang-format: ok" || echo "clang-format: optional missing"

clean:
	rm -rf $(BUILD_DIR)
