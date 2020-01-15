/* sys11 shell */
/* TODO modularize this in two calls: readline and execute,
 * so each can be reused. */

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
snocom:	.asciz "???"

	.section .scommands_start
scommands:
	.asciz	"echo"
	.word	shell_echo

	.section .scommands_end
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
/* Read a line in buffer pointed by sp0, whith length (byte) in sp1.L */
	.func	shell_readline
	.global	shell_readline
shell_readline:
	clra
	clrb
	std	*st0		/* Clear current command len */
	ldx	*sp0
	ldab	*(sp1+1)
	abx
	dex
	staa	0,X		/* Put a final zero */

.Lcharloop:
	jsr	serial_getchar
	ldaa	scmdopts
	bita	#OPT_ECHO
	beq	.Lnoecho
	jsr	serial_putchar /* echo */
.Lnoecho:
	cmpb	#0x0D
	beq	.Lrdldone	/* User pressed return -> readline done */
	cmpb	#0x08
	beq	.Lbackspace
	cmpb	#0x7F
	beq	.Lbackspace
	ldaa	*(st0+1)
	cmpa	#SCMD_MAX
	beq	.Lcharloop	/* Overflow: dont store char, but still wait for LF */
	stab	0,X
	inc	*(st0+1)
	inx
	bra	.Lcharloop

	/* If character is a backspace (0x08 or 0x7F), special handling */
.Lbackspace:
	ldaa	*(st0+1)
	beq	.Lcharloop	/* Len already zero: do nothing */
	deca
	staa	*(st0+1)	/* Decrese char count */
	dex			/* Point to previous char */
	bra	.Lcharloop
.Lrdldone:
	clra
	staa	0,X		/* Store a final zero */
	rts
	.endfunc

/*===========================================================================*/
/* Parse sp1 bytes stored at address sp0 */
	.func	shell_parse
	.global	shell_parse
shell_parse:
	clra
	staa	argc		/* No arguments parsed so far */

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
	beq	.Lparsedone	/* Then finish parsing. */
	inx			/* Prepare for next char */
	bra	.Lsplit		/* Try again (next char) */
.Lfoundspace:
	clra			/* Load end of string */
	staa	0,X		/* Where we had a space */
	inx			/* And point to char right after space */
	stx	*st0		/* Is now a pointer to the first argument */
	bra	.Lparseloop
.Lparsedone:
	rts
	.endfunc

/*===========================================================================*/
	.func	shell_main
	.global shell_main
shell_main:

	ldaa	#OPT_ECHO		/* Default to echo ON */
	staa	scmdopts

.Lcmdloop:
	/* ==================== */
	/* Display main prompt */
	/* ==================== */

	ldx	#sprompt
	stx	*sp0
	jsr	serial_puts

	jsr	serial_crlf	/* Skip current line */

	/* ==================== */
	/* Acquire a line of text */
	/* ==================== */

	ldx	#scmdbuf
	stx	*sp0
	ldx	SCMD_MAX+1	/* scmdbuf has additional byte to store 80 chars + final zero */
	stx	*sp1
	jsr	shell_readline

	/* ==================== */
	/* Split command into arguments */
	/* ==================== */

	std	*sp0
	jsr	shell_parse

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
	beq	.Lnocommand	/* Reached end of command list, nothing found, acq next user cmd */
	bra	.Lnextcmd	/* Compare cmd buffer with next command in list */
.Lnocommand:
	ldx	#snocom
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf

.if 1
	/* Debug: display arg count */
	clra
	ldab	argc
	std	*sp0
	jsr	serial_putdec
	jsr	serial_crlf
	clra
	clrb
	std	*st0
.Lnextarg:
	std	*sp0
	jsr	serial_putdec
	.section .rodata
arrow:	.asciz	" -> "
	.text
	ldx	#arrow
	stx	*sp0
	jsr	serial_puts
	ldd	*st0
	lsld
	ldx	#argv
	abx
	ldd	0,X
	std	*sp0
	jsr	serial_puts
	jsr	serial_crlf
	/* set for next arg */
	ldd	*st0
	incb
	std	*st0
	cmpb	argc
	blo	.Lnextarg
.endif

	bra	.Lcmdloop
.Lfound:
	ldx	*st0	/* st0 stored the pointer right after the cmd name */
	ldx	0,X	/* Get address stored right after the name */
	jsr	0,X
	
	bra	.Lcmdloop
	rts
	.endfunc

