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
    dw site_timer_a_start, site_timer_a_end
    dw site_timer_b_start, site_timer_b_end

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

    cli
    mov bx, 2730h
    call fm_write
    mov bx, 2400h
    call fm_write
    mov bx, 2500h
    call fm_write
    mov bx, 2705h
    mov dx, 188h
    mov al, bh
    out dx, al
    add dx, 2
    mov al, bl
site_timer_a_start:
    out dx, al
    sub dx, 2
.timer_a:
    in al, dx
    test al, 1
    jz .timer_a
site_timer_a_end:
    mov bx, 2730h
    call fm_write
    mov bx, 2600h
    call fm_write
    mov bx, 270ah
    mov dx, 188h
    mov al, bh
    out dx, al
    add dx, 2
    mov al, bl
site_timer_b_start:
    out dx, al
    sub dx, 2
.timer_b:
    in al, dx
    test al, 2
    jz .timer_b
site_timer_b_end:
    mov bx, 2730h
    call fm_write
    sti

    push es
    mov ax, 0a000h
    mov es, ax
    mov word [es:0], 'A'
    mov word [es:2000h], 0e1h
%assign row 5
%rep 3
    mov word [es:row * 160 + 8], ' '
    mov word [es:row * 160 + 2008h], 0e9h
    mov word [es:row * 160 + 10], ' '
    mov word [es:row * 160 + 200ah], 081h
%assign row row + 9
%endrep
    mov ah, 42h
    mov ch, 0c0h
    int 18h
    mov ah, 40h
    int 18h
    mov ax, 0a800h
    mov es, ax
    mov byte [es:200 * 80], 0ffh
    mov al, 1
    out 6ah, al
    out 0a6h, al
    mov ax, 0b000h
    mov es, ax
    mov byte [es:200 * 80 + 2], 0ffh
    xor al, al
    out 0a6h, al
    mov al, 1
    out 0a8h, al
    xor al, al
    out 0aah, al
    out 0ach, al
    mov al, 15
    out 0aeh, al
    mov al, 2
    out 0a8h, al
    mov al, 3
    out 0aah, al
    mov al, 5
    out 0ach, al
    mov al, 9
    out 0aeh, al
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

fm_write:
    mov dx, 188h
    mov al, bh
    out dx, al
    add dx, 2
    mov al, bl
    out dx, al
    ret

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
