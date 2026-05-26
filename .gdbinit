set architecture i386
set disassembly-flavor intel
set pagination off
set confirm off
file build/kernel.elf
target remote localhost:1234
break kernel_main
continue
