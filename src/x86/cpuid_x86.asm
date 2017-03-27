%include "common.asm"

section .text

;int
;cpuid_x86(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx)

PROC cpuid_x86
    ; save registers
    push ebx
    push ecx
    push edx
    push edi
    ; cpuid
    mov eax, [esp + 20]
    mov ecx, [esp + 24]
    cpuid
    mov edi, [esp + 28]
    mov [edi], eax
    mov edi, [esp + 32]
    mov [edi], ebx
    mov edi, [esp + 36]
    mov [edi], ecx
    mov edi, [esp + 40]
    mov [edi], edx
    mov eax, 0
    ; restore registers
    pop edi
    pop edx
    pop ecx
    pop ebx
    ret;
    align 16

