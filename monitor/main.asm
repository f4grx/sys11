	.include "system.inc"

	.text

	.extern	sci_init
	.extern sci_putchar
	.extern	sci_puts

	.global	_start
_start:
	lds	#0xFF

	/* Map registers and internal RAM to 0000h
	 * By default regs are mapped at 1000h
	 */
	clra
	staa	OPTION+0x1000

	/* Now registers are at zero and we can use direct mode */

	bsr	sci_init

	ldaa	#'X'
	jsr	sci_putchar
	ldaa	#0x0A
	jsr	sci_putchar

	bra .

	ldx	motd
	jsr	sci_puts

	bra .

	.section .rodata
motd:	.asciz	"sys11 monitor 0.1"

