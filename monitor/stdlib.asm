/* Standard routines */
	.include "softregs.inc"

/* sitoa - Convert an unsigned integer to a string
 * Parameters:
 * sp0 StringBuf - pointer to a memory zone that can hold 6 bytes
 * sp1 number    - number (16-bit) to be converted
 * sp2 base      - radix for the number, 10 or 16
 * Side effect: The number is converted into the indicated base
 * Return value: String length in D
 */
	.func	inttostr
	.global	inttostr
inttostr:
	/* First step: Compute length of result */
	clra
	staa	*st0		/* Clear number of digits (single byte) */
	ldd	*sp1		/* Load number once */
.Lrecalc:
	inc	*st0		/* Increase num of digits (at least one to store zero) */
	ldx	*sp2		/* Set dividend at each round */
	idiv			/* D/X, Q in X, R in D, Z if Q=0 */
	xgdx			/* Does not change flags, move again X in D */
	bne	.Lrecalc	/* Divide again until quotient is zero */

	/* Then compute buffer position : buffer + len - 1*/
	ldy	*sp0
	ldab	*st0
	aby
	clrb
	stab	0,Y		/* While we cross it, put a final zero here */
	dey

	/* Then redo divisions to convert into base */
	ldd	*sp1		/* Load number once */
.Lconvert:
	ldx	*sp2		/* Set dividend at each round */
	idiv			/* D/X, Q in X, R in D, Z if Q=0 */
	addb	#0x30		/* Convert number to ascii digit */
	cmpb	#0x39		/* Did we create something larger than 9? */
	bls	.Lzeronine	/* No: Skip conversion to letters */
	addb	#(0x41-0x3A)	/* Add required value to convert values >9 to letters */
.Lzeronine:
	stab	0,Y		/* Store remainder in output buf */
	dey			/* Next digit, previous char */
	cmpx	#0		/* Check if quotient is zero, tested in bne below */
	xgdx			/* Does not change flags, move again X in D */
	bne	.Lconvert	/* Divide again until quotient is zero */

	/* Done, return number of emitted chars in D */
	clra
	ldab	*st0
	rts
	.endfunc

