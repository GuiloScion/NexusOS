[BITS 16]
[ORG 0x7C00]

; NexusOS BIOS bootloader - 512 byte MBR
; Loads kernel to 0x10000, transitions to 32-bit protected mode,
; then jumps to the kernel entry point.

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Preserve the BIOS-provided boot drive number in DL
    mov [boot_drive], dl

    ; Greeting
    mov si, msg_loading
    call print_string

    ; ---- Load kernel from disk to 0x10000 ----
    ; ES:BX = 0x1000:0x0000  ->  physical 0x10000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02          ; BIOS read sectors
    mov al, 32            ; 32 sectors = 16 KiB (room to grow)
    mov ch, 0             ; cylinder 0
    mov cl, 2             ; start at sector 2 (sector 1 = MBR)
    mov dh, 0             ; head 0
    mov dl, [boot_drive]
    int 0x13
    jc  disk_error

    ; ---- Enable A20 via fast gate (port 0x92) ----
    in  al, 0x92
    or  al, 2
    out 0x92, al

    ; ---- Load GDT and enter protected mode ----
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    ; Far jump flushes pipeline and loads 32-bit CS
    jmp 0x08:protected_mode

disk_error:
    mov si, msg_disk_err
    call print_string
.halt:
    cli
    hlt
    jmp .halt

; --- 16-bit helpers ---
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

; --- 32-bit protected mode entry ---
[BITS 32]
protected_mode:
    mov ax, 0x10          ; data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Jump to the kernel entry point loaded at 0x10000.
    ; kernel_entry.asm starts with [BITS 32] code.
    jmp 0x08:0x10000

; --- Data ---
boot_drive:   db 0
msg_loading:  db "Booting NexusOS...", 0x0D, 0x0A, 0
msg_disk_err: db "Disk read error!", 0

; --- GDT ---
; 0x00 = null, 0x08 = 32-bit code, 0x10 = 32-bit data,
; 0x18 = 64-bit code (used later by kernel_entry to enter long mode)
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF   ; 32-bit code, base=0, limit=4G
    dq 0x00CF92000000FFFF   ; 32-bit data, base=0, limit=4G
    dq 0x00AF9A000000FFFF   ; 64-bit code (L=1)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Pad to 510 bytes + boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
