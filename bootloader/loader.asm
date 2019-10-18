.text
.global _start

_start:
	#prepare indexed access to regs
	ldx	0x1000
	#init UART
_receive:
	#wait for SOF
	#sof received. point to buffer start, init csum
	#receive chars, update checksum, until eof
	#eof or overflow
	#prepare response
	#checksum ok?
	#no: send error code and go back to rx
	#execute command
	#send response
	#done
	jmp _receive
	#
	rts

