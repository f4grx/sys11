/* ld script for 68hc11 bootstrap loader */
MEMORY
{
  iram (rwx): ORIGIN = 0x0000, LENGTH = 256
}

ENTRY(_start)

SECTIONS
{
  . = 0x0000;
  .text : {
    *(.text)
    FILL(0x00);
    . = ORIGIN( iram ) + LENGTH( iram ) - 2;
    BYTE(0x55);
    BYTE(0x55);
  } > iram

  .data : { *(.data) } > iram
  .bss  : { *(.bss) } > iram
}

