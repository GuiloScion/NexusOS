.PHONY: all run clean debug

# Tools
NASM    := nasm
CC      := gcc
LD      := ld
OBJCOPY := objcopy
QEMU    := qemu-system-x86_64

# Flags
NASM_BOOT_FLAGS   := -f bin
NASM_KERNEL_FLAGS := -f elf64

CC_FLAGS := -ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
            -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
            -m64 -Wall -Wextra -c

LD_FLAGS := -m elf_x86_64 -nostdlib -T kernel/linker.ld

# Paths
BUILD_DIR        := build
BOOTLOADER       := $(BUILD_DIR)/bootloader.bin
KERNEL_OBJ       := $(BUILD_DIR)/kernel.o
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
KERNEL_ELF       := $(BUILD_DIR)/kernel.elf
KERNEL_BIN       := $(BUILD_DIR)/kernel.bin
OS_IMAGE         := $(BUILD_DIR)/os.bin

all: $(OS_IMAGE)

$(BUILD_DIR):
	mkdir -p $@

# Bootloader: flat binary, 512 bytes
$(BOOTLOADER): bootloader/boot.asm | $(BUILD_DIR)
	$(NASM) $(NASM_BOOT_FLAGS) $< -o $@

# Kernel entry stub: ELF64 object
$(KERNEL_ENTRY_OBJ): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) $(NASM_KERNEL_FLAGS) $< -o $@

# Kernel C code: ELF64 object
$(KERNEL_OBJ): kernel/kernel.c | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) $< -o $@

# Link kernel to ELF (entry stub MUST come first so _start is at 0x10000)
$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) kernel/linker.ld
	$(LD) $(LD_FLAGS) $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) -o $@

# Flatten ELF to raw binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# Final disk image: bootloader (sector 1) + kernel (sectors 2..N)
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL_BIN)
	cat $(BOOTLOADER) $(KERNEL_BIN) > $@
	@truncate -s %512 $@

# Codespaces / headless: serial to terminal, no graphical display
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) \
	        -serial stdio -display none -no-reboot

debug: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) \
	        -serial stdio -display none -s -S &
	gdb -ex "target remote localhost:1234" \
	    -ex "symbol-file $(KERNEL_ELF)" \
	    -ex "break *0x10000"

clean:
	rm -rf $(BUILD_DIR)
