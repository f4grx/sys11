/* Bootloader program to test external memory in the hc11 system
 * (c) 2019 Sebastien Lorquet
 * Released under WTFPL
 * Behaviour:
 * This GNU AS listing assembles to a short binary that is padded to 256
 * bytes by the linker script.
 * It can be loaded at address 0x0000 in the hc11 via the bootstrap mechanism.
 */

	.equ REGS  , 0x1000
	.equ START , 0xFFD6
	.equ COUNT , 42

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
;	cli

/* Enable extended mode */
	ldaa	HPRIO,X
	oraa	#HPRIO_MDA		/* Enable extended mode */
;	anda	#~(HPRIO_RBOOT|HPRIO_SMOD|HPRIO_IRV)
	staa	HPRIO,X

/* Let the bootstrap loader finish sending the last ACK byte */
waitack:
	brclr	SCSR,X #SCSR_TC waitack

doit:
	ldy	#COUNT
	sty	len
	ldy	#START
	sty	addr

loop:
	ldy	addr
	ldaa	0,y
	bsr	serhex
	iny
	sty	addr

	ldy	len
	dey
	sty	len

	bne	loop
	bsr	serlf
	bra	.

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
	.data
addr:	.word	0
len:	.word	0

