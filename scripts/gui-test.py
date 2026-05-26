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


def send_monitor_command(sock, command, delay=0.08):
    sock.sendall(command.encode("ascii") + b"\n")
    time.sleep(delay)


def send_text(sock, text):
    for ch in text:
        send_monitor_command(sock, f"sendkey {KEY_NAMES.get(ch, ch)}", 0.03)


def click_mouse(sock):
    send_monitor_command(sock, "mouse_button 1", 0.05)
    send_monitor_command(sock, "mouse_button 0", 0.05)


def double_click_terminal_icon(sock):
    send_monitor_command(sock, "mouse_move -10000 10000")
    send_monitor_command(sock, "mouse_move 40 -52")
    click_mouse(sock)
    click_mouse(sock)


def move_mouse_to(sock, x, y):
    send_monitor_command(sock, "mouse_move -10000 10000")
    send_monitor_command(sock, f"mouse_move {x} {-y}")


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


def read_ppm(path):
    with open(path, "rb") as handle:
        header = handle.readline().strip()
        if header != b"P6":
            raise RuntimeError(f"{path}: unsupported PPM header {header!r}")
        line = handle.readline()
        while line.startswith(b"#"):
            line = handle.readline()
        width, height = [int(value) for value in line.split()]
        max_value = int(handle.readline())
        if max_value != 255:
            raise RuntimeError(f"{path}: unsupported PPM max value {max_value}")
        data = handle.read()
    return width, height, data


def assert_nonblank_ppm(path, min_unique_colors):
    width, height, data = read_ppm(path)
    if width < 300 or height < 180:
        raise RuntimeError(f"{path}: screenshot is unexpectedly small: {width}x{height}")
    colors = set()
    for index in range(0, len(data), 3):
        colors.add(data[index:index + 3])
        if len(colors) >= min_unique_colors:
            return
    raise RuntimeError(f"{path}: only {len(colors)} unique colors found")


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else "build/myos.img"
    build_dir = "build"
    log_path = os.path.join(build_dir, "gui-test.log")
    monitor_path = "/tmp/myos-qemu-gui-test.sock"
    screenshots = [
        os.path.join(build_dir, "gui-desktop.ppm"),
        os.path.join(build_dir, "gui-terminal.ppm"),
        os.path.join(build_dir, "gui-typed.ppm"),
        os.path.join(build_dir, "gui-files.ppm"),
        os.path.join(build_dir, "gui-monitor.ppm"),
        os.path.join(build_dir, "gui-about.ppm"),
        os.path.join(build_dir, "gui-notepad.ppm"),
    ]

    os.makedirs(build_dir, exist_ok=True)
    for path in [log_path, monitor_path, *screenshots]:
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
            time.sleep(0.3)
            send_monitor_command(sock, f"screendump {screenshots[0]}")
            double_click_terminal_icon(sock)
            send_monitor_command(sock, "sendkey f1")
            send_monitor_command(sock, f"screendump {screenshots[1]}")
            send_text(sock, "helx")
            send_monitor_command(sock, "sendkey backspace")
            send_text(sock, "p\nabout\n")
            send_monitor_command(sock, f"screendump {screenshots[2]}")
            send_monitor_command(sock, "sendkey f2")
            send_monitor_command(sock, f"screendump {screenshots[3]}")
            send_monitor_command(sock, "sendkey f3")
            send_monitor_command(sock, f"screendump {screenshots[4]}")
            send_monitor_command(sock, "sendkey f4")
            send_monitor_command(sock, f"screendump {screenshots[5]}")
            send_monitor_command(sock, "sendkey f5")
            send_text(sock, " savedx")
            send_monitor_command(sock, "sendkey backspace")
            send_monitor_command(sock, "sendkey ctrl-s")
            send_monitor_command(sock, f"screendump {screenshots[6]}")
            send_monitor_command(sock, "sendkey f1")
            send_text(sock, "cat hello.txt\n")

        content = wait_for_file_contains(log_path, "Window manager: drag focus min max resize.", 10)
        required = [
            "MyOS GUI: desktop initialized with no open apps.",
            "MyOS GUI: Terminal opened.",
            "Commands: help clear about ticks uptime ls cat run procs tasks mem",
            "Window manager: drag focus min max resize.",
            "MyOS GUI: Notepad opened file.",
            "MyOS GUI: Notepad saved file.",
            "saved",
        ]
        missing = [marker for marker in required if marker not in content]
        if missing:
            raise RuntimeError("missing GUI markers: " + ", ".join(missing))

        for screenshot in screenshots:
            assert_nonblank_ppm(screenshot, 4)

        print("GUI visual test passed: screenshots and interaction markers verified.")
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
