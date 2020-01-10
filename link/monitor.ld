/* ld script for 68hc11 monitor EPROM */
MEMORY
{
  regs (rw ): ORIGIN = 0x0000, LENGTH = 0x0040	/* - 0x003F HC11 registers mapped at 0   */
  iram (rwx): ORIGIN = 0x0040, LENGTH = 0x00C0	/* - 0x00FF Internal RAM (page0)         */
  ram  (rwx): ORIGIN = 0x0100, LENGTH = 0x7F00	/* - 0x7FFF On-board ext RAM, 32K        */
  xram (rwx): ORIGIN = 0x8000, LENGTH = 0x4000	/* - 0xBFFF Off-board optional RAM, 16K  */
  io   (rw ): ORIGIN = 0xC000, LENGTH = 0x2000  /* - 0xDFFF I/O space, not memory mapped */
  rom  (rx ): ORIGIN = 0xE000, LENGTH = 0x1FC0	/* - 0xFFBF Monitor EPROM                */
  vecs (r  ): ORIGIN = 0xFFC0, LENGTH = 0x0040	/* - 0xFFFF Special place in EPROM       */
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
		*(.scommands)
		*(.scommands_end)
	} > rom

	.vectors : {
		*(.vectors)
	} > vecs

/* RAM SECTIONS */

	.data : {
		*(.data)
	} > iram
	.bss : {
		*(.bss)
		*(COMMON)
	} > iram

/* EXTERNAL RAM */
	.edata : {
		*(.edata)
		HEAPSTART = .;
	} > ram
	.xdata : {
		HEAPEND = .;
		*(.xdata)
	} > xram
}

