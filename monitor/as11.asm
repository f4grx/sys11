/* 68HC11 assembler */
/* Used to test commands */

	.include "softregs.inc"
	.include "serial.inc"

	.section .scommands
	.asciz	"as11"
	.word	as11

	.section .rodata
msg:	.asciz "AS11 not implemented yet"

	.text

	.func	as11
	.global as11
as11:
	ldx	#msg
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf
	rts
	.endfunc

