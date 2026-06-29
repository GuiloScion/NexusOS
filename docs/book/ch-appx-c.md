# Appendix C. Full source listings

Reference material. The book inlines the *interesting* parts of each file; this appendix gives you the full, unedited source of the four files that are short enough to print and central enough to the bring-up that you'll keep coming back to them: the `Makefile`, the kernel linker script, the 16-bit bootloader, and the 32-to-64-bit kernel entry stub.

If the line numbers in the book's listings ever fall out of sync with the source tree (a single commit will do it), this appendix is your fallback: pin to the `v1.0-book` git tag and the listings here are the literal contents you would check out.

## C.1 `Makefile` — the build pipeline

This is the file that ties everything together: assembling the bootloader to a flat binary, compiling each kernel `.c` and `.asm` to an ELF object, linking them at physical `0x10000` per `kernel/linker.ld`, stripping the ELF wrapper, and concatenating bootloader plus kernel into a bootable disk image. It also builds a FAT12 floppy image with mtools so the storage stack from Chapter 9 has something to read, and exposes the `run`, `rungui`, `text`, and `debug` targets the book refers to throughout.

The two non-obvious bits are worth a second look. `kernel_entry.o` is listed first in the link order so `_start` lands at the lowest address; reorder this and the kernel will not boot. The `FORCE` dependency on the bootloader is there because make cannot otherwise see that `BOOT_DEFINES` changed between a `make all` and a `make text`, and would silently reuse the previous build's bootloader against a newly-compiled kernel — a subtle hazard the comment calls out.

```makefile
.PHONY: all run rungui text clean debug FORCE

NASM    := nasm
CC      := gcc
LD      := ld
OBJCOPY := objcopy
QEMU    := qemu-system-x86_64

NASM_BOOT_FLAGS   := -f bin
NASM_KERNEL_FLAGS := -f elf64

# Extra nasm defines for the bootloader. `make text` sets -dTEXT_ONLY to skip
# VBE for firmware that hangs in the BIOS VBE calls under CSM.
BOOT_DEFINES ?=

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

# Bootloader -- depends on FORCE so it is always reassembled. make can't see
# changes to BOOT_DEFINES (e.g. `make all` vs `make text`), so without this a
# switch between GUI and text-only builds would silently reuse the stale image.
FORCE:

$(BOOTLOADER): bootloader/boot.asm FORCE | $(BUILD_DIR)
	$(NASM) $(NASM_BOOT_FLAGS) $(BOOT_DEFINES) $< -o $@

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
	mcopy -i $@ $(BUILD_DIR)/_hello.txt  ::HELLO.TXT

run: $(OS_IMAGE) $(FAT_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 \
	        -drive format=raw,file=$(FAT_IMAGE),if=ide,index=1 \
	        -m 256M \
	        -serial stdio -display none -no-reboot

# Text-only image: skips VBE for firmware that hangs in the BIOS VBE calls.
# Cleans first so the bootloader is reassembled with the new define.
text:
	$(MAKE) clean
	$(MAKE) BOOT_DEFINES=-dTEXT_ONLY all

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
```

## C.2 `kernel/linker.ld` — section layout

The linker script is what tells `ld` where each section of the kernel lives in memory, which is what makes the bootloader's jump to physical `0x10000` actually reach `_start`. Three things are worth pointing at: the `*(.text._start)` line is what forces `_start` to land first; `__bss_start` and `__bss_end` are exported so the entry stub can zero the BSS region before any C runs; and `__kernel_end` is exported so the physical-memory manager (Chapter 7) knows where the kernel image ends and free memory begins.

```ld
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text ALIGN(16) : {
        *(.text._start)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(16) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(16) : {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(16) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
```

## C.3 `bootloader/boot.asm` — the 512-byte MBR

This is the entire bootloader. It runs in 16-bit real mode at physical `0x7C00`, collects the E820 memory map (so the kernel will have it after BIOS is gone), optionally asks the video BIOS for a linear-framebuffer mode at 1024×768, reads the kernel from disk to `0x10000` via the `int 13h` LBA extensions, enables A20, loads a GDT covering 32- and 64-bit segments, switches to 32-bit protected mode, and far-jumps to the kernel. Everything fits in 510 bytes plus the `0x55 0xAA` boot signature.

```asm
[BITS 16]
[ORG 0x7C00]

; NexusOS BIOS bootloader - 512 byte MBR.
;
;   1. Collect the BIOS E820 memory map at 0x9000/0x9008.
;   2. Set a VESA linear-framebuffer mode (0x118); on failure the kernel
;      falls back to its text console.
;   3. Load the kernel to 0x10000 via int 13h LBA extensions.
;   4. Enable A20, install GDT, enter protected mode, jump to the kernel.

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl
    mov si, msg_loading
    call print_string

    ; ---------------- E820 memory map -------------------------------
    ;   dword [0x9000] = entry count, 24-byte entries from 0x9008.
    ;   (If firmware returns a map with no usable region, the kernel
    ;    substitutes a conservative default -- see pmm_init.)
    xor ax, ax
    mov es, ax
    mov di, 0x9008
    xor ebx, ebx
    xor bp, bp
.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150         ; "SMAP"
    mov ecx, 24
    mov dword [es:di + 20], 1   ; default ACPI 3.0 attr = "valid"
    int 0x15
    jc  .e820_done              ; CF -> end of list (or unsupported)
    cmp eax, 0x534D4150
    jne .e820_done
    add di, 24                  ; advance every entry -> loop must end
    inc bp
    test ebx, ebx               ; ebx == 0 -> last entry
    jz  .e820_done
    cmp bp, 64
    jb  .e820_loop
.e820_done:
    mov [0x9000], bp
    mov word [0x9002], 0
    mov word [0x9004], 0
    mov word [0x9006], 0

    ; ---------------- VBE linear-framebuffer mode ------------------
%ifdef TEXT_ONLY
    mov dword [0x9714], 0
%else
    ; Get mode info for 1024x768 (VBE 0x118), then set it with the LFB bit.
    mov ax, 0x4F01
    mov cx, 0x118
    mov di, 0x9800
    int 0x10
    cmp ax, 0x004F
    jne .vbe_fail
    mov ax, 0x4F02
    mov bx, 0x118
    or  bx, 0x4000             ; bit 14 = linear framebuffer
    int 0x10
    cmp ax, 0x004F
    jne .vbe_fail
    mov eax, [0x9800 + 40]
    mov [0x9700], eax
    movzx eax, word [0x9800 + 16]
    mov [0x9704], eax
    movzx eax, word [0x9800 + 18]
    mov [0x9708], eax
    movzx eax, word [0x9800 + 20]
    mov [0x970C], eax
    movzx eax, byte [0x9800 + 25]
    mov [0x9710], eax
    mov dword [0x9714], 1
    jmp .vbe_done
.vbe_fail:
    mov dword [0x9714], 0
.vbe_done:
%endif

    ; ---------------- Load kernel to 0x10000 (int 13h LBA) ----------
    mov si, kernel_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc  disk_error

    ; ---------------- A20 + GDT + PM --------------------------------
    in  al, 0x92
    or  al, 2
    out 0x92, al

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode

disk_error:
    mov si, msg_disk_err
    call print_string
.halt:
    cli
    hlt
    jmp .halt

print_string:
    mov ah, 0x0E
    mov bx, 0x0007             ; page 0, light-grey attribute
.loop:
    lodsb
    test al, al
    jz   .done
    int  0x10
    jmp  .loop
.done:
    ret

[BITS 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x08:0x10000

boot_drive:   db 0
%ifdef TEXT_ONLY
msg_loading:  db "NexusOS text-mode boot...", 0x0D, 0x0A, 0
%else
msg_loading:  db "Booting NexusOS...", 0x0D, 0x0A, 0
%endif
msg_disk_err: db "Disk read error!", 0

; Disk Address Packet for the int 13h LBA read of the kernel.
align 4
kernel_dap:
    db 0x10
    db 0
    dw 127
    dw 0x0000
    dw 0x1000
    dq 1

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF       ; 32-bit code
    dq 0x00CF92000000FFFF       ; 32-bit data
    dq 0x00AF9A000000FFFF       ; 64-bit code
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
```

## C.4 `kernel/kernel_entry.asm` — the 32-to-64 jump

The bootloader far-jumps to physical `0x10000` in 32-bit protected mode; this is the first code at that address. Its job is narrow: build a minimal set of bootstrap page tables that map the first 2 MiB, enable PAE and long mode, turn on paging, then far-jump into 64-bit `long_mode`, set up a stack, zero the BSS, and call `kernel_main` in C. Once the C kernel has a physical-memory allocator (Chapter 7), it builds proper full-coverage page tables and discards these.

```asm
; kernel_entry.asm
;
; Entered in 32-bit protected mode from the bootloader at physical
; 0x10000. Sets up identity-mapped paging for the first 2 MiB (with a
; single 2 MiB page), switches to long mode, and calls kernel_main.

extern kernel_main

section .text
global _start

[BITS 32]
_start:
    cli

    ; Zero 16 KiB of page-table scratch at 0x70000.
    mov edi, 0x70000
    xor eax, eax
    mov ecx, 0x4000 / 4
    rep stosd

    ; PML4[0] -> PDPT
    mov dword [0x70000], 0x71000 | 0x03
    ; PDPT[0] -> PD
    mov dword [0x71000], 0x72000 | 0x03
    ; PD[0] -> 2 MiB page at 0x0  (PS=1 in flags)
    mov dword [0x72000], 0x00000000 | 0x83

    mov eax, 0x70000
    mov cr3, eax

    mov eax, cr4
    or  eax, 1 << 5             ; CR4.PAE
    mov cr4, eax

    mov ecx, 0xC0000080         ; IA32_EFER
    rdmsr
    or  eax, 1 << 8             ; LME
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31            ; PG
    mov cr0, eax

    jmp 0x18:long_mode

[BITS 64]
long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, 0x90000

    ; Zero the .bss section. objcopy stripped it out of the binary,
    ; so the memory contents are undefined; clear them.
    extern __bss_start
    extern __bss_end
    mov   rdi, __bss_start
    mov   rcx, __bss_end
    sub   rcx, rdi
    shr   rcx, 3            ; bytes -> qwords
    xor   rax, rax
    cld
    rep stosq

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
```

## A note on staying in sync with the source

The line numbers in the book's listings match the `v1.0-book` git tag of the NexusOS repository. If you cloned a later commit and find the numbers do not line up with what you see, either checkout the tag (`git checkout v1.0-book`) or treat the function names in the captions as the authoritative pointer and the line numbers as approximate. Future printings of the book will re-cut the listings against the current tag.

