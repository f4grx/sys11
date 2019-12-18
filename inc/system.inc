/* System registers */
	.equ	REGBASE, 0x1000 /* Just defined for nice assembly output*/

	.equ	OPTION, 0x39
	.equ	HPRIO , 0x3C
	.equ	INIT  , 0x3D	/* Used to map regs and iram */

/* HPRIO bits */
	.equ	HPRIO_PSEL0, 0x01
	.equ	HPRIO_PSEL1, 0x02
	.equ	HPRIO_PSEL2, 0x04
	.equ	HPRIO_PSEL3, 0x08
	.equ	HPRIO_IRV  , 0x10
	.equ	HPRIO_MDA  , 0x20
	.equ	HPRIO_SMOD , 0x40
	.equ	HPRIO_RBOOT, 0x80
