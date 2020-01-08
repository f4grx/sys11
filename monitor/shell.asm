/* sys11 shell */
	.include "serial.inc"

	.equ SCMD_MAX, 80
	.equ ARGV_MAX, 8

	.equ OPT_ECHO, 0x01

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
argc:
	.byte	0
argv:
	.space	ARGV_MAX * 2

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
	ldaa	#OPT_ECHO		/* Default to echo ON */
	staa	scmdopts		/* Clear shell options */
	clra
	staa	scmdlen			/* Clear command len */
	staa	scmdbuf+SCMD_MAX	/* Put a final zero */

.Lcmdloop:
	ldx	#sprompt
	stx	*sp0
	jsr	serial_puts
	ldx	#scmdbuf

	/* ==================== */
	/* Acquire a line of text */
	/* ==================== */

.Lcharloop:
	jsr	serial_getchar
	ldaa	scmdopts
	bita	#OPT_ECHO
	beq	.Lnoecho
	jsr	serial_putchar /* echo */
.Lnoecho:
	cmpb	#0x0D
	beq	.Lparse		/* User pressed return */
	ldaa	scmdlen
	cmpa	#SCMD_MAX
	beq	.Lcharloop	/* Overflow: dont store char, but still wait for LF */
	stab	0,X
	inc	scmdlen
	inx
	bra	.Lcharloop

	/* ==================== */
	/* Split command into arguments */
	/* ==================== */

.Lparse:
	clra
	staa	argc		/* No arguments parsed so far */
	staa	0,X		/* Store a final zero */
	jsr	serial_crlf	/* Skip current line */

	ldx	#scmdbuf
	stx	*st0		/* st0 contains current string pointer */

.Lparseloop:
	ldab	argc		/* D now contains cur number of arguments */
	lsld			/* D now contains offset in argv array to store arg N */
	addd	#argv		/* D now contains destination pointer to store arg N */
	ldx	*st0		/* X now contains pointer to beginning of arg N */
	xgdx			/* Swap X and D because we can only store at X */
	std	0,X		/* Store pointer to arg N */
	ldab	argc		/* Prepare for storage of next arg (keep a clear) */
	cmpb	#ARGV_MAX
	beq	.Lsplitdone
	inc	argc

	/* Skip all chars until next space or end of string. */
	ldx	*st0
.Lsplit:
	ldaa	0,X		/* Get command char */
	cmpa	#0x20		/* If its a space, */
	beq	.Lfoundspace	/* Then current arg is complete */
	cmpa	#0		/* If end of string, */
	beq	.Lsplitdone	/* Then finish parsing. */
	inx			/* Prepare for next char */
	bra	.Lsplit		/* Try again (next char) */
.Lfoundspace:
	clra			/* Load end of string */
	staa	0,X		/* Where we had a space */
	inx			/* And point to char right after space */
	stx	*st0		/* Is now a pointer to the first argument */
	bra	.Lparseloop
.Lsplitdone:

	/* ==================== */
	/* Find command in list */
	/* ==================== */

	ldx	#scommands	/* Command List head */
	stx	*sp0		/* Init loop with pointer to first command */
.Lnextcmd:
	jsr	strlen
	addd	*sp0
	xgdx
	inx			/* Skip final zero */
	stx	st0		/* Store in temp for reuse when skipping to next cmd */
	ldx	argv
	stx	*sp1		/* Command name in sp1 */
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

