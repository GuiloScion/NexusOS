; idt_stubs.asm
;
; One stub per interrupt vector (256 total). Each stub:
;   1. pushes a dummy 0 if the CPU did not push an error code
;   2. pushes the vector number
;   3. jumps to isr_common, which saves GPRs and calls the C dispatcher
;
; The address of every stub is exported through isr_stub_table so the C
; side can install them into the IDT without having to declare 256
; separate externs.

[BITS 64]

extern interrupt_dispatch

section .text

; --- Macros for stub generation -------------------------------------

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0           ; fake error code
    push qword %1          ; vector
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    ; CPU already pushed an error code
    push qword %1          ; vector
    jmp isr_common
%endmacro

; --- Common tail ----------------------------------------------------

isr_common:
    ; Save the general-purpose registers in the order declared in
    ; interrupt_frame_t (top of struct = top of stack after pushes).
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    cld
    mov  rdi, rsp           ; pass frame pointer
    call interrupt_dispatch

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    add rsp, 16             ; drop vector + error code
    iretq

; --- Stub instances -------------------------------------------------
; Vectors that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30.

%assign i 0
%rep 256
    %if (i == 8) || (i == 10) || (i == 11) || (i == 12) || (i == 13) || \
        (i == 14) || (i == 17) || (i == 21) || (i == 29) || (i == 30)
        ISR_ERR i
    %else
        ISR_NOERR i
    %endif
    %assign i i+1
%endrep

; --- Table of stub addresses for the C side -------------------------

section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_ %+ i
    %assign i i+1
%endrep
