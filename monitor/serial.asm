/* Serial port console routines */

	.include "sci.inc"

	.text

	.func	serial_init
	.global serial_init
serial_init:
	ldaa	#0x22
	staa	BAUD
	ldaa	#0x0C
	staa	SCCR2
	rts
	.endfunc


/*
 * SERIAL_PUTCHAR
 * Input : Character in B
 * Output: None
 */
	.func	serial_putchar
	.global	serial_putchar
serial_putchar:
	brclr	*SCSR #SCSR_TDRE, serial_putchar
	stab	*SCDR
	rts
	.endfunc

	.func	serial_crlf
	.global	serial_crlf
serial_crlf:
	ldab	#0x0D
	jsr	serial_putchar
	ldab	#0x0A
	jsr	serial_putchar
	rts
	.endfunc

/*
 * SERIAL_PUTS
 * Input : String pointer in sp0
 * Output: None
 * Destroys: B, X
 */
	.func	serial_puts
	.global serial_puts
serial_puts:
	ldx	*sp0
.Lnext:
	ldab	0,X		/* Load char pointed b X */
	beq	.Ldone
	bsr	serial_putchar
	inx
	bra	.Lnext
.Ldone:
	rts
	.endfunc

/*
 * SERIAL_GETCHAR
 8 Get a serial char.
 * Input: none
 * Output: received char in B
 */
	.func	serial_getchar
	.global	serial_getchar
serial_getchar:
	brclr	*SCSR #SCSR_RDRF, serial_getchar
	ldab	*SCDR
	rts
	.endfunc

