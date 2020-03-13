/* forth interpreter */

	.include "softregs.inc"
	.include "serial.inc"

	.section .scommands
	.asciz	"forth"
	.word	forth

	.section .rodata
msg:	.asciz "Forth for sys11 TODO"

	.text

	.func	forth
	.global forth
forth:
	ldx	#msg
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf
	rts
	.endfunc

