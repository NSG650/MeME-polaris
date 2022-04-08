DEBUG = off

override SYSTEM_CC_TARGET := $(shell gcc -dumpmachine | head -c 6)
ifeq ($(SYSTEM_CC_TARGET), x86_64)
CC := gcc
else
override SYSTEM_CC_TARGET := $(shell x86_64-elf-gcc -dumpmachine | head -c 6)
ifeq ($(SYSTEM_CC_TARGET), x86_64)
CC := x86_64-elf-gcc
else
$(error No suitable x86_64 GCC compiler found, please install an x86_64-elf-gcc toolchain)
endif
endif

override C_FILES := $(shell find -L src -type f -name '*.c')
override ASM_FILES := $(shell find -L src -type f -name '*.asm')
override OBJ := $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)

override CHARDFLAGS := \
    -std=gnu99 \
    -masm=intel \
    -fno-pic \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -ffreestanding \
    -fno-stack-protector \
    -I./src/

ifeq ($(DEBUG), qemu)
override CHARDFLAGS += -D_KERNEL_QEMU_OUTPUT_
else ifeq ($(DEBUG), vga)
override CHARDFLAGS += -D_KERNEL_VGA_OUTPUT_
endif

override CLINKFLAGS := \
    -nostdlib \
    -no-pie \
    -T ./linker.ld

CFLAGS = -Wall -Wextra -O2

.PHONY: all
all: image.hdd

image.hdd: kernel.elf
	[ -d limine ] || git clone https://github.com/limine-bootloader/limine.git --branch=v3.0-branch-binary --depth=1
	make -C limine
	rm -f $@
	dd if=/dev/zero bs=1M count=0 seek=64 of=$@
	parted -s $@ mklabel gpt
	parted -s $@ mkpart primary 2048s 100%
	echfs-utils -g -p0 $@ quick-format 512
	echfs-utils -g -p0 $@ import kernel.elf boot/kernel.elf
	echfs-utils -g -p0 $@ import limine.cfg boot/limine.cfg
	echfs-utils -g -p0 $@ import limine/limine.sys boot/limine.sys
	limine/limine-deploy $@

kernel.elf: $(OBJ)
	$(CC) $(OBJ) $(CLINKFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf64 -o $@

.PHONY: run
run: image.hdd
	qemu-system-x86_64 -net none -enable-kvm -cpu host -m 2G -hda image.hdd -debugcon stdio

.PHONY: clean
clean:
	rm -f $(OBJ) $(BINS) kernel.elf image.hdd

.PHONY: distclean
distclean: clean
	rm -rf limine
