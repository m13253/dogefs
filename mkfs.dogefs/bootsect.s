;
; Copyright (C) 2017 Yuchen Ma <15208850@hdu.edu.cn>
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

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
