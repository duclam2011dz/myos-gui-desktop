#!/usr/bin/env python3
import struct
import sys

SECTOR_SIZE = 512
FS_SECTORS = 32
MAX_FILES = 8
ENTRY_SIZE = 48
MAGIC = b"MYFS"

FILES = {
    "hello.txt": b"Hello from MyOS diskfs.\nThis file was loaded from disk sectors through ATA PIO.\n",
    "system.txt": b"MyOS diskfs: read-only sector-backed filesystem image.\n",
    "/docs/system.txt": b"MyOS diskfs supports slash paths, metadata, and offset reads.\n",
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
    elf[32:36] = u32(0)
    elf[36:40] = u32(0)
    elf[40:42] = struct.pack("<H", elf_header_size)
    elf[42:44] = struct.pack("<H", program_header_size)
    elf[44:46] = struct.pack("<H", 1)
    elf[46:48] = struct.pack("<H", 0)
    elf[48:50] = struct.pack("<H", 0)
    elf[50:52] = struct.pack("<H", 0)

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

USER_PAYLOAD = build_user_payload()
FILES["hello.mx"] = build_mexe_program(USER_PAYLOAD)
FILES["hello.elf"] = build_elf_program(USER_PAYLOAD)
FILES["/bin/hello.elf"] = FILES["hello.elf"]


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "build/fs.img"
    image = bytearray(SECTOR_SIZE * FS_SECTORS)
    entries = []
    data_sector = 1

    for name, data in FILES.items():
        sectors = (len(data) + SECTOR_SIZE - 1) // SECTOR_SIZE
        entries.append((name.encode("ascii"), data_sector, len(data)))
        offset = data_sector * SECTOR_SIZE
        image[offset:offset + len(data)] = data
        data_sector += sectors

    image[0:4] = MAGIC
    image[4:8] = struct.pack("<I", len(entries))

    offset = 16
    for name, start_sector, size in entries[:MAX_FILES]:
        name = name[:31] + b"\0"
        name = name.ljust(32, b"\0")
        image[offset:offset + ENTRY_SIZE] = name + struct.pack("<II", start_sector, size) + bytes(8)
        offset += ENTRY_SIZE

    with open(output, "wb") as handle:
        handle.write(image)


if __name__ == "__main__":
    main()
