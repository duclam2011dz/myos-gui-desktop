# MyOS Roadmap

MyOS currently has a BIOS boot sector plus stage2 loader, E820 memory handoff,
VBE linear framebuffer graphics with VGA mode 13h fallback, high-memory
framebuffer paging, a pixel/font renderer, freestanding C++ GUI modules,
double-buffered Windows-like desktop GUI, PS/2 keyboard and mouse input, a
normalized GUI input event layer, a taskbar with Start button, Shutdown/Restart
power actions, CMOS-backed clock/date, app indicator, PNG-derived wallpaper and cursor assets, Terminal
shortcut, File Explorer, Notepad text editor, System
Monitor, About app, window controls,
32-bit protected mode, kernel-owned GDT/TSS, ring 3 user mode, `int 0x80`
syscalls, MEXE and ELF32 user programs loaded from diskfs, IDT/IRQ handling,
PIT IRQ0, full IRQ-return preemptive task switching, COM1 logging, 128 MiB
identity paging, per-process user address spaces with CR3 switching, dynamic
page map/unmap APIs, E820-backed PMM, hardened PMM-backed heap, ATA PIO
reads/writes, PCI enumeration, a directory-aware disk-backed filesystem image
with initrd fallback,
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
- Asset pipeline and project structure:
  - Build-time PNG decoding now converts `assets/source/wallpaper.png` and
    `assets/source/cursor_atlas.png` into compact `.myimg` runtime assets.
  - Diskfs packages `/assets/wallpaper.myimg` and
    `/assets/cursor_pointer.myimg`; the desktop renders those assets instead of
    the old hard-coded gradient wallpaper and bitmask cursor.
  - Cursor assets now include matching 12x16 pointer and text-entry `|`
    variants generated from the atlas.
  - Source folders are grouped by responsibility: build/test/dev tools live
    under `tools/`, drivers under `kernel/drivers/{input,platform,storage,video}`,
    assets under `kernel/assets`, and diskfs under `kernel/fs/diskfs`.
- Filesystem and reliability:
  - Diskfs v2 now uses a superblock, sector allocation bitmap, inode table,
    directory-aware path traversal, and a larger 4096-sector filesystem image.
  - Diskfs adds a small dirty metadata journal marker, cached sectors with dirty
    writeback, explicit flush, and fsck validation for bounds, overlaps, and
    bitmap mismatches.
  - Truncate, delete, rename, and fsck are available through kernel APIs and
    shell/GUI Terminal commands while preserving the old read/write APIs.
- Driver and platform polish:
  - Keyboard input now exposes press/release events, keycodes, ASCII, and
    modifiers while keeping the old character stream for the shell.
  - PS/2 mouse init negotiates IntelliMouse wheel packets and exposes wheel,
    middle, and right button state; GUI Terminal can scroll from wheel events.
  - CMOS RTC provides real taskbar wall-clock time.
  - PCI config-space scanning records devices and exposes a `pci` shell command.
- Cursor and power UX:
  - GUI cursor drawing now follows the real mouse coordinates directly without
    interpolation or periodic animation invalidation.
  - Cursor updates use a small framebuffer overlay that restores the previous
    background and redraws only the cursor region when the pointer moves.
  - Typing printable text temporarily switches the pointer to the text cursor.
  - Start Menu includes Shutdown and Restart actions wired to platform power
    routines.
- File Explorer polish:
  - File Explorer now labels TXT, ELF, MX, MYIMG, and generic files with matching
    type icons instead of showing every row as TXT.
  - Double-clicking ELF/MX files dispatches through a launcher hook; current
    terminal programs open Terminal and run from diskfs, while future GUI app
    binaries can be routed to GUI windows without changing File Explorer.

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
     explicit focus-change events, and optional window snap zones.

2. GUI applications
   - Current state: Terminal, File Explorer, Notepad, System Monitor, and About
     are implemented as separate GUI app modules with Start menu, function-key,
     and desktop shortcut launch paths.
   - File Explorer now lists diskfs entries with type-specific icons and double-click
     opens text files in Notepad or launches ELF/MX programs.
   - Notepad can view/edit diskfs text files, supports Backspace while typing,
     and persists edits with `Ctrl+S`.
   - Next app polish: cursor navigation, text selection, vertical scrolling, and
     status/error surfacing in Notepad.
   - Next utility apps: richer process/task tables and refresh controls in
     System Monitor; settings controls for theme, terminal font scale, and input
     diagnostics.

3. Terminal UX
   - Add text selection and copy/paste buffers.
   - Add visible scrollbars and configurable wheel/page scroll speed.
   - Add configurable font scale for high-DPI modes.

4. Driver/platform polish
   - Current state: keyboard event ring, modifiers, release events,
     IntelliMouse wheel packets, CMOS time, and PCI enumeration are implemented.
   - Next platform work: identify PCI IDE/class drivers, add controller-specific
     driver binding, and surface PCI device names in GUI diagnostics.
   - Next input work: add Alt-Tab, text selection shortcuts, and full scancode
     set coverage for extended keys.
   - Next cursor work: introduce a cursor manager with window/client shape
     selection, investigate hardware cursor support for supported graphics
     backends, add a richer cursor shape set, support independent high-frequency
     cursor updates, and add frame/input timing diagnostics.

5. Filesystem and reliability
   - Current state: diskfs v2 has directory-aware paths, inodes, bitmap
     allocation, dirty sector cache, metadata journal marker, truncate/delete/
     rename, and fsck reporting.
   - Next reliability work: replace the journal marker with replayable metadata
     records and add automated corruption-image tests.
   - Next filesystem work: support non-contiguous extents and directory listing
     filters so large files no longer require contiguous sector runs.

## Suggested Immediate Milestone

Next time, generalize the C++ window manager and widget model. The OS now has
asset-backed visuals, direct cursor overlay updates, power actions, dirty-region
tracking, and C++ GUI modules; the next bottleneck is replacing single-app
assumptions with reusable window/widget abstractions.
