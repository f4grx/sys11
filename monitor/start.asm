	.include "system.inc"
	.include "ioport.inc"
	.include "mm.inc"

	.text

	.extern	sci_init
	.extern sci_putchar
	.extern	sci_puts

	.func	_start
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

	jsr	sci_init
	jsr	mm_init

	/* ==================== */
	/* Start message */
	/* ==================== */

	ldx	#motd
	stx	*sp0
	jsr	sci_puts

	/* ==================== */
	/* Simple test */
	/* ==================== */

	ldx	#10
	stx	*sp0
	jsr	mm_alloc
	std	*adr1

	ldx	#20
	stx	*sp0
	jsr	mm_alloc
	std	*adr2

	ldx	*adr1
	stx	*sp0
	jsr	mm_free

	ldx	#10
	stx	*sp0
	jsr	mm_alloc
	std	*adr1

	ldx	*adr1
	stx	*sp0
	jsr	mm_free

	ldx	*adr2
	stx	*sp0
	jsr	mm_free

	/* ==================== */
	/* Fall in the idle loop */
	/* ==================== */

idle:
.ifndef DEBUG
	stop	/* Do nothing until some interrupt happens */
.endif
	bra idle
	.endfunc

	.data
adr1:	.word 0
adr2:	.word 0
adr3:	.word 0
	
	.section .rodata
motd:	.asciz	"sys11 monitor by f4grx v0.1\r\n"

