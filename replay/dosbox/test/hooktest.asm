; The guest of hooktest.c: a COM program that checks a hidden file, writes
; OUT.TXT, draws one text cell and one graphics byte, and counts frames by
; polling the GDC's vblank bit, with an IRQ 2 handler that counts VSyncs.
; Exits with status 7. The table at 102h gives hooktest.c its addresses.

cpu 386
org 100h

start:
    jmp short main
table:
    dw site_regs, site_vsync, site_vblank_end
    dw regs_value, frames, irqs, limit

main:
    mov ax, 3d00h
    mov dx, secret
    int 21h
    mov byte [hidden], '1'
    jc .create
    mov byte [hidden], '0'
.create:
    mov ah, 3ch
    xor cx, cx
    mov dx, outname
    int 21h
    jc fail
    mov bx, ax
    mov ah, 40h
    mov cx, 1
    mov dx, hidden
    int 21h
    mov ah, 3eh
    int 21h

    xor ax, ax
site_regs:
    mov [regs_value], ax

    push es
    mov ax, 0a000h
    mov es, ax
    mov word [es:0], 'A'
    mov word [es:2000h], 0e1h
    mov ah, 42h
    mov ch, 0c0h
    int 18h
    mov ah, 40h
    int 18h
    mov ax, 0a800h
    mov es, ax
    mov byte [es:200 * 80], 0ffh
    pop es

    cli
    push ds
    xor ax, ax
    mov ds, ax
    mov ax, [0ah * 4]
    mov dx, [0ah * 4 + 2]
    mov word [0ah * 4], irq2
    mov [0ah * 4 + 2], cs
    pop ds
    mov [old_vector], ax
    mov [old_vector + 2], dx
    in al, 02h
    and al, 0fbh
    out 02h, al
    out 64h, al
    sti

frame:
    in al, 60h
    test al, 20h
site_vsync:
    jz frame
    inc word [frames]
wait_end:
    in al, 60h
    test al, 20h
site_vblank_end:
    jnz wait_end
    mov ax, [frames]
    cmp ax, [limit]
    jb frame

    cli
    in al, 02h
    or al, 04h
    out 02h, al
    mov ax, [old_vector]
    mov dx, [old_vector + 2]
    push ds
    xor bx, bx
    mov ds, bx
    mov [0ah * 4], ax
    mov [0ah * 4 + 2], dx
    pop ds
    sti
    mov ax, 4c07h
    int 21h

fail:
    mov ax, 4c01h
    int 21h

irq2:
    push ax
    push ds
    mov ax, cs
    mov ds, ax
    inc word [irqs]
    out 64h, al
    mov al, 20h
    out 00h, al
    pop ds
    pop ax
    iret

secret db 'SECRET.TXT', 0
outname db 'OUT.TXT', 0
hidden db 0
align 2
regs_value dw 0
frames dw 0
irqs dw 0
limit dw 0ffffh
old_vector dd 0
