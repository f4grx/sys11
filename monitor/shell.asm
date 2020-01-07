/* sys11 shell */
	.include "serial.inc"

	.equ SCMD_MAX, 80

/*===========================================================================*/
	.section .rodata
sprompt:
	.asciz "sys11>"
son:	.asciz "ON"
soff:	.asciz "OFF"

scommands:
	.asciz	"echo"
	.word	shell_echo
	.byte	0xFF	/* End of list */

/*===========================================================================*/
	.section .edata
scmdbuf:
	.space	SCMD_MAX+1 /* Storage for command line */
scmdlen:
	.byte	0
scmdopts:
	.byte	0
	.equ	OPT_ECHO, 0x01
scmdparams:
	.word	0	/* Pointer to first argument of command */

	.text

/*===========================================================================*/
	.func	shell_echo
shell_echo:
	ldaa	scmdopts
	eora	#OPT_ECHO
	beq	.Lnoecho2
	ldx	#son
	bra	.Ldisp
.Lnoecho2:
	ldx	#soff
.Ldisp:
	staa	scmdopts
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf
	rts
	.endfunc

/*===========================================================================*/
	.func	shell_main
	.global shell_main
shell_main:
	ldaa	#OPT_ECHO
	staa	scmdopts		/* Clear shell options */
	clra
	staa	scmdlen			/* Clear command len */
	staa	scmdbuf+SCMD_MAX	/* Put a final zero */

.Lcmdloop:
	ldx	#sprompt
	stx	*sp0
	jsr	serial_puts
	ldx	#scmdbuf

.Lcharloop:
	jsr	serial_getchar
	ldaa	scmdopts
	bita	#OPT_ECHO
	beq	.Lnoecho
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
	staa	0,X		/* Store a final zero */
	jsr	serial_crlf	/* Skip current line */

	/* Put a zero after the command word, before params */
	ldx	#0
	stx	scmdparams	/* Initially, there are no cmd params */
	ldx	#scmdbuf
.Lsplit:
	ldaa	0,X		/* Get command char */
	cmpa	#0x20		/* If its a space, */
	beq	.Lfoundspace	/* Then command name is complete */
	cmpa	#0		/* If end of string, */
	beq	.Lsplitdone	/* Finish parsing. No args were found */
	inx			/* Prepare for next char */
	bra	.Lsplit		/* Try again (next char) */
.Lfoundspace:
	clra			/* Load end of string */
	staa	0,X		/* Where we had a space */
	inx			/* And point to char right after space */
	stx	scmdparams /* Is now a pointer to the first argument */

.Lsplitdone:
	/* Find command in list */
	ldx	#scommands
	stx	*sp0		/* Init loop with pointer to first command */
.Lnextcmd:
	jsr	strlen
	addd	*sp0
	xgdx
	inx			/* Skip final zero */
	stx	st0		/* Store in temp for reuse when skipping to next cmd */
	ldx	#scmdbuf
	stx	*sp1
	jsr	strcmp		/* Compare with buffered command */
	beq	.Lfound
	/* Typed command is not the current command */
	ldx	*st0
	inx
	inx			/* skip function pointer */
	stx	*sp0
	ldaa	0,X
	cmpa	#0xFF
	beq	.Lcmdloop	/* Reached end of command list, nothing found, acq next cmd */
	bra	.Lnextcmd	/* Compare cmd buffer with next command in list */

.Lfound:
	ldx	*st0	/* st0 stored the pointer right after the cmd name */
	ldx	0,X	/* Get address stored right after the name */
	jsr	0,X
	
	bra	.Lcmdloop
	rts
	.endfunc

