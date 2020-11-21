DEBUG = off

SYSTEM_CC_TARGET := $(shell gcc -dumpmachine | head -c 6)
ifeq ($(SYSTEM_CC_TARGET), x86_64)
CC = gcc
else
SYSTEM_CC_TARGET := $(shell x86_64-elf-gcc -dumpmachine | head -c 6)
ifeq ($(SYSTEM_CC_TARGET), x86_64)
CC = x86_64-elf-gcc
else
$(error No suitable x86_64 GCC compiler found, please install an x86_64-elf-gcc toolchain)
endif
endif

C_FILES = $(shell find -L src -type f -name '*.c')
ASM_FILES = $(shell find -L src -type f -name '*.asm')
OBJ = $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)

CHARDFLAGS := \
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
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_QEMU_OUTPUT_
else ifeq ($(DEBUG), vga)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_VGA_OUTPUT_
endif

CLINKFLAGS := \
    -nostdlib \
    -no-pie \
    -T ./linker.ld

CFLAGS = -Wall -Wextra -O2

.PHONY: clean run

image.hdd: kernel.elf
	[ -d limine ] || git clone https://github.com/limine-bootloader/limine.git --branch=v0.6.3 --depth=1
	make -C limine limine-install
	rm -f $@
	dd if=/dev/zero bs=1M count=0 seek=64 of=$@
	parted -s $@ mklabel gpt
	parted -s $@ mkpart primary 2048s 6143s
	parted -s $@ mkpart primary 6144s 131038s
	echfs-utils -g -p1 $@ quick-format 512
	echfs-utils -g -p1 $@ import kernel.elf boot/kernel.elf
	echfs-utils -g -p1 $@ import limine.cfg boot/limine.cfg
	limine/limine-install limine/limine.bin $@ 2048

kernel.elf: $(OBJ)
	$(CC) $(OBJ) $(CLINKFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf64 -o $@

run: image.hdd
	qemu-system-x86_64 -net none -enable-kvm -cpu host -m 2G -hda image.hdd -debugcon stdio

clean:
	rm -f $(OBJ) $(BINS) kernel.elf
