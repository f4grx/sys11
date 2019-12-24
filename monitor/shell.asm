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
	staa	0,X		/* Store a final zero */
	jsr	serial_crlf	/* Skip current line */

	/* TODO: put a zero after the command word, before params */

	/* Find command in list */
	ldx	#scommands
	stx	*sp0		/* Init loop with pointer to first command */
.Lnextcmd:
	ldx	#scmdbuf
	stx	*sp1
	jsr	strcmp		/* Compare buffered command */
	beq	.Lfound
	jsr	strlen
	addd	*sp0
	addd	#3	/* skip final zero and function pointer */
	std	*sp0
	ldx	*sp0
	ldaa	0,X
	cmpa	#0xFF
	beq	.Lcmdloop
	bra	.Lnextcmd

.Lfound:
	jsr	strlen
	addd	*sp0
	addd	#1	/* skip final zero */
	std	*sp0
	ldx	*sp0
	ldx	0,X	/* Get address stored right after the name */
	jsr	0,X
	
	/* Just echo */
	ldx	#scmdbuf
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf
	bra	.Lcmdloop
	rts
	.endfunc

