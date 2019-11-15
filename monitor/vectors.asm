	.text
	.equ noit, 0xFFFF

	.section .vectors

	.extern _start

	.global _vectors

_vectors:
	.word noit /* SCI */
	.word noit /* SPI */
	.word noit /* PAI edge */
	.word noit /* PA overflow */
	.word noit /* Timer overflow */
	.word noit /* OC5 */
	.word noit /* OC4 */
	.word noit /* OC3 */
	.word noit /* OC2 */
	.word noit /* OC1 */
	.word noit /* IC3 */
	.word noit /* IC2 */
	.word noit /* IC1 */
	.word noit /* Real Time Int */
	.word noit /* IRQ */
	.word noit /* XIRQ */
	.word noit /* SWI */
	.word noit /* Illegal Opcode */
	.word noit /* COP Fail */
	.word noit /* Clock Monitor */
	.word _start
