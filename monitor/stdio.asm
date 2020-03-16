/* Usual routines */
	.include "serial.inc"
	.include "stdio.inc"

	.section .edata
	.global  rlopts
rlopts:
	.byte	0

	.text

/*===========================================================================*/
/* Read a line in buffer pointed by sp0, whith length (byte) in sp1.L */
	.func	readline
	.global	readline
readline:

	/* Save used temps */

	ldx	*st0
	pshx

	clrb
	stab	*(st0+1)	/* Clear current command len */
	dec	*(sp1+1)	/* Reduce buffer length so a final zero can be stored */
	ldx	*sp0		/* Reload dest ptr for char acq */

.Lcharloop:
	jsr	serial_getchar
	ldaa	rlopts
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
	cmpa	*(sp1+1)
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

	/* Set return value */

	clra			/* Cleanup Result MSB */
	staa	0,X		/* Meanwhile, store a final zero */
	ldab	*(st0+1)	/* Return nr of chars in result LSB */

	/* Restore temps */

	pulx
	stx	*st0

	rts
	.endfunc
