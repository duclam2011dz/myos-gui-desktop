#!/usr/bin/env sh
set -eu

image="${1:-build/myos.img}"
log="${2:-build/smoke-test.log}"

timeout 5s qemu-system-i386 \
    -drive format=raw,file="$image" \
    -display none \
    -serial stdio \
    -no-reboot >"$log" 2>&1 || status=$?

status="${status:-0}"
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    cat "$log"
    exit "$status"
fi

if ! grep -q "MyOS serial: kernel_main reached." "$log"; then
    cat "$log"
    echo "Smoke test failed: kernel serial marker not found."
    exit 1
fi

if ! grep -q "MyOS keyboard: IRQ1 enabled." "$log"; then
    cat "$log"
    echo "Smoke test failed: keyboard IRQ marker not found."
    exit 1
fi

if ! grep -q "MyOS GUI: desktop initialized." "$log"; then
    cat "$log"
    echo "Smoke test failed: GUI desktop marker not found."
    exit 1
fi

if ! grep -q "MyOS graphics: VBE linear framebuffer enabled." "$log"; then
    cat "$log"
    echo "Smoke test failed: VBE framebuffer marker not found."
    exit 1
fi

if ! grep -q "MyOS mouse: PS/2 mouse enabled." "$log"; then
    cat "$log"
    echo "Smoke test failed: mouse marker not found."
    exit 1
fi

if ! grep -q "MyOS timer: PIT initialized at 100 Hz." "$log"; then
    cat "$log"
    echo "Smoke test failed: PIT timer marker not found."
    exit 1
fi

echo "Smoke test passed: GUI desktop reached kernel_main."
