; The guest of the dostest port (port.c): a COM program with one site of
; each kind. Each loop waits for one VSync as Borland frame_delay does, on
; a counter of its IRQ 2 handler, and then reads the keys. While the shot
; key is held, it polls the keys with no wait. Every eighth loop it waits
; for the GDC's vblank as z_vsync_wait does, and every sixteenth it calls a
; far delay of 50 ms that never returns by itself. It exits once the ok
; key is held. It reads the first byte of STAGE.TXT, and with H there it
; hangs after frame 5. The data lies at 104h for port.c.

cpu 386
org 100h

start:
    jmp near main
    nop

; port.c reads these words.
frames dw 0
vsyncs dw 0
count1 dw 0
keys dw 0
last_key dw 0
presses dw 0
polls dw 0
gdc_waits dw 0
delays dw 0
staged dw 0
old_vector dd 0

stage_name db 'STAGE.TXT', 0
out_name db 'OUT.TXT', 0
out_text db 'ok'

main:
    mov ax, 3d00h
    mov dx, stage_name
    int 21h
    jc .write
    mov bx, ax
    mov ah, 3fh
    mov cx, 1
    mov dx, staged
    int 21h
    mov ah, 3eh
    int 21h
.write:
    mov ah, 3ch
    xor cx, cx
    mov dx, out_name
    int 21h
    jc fail
    mov bx, ax
    mov ah, 40h
    mov cx, 2
    mov dx, out_text
    int 21h
    mov ah, 3eh
    int 21h

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

next:
    mov word [count1], 0
wait_vsync:
    cmp word [count1], 1
site_vsync:
    jb wait_vsync
    inc word [frames]
site_call:
    nop

    call read_keys
    mov ah, 01h
    int 18h
    test bh, bh
    jz check_hang
    mov ah, 00h
    int 18h
    mov [last_key], ax
    inc word [presses]

check_hang:
    cmp byte [staged], 'H'
    jne check_poll
    cmp word [frames], 5
    jb check_poll
hang:
    jmp short hang

check_poll:
    test byte [keys], 01h
    jz check_gdc
poll:
    inc word [polls]
    call read_keys
    test byte [keys], 01h
site_poll:
    jnz poll

check_gdc:
    test word [frames], 7
    jnz check_delay
vblank_end:
    in al, 60h
    test al, 20h
site_vblank_end:
    jnz vblank_end
gdc_vsync:
    in al, 60h
    test al, 20h
site_gdc_vsync:
    jz gdc_vsync
    inc word [gdc_waits]

check_delay:
    test word [frames], 15
    jnz check_done
    push word 50
    push cs
    call delay
    add sp, 2
    inc word [delays]

check_done:
    test byte [keys], 02h
    jz next

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
    mov ax, 4c00h
    int 21h

fail:
    mov ax, 4c01h
    int 21h

; Sets keys: bit 0 for Z, the shot key, and bit 1 for Return, the ok key.
read_keys:
    xor dl, dl
    mov ax, 0405h
    int 18h
    test ah, 02h
    jz .ok
    or dl, 01h
.ok:
    mov ax, 0403h
    int 18h
    test ah, 10h
    jz .set
    or dl, 02h
.set:
    mov [keys], dl
    ret

; A far procedure of one word argument that only the harness ends.
delay:
site_delay:
    jmp short delay

irq2:
    push ax
    push ds
    mov ax, cs
    mov ds, ax
    inc word [count1]
    inc word [vsyncs]
    out 64h, al
    mov al, 20h
    out 00h, al
    pop ds
    pop ax
    iret
