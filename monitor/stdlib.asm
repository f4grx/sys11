/* Standard routines */
	.include "softregs.inc"

/* sitoa - Convert an unsigned integer to a string
 * Parameters:
 * PUSH16 StringBuf - pointer to a memory zone that can hold 6 bytes
 * PUSH16 number    - number (16-bit) to be converted
 * PUSH8 base      - radix for the number, 10 or 16
 * Side effect: The number is converted into the indicated base
 * Return value: String length in D
 */
	.global	ntoa
itoa:
	pulb
	clra
	std	sr0	/* sr0 contains radix */
	pulx
	stx	sr1	/* sr1 contains number to be converted */
	pulx
	stx	sr2	/* sr2 contains string pointer */

