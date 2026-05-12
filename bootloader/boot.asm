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
    
    ; Load kernel (simplified - just halt for now)
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

; Padding to 510 bytes
times 510 - ($ - $$) db 0

; Boot signature
dw 0xAA55
