[BITS 16]
[ORG 0x7C00]

; NexusOS BIOS bootloader - 512 byte MBR.
;
; Responsibilities:
;   1. Read the physical memory map from BIOS (int 0x15, eax=0xE820)
;      and stash it at 0x9000 (count) + 0x9008 (24-byte entries).
;      Must be done in real mode -- BIOS is gone once left.
;   2. Load the kernel image to physical 0x10000.
;   3. Enable A20, install GDT, enter 32-bit protected mode.
;   4. Far-jump to the kernel entry stub at 0x10000.

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
    ; Output:
    ;   dword [0x9000] = entry count
    ;   24-byte entries starting at 0x9008
    xor ax, ax
    mov es, ax
    mov di, 0x9008
    xor ebx, ebx
    xor bp, bp                  ; bp = entry count
.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150         ; "SMAP"
    mov ecx, 24
    mov dword [es:di + 20], 1   ; default ACPI 3.0 attr = "valid"
    int 0x15
    jc  .e820_done              ; CF on first call -> unsupported
    cmp eax, 0x534D4150
    jne .e820_done
    cmp ecx, 20
    jb  .e820_skip
    inc bp
    add di, 24
.e820_skip:
    test ebx, ebx
    jz  .e820_done
    cmp bp, 64                  ; cap at 64 entries (1.5 KiB)
    jb  .e820_loop
.e820_done:
    mov [0x9000], bp
    mov word [0x9002], 0
    mov word [0x9004], 0
    mov word [0x9006], 0

    ; ---------------- Load kernel to 0x10000 ------------------------
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 128                 ; 128 sectors = 64 KiB (room to grow)
    mov ch, 0
    mov cl, 2                   ; first kernel sector
    mov dh, 0
    mov dl, [boot_drive]
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
msg_loading:  db "Booting NexusOS...", 0x0D, 0x0A, 0
msg_disk_err: db "Disk read error!", 0

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
