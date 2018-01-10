[ORG 0x7c00]

	jmp	main
	times	16-($-$$) db 0xcc
	times	96-($-$$) db 0
msg	db	'Error: This device is not bootable.', 13, 10, 0
main:
	xor	ax, ax
	mov	ds, ax
	mov	si, msg
loop:
	lodsb
	or	al, al
	jz	hang
	mov	ah, 0x0e
	int	0x10
	jmp	loop
hang:
	hlt
	jmp	hang
	times	5 db 0xcc
	times	510-($-$$) db 0
	dw	0xaa55
