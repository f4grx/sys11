/* Bootloader program to write arbitrary memory in the hc11 system
 * (c) 2019 Sebastien Lorquet
 * Released under WTFPL
 * Behaviour:
 * This GNU AS listing assembles to a short binary that is padded to 256
 * bytes by the linker script.
 * It can be loaded at address 0x0000 in the hc11 via the bootstrap mechanism.
 * Once executed it waits for HDLC packets at 9600 bauds on the SCI, and if
 * checksum (last byte) is valid, it executes the command in the first byte.
 */

	.equ REGS, 0x1000
	.equ BUF,  0x100 /* 256 bytes byffer for command reception */
	.equ SOF,  0x7E
	.equ ESC,  0x7D
	.equ MASK, 0x20

/* UART defs */
	.include "uart.inc"

/* Start of program */

	.text
	.global _start
_start:
	/* prepare indexed access to regs */
	ldx	#REGS

	/* init UART */
	ldaa	#0x30		/* prediv 13, postdiv 1, 9600 bauds @ 8 MHz */
	staa	BAUD,X
	ldaa	#0x0C		/* No interrupts, enable TX and RX */
	staa	SCCR2,X

	/* Wait for reception of SOF (0x7E) */
_rxsof:
	brclr	SCSR,X 0x20 _rxsof
	ldaa	SCDAT,X             /* Got a byte */
	cmpa	#SOF                /* Is it SOF? */
	bne     _rxsof              /* No: try agn */

	/* we got a SOF. Now receive up to 256 bytes */
	ldy     #0 /* This is the current buffer length */
	ldab	#0 /* This is the checksum */

_rxdata:
	brclr	SCSR,X 0x20 _rxdata
	ldaa	SCDAT,X             /* Got a data byte */
    
	/* prepare response */
	/* checksum ok?

	/* no: send error code and go back to rx */
	/* execute command */
	/* send response */
	/* done */
	jmp     _rxsof /* Wait for next command */

