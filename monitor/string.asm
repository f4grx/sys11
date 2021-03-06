/*===========================================================================*/
/* String manipulation functions */

/* usual uint16_t strlen(char *sp0)
 * Input: pointer to string on stack
 * Output: length in D
 * Destroys D,X,Y
 */
	.func	strlen
	.global strlen
strlen:
	ldx	*sp0
	ldy	#0
.Lagain:
	ldaa	0,X	/* Get pointed char */
	beq	.Ldone	/* Pointed char zero: end of string */
	inx		/* point at next char */
	iny		/* increment string length */
	bra	.Lagain	/* Continue with next char */
.Ldone:
	xgdy
	rts
	.endfunc

/*===========================================================================*/
/* usual strcmp(char *sp0, char *sp1)
 * Comparison information is returned via flags
 * Z set if both string are equal
 * Z clr and N clr if sp1 larger than sp0
 * Z clr and N set if sp1 smaller than sp0
 * This means than beq and friends can be used right after calling this routine.
 */
	.func	strcmp
	.global	strcmp
strcmp:
	ldx	*sp0
	ldy	*sp1
.Lloop:
	ldaa	0,X	/* A <- *str1 */
	ldab	0,Y	/* B <- *str2  */
	cba		/* compute/test *str1 - *str2 */
	bne	.Lend	/* Both chars not equal: Comparison is done. */
	/* Both chars equal. test for end of string */
	tsta		/* test all bits in *str1 */
	beq	.Lend	/* They are zero -> end of str1 AND str2 - equality*/
	/* Equality, but not end of string. Try again with next chars */
	inx
	iny
	bra	.Lloop
.Lend:
	rts
	.endfunc

