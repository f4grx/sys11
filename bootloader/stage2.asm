/* Bootloader program to write arbitrary memory in the hc11 system
 * (c) 2019 Sebastien Lorquet
 * Released under WTFPL
 * Behaviour:
 * This GNU AS listing assembles to a short binary that is padded to 256
 * bytes by the linker script.
 * It can be loaded at address 0x0000 in the hc11 via the bootstrap mechanism.
 * Once executed it waits for S-records at the current SCI speed.
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

/* Enable extended mode but stay in special mode*/
	bset	HPRIO,X #HPRIO_MDA /* Enable extended mode */
;	bclr	HPRIO,X #(HPRIO_RBOOT|HPRIO_SMOD|HPRIO_IRV)
	bclr	HPRIO,X #(HPRIO_RBOOT|HPRIO_IRV)

	bset	PACTL,X #0x80 /* Enable LED */
	bclr	PORTA,X #0x80 /* LED ON */

	/* Let the bootstrap loader finish sending the last ACK byte */
waitack:
	brclr	SCSR,X #SCSR_TC waitack

/* UART Init is already done by the bootstrap loader at a decent speed. */

/* Wait for reception of SREC (S) */
rxsrec:
	brclr	SCSR,X #SCSR_RDRF rxsrec
	ldaa	SCDR,X		/* Got a byte */
	cmpa	#SREC		/* Is it S? */
	bne	rxsrec		/* No: try agn */

	/* we got a S. Now receive up to 256 bytes or LF */
	ldy	#BUF		/* Where to store the received bytes*/

rxdata:
	brclr	SCSR,X #SCSR_RDRF rxdata
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
txrep:
	bsr	sertx
	bra	rxsrec		/* Wait for next byte */

	/* Execute the command */
exec:
	/*check record type we handle S1 and S9*/
	ldy	#BUF
	ldaa	0,Y
	staa	type
	iny

	/* Decode hex pair byte to len */
	bsr	hex2dec
	bcs	nostore
	suba	#3	/* remove addr and csum*/
	staa	len
	iny
	iny

	/* Decode hex quad to addr */
	bsr	hex2dec
	bcs	nostore
	staa	addr+0
	iny
	iny
	bsr	hex2dec
	bcs	nostore
	staa	addr+1
	iny
	iny
		
	ldaa	type
	cmpa	#'1'
	beq	sone
	cmpa	#'9'
	beq	snine	
	bra	nostore

sone:
	bsr	hex2dec
	bcs	nostore
	iny
	iny

	pshx
	ldx	addr
	staa	0,X

	/* Memory may be a 2864 EEPROM
	 * Use /DATA polling to wait end of programming
	 */

datapoll:
	ldab	0,X
	cba
	bne	datapoll

	/* Programming done */

	inx
	stx	addr
	pulx

	dec	len
	bne	sone
	ldaa	#'*'
	bra	txrep

snine:
	ldaa	#'>'
	bsr	sertx
	ldx	addr
	pshx
	rts

;------------------------------------------------------------------------------
; Decode pair of hex chars pointed by Y
; If error, carry set, else carry clear and result in A
hex2dec:
	ldaa	0,Y
	bsr	convnibble
	bcs	err
	lsla
	lsla
	lsla
	lsla
	tab
	ldaa	1,Y
	bsr	convnibble	
	bcs	err
	aba
	clc
err:	
	rts
	
;------------------------------------------------------------------------------
; Convert hex char 0-9,A-F to half-byte
convnibble:
	cmpa	#'0'
	blo	hd_fail	/* Below 0 */
	cmpa	#'9'
	bhi	letter  /* Above 9 */
	suba	#'0'
	clc
	rts
letter:
	cmpa	#'F'
	bhi	hd_fail /* Above F */
	cmpa	#'A'
	blo	hd_fail /* Below A*/
	suba	#'A'
	adda	#10
	clc
	rts
hd_fail:
	sec
	rts

;------------------------------------------------------------------------------
;send char in A on serial line
; Destroys: Nothing
sertx:
	staa	SCDR,X
txwait:
	brclr	SCSR,X #SCSR_TDRE txwait
	rts

;------------------------------------------------------------------------------
	.data
type:	.byte	0	/* S-rec type*/
len:	.byte	0	/* nbr of bytes */
addr:	.word	0	/* address */

