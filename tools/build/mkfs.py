#!/usr/bin/env python3
import struct
import sys
from pathlib import Path

SECTOR_SIZE = 512
FS_SECTORS = 4096
MAX_INODES = 64
INODE_SIZE = 64
MAGIC = b"MYF2"
JOURNAL_CLEAN = 0
TYPE_FILE = 1
TYPE_DIR = 2

BITMAP_START = 2
BITMAP_SECTORS = 1
INODE_START = 3
INODE_SECTORS = (MAX_INODES * INODE_SIZE + SECTOR_SIZE - 1) // SECTOR_SIZE
DATA_START = INODE_START + INODE_SECTORS

FILES = {
    "/hello.txt": b"Hello from MyOS diskfs.\nThis file was loaded from disk sectors through ATA PIO.\n",
    "/system.txt": b"MyOS diskfs v2: directory-aware writable filesystem image.\n",
    "/docs/system.txt": b"MyOS diskfs supports directories, metadata, cached sectors, and fsck.\n",
    "/etc/motd": b"Welcome to MyOS userland.\n",
}


def u32(value):
    return struct.pack("<I", value)


def build_user_payload():
    code = bytearray()
    patches = []

    def mov_eax(value):
        code.extend(b"\xB8" + u32(value))

    def mov_ebx(value):
        code.extend(b"\xBB" + u32(value))

    def mov_ecx(value):
        code.extend(b"\xB9" + u32(value))

    def int80():
        code.extend(b"\xCD\x80")

    def patchable_ptr(label):
        patches.append((len(code), label))
        code.extend(u32(0))

    def syscall_write(label, length):
        mov_eax(1)
        code.append(0xBB)
        patchable_ptr(label)
        mov_ecx(length)
        int80()

    def syscall_open(label, length):
        mov_eax(5)
        code.append(0xBB)
        patchable_ptr(label)
        mov_ecx(length)
        int80()
        code.extend(b"\x89\xC7")

    def syscall_read(buffer_label, length):
        code.extend(b"\x89\xFB")
        mov_eax(6)
        code.append(0xB9)
        patchable_ptr(buffer_label)
        code.extend(b"\xBA" + u32(length))
        int80()

    def syscall_close():
        code.extend(b"\x89\xFB")
        mov_eax(7)
        int80()

    labels = {}
    start_message = b"Hello from disk-loaded ring 3 program.\n"
    file_name = b"hello.txt"
    prefix = b"Read via sys_read: "

    syscall_write("start_message", len(start_message))
    mov_eax(4)
    int80()
    mov_eax(3)
    int80()
    syscall_open("file_name", len(file_name))
    syscall_read("read_buffer", 96)
    syscall_write("prefix", len(prefix))
    syscall_write("read_buffer", 80)
    syscall_close()
    mov_eax(2)
    mov_ebx(7)
    int80()
    code.extend(b"\xEB\xFE")

    labels["start_message"] = len(code)
    code.extend(start_message)
    labels["file_name"] = len(code)
    code.extend(file_name)
    labels["prefix"] = len(code)
    code.extend(prefix)
    labels["read_buffer"] = len(code)
    code.extend(bytes(128))

    for offset, label in patches:
        code[offset:offset + 4] = u32(0x00400000 + labels[label])
    return bytes(code)


def build_mexe_program(payload):
    return b"MEXE" + u32(0) + u32(len(payload)) + u32(0) + payload


def build_elf_program(payload):
    elf_header_size = 52
    program_header_size = 32
    payload_offset = 0x100
    base = 0x00400000
    elf = bytearray(payload_offset + len(payload))
    elf[0:16] = b"\x7FELF" + bytes([1, 1, 1]) + bytes(9)
    elf[16:18] = struct.pack("<H", 2)
    elf[18:20] = struct.pack("<H", 3)
    elf[20:24] = u32(1)
    elf[24:28] = u32(base)
    elf[28:32] = u32(elf_header_size)
    elf[40:42] = struct.pack("<H", elf_header_size)
    elf[42:44] = struct.pack("<H", program_header_size)
    elf[44:46] = struct.pack("<H", 1)
    ph = elf_header_size
    elf[ph:ph + 4] = u32(1)
    elf[ph + 4:ph + 8] = u32(payload_offset)
    elf[ph + 8:ph + 12] = u32(base)
    elf[ph + 12:ph + 16] = u32(base)
    elf[ph + 16:ph + 20] = u32(len(payload))
    elf[ph + 20:ph + 24] = u32(len(payload))
    elf[ph + 24:ph + 28] = u32(7)
    elf[ph + 28:ph + 32] = u32(0x1000)
    elf[payload_offset:payload_offset + len(payload)] = payload
    return bytes(elf)


def add_programs():
    payload = build_user_payload()
    FILES["/hello.mx"] = build_mexe_program(payload)
    if "/hello.elf" not in FILES:
        FILES["/hello.elf"] = build_elf_program(payload)
        FILES["/bin/hello.elf"] = FILES["/hello.elf"]


def add_user_apps(paths):
    for source in paths:
        path = Path(source)
        if not path.exists():
            raise RuntimeError(f"user app is missing: {source}")
        data = path.read_bytes()
        fs_path = f"/apps/{path.name}"
        FILES[fs_path] = data
        if path.name == "hello.elf":
            FILES["/hello.elf"] = data
            FILES["/bin/hello.elf"] = data


def add_assets():
    for fs_path, source in (
        ("/assets/wallpaper.myimg", "build/assets/wallpaper.myimg"),
        ("/assets/cursor_pointer.myimg", "build/assets/cursor_pointer.myimg"),
        ("/assets/cursor_text.myimg", "build/assets/cursor_text.myimg"),
    ):
        path = Path(source)
        if path.exists():
            FILES[fs_path] = path.read_bytes()


def split_path(path):
    return [part for part in path.split("/") if part]


def pack_inode(kind, parent, name, first_sector=0, sector_count=0, size=0):
    encoded = name.encode("ascii")[:31]
    name_buf = encoded + b"\0" + bytes(32 - len(encoded) - 1)
    return struct.pack("<BBHIIII32s12s", kind, 0, 0, size, first_sector, sector_count, parent, name_buf, bytes(12))


def main():
    output = Path(sys.argv[1] if len(sys.argv) > 1 else "build/fs.img")
    add_user_apps(sys.argv[2:])
    add_programs()
    add_assets()

    image = bytearray(SECTOR_SIZE * FS_SECTORS)
    inodes = [{"type": TYPE_DIR, "parent": 0, "name": "", "size": 0, "first": 0, "sectors": 0}]
    lookup = {"/": 0}

    def ensure_dir(parts):
        path = ""
        parent = 0
        for part in parts:
            path += "/" + part
            if path not in lookup:
                if len(inodes) >= MAX_INODES:
                    raise RuntimeError("too many inodes")
                lookup[path] = len(inodes)
                inodes.append({"type": TYPE_DIR, "parent": parent, "name": part, "size": 0, "first": 0, "sectors": 0})
            parent = lookup[path]
        return parent

    next_sector = DATA_START
    for path, data in sorted(FILES.items()):
        parts = split_path(path)
        parent = ensure_dir(parts[:-1])
        sectors = max(1, (len(data) + SECTOR_SIZE - 1) // SECTOR_SIZE)
        if len(inodes) >= MAX_INODES:
            raise RuntimeError("too many inodes")
        if next_sector + sectors > FS_SECTORS:
            raise RuntimeError("filesystem image is too small")
        image[next_sector * SECTOR_SIZE:next_sector * SECTOR_SIZE + len(data)] = data
        inodes.append({"type": TYPE_FILE, "parent": parent, "name": parts[-1],
                       "size": len(data), "first": next_sector, "sectors": sectors})
        next_sector += sectors

    used = bytearray(SECTOR_SIZE * BITMAP_SECTORS)
    for sector in range(next_sector):
        used[sector // 8] |= 1 << (sector % 8)

    image[0:4] = MAGIC
    image[4:8] = struct.pack("<I", FS_SECTORS)
    image[8:12] = struct.pack("<I", MAX_INODES)
    image[12:16] = struct.pack("<I", len(inodes))
    image[16:20] = struct.pack("<I", BITMAP_START)
    image[20:24] = struct.pack("<I", BITMAP_SECTORS)
    image[24:28] = struct.pack("<I", INODE_START)
    image[28:32] = struct.pack("<I", INODE_SECTORS)
    image[32:36] = struct.pack("<I", DATA_START)
    image[36:40] = struct.pack("<I", JOURNAL_CLEAN)
    image[BITMAP_START * SECTOR_SIZE:(BITMAP_START + BITMAP_SECTORS) * SECTOR_SIZE] = used

    inode_offset = INODE_START * SECTOR_SIZE
    for index, inode in enumerate(inodes):
        image[inode_offset + index * INODE_SIZE:inode_offset + (index + 1) * INODE_SIZE] = pack_inode(
            inode["type"], inode["parent"], inode["name"], inode["first"], inode["sectors"], inode["size"])

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image)
    print(f"diskfs v2: wrote {output} with {len(inodes)} inodes and {next_sector - DATA_START} data sectors")


if __name__ == "__main__":
    main()
