[BITS 16]
[ORG 0x1000]

extern kernel_main

section .text
    global _start

_start:
    cli
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to protected mode
    jmp 0x08:protected_mode

[BITS 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov esp, 0x90000
    
    ; Enable PAE
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    
    ; Set up paging
    mov edi, 0x70000
    xor eax, eax
    mov ecx, 0x1000
    rep stosd
    
    ; PML4
    mov dword [0x70000], 0x71000 | 3
    mov dword [0x70004], 0
    
    ; PDPT
    mov dword [0x71000], 0x72000 | 3
    mov dword [0x71004], 0
    
    ; PDT
    mov dword [0x72000], 0x73000 | 3
    mov dword [0x72004], 0
    
    ; PT
    mov dword [0x73000], 0 | 3
    mov dword [0x73004], 0
    
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
    
    jmp 0x18:long_mode

[BITS 64]
long_mode:
    xor rax, rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov rsp, 0x90000
    
    call kernel_main
    
    cli
    hlt

section .data
    align 16
    gdt:
        dq 0
        dq 0x00cf9a000000ffff
        dq 0x00cf92000000ffff
        dq 0x00af9a000000ffff
    
    gdt_descriptor:
        dw gdt_end - gdt - 1
        dq gdt
    gdt_end:
