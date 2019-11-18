/* String manipulation functions */

/* usual uint16_t strlen(char *X)
 * Input: pointer to string on stack
 * Output: length in D
 * Destroys D,X,Y
 */
	.global strlen
strlen:
	pulx
	ldy	#0
again:
	ldaa	0,X	/* Get pointed char */
	beq	done	/* Pointed char zero: end of string */
	inx		/* point at next char */
	iny		/* increment string length */
	bra	again	/* Continue with next char */
done:
	xgdy
	rts

