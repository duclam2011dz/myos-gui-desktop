#!/usr/bin/env python3
import os
import socket
import subprocess
import sys
import time


KEY_NAMES = {
    " ": "spc",
    "\n": "ret",
    "-": "minus",
    "=": "equal",
    "/": "slash",
    ".": "dot",
    ",": "comma",
}


def send_monitor_command(sock, command):
    sock.sendall(command.encode("ascii") + b"\n")
    time.sleep(0.08)


def send_text(sock, text):
    for ch in text:
        key = KEY_NAMES.get(ch, ch)
        send_monitor_command(sock, f"sendkey {key}")


def click_mouse(sock):
    send_monitor_command(sock, "mouse_button 1")
    send_monitor_command(sock, "mouse_button 0")


def double_click_terminal_icon(sock):
    send_monitor_command(sock, "mouse_move -10000 10000")
    send_monitor_command(sock, "mouse_move 40 -52")
    click_mouse(sock)
    click_mouse(sock)


def wait_for_file_contains(path, marker, timeout_seconds):
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                content = handle.read()
            if marker in content:
                return content
        time.sleep(0.1)
    return ""


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else "build/myos.img"
    log_path = sys.argv[2] if len(sys.argv) > 2 else "build/shell-test.log"
    monitor_path = "/tmp/myos-qemu-monitor.sock"

    for path in (log_path, monitor_path):
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass

    qemu = subprocess.Popen([
        "qemu-system-i386",
        "-drive", f"format=raw,file={image}",
        "-display", "none",
        "-serial", f"file:{log_path}",
        "-monitor", f"unix:{monitor_path},server,nowait",
        "-no-reboot",
    ])

    try:
        wait_for_file_contains(log_path, "MyOS GUI: desktop initialized.", 5)

        deadline = time.time() + 5
        while not os.path.exists(monitor_path):
            if time.time() > deadline:
                raise RuntimeError("QEMU monitor socket was not created")
            time.sleep(0.1)

        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(monitor_path)
            time.sleep(0.2)
            double_click_terminal_icon(sock)
            send_monitor_command(sock, "sendkey f1")
            time.sleep(0.2)
            send_text(sock, "help\n")
            send_text(sock, "about\n")
            send_text(sock, "ticks\n")
            send_text(sock, "uptime\n")
            send_text(sock, "ls\n")
            send_text(sock, "run /bin/hello.elf\n")

        content = wait_for_file_contains(log_path, "Program exited with code 7", 20)
        required = [
            "MyOS GUI: desktop initialized with no open apps.",
            "MyOS GUI: double buffering enabled.",
            "MyOS graphics: VBE linear framebuffer enabled.",
            "MyOS GUI: Terminal opened.",
            "MyOS GUI terminal: command entered.",
            "Commands: help clear about ticks uptime ls cat run procs tasks mem",
            "MyOS GUI double-buffered desktop.",
            "Window manager: drag focus min max resize.",
            "Timer ticks=",
            "Uptime seconds=",
            "diskfs:",
            "/bin/hello.elf",
            "Loading user program: /bin/hello.elf",
            "Hello from disk-loaded ring 3 program.",
            "Program exited with code 7",
        ]

        missing = [marker for marker in required if marker not in content]
        if missing:
            if os.path.exists(log_path):
                with open(log_path, "r", encoding="utf-8", errors="replace") as handle:
                    print(handle.read())
            raise RuntimeError("missing shell-test markers: " + ", ".join(missing))

        print("GUI terminal test passed: desktop commands verified.")
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=2)
        except subprocess.TimeoutExpired:
            qemu.kill()
            qemu.wait()
        try:
            os.unlink(monitor_path)
        except FileNotFoundError:
            pass


if __name__ == "__main__":
    main()
