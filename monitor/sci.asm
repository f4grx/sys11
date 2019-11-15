/* Serial port console routines */

	.include "sci.inc"

	.text

	.global sci_init
sci_init:
	rts

/*
 * SCI_PUTCHAR
 * Input : Character in A
 * Output: None
 * Destroys: None
 */
	.global sci_putchar
sci_putchar:
	brclr	*SCSR #SCSR_TDRE, sci_putchar
	staa	SCDR
	rts


/*
 * SCI_PUTS
 * Input : String pointer in X
 * Output: None
 * Destroys: A, X
 */
	.global sci_puts
sci_puts:
	ldaa	0,X		/* Load char pointed b X */
	beq	sci_puts_end
	bsr	sci_putchar
	inx
	bra	sci_puts
sci_puts_end:
	rts
