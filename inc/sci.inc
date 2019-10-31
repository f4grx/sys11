/* Include file for sci reg definitions */

	.equ BAUD , 0x2B
	.equ SCCR1, 0x2C
	.equ SCCR2, 0x2D
	.equ SCSR , 0x2E
	.equ SCDR , 0x2F

/* BAUD bits */

	.equ BAUD_SCR0_BIT   ,0 /* Baud rate select 0 */
	.equ BAUD_SCR1_BIT   ,1 /* Baud rate select 1 */
	.equ BAUD_SCR2_BIT   ,2 /* Baud rate select 2 */
	.equ BAUD_RCKB_BIT   ,3 /* (TEST) Baud rate clock test */
	.equ BAUD_SCP0_BIT   ,4 /* Prescaler Select 0 */
	.equ BAUD_SCP1_BIT   ,5 /* Prescaler Select 1 */
	.equ BAUD_TCLR_BIT   ,7 /* (TEST) Clear baud-rate timing chain */

	.equ BAUD_SCR0       ,0x01
	.equ BAUD_SCR1       ,0x02
	.equ BAUD_SCR2       ,0x04
	.equ BAUD_RCKB       ,0x08
	.equ BAUD_SCP0       ,0x10
	.equ BAUD_SCP1       ,0x20
	.equ BAUD_TCLR       ,0x80

	.equ BAUD_PRESC_MASK ,0x30
	.equ BAUD_PRESC_1    ,0x00
	.equ BAUD_PRESC_3    ,0x10
	.equ BAUD_PRESC_4    ,0x20
	.equ BAUD_PRESC_13   ,0x30

	.equ BAUD_RATE_MASK ,0x07
	.equ BAUD_RATE_1    ,0x00
	.equ BAUD_RATE_2    ,0x01
	.equ BAUD_RATE_4    ,0x02
	.equ BAUD_RATE_8    ,0x03
	.equ BAUD_RATE_16   ,0x04
	.equ BAUD_RATE_32   ,0x05
	.equ BAUD_RATE_64   ,0x06
	.equ BAUD_RATE_128  ,0x07

/* SCCR1 bits */

	.equ SCCR1_WAKE_BIT ,3 /* Wake method select */
	.equ SCCR1_M_BIT    ,4 /* Character length 0=>8N1, 1=>9N1 */
	.equ SCCR1_T8_BIT   ,6 /* Transmit data bit 8 */
	.equ SCCR1_R8_BIT   ,7 /* Receive data bit 8 */

/* SCCR2 bits */

	.equ SCCR2_SBK_BIT    ,0 /*  */
	.equ SCCR2_RWU_BIT    ,1 /*  */
	.equ SCCR2_RE_BIT     ,2 /*  */
	.equ SCCR2_TE_BIT     ,3 /*  */
	.equ SCCR2_ILIE_BIT   ,4 /*  */
	.equ SCCR2_RIE_BIT    ,5 /*  */
	.equ SCCR2_TCIE_BIT   ,6 /* Transmit Complete Interrupt Enable */
	.equ SCCR2_TIE_BIT    ,7 /* Transmit Interrupt Enable */

/* SCSR bits */

	.equ SCSR_FE_BIT   ,1
	.equ SCSR_NF_BIT   ,2
	.equ SCSR_OR_BIT   ,3
	.equ SCSR_IDLE_BIT ,4
	.equ SCSR_RDRF_BIT ,5    /* Receive data register full */
	.equ SCSR_TC_BIT   ,6
	.equ SCSR_TDRE_BIT ,7

	.equ SCSR_FE       ,0x02
	.equ SCSR_NF       ,0x04
	.equ SCSR_OR       ,0x08
	.equ SCSR_IDLE     ,0x10
	.equ SCSR_RDRF     ,0x20
	.equ SCSR_TC       ,0x40
	.equ SCSR_TDRE     ,0x80
