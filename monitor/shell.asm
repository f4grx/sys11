/* sys11 shell */
	.include "serial.inc"

	.equ SCMD_MAX, 80
	.section .rodata
sprompt:
	.asciz "sys11>"

	.section .edata
scmdbuf:
	.space	SCMD_MAX+1 /* Storage for command line */
scmdlen:
	.byte	0
scmdopts:
	.byte	0
	.equ	OPT_ECHO, 0x01

	.text

	.func	shell_main
	.global shell_main
shell_main:
	clra
	staa	scmdlen
	staa	scmdbuf+SCMD_MAX /* Put a final zero */

.Lcmdloop:
	ldx	#sprompt
	stx	*sp0
	jsr	serial_puts
	ldx	#scmdbuf

.Lcharloop:
	jsr	serial_getchar
	ldy	#scmdopts
	brclr	0,Y #OPT_ECHO, .Lnoecho
	jsr	serial_putchar /* echo */
.Lnoecho:
	cmpb	#0x0D
	beq	.Lexec
	ldaa	scmdlen
	cmpa	#SCMD_MAX
	beq	.Lcharloop /* Overflow: dont store, but wait for LF */
	stab	0,X
	inc	scmdlen
	inx
	bra	.Lcharloop

.Lexec:
	clra
	staa	0,X
	jsr	serial_crlf

	/* Find command in list */

	/* Just echo */
	ldx	#scmdbuf
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf
	bra	.Lcmdloop
	rts
	.endfunc

