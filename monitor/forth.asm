/* forth interpreter */

	.include "softregs.inc"
	.include "serial.inc"
	.include "stdio.inc"

	.equ	IBUF_LEN, 80

	.section .edata
ibuf:	.space	IBUF_LEN+1

	.section .rodata
msg:	.asciz "Forth for 68hc11\r\n"

	.text

	.func	app_main
	.global app_main
app_main:
	ldaa	#OPT_ECHO		/* Default to echo ON */
	staa	rlopts

        /* Display prompt */

	ldx	#msg
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf

        /* Read a line and interpret */

.Linterp:
	ldx	#ibuf
	stx	*sp0
	ldx	#(IBUF_LEN)	/* scmdbuf has additional byte to store 80 chars + final zero */
	stx	*sp1
	jsr	readline
	jsr	serial_crlf	/* Skip current line */

.if 0
	/* Test display for readline */
	ldx	#scmdbuf
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf	/* Skip current line */
	bra	.Linterp
.endif
	rts
	.endfunc

