[BITS 16]

extern kernel_main

section .text
    global _start

_start:
    ; In real mode, loaded at 0x1000:0x0000
    ; Switch to protected mode first
    
    cli                     ; Disable interrupts
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Set PE bit in CR0 to enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to 32-bit code
    jmp 0x08:protected_mode

[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, 0x90000
    
    ; Now switch to long mode (64-bit)
    ; Enable PAE
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    
    ; Set up page table at 0x70000
    mov edi, 0x70000
    xor eax, eax
    mov ecx, 4096
    rep stosd
    
    ; PML4 at 0x70000
    mov dword [0x70000], 0x71000 | 3
    ; PDPT at 0x71000
    mov dword [0x71000], 0x72000 | 3
    ; PDT at 0x72000
    mov dword [0x72000], 0x73000 | 3
    ; PT at 0x73000
    mov dword [0x73000], 0 | 3
    
    ; Set CR3 to page table
    mov eax, 0x70000
    mov cr3, eax
    
    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    ; Far jump to 64-bit code
    jmp 0x18:long_mode

[BITS 64]
long_mode:
    ; We're now in 64-bit long mode
    xor rax, rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up 64-bit stack
    mov rsp, 0x90000
    
    ; Call kernel main
    call kernel_main
    
    ; Halt
    cli
    hlt

; GDT for protected mode
gdt_start:
    dq 0                    ; Null descriptor
    dq 0x00cf9a000000ffff   ; Code segment (32-bit)
    dq 0x00cf92000000ffff   ; Data segment (32-bit)
    dq 0x00af9a000000ffff   ; Code segment (64-bit)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
