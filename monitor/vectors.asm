	.text
	.equ RESERVED, 0xFFFF
noit:	rti

	.section .vectors

	.extern _start

	.global _vectors

_vectors:
	.word RESERVED	/* FFC0 */
	.word RESERVED	/* FFC2 */
	.word RESERVED	/* FFC4 */
	.word RESERVED	/* FFC6 */
	.word RESERVED	/* FFC8 */
	.word RESERVED	/* FFCA */
	.word RESERVED	/* FFCC */
	.word RESERVED	/* FFCE */
	.word RESERVED	/* FFD0 */
	.word RESERVED	/* FFD2 */
	.word RESERVED	/* FFD4 */

	.word noit	/* FFD6 SCI */
	.word noit	/* FFD8 SPI */
	.word noit	/* FFDA PAI edge */
	.word noit	/* FFDC PA overflow */
	.word noit	/* FFDE Timer overflow */
	.word noit	/* FFE0 OC5 */
	.word noit	/* FFE2 OC4 */
	.word noit	/* FFE4 OC3 */
	.word noit	/* FFE6 OC2 */
	.word noit	/* FFE8 OC1 */
	.word noit	/* FFEA IC3 */
	.word noit	/* FFEC IC2 */
	.word noit	/* FFEE IC1 */
	.word noit	/* FFF0 Real Time Int */
	.word noit	/* FFF2 IRQ */
	.word noit	/* FFF4 XIRQ */
	.word noit	/* FFF6 SWI */
	.word noit	/* FFF8 Illegal Opcode */
	.word noit	/* FFFA COP Fail */
	.word noit	/* FFFC Clock Monitor */
	.word _start	/* FFFE RESET */

