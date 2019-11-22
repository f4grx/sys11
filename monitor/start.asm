	.include "system.inc"
	.include "ioport.inc"
	.include "mm.inc"

	.text

	.extern	sci_init
	.extern sci_putchar
	.extern	sci_puts

	.global	_start
_start:
	/* Note: At this point we have no stack yet */

	/* Map registers and internal RAM to 0000h
	 * By default regs are mapped at 1000h
	 * This ONLY works if the monitor starts at boot, else this must be done
	 * by the bootstrap loader in special mode.
	 */
	clra
	staa	INIT+0x1000

	/* Now registers are at zero and we can use direct mode */

	/* Put an initial stack at the end of the internal RAM */
	lds	#0xFF

	bset	*PORTA #0x80 /* LED OFF */

	/* ==================== */
	/* Initialize the system */
	/* ==================== */

	bsr	sci_init

	ldx	#motd
	jsr	sci_puts

	ldx	#10
	pshx
	jsr	mm_alloc

	/* ==================== */
	/* Fall in the idle loop */
	/* ==================== */

idle:
.ifndef DEBUG
	wai		/* Do nothing until some interrupt happens */
.endif
	bra idle
	
	.section .rodata
motd:	.asciz	"sys11 monitor by f4grx v0.1\r\n"

