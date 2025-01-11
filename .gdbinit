set confirm off
set architecture riscv:rv64
target remote 127.0.0.1:1234
symbol-file build/kernel
set disassemble-next-line auto
display/12i $pc-8
set riscv use-compressed-breakpoints yes
# break *0x1000
break *0x8020f6d8
