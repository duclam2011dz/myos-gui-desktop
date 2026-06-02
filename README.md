# MyOS

Minimal bootable x86 OS skeleton written in Assembly and C.

Current kernel features:

- BIOS boot sector loads a stage2 loader, then stage2 loads the flat kernel.
- 32-bit protected mode.
- VBE linear framebuffer graphics with VGA mode 13h fallback.
- Double-buffered Windows-like desktop GUI with PNG-derived wallpaper and cursor
  assets, taskbar, Start button, CMOS-backed clock/date, app indicator, Terminal
  shortcut, Shutdown/Restart actions, and window controls.
- COM1 serial logging.
- IDT with CPU exception and IRQ stubs.
- PIC remap with IRQ1 keyboard support.
- Identity-mapped paging for the first 128 MiB.
- Buffered keyboard input layer with press/release events, keycodes, modifiers,
  shift/caps support, and a shell-compatible character stream.
- PS/2 mouse driver with IRQ12 packet handling, IntelliMouse wheel packets, and
  middle/right button state.
- Normalized GUI input event layer over keyboard and mouse state.
- GUI Terminal app rendered through the framebuffer with drag/focus, minimize,
  maximize/restore, resize, close, scrollback, command history, and cursor blink.
- Exception diagnostics with register dump and page-fault `CR2`.
- PIT timer IRQ0 at 100 Hz with shell tick/uptime commands.
- Physical page allocator backed by BIOS E820 usable memory below 128 MiB.
- Kernel heap backed by PMM pages.
- BIOS E820 memory map handoff.
- Full IRQ-return preemptive scheduler driven by PIT ticks.
- Task states and interrupt-frame context switching for kernel/user frames.
- Per-process user address spaces with CR3 switching for ring 3 programs.
- ATA PIO sector read/write.
- PCI bus enumeration and CMOS RTC wall-clock reads.
- Disk-backed writable diskfs v2 image with initrd fallback, real directory
  traversal, inode metadata, bitmap allocation, dirty sector cache, metadata
  journal marker, and fsck validation.
- Dynamic paging APIs: `paging_map_page`, `paging_unmap_page`, `paging_get_physical`.
- Heap hardening: aligned allocations, double-free accounting, whole-region PMM release.
- Ring 3 user mode with GDT/TSS, `int 0x80` syscall gate, and shell `usertest`.
- Disk-backed user program loading through shell `run hello.mx`.
- Minimal ELF32 user program loading through shell `run hello.elf`.
- ELF32 loading from slash paths such as `run /bin/hello.elf`.
- User process table with PID, process state, exit code, and per-process file descriptors.
- User syscall layer with `write`, `exit`, `yield`, `uptime`, `open`, `read`, `close`, `getpid`, `waitpid`, and `writefile`.
- Directory-aware diskfs reads/writes with offset-based file access for user descriptors.
- Diskfs truncate/delete/rename/fsck commands and APIs.
- Process lifecycle shell commands: `spawn`, `wait`, and `reap`.
- Build-time PNG asset pipeline that converts `assets/source/wallpaper.png` and
  `assets/source/cursor_atlas.png` into compact diskfs runtime assets.
- Smaller pointer cursor plus a text-entry `|` cursor variant while typing.
- File Explorer shows TXT/ELF/MX/MYIMG file labels and can run diskfs ELF/MX
  programs through Terminal on double-click.
- Graphics foundation with bootloader framebuffer handoff, high-memory
  framebuffer paging, framebuffer surface abstraction, drawing primitives, and
  bitmap-style font rendering.
- GUI dirty-region tracking with coalesced input processing and faster fill/blit
  framebuffer primitives.
- Modular C++ GUI apps: Terminal, File Explorer, Notepad, System Monitor, and About.
- File Explorer can open text files in Notepad; Notepad edits in-memory text and
  persists changes to diskfs with `Ctrl+S`.

See [docs/ROADMAP.md](docs/ROADMAP.md) for recommended next milestones.

## Requirements

Installed in this environment:

- `nasm`
- `i686-linux-gnu-gcc`
- `i686-linux-gnu-g++` preferred for GUI C++ modules, with host `g++ -m32`
  fallback when the cross C++ compiler is not installed
- `i686-linux-gnu-ld`
- `i686-linux-gnu-objcopy`
- `make`
- `qemu-system-i386`
- `gdb`
- `gdb-multiarch`

Install the toolchain on a fresh Ubuntu/WSL environment:

```sh
sudo apt-get update
sudo apt-get install -y nasm make qemu-system-i386 gdb gdb-multiarch \
    gcc-i686-linux-gnu g++-i686-linux-gnu binutils-i686-linux-gnu
```

The build defaults to the `i686-linux-gnu-` cross toolchain. Override it with
`make CROSS_COMPILE=...` if you later install a true bare-metal `i686-elf-`
toolchain. GUI C++ files are compiled freestanding with exceptions, RTTI,
thread-safe statics, and libstdc++ runtime use disabled.

## Build

```sh
make
```

This decodes the PNG assets, creates `build/fs.img`, and writes
`build/myos.img`, a raw bootable disk image.

## Run

```sh
make run
```

For terminal-only smoke testing through the serial port:

```sh
make run-serial
```

## Test

```sh
make check
```

This starts QEMU headlessly for a short smoke test and passes when the kernel emits
`MyOS serial: kernel_main reached.`, the VBE framebuffer marker, the mouse marker,
and the GUI desktop marker through COM1.

For a deeper automated shell test:

```sh
make test-shell
```

This uses the QEMU monitor to open the GUI Terminal and type commands such as
`help`, `ls`, `fsck`, `pci`, and `run /bin/hello.elf`, then verifies serial
markers for GUI command output and user program execution.

For screenshot-backed GUI regression testing:

```sh
make test-gui
```

This drives QEMU through the monitor, captures PPM screenshots under `build/`,
and checks that the desktop, Terminal, and typed command frames are nonblank.

Run all available tests:

```sh
make test
```

For local CI-style checks:

```sh
make ci
```

Extra build inspection targets:

```sh
make lint
make format-check
make compile-commands
make disasm
make symbols
```

## Structure

```text
assets/source              Source PNG assets
assets/manifest.json       Build-time asset crop/scale/format manifest
tools/build                Image and asset build tools
tools/test                 QEMU smoke, shell, and screenshot tests
tools/dev                  Developer helper tools
arch/x86/boot              BIOS boot sector, stage2 loader, kernel entry
arch/x86                   GDT/TSS, user mode transition, context switch helpers
arch/x86/interrupts        IDT, ISR stubs, syscall gate
include/myos               Public kernel headers
kernel/assets              Runtime asset loader and bitmap compositing
kernel/core                Kernel entry orchestration
kernel/drivers/input       Keyboard and PS/2 mouse drivers
kernel/drivers/platform    Serial, PIT, CMOS RTC, and PCI enumeration
kernel/drivers/storage     ATA PIO storage driver
kernel/drivers/video       VGA text fallback
kernel/graphics            Framebuffer surfaces, drawing primitives, blit/fill paths
kernel/mm                  Paging, PMM, heap
kernel/sched               Scheduler/tasking foundation
kernel/fs                  Initrd fallback
kernel/fs/diskfs           Diskfs v2 format, cache, journal marker, fsck, ops
kernel/input               Normalized GUI input event layer
kernel/gui/core            GUI event loop, invalidation, compositor state, shared types
kernel/gui/wm              Window hit testing, drag, resize, and shared window actions
kernel/gui/desktop         Desktop shortcuts, Start menu, taskbar, asset wallpaper
kernel/gui/apps            GUI applications such as Terminal
kernel/shell               Interactive shell
kernel/lib                 Freestanding utility code
```

## Debug

Terminal 1:

```sh
make debug
```

Terminal 2:

```sh
gdb -x .gdbinit
```

The GDB session loads `build/kernel.elf`, connects to QEMU on port 1234, and breaks at `kernel_main`.

For GUI debugging with serial logging and a QEMU monitor socket:

```sh
make debug-gui
```

VS Code users can also run the `Debug QEMU Server` or `Debug GUI QEMU Server`
task, then start the
`Attach to MyOS QEMU` debug configuration.

## GUI

Run `make run`. The OS boots straight into the VBE graphics desktop with QEMU's
host cursor hidden by default. Double-click the Terminal desktop shortcut, or
click Start and choose Terminal, then type:

```text
help
about
ticks
uptime
ls
run /bin/hello.elf
cat /etc/motd
write note.txt hello
rename note.txt renamed.txt
truncate renamed.txt 4
delete renamed.txt
fsck
pci
mem
tasks
procs
clear
```

GUI shortcuts:

```text
F1 Terminal
F2 File Explorer
F3 System Monitor
F4 About
F5 Open first diskfs file in Notepad
Ctrl+S Save focused Notepad document
Start > Shutdown powers off QEMU/ACPI-compatible environments
Start > Restart reboots through the PS/2 controller
```
