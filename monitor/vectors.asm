	.section .vectors

	.extern _start

	.global _vectors

_vectors:
	.word 0 /* SCI */
	.word 0 /* SPI */
	.word 0 /* PAI edge */
	.word 0 /* PA overflow */
	.word 0 /* Timer overflow */
	.word 0 /* OC5 */
	.word 0 /* OC4 */
	.word 0 /* OC3 */
	.word 0 /* OC2 */
	.word 0 /* OC1 */
	.word 0 /* IC2 */
	.word 0 /* IC1 */
	.word 0 /* IC0 */
	.word 0 /* Real Time Int */
	.word 0 /* IRQ */
	.word 0 /* XIRQ */
	.word 0 /* SWI */
	.word 0 /* Illegal Opcode */
	.word 0 /* COP Fail */
	.word 0 /* Clock Monitor */
	.word _start
