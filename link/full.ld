/* ld script for 68hc11 bootstrap loader */
MEMORY
{
  regs (rw ): ORIGIN = 0x0000, LENGTH = 0x40	/* HC11 registers mapped at 0 */
  iram (rwx): ORIGIN = 0x0040, LENGTH = 0xC0	/* Internal RAM */
  ram  (rwx): ORIGIN = 0x0100, LENGTH = 0x7F00	/* On-board ext RAM, 32k */
  xram (rwx): ORIGIN = 0x8000, LENGTH = 0x4000	/* Off-board optional RAM, 8K */
  rom  (rx ): ORIGIN = 0xE000, LENGTH = 0x2000	/* Monitor EPROM */
  vecs (r  ): ORIGIN = 0xFFC0, LENGTH = 0x0040	/* Special place in EPROM */
}

ENTRY(_start)

SECTIONS
{

/* ROM SECTIONS */

	.text : {
		*(.text)
	} > rom

	.rodata : {
		*(.rodata)
	} > rom

	.vectors : {
		*(.vectors)
	} > vecs

/* RAM SECTIONS */

	.data : {
		*(.softregs)
		*(.data)
	} > iram
	.bss : {
		*(.bss)
	} > ram
}

