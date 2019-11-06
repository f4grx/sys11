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

/* HC11 defs */
	.include "sci.inc"
	.include "ioport.inc"
	.include "timer.inc"

/* Start of program */

	.text
	.global _start
_start:
/* prepare indexed access to regs */
	ldx		#REGS

/* switch on the LED*/
	ldaa	PACTL,X
	oraa	#0x80
	staa	PACTL,X
	bsr	ledon

	/* Let the bootstrap loader finish sending the last ACK byte */
waitack:
	brclr	SCSR,X #SCSR_TC waitack

/* UART Init is already done by the bootstrap loader at a decent speed. */

/* Wait for reception of SOF (0x7E) */
rxsof:
	bsr	ledon		/* LED ON when waiting for a packet */
	brclr	SCSR,X #SCSR_RDRF rxsof
	ldaa	SCDR,X		/* Got a byte */
	cmpa	#SOF		/* Is it SOF? */
	bne	rxsof		/* No: try agn */

	/* we got a SOF. Now receive up to 256 bytes */
	ldy	#0		/* This is the current buffer length */
	clrb			/* This is the checksum */
	clv			/* overflow flag used to mark escape */

rxdata:
	bsr	ledoff		/* LED OFF when receiving data */
	brclr	SCSR,X #SCSR_RDRF rxdata
	ldaa	SCDR,X             /* Got a data byte */

	/* Apply escape if needed */
	bvc	notesc
	eora	#MASK
	clv			/* Next byte not escaped */
notesc:

	cmpa	#SOF
	beq	exec		/* End of packet reached */

	cmpa	#ESC
	bne	databyte	/* This is not an escape command*/

/* We reach this point if this is an escape command */
	sev			/* Next received byte will be unescaped */
	bra	rxdata		/* Wait for next byte */

databyte:
	/* TODO update checksum in B */
	staa	0,Y
	iny
	/* TODO check for overflow */
	bra	rxdata		/* Wait for next byte */

	/* Execute the command */
exec:
	/* checksum ok? must be zrro */

	/* no: send error code and go back to rx */
	/* execute command */
	/* send response */
	/* done */
	bra	rxsof		/* Wait for next command */


; Switches LED ON
; Destroys A
ledon:
	ldaa	PORTA,X
	anda	#0x7F
	staa	PORTA,X
	rts

; Switches LED OFF
; Destroys A
ledoff:
	ldaa	PORTA,X
	oraa	#0x80
	staa	PORTA,X
	rts

