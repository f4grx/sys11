/* RAM based block device.
 * This is mainly used for testing.
 * This block device uses normal CPU accessible RAM
 * It can be used to build a ramdisk.
 */

	.include "device.inc"

	.equ	NBLOCKS, 32	/* 32 blocks: 8K */ 

	.section .edata
storage:
	.space NBLOCKS * BDEV_BLOCKSIZE

	.section .rodata
bdev_cpuram_name:
	.asciz	"bdev_cpuram"

	.text

/*===========================================================================*/
	.func	bdev_cpuram_getblocks
bdev_cpuram_getblocks:
	ldd	#NBLOCKS
	rts
	.endfunc

/*===========================================================================*/
	.func	bdev_cpuram_bread
bdev_cpuram_bread:
	rts
	.endfunc

/*===========================================================================*/
	.func	bdev_cpuram_bwrite
bdev_cpuram_bwrite:
	rts
	.endfunc

/*===========================================================================*/
/* Define the driver vtable */
	.section .rodata
	.global	bdev_ram
bdev_ram:
	.word	bdev_cpuram_name
	.word	bdev_cpuram_getblocks
	.word	bdev_cpuram_bread
	.word	bdev_cpuram_bwrite

