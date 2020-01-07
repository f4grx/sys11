/* SPI bus driver */

	.include "spi.inc"

	.text

/*===========================================================================*/
/*
 * SPI one-time initialization.
 * - setups PORTD lines
 * - setups nSS as a normal GPIO under software control
 */
	.func	spi_init
	.global	spi_init
spi_init:
	rts
	.endfunc

/*===========================================================================*/
/*
 * Setups the bus for the next transfer
 * sp0 contains flags to configure bus.
 * sp1 contains speed divider
 * word length is fixed to 8 and order is fixed to MSB first
 */
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

