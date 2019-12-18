/* sys11 shell */

	.equ SCMD_MAX, 80
	.section .edata
scmdbuf:
	.space	SCMD_MAX+1 /* Storage for command line */
scmdlen:
	.byte	0

	.text

	.func	shell_main
	.global shell_main
shell_main:
	clra
	staa	scmdlen
	staa	scmdbuf+SCMD_MAX /* Put a final zero */

	/* Get a command in the buffer */
.Lcharloop:
	jsr	sci_getchar
	cmpa	#0x0A
	beq	.Lexec
	ldx	scmdlen
	cmpx	#SCMD_MAX
	beq	.Lcharloop /* Overflow: dont store, but wait for LF */
	xgdx
	addd	scmdlen
	xgdx
	staa	0,X
	inc	scmdlen
	bra	.Lcharloop

.Lexec:
	/* Just echo */
	ldx	scmdbuf
	stx	*sp0
	jsr	sci_puts
	rts
	.endfunc

