stage2.asm Bootloader
=====================

This program is a second stage bootloader to test programs without having an
EPROM programmer.

In bootstrap mode, the 68HC11 will load 256 bytes from the SCI into the internal
RAM at address zero, and then run it.

We use this feature to allow loading a full s-record file in the memory map.

The EPROM socket has 32 pins even if a 27256 is 28-pin. This header has the
~WR signal on it. This allows the use of a special small PCB header to use a
62256 in place of the normal 27256 (basically, switch around the A14, VPP, and
~WR lines).

With this setup, it is possible to load some code in the D000h.FFFFh range and
run it as if the system had real EPROM. This can be used to develop the first
programs that you will run on this board.

Of course this means that the program is volatile, unless you use some
battery-backed RAM from Dallas or a 2864 EEPROM.

Loading algorithm:
------------------

* We Reuse UART settings installed by the bootstrap ROM
* Init extended mode by clearing HPRIO.MODA
* Wait for a S character
* Read all chars until LF or overflow
* Read type char (0-9)
* Read Length and address hex pairs
* If this is a S1 line, convert hex pairs, write to mem
* If this is a S9 line, jump to address
* Else signal an error

This is pretty much all that can be done safely in 256 bytes of RAM.

Note that there is still room for optimization in this code, that was
written very naively.

Build process
-------------
The only requirement is binutils compiled for 68hc11 and make

The Makefile is common and uses some make iterations and magic to generate
build targets.

Other programs
--------------
This directory also contains some small test programs that can run directly
from the bootstrap rom:
* a memory dumper
* a copy of this, made to read the internal EEPROM of a HC11E2
* a RAM tester to ensure the external bus is wired properly

These programs can be uploaded in a HC11 in bootstrap mode using upload.py from
the tools directory.

