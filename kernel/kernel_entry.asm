; kernel_entry.asm
;
; Entered in 32-bit protected mode from the bootloader at physical
; 0x10000. Sets up identity-mapped paging for the first 2 MiB (with a
; single 2 MiB page), switches to long mode, and calls kernel_main.
;
; The new VMM in kernel.c will rebuild proper 4-level page tables once
; the PMM is available. This stub only needs enough mapping to reach
; that point.

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
