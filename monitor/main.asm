	.include "system.inc"

	.text

	.extern	sci_init
	.extern	sci_puts

	.global	_start
_start:
	/* Map registers and internal RAM to 0000h
	 * By default regs are mapped at 1000h
	 */
	clra
	staa	0x1000+OPTION
	/* Now registers are at zero and we can use direct mode */

	bsr	sci_init

	ldx	motd
	bsr	sci_puts

	bra .

	.section .rodata
motd:	.asciz	"sys11 monitor 0.1"

