; Comet OS - multiboot1 entry (GRUB sets up a linear 32bpp framebuffer for us)
MB_MAGIC  equ 0x1BADB002
MB_FLAGS  equ 0x00000007            ; align modules | meminfo | video mode
bits 32
section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd -(MB_MAGIC + MB_FLAGS)
    dd 0, 0, 0, 0, 0                ; address fields (unused for ELF)
    dd 0                            ; linear graphics mode
    dd 1280, 800, 32                ; preferred resolution

section .bss
align 16
stack_bottom: resb 131072
stack_top:

section .text
global _start
global gdt_flush
extern kmain
_start:
    cli
    mov esp, stack_top
    push dword 0
    popfd
    push ebx                        ; multiboot info
    push eax                        ; magic
    call kmain
.hang:
    cli
    hlt
    jmp .hang

gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret
