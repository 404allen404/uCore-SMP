.PHONY: clean build_kernel all user doc
all: user build_kernel copy_bin

U = user
K = os

TOOLPREFIX = ${RISCV}/bin/riscv64-unknown-elf-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
PY = python3
GDB = $(TOOLPREFIX)gdb
CP = cp
MKDIR_P = mkdir -p

BUILDDIR = build
C_SRCS = $(wildcard $K/*.c) $(wildcard $K/*/*.c)
AS_SRCS = $(wildcard $K/*.S) $(wildcard $K/*/*.S)
C_OBJS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(C_SRCS))))
AS_OBJS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(AS_SRCS))))

OBJS = $(C_OBJS) $(AS_OBJS)

# FS_IMG = riscv64-rootfs.img
FS_IMG = fs.img

HEADER_DEP = $(addsuffix .d, $(basename $(C_OBJS)))

ifeq (,$(findstring link_app.o,$(OBJS)))
	AS_OBJS += $(BUILDDIR)/$K/link_app.o
endif

-include $(HEADER_DEP)

ifndef CPUS
CPUS := 5
endif

CFLAGS = -Wall -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I$K
CFLAGS += -DNCPU=$(CPUS)
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -D QEMU

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$(AS_OBJS): $(BUILDDIR)/$K/%.o : $K/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(C_OBJS): $(BUILDDIR)/$K/%.o : $K/%.c  $(BUILDDIR)/$K/%.d
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(HEADER_DEP): $(BUILDDIR)/$K/%.d : $K/%.c
	@mkdir -p $(@D)
	@set -e; rm -f $@; $(CC) -MM $< $(INCLUDEFLAGS) > $@.$$$$; \
        sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
        rm -f $@.$$$$

os/link_app.o: os/link_app.S
os/link_app.S: scripts/pack.py
	@$(PY) scripts/pack.py
os/kernel_app.ld: scripts/kernelld.py
	@$(PY) scripts/kernelld.py

build_kernel: build/kernel

build/kernel: $(OBJS) os/kernel_app.ld os/link_app.S
	$(LD) $(LDFLAGS) -T os/kernel_app.ld -o $(BUILDDIR)/kernel $(OBJS)
	$(OBJDUMP) -S $(BUILDDIR)/kernel > $(BUILDDIR)/kernel.asm
	$(OBJCOPY) -O binary -S $(BUILDDIR)/kernel $(BUILDDIR)/kernel.bin
	$(OBJDUMP) -t $(BUILDDIR)/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILDDIR)/kernel.sym
	gzip -9 -cvf $(BUILDDIR)/kernel.bin > $(BUILDDIR)/kernel.bin.gz
	mkimage -A riscv -O linux -C gzip -T kernel -a 80200000 -e 80200000 -n "uCore-SMP" -d $(BUILDDIR)/kernel.bin.gz $(BUILDDIR)/ucore
	@echo 'Build kernel done'

clean: 
	rm -rf $(BUILDDIR) nfs/fs os/kernel_app.ld os/link_app.S os.bin

BOOTLOADER := ./bootloader/fw_jump.bin

QEMU = $(QEMU_5_0_0)/bin/qemu-system-riscv64

QEMUOPTS = \
	-nographic \
	-smp $(CPUS) \
	-machine virt \
	-m 256M \
	-bios $(BOOTLOADER) \
	-kernel build/kernel \
	-drive file=$(U)/$(FS_IMG),if=none,format=raw,id=x0 \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

run: build/kernel
	$(QEMU) $(QEMUOPTS)

QEMUGDB = -s
debug: build/kernel .gdbinit
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB) 

user:
	make -C user

doc:
	make -C doc/ori_doc

copy_bin:
	cp $(BUILDDIR)/ucore os.bin

