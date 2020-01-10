/* Interface to device manager */
	.equ DEV_BLOCKSIZE, 256

	.extern device_init
	.extern device_getblocks
	.extern	device_readb
	.extern	device_writeb

