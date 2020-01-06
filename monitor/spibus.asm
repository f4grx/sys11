/* SPI bus driver */

	.include "spi.inc"

	.text

/*===========================================================================*/
	.func	spi_setup
	.global	spi_setup
spi_setup:
	rts
	.endfunc

/*===========================================================================*/
	.func	spi_transac
	.global	spi_transac
spi_transac:
	rts
	.endfunc

/*===========================================================================*/
	.func	spi_byte
	.global	spi_byte
spi_byte:
	rts
	.endfunc

