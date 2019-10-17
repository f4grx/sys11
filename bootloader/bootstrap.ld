/* ld script for 68hc11 bootstrap loader */
MEMORY
{
  iram (rwx): ORIGIN = 0x0000, LENGTH = 256
}

ENTRY(_start)

/*PHDRS
{
  load PT_LOAD ;
}*/

SECTIONS
{
  . = 0x0000;
  .text : { *(.text) } > iram
  .data : { *(.data) } > iram
  .bss  : { *(.bss) } > iram
  .pad  : {
    FILL(0xAA);
    . = ORIGIN( iram ) + LENGTH( iram ) - 1;
    BYTE(0x55);
  } >iram
}

