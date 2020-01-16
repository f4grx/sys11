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
	.include "errno.inc"

	.section .scommands
	.asciz	"lsdev"
	.word	devlist

	.equ	NUM_DEVS, 8	/* Max number of devices */
	/* Offsets in the vtable */
	.equ	BDEV_NAMEPTR  , 0
	.equ	BDEV_GETBLOCKS, 2
	.equ	BDEV_BREAD    , 4
	.equ	BDEV_BWRITE   , 6

	.section .rodata
dhdr:	.asciz	"dev\tblocks\tname"

	.section .edata
devices:
	.space NUM_DEVS * 4	/* Each dev has a vtable pointer and a private pointer */

	.text

	.extern	bdev_ram

/*===========================================================================*/
/* Initialize the device layer */
	.func	device_init
	.global device_init
device_init:
	clra
	clrb
	ldx	#devices
	ldab	#NUM_DEVS
	lslb		/* get 2*NDEV */
	lslb		/* get 4*NDEV */
	decb		/* get 4*NDEV-1 */
	clra
.Linitdev:
	staa	0,X
	inx
	decb
	bne	.Linitdev

	/* Install the bdev_ram device */
	ldx	#bdev_ram
	stx	devices
	rts
	.endfunc

/*===========================================================================*/
/* shell command to report the list of devices */
	.func	devlist
	.global devlist
devlist:
	ldx	*st0
	pshx

	/* Disp list header */

	ldx	#dhdr
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf

	/* Prepare dev enumeration */

	clra
	clrb
	std	*st0
.Ldodev:

	/* Print device index */

	ldd	*st0
	std	*sp0
	jsr	serial_putdec

	ldab	#'\t'
	jsr	serial_putchar

	/* Get blocks for device N */

	ldx	*st0
	stx	*sp0
	jsr	device_getblocks
	std	*sp0
	blt	.Ldonodev
	jsr	serial_putdec
	ldab	#'\t'
	jsr	serial_putchar

	/* Display device name */

	ldx	*st0
	stx	*sp0
	jsr	device_getname
	std	*sp0
	jsr	serial_puts
	
	bra	.Lnextdev

.Ldonodev:
	.section .rodata
nodev:	.asciz "None"
	.text
	ldx	#nodev
	stx	*sp0
	jsr	serial_puts
	bra	.Lnextdev

.Lnextdev:
	jsr	serial_crlf

	/* Prepare for next device */

	ldab	*(st0+1)
	incb
	stab	*(st0+1)
	cmpb	#NUM_DEVS
	blo	.Ldodev	

	pulx
	stx	*st0	
	rts
	.endfunc

/*===========================================================================*/
/* Find device pointer */
	.func	dev_findptr
dev_findptr:
	ldd	*sp0		/* Get device index */
	lsld			/* Convert to... */
	lsld			/* Dev table offset */
	addd	#devices	/* Add beginning of table */
	xgdx			/* store dev vtable store addr in X*/
	ldx	0,X		/* Get vtable address */
	rts
	.endfunc

/*===========================================================================*/
/* Returns in D the name (or null) of blocks for device in sp0 */
	.func	device_getname
	.global device_getname
device_getname:
	jsr	dev_findptr
	beq	.Lnullret
	ldd	BDEV_NAMEPTR,X/* Get str ptr for name */
	bra	.Lgnret
.Lnullret:
	clra
	clrb
.Lgnret:
	rts
	.endfunc

/*===========================================================================*/
/* Returns in D the num of blocks (or -ENODEV) for device in sp0 */
	.func	device_getblocks
	.global device_getblocks
device_getblocks:
	jsr	dev_findptr
	beq	.Lenodev
	ldx	BDEV_GETBLOCKS,X/* Get fn ptr for getblocks */
	jsr	0,X		/* Execute and define retcode*/
	bra	.Lgbret
.Lenodev:
	ldd	#(-ENODEV)
.Lgbret:
	rts
	.endfunc

/*===========================================================================*/
/* Returns in buffer at sp1 the contents of block sp2 of device sp0 */
	.func	device_readb
	.global device_readb
device_readb:
	jsr	dev_findptr
	beq	.Lenodev
	ldx	BDEV_BREAD,X/* Get fn ptr for getblocks */
	jsr	0,X		/* Execute and define retcode*/
	rts
	.endfunc

/*===========================================================================*/
/* Write contents at sp1 in block sp2 of device sp0 */
	.func	device_writeb
	.global device_writeb
device_writeb:
	jsr	dev_findptr
	beq	.Lenodev
	ldx	BDEV_BWRITE,X/* Get fn ptr for getblocks */
	jsr	0,X		/* Execute and define retcode*/
	rts
	rts
	.endfunc

