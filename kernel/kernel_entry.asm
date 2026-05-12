[BITS 64]

extern kernel_main

section .text
    global _start

_start:
    ; Call kernel main function
    call kernel_main
    
    ; Halt if kernel_main returns
    cli
    hlt
