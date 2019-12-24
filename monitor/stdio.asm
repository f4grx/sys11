/* Usual routines */

	.equ	FMT_HEX , 0x01 /* Display as base 16, else base 10 */
	.equ	FMT_WIDE, 0x02 /* Display 4 (not 2) hex digits or 5 (not 3) decimal digits */
	.text
	.global kprintf

kprintf:
	pulx
.Lcharloop:
	stx	sr0
	ldaa	0,X
	beq	.Lend		/* Final zero */
	cmpaa	#'%'
	bne	.Lnormal	/* Normal char */
	/* Else, format char */
	clr	sr1		/* sr1 contains format options */
.Lmorefmt:
	inx
	ldaa	0,X
	beq	.Lend		/* end of string just after format char ! */
	cmpaa	#'%'
	beq	.Lnormal	/* %% sequence to print a single % */
	cmpaa	#'X'
	beq	.Lhexw
	cmpaa	#'x'
	beq	.Lhexn
	cmpaa	#'D'
	beq	.Ldecw
	cmpaa	#'d'
	bne	.Lnext
.Ldecw:
	bset	*sr1, FMT_WIDE
	bra	.Lconvert
.Lhexw:
	bset	*sr1, FMT_WIDE
.Lhexn:
	bset	*sr1, FMT_HEX
	
.Lprint:
	
.Lnormal:
	jsr	sci_putchar
.Lnext:
	ldx	sr0
	inx
	bra	.Lcharloop
.Lend:
	rts
