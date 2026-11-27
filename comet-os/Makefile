# Comet OS - build with:  make iso      (or ./build.sh)
CC      ?= gcc
LD      ?= ld
NASM    ?= nasm
CFLAGS  := -m32 -std=gnu11 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-80387 -nostdlib \
           -Wall -Wno-misleading-indentation -Wno-unused-function -Wno-unused-variable
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib -z max-page-size=0x1000 --build-id=none
BUILD   := build
CSRC    := $(wildcard src/*.c)
COBJ    := $(patsubst src/%.c,$(BUILD)/%.o,$(CSRC))
AOBJ    := $(BUILD)/boot.o $(BUILD)/isr.o
ISO     := comet-os.iso

all: iso
$(BUILD)/%.o: src/%.c src/kernel.h src/font.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: src/%.asm
	@mkdir -p $(BUILD)
	$(NASM) -f elf32 $< -o $@
$(BUILD)/comet.elf: $(AOBJ) $(COBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(AOBJ) $(COBJ)
	@grub-file --is-x86-multiboot $@ 2>/dev/null && echo "multiboot header OK" || true
iso: $(BUILD)/comet.elf grub/grub.cfg
	@rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/comet.elf $(BUILD)/iso/boot/comet.elf
	cp grub/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(BUILD)/iso 2>&1 | tail -3
	@echo "==> built $(ISO)"
run-qemu: iso
	qemu-system-i386 -cdrom $(ISO) -m 512 -vga std -boot d
clean:
	rm -rf $(BUILD) $(ISO)
.PHONY: all iso run-qemu clean
