	.text
	.equ REGS, 0x1000
	.equ BUF,  0x100 /* 256 bytes byffer for command reception */
	.equ SOF,  0x7E
	.equ ESC,  0x7D
	.equ MASK, 0x20

/* UART defs */
	.set BAUD , 0x2B
	.set SCCR2, 0x2D
	.set SCSR , 0x2E
	.set SCDAT, 0x2F

/* Start of program */

	.global _start
_start:
	/* prepare indexed access to regs */
	ldx	#REGS
	/* init UART */
	ldaa	#0x30		/* prediv 13, postdiv 1, 9600 bauds @ 8 MHz */
	staa	BAUD,X
	ldaa	#0x0C		/* No interrupts, enable TX and RX */
	staa	SCCR2,X

	/* Wait for character reception */
_rxsof:
	brclr	SCSR,X 0x20 _rxsof
	ldaa	SCDAT,X

	#wait for SOF
	cmpa	#SOF
	bne	_rxsof

	#we got a SOF. Now receive up to 256 bytes
	ldy	#0 /* This is the current buffer length */
	ldab	#0 /* This is the checksum */
_rxdata:
	brclr	SCSR,X 0x20 _rxdata
	ldaa	SCDAT,X

	#sof received. point to buffer start, init csum
	#receive chars, update checksum, until eof
	#eof or overflow
	#prepare response
	#checksum ok?
	#no: send error code and go back to rx
	#execute command
	#send response
	#done
	jmp _rxsof
	rts

