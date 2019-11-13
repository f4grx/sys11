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

	.equ REGS  , 0x1000
	.equ BUF   , 0x100 /* 256+1 bytes buffer for command reception, in external RAM */
	.equ BUFEND, 0x201 /* First byte past the buffer */
	.equ SREC  , 'S'
	.equ EOL   , 0x0A

/* HC11 defs */
	.include "sci.inc"
	.include "ioport.inc"
	.include "timer.inc"
	.include "system.inc"

/* Start of program */

	.text
	.global _start
_start:
/* prepare indexed access to regs */
	ldx	#REGS

/* Enable extended mode */
	ldaa	HPRIO,X
	oraa	#HPRIO_MDA		/* Enable extended mode */
	anda	#~(HPRIO_RBOOT|HPRIO_SMOD|HPRIO_IRV)
	staa	HPRIO,X

/* switch on the LED*/
	ldaa	PACTL,X
	oraa	#0x80
	staa	PACTL,X
	bsr	ledon

	/* Let the bootstrap loader finish sending the last ACK byte */
waitack:
	brclr	SCSR,X #SCSR_TC waitack

/* UART Init is already done by the bootstrap loader at a decent speed. */

/* Wait for reception of SREC (S) */
rxsrec:
	bsr	ledon		/* LED ON when waiting for a packet */
	brclr	SCSR,X #SCSR_RDRF rxsrec
	ldaa	SCDR,X		/* Got a byte */
	cmpa	#SREC		/* Is it S? */
	bne	rxsrec		/* No: try agn */

	/* we got a S. Now receive up to 256 bytes or LF */
	ldy	#BUF		/* Where to store the received bytes*/

rxdata:
	brclr	SCSR,X #SCSR_RDRF rxdata
	bsr	ledoff		/* LED OFF when receiving */
	ldaa	SCDR,X             /* Got a data byte */
	cmpa	#EOL
	beq	exec
	cpy	#BUFEND		/* We can receive up to 256+1 bytes */
	beq	nostore		/* Skip store if we have overflow */
	staa	0,Y
	iny
	bra	rxdata

nostore:
	ldaa	#'!'
	bsr	sertx
	bra	rxsrec		/* Wait for next byte */

	/* Execute the command */
exec:
	ldaa	#'>'
	bsr	sertx
	bra	rxsrec		/* Wait for next command */

;------------------------------------------------------------------------------
; Switches LED ON
; Destroys A
ledon:
	ldaa	PORTA,X
	anda	#0x7F
	staa	PORTA,X
	rts

;------------------------------------------------------------------------------
; Switches LED OFF
; Destroys A
ledoff:
	ldaa	PORTA,X
	oraa	#0x80
	staa	PORTA,X
	rts

;------------------------------------------------------------------------------
;send char in A on serial line
; Destroys: Nothing
sertx:
	staa	SCDR,X
txwait:
	brclr	SCSR,X #SCSR_TC txwait
	rts

