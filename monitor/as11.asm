/* hc11 assembler app for shell */

	.include "softregs.inc"
	.include "serial.inc"

	.section .scommands
	.asciz	"as11"
	.word	as11

	.section .rodata
msg:	.asciz "AS11 TODO :("

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

