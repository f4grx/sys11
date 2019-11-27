/* String manipulation functions */

/* usual uint16_t strlen(char *X)
 * Input: pointer to string on stack
 * Output: length in D
 * Destroys D,X,Y
 */
	.global strlen
strlen:
	pulx
	clra
	clrb		/* Clear D in 2 bytes instead of 3 using ldd */
.Lagain:
	ldaa	0,X	/* Get pointed char */
	beq	.Ldone	/* Pointed char zero: end of string */
	inx		/* point at next char */
	addd	#1	/* increment string length */
	bra	.Lagain	/* Continue with next char */
.Ldone:
	rts

