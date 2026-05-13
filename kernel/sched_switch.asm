; sched_switch.asm -- the bottom half of a context switch.
;
; void _switch_to(uint64_t new_rsp);   // System V: rdi = new_rsp
;
; Assumes new_rsp points to a fully-formed interrupt frame (15 saved GPRs,
; then vector + error_code, then the CPU-pushed iretq frame: rip/cs/rflags/
; rsp/ss). The pop order and `add rsp, 16` MUST mirror isr_common in
; idt_stubs.asm exactly. This routine does not return -- iretq jumps to
; whatever rip is in the new frame.

section .text
bits 64
global _switch_to

_switch_to:
    mov     rsp, rdi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    add     rsp, 16        ; discard saved vector + error_code
    iretq
