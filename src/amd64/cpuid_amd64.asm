%include "common.asm"

section .text

;The first six integer or pointer arguments are passed in registers
;RDI, RSI, RDX, RCX, R8, and R9

;int
;cpuid_amd64(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx)

PROC cpuid_amd64
    ; save registers
    push rbx

    push rdx
    push rcx
    push r8
    push r9

    mov rax, rdi
    mov rcx, rsi
    cpuid
    pop rdi
    mov [rdi], edx
    pop rdi
    mov [rdi], ecx
    pop rdi
    mov [rdi], ebx
    pop rdi
    mov [rdi], eax
    mov rax, 0
    ; restore registers
    pop rbx
    ret
    align 16

