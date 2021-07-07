/* Bootloader program to test external memory in the hc11 system
 * (c) 2019 Sebastien Lorquet
 * Released under WTFPL
 * Behaviour:
 * This GNU AS listing assembles to a short binary that is padded to 256
 * bytes by the linker script.
 * It can be loaded at address 0x0000 in the hc11 via the bootstrap mechanism.
 * Program goal: Test the 16K XMEM located at 8000..BFFF
 */

	.equ REGS  , 0x1000

/* HC11 defs */
	.include "sci.inc"
	.include "ioport.inc"
	.include "timer.inc"
	.include "system.inc"

    .equ START, 0x8000
    .equ END,   0xC000

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
	anda	#~(HPRIO_RBOOT|HPRIO_SMOD|HPRIO_IRV)
	staa	HPRIO,X

/* Let the bootstrap loader finish sending the last ACK byte */
waitack:
	brclr	SCSR,X #SCSR_TC waitack

start_test:
	ldab	#0x01
	bsr	testram
	comb
	bsr	testram

	ldab	#0x02
	bsr	testram
	comb
	bsr	testram

	ldab	#0x04
	bsr	testram
	comb
	bsr	testram

	ldab	#0x08
	bsr	testram
	comb
	bsr	testram

	ldab	#0x10
	bsr	testram
	comb
	bsr	testram

	ldab	#0x20
	bsr	testram
	comb
	bsr	testram

	ldab	#0x40
	bsr	testram
	comb
	bsr	testram

	ldab	#0x80
	bsr	testram
	comb
	bsr	testram

	ldaa	#'Z'
	bsr	sertx
	bsr	serlf
	bra	.


testram:
	tba
	bsr	serhex
	bsr	serlf
	ldy	#START	/* start test in xmem */
fill:
	stab	0,Y
	iny			/* test next byte */
	cpy	#0x1000		/* until we reach regs! */
	bne	nextfill
	ldy	#0x1040
nextfill:
	cpy	#END		/* until we end of ram! */
	bne	fill

checkram:
	ldy	#START
check:
	tba
	eora	0,Y
	beq	ok
fail:
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
	ldaa	#'!'
	bsr	sertx
	bsr	serlf
ok:
	iny			/* test next byte */
	cpy	#0x1000		/* until we reach regs! */
	bne	nextcheck
	ldy	#0x1040
nextcheck:
	cpy	#END		/* until we end of ram! */
	bne	check
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
	brclr	SCSR,X #SCSR_TDRE txwait
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
