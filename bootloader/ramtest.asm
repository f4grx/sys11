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
	.equ SOF   , 0x7E
	.equ ESC   , 0x7D
	.equ MASK  , 0x20

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
	anda	#~HPRIO_RBOOT		/* Disable bootstrap ROM */
	anda	#~HPRIO_SMOD		/* Disable special mode */
;	anda	#~HPRIO_IRV
	staa	HPRIO,X

/* switch on the LED*/
	ldaa	PACTL,X
	oraa	#0x80
	staa	PACTL,X
	bsr	ledoff		/* ext mem error */

/* Let the bootstrap loader finish sending the last ACK byte */
waitack:
	brclr	SCSR,X #SCSR_TC waitack

start_test:
	ldaa	#'A'
	bsr	sertx
	bsr	serlf

	ldaa	#0xff
	tab
	bsr	serhex
	bsr	serlf
	bsr	testram

	ldaa	#0x00
	tab
	bsr	serhex
	bsr	serlf
	bsr	testram

	ldaa	#'Z'
	bsr	sertx
	bra	.


testram:
	ldy	#0x100		/* start test after internal ram */
dotest:

	tba
	staa	0,Y		/* store A in ext mem */
	nop			/* wait a bit */
	eora	0,Y		/* read ext mem and xor into A */
	
	bne	ramfail		/* xor not zero: fail */
;	ldaa	#'S'
	bra	next
ramfail:
	psha
	pshb
	pshy
	xgdy
	bsr	serhex
	tba
	bsr	serhex
	ldaa	#' '
	bsr	sertx
	puly
	pulb
	pula
	bsr	serhex
	ldaa	#'F'
	bsr	sertx
	bsr	serlf
next:
	iny			/* test next byte */
	cpy	#0x1000		/* until we reach regs! */
	beq	skip
	cpy	#0x8000		/* until we reach regs! */
	bne	dotest
	rts
skip:
	ldy	#0x1040
	bra	dotest

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
;send byte in A as hex pair
;Destroys: A
serhex:
	psha		/* Save A for low nibble */
	lsra		/* Keep high nibble */
	lsra
	lsra
	lsra
	cmpa	#0x0A /* Check if 0-9 or A-F*/
	blt	zeronineleft
	adda	#('A'-10)
	bra	txhi
zeronineleft:
	adda	#'0'
txhi:
	bsr	sertx
	pula		/* Restore original A */
	anda	#0x0F	/* Keep low nibble */
	cmpa	#0x0A /* Check if 0-9 or A-F*/
	blt	zeronineright
	adda	#('A'-10)
	bra	txlo
zeronineright:
	adda	#'0'
txlo:
	/* Fall through */

;------------------------------------------------------------------------------
;send char in A on serial line
; Destroys: Nothing
sertx:
	staa	SCDR,X
txwait:
	brclr	SCSR,X #SCSR_TC txwait
	rts
;------------------------------------------------------------------------------
;send char in A on serial line
; Destroys: Nothing
serlf:
	psha
	ldaa	#0x0A
	staa	SCDR,X
	pula
	bra	txwait

;------------------------------------------------------------------------------
