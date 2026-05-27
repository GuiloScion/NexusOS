.PHONY: all run rungui clean debug

NASM    := nasm
CC      := gcc
LD      := ld
OBJCOPY := objcopy
QEMU    := qemu-system-x86_64

NASM_BOOT_FLAGS   := -f bin
NASM_KERNEL_FLAGS := -f elf64

CC_FLAGS := -ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
            -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
            -m64 -Wall -Wextra -std=c11 -c

LD_FLAGS := -m elf_x86_64 -nostdlib -T kernel/linker.ld

BUILD_DIR  := build

# --- Sources ---------------------------------------------------------
KERNEL_C_SRCS   := $(wildcard kernel/*.c)
KERNEL_ASM_SRCS := $(filter-out kernel/kernel_entry.asm,$(wildcard kernel/*.asm))

KERNEL_C_OBJS   := $(patsubst kernel/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS))
KERNEL_ASM_OBJS := $(patsubst kernel/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

# kernel_entry.o must be first so _start lands at 0x10000
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
KERNEL_OBJS      := $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

BOOTLOADER := $(BUILD_DIR)/bootloader.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
OS_IMAGE   := $(BUILD_DIR)/os.bin
FAT_IMAGE  := $(BUILD_DIR)/fat.img

all: $(OS_IMAGE) $(FAT_IMAGE)

$(BUILD_DIR):
	mkdir -p $@

# Bootloader
$(BOOTLOADER): bootloader/boot.asm | $(BUILD_DIR)
	$(NASM) $(NASM_BOOT_FLAGS) $< -o $@

# Kernel entry stub (explicit rule)
$(KERNEL_ENTRY_OBJ): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) $(NASM_KERNEL_FLAGS) $< -o $@

# Generic rules
$(BUILD_DIR)/%.o: kernel/%.c | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) $< -o $@

$(BUILD_DIR)/%.o: kernel/%.asm | $(BUILD_DIR)
	$(NASM) $(NASM_KERNEL_FLAGS) $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) kernel/linker.ld
	$(LD) $(LD_FLAGS) $(KERNEL_OBJS) -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(OS_IMAGE): $(BOOTLOADER) $(KERNEL_BIN)
	cat $(BOOTLOADER) $(KERNEL_BIN) > $@
	@truncate -s '>1M' $@

# --- FAT image -------------------------------------------------------
# 1.44 MB floppy formatted FAT12 with a couple of sample files.
# Uses mtools (mformat, mcopy) so we don't need root or a loopback mount.
$(FAT_IMAGE): | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1024 count=1440 status=none
	mformat -i $@ -f 1440 ::
	@printf 'hello, world from a FAT12 filesystem\n' > $(BUILD_DIR)/_hello.txt
	@printf 'NexusOS storage stack test file.\nIf you can read this, ATA + FAT12 work.\n' > $(BUILD_DIR)/_readme.txt
	mcopy -i $@ $(BUILD_DIR)/_hello.txt  ::HELLO.TXT
	mcopy -i $@ $(BUILD_DIR)/_readme.txt ::README.TXT

run: $(OS_IMAGE) $(FAT_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 \
	        -drive format=raw,file=$(FAT_IMAGE),if=ide,index=1 \
	        -m 256M \
	        -serial stdio -display none -no-reboot

# Interactive GUI window (needs a display, e.g. WSLg on Windows). Click into
# the window to grab the mouse; Ctrl-Alt-G releases it.
rungui: $(OS_IMAGE) $(FAT_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 \
	        -drive format=raw,file=$(FAT_IMAGE),if=ide,index=1 \
	        -m 256M \
	        -serial stdio -display gtk -no-reboot

debug: $(OS_IMAGE) $(FAT_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 \
	        -drive format=raw,file=$(FAT_IMAGE),if=ide,index=1 \
	        -m 256M \
	        -serial stdio -display none -s -S &
	gdb -ex "target remote localhost:1234" \
	    -ex "symbol-file $(KERNEL_ELF)" \
	    -ex "break kernel_main"

clean:
	rm -rf $(BUILD_DIR)
