/* Memory management routines */

	.equ	RAMSTART, 0x0100
	.equ	RAMEND  , 0x7FFF

	.text
	.global	mm_init
mm_init:
	rts

	.global mm_alloc
mm_alloc:
	ldx	#0
	rts

	.global mm_free
mm_free:
	rts

