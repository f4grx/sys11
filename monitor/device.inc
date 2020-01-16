/* Interface to device manager */
	.equ BDEV_BLOCKSIZE, 256

/* Init the device subsystem */
	.extern device_init

/* Get number of blocks in a device.
 * IN: sp0: block device index
 * OUT:  D: number of blocks
 */
	.extern device_getblocks

/* Read a block from a device
 * IN: sp0: block device index
 *     sp1: block index to read
 *     sp2: destination buffer (256 bytes)
 */
	.extern	device_readb

/* Write a block to a device
 * IN: sp0: block device index
 *     sp1: block index to write
 *     sp2: source buffer (256 bytes)
 */
	.extern	device_writeb

/* Documentation for block driver functions implemented by drivers
 * bdev_driver_getblocks
 * vtable offset: 2
 *   IN: sp0: Pointer to driver private data
 *  OUT:   D: Number of blocks
 *
 * bdev_driver_bread
 * Read data from a block.
 * vtable index: 4
 *   IN: sp0: Pointer to driver private data
 *   IN: sp1: Index of block to read
 *   IN: sp2: Pointer to 256-byte destination buffer
 *  OUT:   D: 0 if OK, other if error
 *
 * bdev_driver_bwrite
 * Write data to a block.
 * vtable index: 6
 *   IN: sp0: Pointer to driver private data
 *   IN: sp1: Index of block to write
 *   IN: sp2: Pointer to 256-byte source buffer
 *  OUT:   D: 0 if OK, other if error
 */ 
