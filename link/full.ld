/* ld script for 68hc11 bootstrap loader */
MEMORY
{
  regs (rw ): ORIGIN = 0x0000, LENGTH = 0x40
  iram (rwx): ORIGIN = 0x0040, LENGTH = 0xC0
  ram  (rwx): ORIGIN = 0x0100, LENGTH = 0x7F00
  xram (rwx): ORIGIN = 0x8000, LENGTH = 0x4000
  rom  (rx ): ORIGIN = 0xE000, LENGTH = 0x2000
  vecs (r  ): ORIGIN = 0xFFD6, LENGTH = 0x002E
}

ENTRY(_start)

SECTIONS
{
	.text : {
		*(.text)
	} > rom

	.vectors : {
		*(.vectors)
	} > vecs

	.ndata : {
		*(.ndata)
	} > iram
	.data : {
		*(.data)
	} > ram
	.bss : {
		*(.bss)
	} > ram
}


