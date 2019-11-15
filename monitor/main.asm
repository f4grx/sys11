	.include "system.inc"
	.include "ioport.inc"

	.text

	.extern	sci_init
	.extern sci_putchar
	.extern	sci_puts

	.global	_start
_start:
	lds	#0xFF

	/* Map registers and internal RAM to 0000h
	 * By default regs are mapped at 1000h
	 * This ONLY works if the monitor starts at boot, else this must be done by the bootstrap loader in special mode.
	 */
	clra
	staa	INIT+0x1000

	/* Now registers are at zero and we can use direct mode */

	bset	*PORTA #0x80 /* LED OFF */

	bsr	sci_init

	ldx	#motd
	jsr	sci_puts

	bra .

ledon:
	
	rts

	.section .rodata
motd:	.asciz	"sys11 monitor 0.1\r\n"

