/* Storage device manager */
/* sys11 manage storage devices using numerically indexed devices.
 * Each device can be implemented by an independent driver.
 * When calling a generic method of the device manager, the call
 * is dispatched to the correct device driver.
 *
 * IMPORTANT: Block size is fixed at 256 bytes !
 * This is OK for ROM/RAM and EEPROM based devices.
 *
 * devices is an array of pointers to a table of information that MUST
 * hold the following pointers:
 * offset description
 * 0      pointer to a device type name string
 * 2      pointer to function that returns the block count
 * 4      pointer to function that reads a block
 * 6      pointer to function that writes a block
 */

	.include "softregs.inc"
	.include "serial.inc"

	.section .scommands
	.asciz	"devlist"
	.word	devlist

	.equ	NUM_DEVS, 8	/* Max number of devices */

	.section .rodata
dhdr:	.asciz	"dev blocks name"

	.section .edata
devices:
	.space NUM_DEVS * 2
scratchpad:
	.space 32

	.text

/*===========================================================================*/
/* Initialize the device layer */
	.func	device_init
	.global device_init
device_init:
	rts
	.endfunc

/*===========================================================================*/
/* shell command to report the list of devices */
	.func	devlist
	.global devlist
devlist:
	/* Disp list header */

	ldx	#dhdr
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf

	/* Prepare dev enumeration */

	ldx	#scratchpad
	stx	*sp0
	clra
	ldab	#10
	std	*sp2
	staa	*st0
.Ldodev:
	clra
	ldab	*st0
	std	*sp1
	jsr	inttostr
	jsr	serial_puts
	jsr	serial_crlf

	/* Prepare for next device */

	ldab	*st0
	incb
	stab	*st0
	cmpb	#NUM_DEVS
	blo	.Ldodev		
	rts
	.endfunc

/*===========================================================================*/
/* Returns in D the num of blocks for device in sp0 */
	.func	device_getblocks
	.global device_getblocks
device_getblocks:
	rts
	.endfunc

/*===========================================================================*/
/* Returns in buffer at sp1 the contents of block sp2 of device sp0 */
	.func	device_readb
	.global device_readb
device_readb:
	rts
	.endfunc

/*===========================================================================*/
/* Write contents at sp1 in block sp2 of device sp0 */
	.func	device_writeb
	.global device_writeb
device_writeb:
	rts
	.endfunc

