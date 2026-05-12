[BITS 16]
[ORG 0x7C00]

; NexusOS BIOS bootloader - 512 byte MBR

start:
    ; Initialize registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Clear screen
    mov ax, 0x0003
    int 0x10
    
    ; Display welcome message
    mov si, msg_welcome
    call print_string
    
    ; Load kernel from disk (simplified - load sectors 1-3)
    mov si, msg_loading
    call print_string
    
    ; Read kernel from disk
    mov ah, 0x02        ; BIOS read sectors
    mov al, 4           ; Read 4 sectors
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start at sector 2 (sector 1 is bootloader)
    mov dh, 0           ; Head 0
    mov dl, 0x80        ; Drive 0 (first hard drive)
    mov bx, 0x1000      ; Load to 0x1000
    mov es, bx
    xor bx, bx
    int 0x13
    
    jc disk_error
    
    mov si, msg_kernel_loaded
    call print_string
    
    ; Jump to kernel
    jmp 0x1000:0x0000

disk_error:
    mov si, msg_error
    call print_string
    jmp halt

print_string:
    ; Print string at DS:SI
    mov ah, 0x0E        ; BIOS print character function
.loop:
    lodsb               ; Load byte from SI into AL
    cmp al, 0           ; Check for null terminator
    je .done
    int 0x10            ; BIOS interrupt
    jmp .loop
.done:
    ret

halt:
    cli                 ; Disable interrupts
    hlt                 ; Halt CPU
    jmp halt

msg_welcome: db "NexusOS Bootloader Loading...", 0x0D, 0x0A, 0
msg_loading: db "Loading kernel from disk...", 0x0D, 0x0A, 0
msg_kernel_loaded: db "Kernel loaded! Jumping to kernel...", 0x0D, 0x0A, 0
msg_error: db "Disk read error!", 0x0D, 0x0A, 0

; Padding to 510 bytes
times 510 - ($ - $$) db 0

; Boot signature
dw 0xAA55
