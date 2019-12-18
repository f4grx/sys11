/* Serial port console routines */

	.include "sci.inc"

	.text

	.func	sci_init
	.global sci_init
sci_init:
	ldaa	#0x22
	staa	BAUD
	ldaa	#0x0C
	staa	SCCR2
	rts
	.endfunc

/*
 * SCI_PUTS
 * Input : String pointer in sp0
 * Output: None
 * Destroys: A, X
 */
	.func	sci_puts
	.global sci_puts
sci_puts:
	ldx	*sp0
.Lnext:
	ldaa	0,X		/* Load char pointed b X */
	beq	.Ldone
.Lwait:
	brclr	*SCSR #SCSR_TDRE, .Lwait
	staa	*SCDR
	inx
	bra	.Lnext
.Ldone:
	rts
	.endfunc

