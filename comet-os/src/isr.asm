; Comet OS - interrupt stubs (48 vectors)
bits 32
extern isr_handler
section .text
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47
isr_common:
    pushad
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call isr_handler
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popad
    add esp, 8
    iretd

section .data
global isr_table
isr_table:
    dd isr0
    dd isr1
    dd isr2
    dd isr3
    dd isr4
    dd isr5
    dd isr6
    dd isr7
    dd isr8
    dd isr9
    dd isr10
    dd isr11
    dd isr12
    dd isr13
    dd isr14
    dd isr15
    dd isr16
    dd isr17
    dd isr18
    dd isr19
    dd isr20
    dd isr21
    dd isr22
    dd isr23
    dd isr24
    dd isr25
    dd isr26
    dd isr27
    dd isr28
    dd isr29
    dd isr30
    dd isr31
    dd isr32
    dd isr33
    dd isr34
    dd isr35
    dd isr36
    dd isr37
    dd isr38
    dd isr39
    dd isr40
    dd isr41
    dd isr42
    dd isr43
    dd isr44
    dd isr45
    dd isr46
    dd isr47
