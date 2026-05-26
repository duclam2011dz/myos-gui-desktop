# MyOS Roadmap

MyOS currently has a BIOS boot sector plus stage2 loader, E820 memory handoff,
VBE linear framebuffer graphics with VGA mode 13h fallback, high-memory
framebuffer paging, a pixel/font renderer, freestanding C++ GUI modules,
double-buffered Windows-like desktop GUI, PS/2 keyboard and mouse input, a normalized GUI input event layer, a taskbar
with Start button, clock/date, app indicator, wallpaper, compact OS-rendered
mouse cursor, Terminal shortcut, File Explorer, Notepad text editor, System
Monitor, About app, window controls,
32-bit protected mode, kernel-owned GDT/TSS, ring 3 user mode, `int 0x80`
syscalls, MEXE and ELF32 user programs loaded from diskfs, IDT/IRQ handling,
PIT IRQ0, full IRQ-return preemptive task switching, COM1 logging, 128 MiB
identity paging, per-process user address spaces with CR3 switching, dynamic
page map/unmap APIs, E820-backed PMM, hardened PMM-backed heap, ATA PIO
reads/writes, a path-aware disk-backed filesystem image with initrd fallback,
automated QEMU GUI terminal and screenshot tests, optional clang lint/format
targets, compile command generation, CI checks, disassembly output, symbol dumps,
and exception diagnostics.

## Completed Since Last Roadmap

- True high-resolution framebuffer:
  - Stage2 now probes VBE modes, prefers a linear framebuffer mode, and falls
    back to VGA mode 13h only if VBE is unavailable.
  - Stage2 passes framebuffer address, pitch, resolution, bpp, pixel format, and
    mode number to `kernel_main`.
  - Paging maps the boot framebuffer before enabling paging, so high physical
    LFB addresses work safely.
  - Graphics supports indexed, RGB565, RGB888, and XRGB8888 surfaces with VGA
    palette-index color mapping for existing GUI colors.
- Flicker and input polish:
  - GUI backbuffer allocation now matches the active framebuffer size and format.
  - Same-format blits use row copies, rectangle blits, and faster fill paths
    instead of slow per-pixel get/put loops.
  - GUI input dispatch drains keyboard/mouse events before one coalesced redraw,
    reducing visible terminal typing latency.
  - GUI tracks dirty rectangles for cursor, window, and taskbar invalidation.
  - QEMU `make run` and `make debug` use `gtk,show-cursor=off` by default so the
    desktop shows the OS cursor instead of the host cursor.
- GUI and mouse polish:
  - Mouse bounds now track the active framebuffer resolution.
  - The OS cursor is smaller and cleaner.
  - Cursor drawing clips at framebuffer edges.
  - Click handling uses press/release targets, slop checks, and missed-click
    recovery from the mouse driver.
  - Terminal window default size and maximize behavior adapt to the active
    resolution.
  - Terminal text wraps to the current window width and no longer renders outside
    the window when a line is full.
  - Desktop shortcut double-click timing is more forgiving.
  - The desktop, taskbar, Start menu, and Terminal chrome now use an original
    Windows-like visual style.
- Build and test polish:
  - Smoke tests now validate the VBE framebuffer marker.
  - GUI terminal integration tests wait long enough for VBE-mode QEMU runs and
    validate the current disk-loaded ELF output.
  - `make test-gui` captures QEMU screenshots and checks for nonblank visual
    output after desktop boot, Terminal open, and typing.
  - `make lint`, `make format-check`, and `make compile-commands` provide clearer
    local debugging and editor feedback, skipping optional clang tools when they
    are unavailable.
- GUI modularization and C++:
  - GUI implementation moved from one large C file into `kernel/gui/core`,
    `kernel/gui/wm`, `kernel/gui/desktop`, and `kernel/gui/apps`.
  - Desktop drawing and Terminal behavior are now C++ classes compiled without
    exceptions, RTTI, STL, or libstdc++ runtime dependencies.
  - Window hit testing, drag, and resize behavior moved into a reusable
    `WindowManager` C++ module.
  - File Explorer, System Monitor, and About are available from the Start menu
    and through F2/F3/F4 shortcuts.
  - Desktop shortcuts now launch Terminal, File Explorer, System Monitor, and
    About by double-click.
  - File Explorer can open diskfs text files in Notepad; Notepad supports text
    editing and `Ctrl+S` save back to diskfs.
  - Keyboard shortcuts now use reserved GUI key codes, so Backspace no longer
    collides with F4/About and works correctly in Terminal and Notepad.
  - Desktop shortcut labels now render full app names: Terminal, File Explorer,
    System Monitor, and About.
  - Build prefers `i686-linux-gnu-g++` and falls back to host `g++ -m32` when the
    cross C++ package cannot be installed.
  - Stage2 now advances `ES` while loading the kernel so kernels larger than
    64 KiB no longer wrap and overwrite their first segment.
- Filesystem reliability:
  - Diskfs now validates directory entries, file sizes, sector ranges, and
    overlapping extents during mount.
  - Diskfs write paths validate names and support zero-length file contents
    without corrupting directory metadata.

## Recommended Next Updates

1. Window manager next steps
   - Current state: generalized hit testing, z-order focus, drag, resize,
     minimize, maximize, restore, close, task buttons, Start menu launch, and
     desktop shortcut launch are implemented across Terminal, File Explorer,
     System Monitor, About, and Notepad.
   - Next refactor: replace the fixed app fields in `GuiSystem` with a compact
     reusable window registry and per-window paint/input callbacks.
   - Next widgets: extract titlebar buttons, menu rows, task buttons, scrollbars,
     desktop icons, and text surfaces into reusable C++ widget primitives.
   - Next interaction: add z-ordered taskbar arrangement, Alt-Tab focus cycling,
     keyboard focus events, and optional window snap zones.

2. GUI applications
   - Current state: Terminal, File Explorer, Notepad, System Monitor, and About
     are implemented as separate GUI app modules with Start menu, function-key,
     and desktop shortcut launch paths.
   - File Explorer now lists diskfs entries with text-file icons and double-click
     opens text files in Notepad.
   - Notepad can view/edit diskfs text files, supports Backspace while typing,
     and persists edits with `Ctrl+S`.
   - Next app polish: cursor navigation, text selection, vertical scrolling, and
     status/error surfacing in Notepad.
   - Next utility apps: process/task tables and refresh controls in System
     Monitor; settings controls for theme, terminal font scale, and input
     diagnostics.

3. Terminal UX
   - Add text selection and copy/paste buffers.
   - Add mouse wheel and scrollbar support.
   - Add configurable font scale for high-DPI modes.

4. Driver/platform polish
   - Add richer PS/2 scancode handling, key release events, modifiers, and mouse
     wheel packet support.
   - Add CMOS time for real wall-clock date instead of the demo fixed date.
   - Add PCI enumeration as a base for better disk/network drivers.

5. Filesystem and reliability
   - Replace flat slash path strings with real directory records.
   - Add a sector cache and dirty block tracking.
   - Add truncate/delete/rename and consistency checks for interrupted writes.
   - Add a small fsck command that reports diskfs validation errors through GUI
     and serial output.

## Suggested Immediate Milestone

Next time, generalize the C++ window manager. The OS now has an event layer,
dirty-region tracking, visual regression tests, faster primitives, and C++ GUI
modules; the next bottleneck is replacing single-app assumptions with reusable
window/widget abstractions.
