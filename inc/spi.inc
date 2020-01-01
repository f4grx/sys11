/* Serial Peripheral Interface registers */

	.equ	SPCR, 0x28
	.equ	SPSR, 0x29
	.equ	SPDR, 0x2A

/* SPCR bits */

	.equ	SPCR_SPR0, 0x01 /* Rate select 0 */
	.equ	SPCR_SPR1, 0x02 /* Rate select 1 */
	.equ	SPCR_CPHA, 0x04 /* Clock phase */
	.equ	SPCR_CPOL, 0x08 /* Clock polarity */
	.equ	SPCR_MSTR, 0x10 /* Master mode select */
	.equ	SPCR_DWOM, 0x20 /* Port D Open Drain enable */
	.equ	SPCR_SPE , 0x40 /* SPI enable */
	.equ	SPCR_SPIE, 0x80 /* Interrupt Enable */

	.equ	SPCR_RATE_MASK, 0x03
	.equ	SPCR_RATE_E2  , 0x00
	.equ	SPCR_RATE_E4  , 0x01
	.equ	SPCR_RATE_E16 , 0x02
	.equ	SPCR_RATE_E32 , 0x03

/* SPSR bits */

	.equ	SPSR_MODF, 0x10 /* Mode fault flag */
	.equ	SPSR_WCOL, 0x40 /* Write collision */
	.equ	SPSR_SPIF, 0x80 /* Reset by reading SPDR */

