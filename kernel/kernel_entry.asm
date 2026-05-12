; kernel_entry.asm
;
; Entry stub for NexusOS kernel.
; The bootloader hands off in 32-bit protected mode with:
;   - flat 4 GiB code/data segments
;   - CS = 0x08, DS/ES/FS/GS/SS = 0x10
;   - the kernel image starts at physical 0x10000
;   - GDT (set up by bootloader) contains a 64-bit code segment at 0x18
;
; This stub sets up identity-mapped paging for the low 2 MiB using a
; single 2 MiB page (PS bit in the PD), transitions to long mode, and
; calls kernel_main.

extern kernel_main

section .text
global _start

[BITS 32]
_start:
    cli

    ; ---- Build minimal page tables at 0x70000 ----
    ; Layout:
    ;   0x70000 = PML4   (entry 0 -> PDPT)
    ;   0x71000 = PDPT   (entry 0 -> PD)
    ;   0x72000 = PD     (entry 0 -> 2 MiB page covering 0..2 MiB)
    ;
    ; A single 2 MiB page is enough to cover both our kernel
    ; (at 0x10000) and the VGA buffer (at 0xB8000).

    ; Zero 16 KiB starting at 0x70000
    mov edi, 0x70000
    xor eax, eax
    mov ecx, 0x4000 / 4
    rep stosd

    ; PML4[0] -> PDPT, Present | Writable
    mov dword [0x70000], 0x71000 | 0x03

    ; PDPT[0] -> PD, Present | Writable
    mov dword [0x71000], 0x72000 | 0x03

    ; PD[0] -> 2 MiB page at 0x00000000
    ; Present | Writable | PageSize(=1, 2 MiB page)
    mov dword [0x72000], 0x00000000 | 0x83

    ; Load CR3 with the PML4 address
    mov eax, 0x70000
    mov cr3, eax

    ; Enable PAE (CR4.PAE = 1)
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; Enable long mode (EFER.LME = 1)
    mov ecx, 0xC0000080      ; IA32_EFER
    rdmsr
    or  eax, 1 << 8          ; LME
    wrmsr

    ; Enable paging (CR0.PG = 1) -> activates long mode
    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    ; Far jump into 64-bit code segment (selector 0x18 from bootloader GDT)
    jmp 0x18:long_mode

[BITS 64]
long_mode:
    ; In long mode, data segment registers are largely ignored, but we
    ; load a defined value to keep things tidy.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, 0x90000

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
