forth interpreter
=================

This directory implements a minimal forth interpreter in 68hc11 GNU assembler.
It can be assembled on any linux computerm or on any computer on which a
m68hc11 targeted binutils can be compiled.

It is kept minimally small on purpose. The goal is not speed, but compactness.

The assembly source is not made to be relocatable. It is a single file that
will assemble to fixed (but configurable) addresses.

Once working, it will be rewritten in a forth-based assembler for assembly on
any forth-executing machine.

Goals:
* Write a forth interpreter supporting the core words
* Write block support using a SPI EEPROM
* Write a 68hc11 assembler in forth
* Compile the forth assembler written in forth

And probably collaborate more with the CollapseOS project in parallel.

Design choices:
* Maximally large RAM space in external RAM
* Any small variables used by the interpreter itself in the internal RAM
* 8K ROM at E000h..FFFFh
* 8K IO space for peripherals at C000h..DFFFh
* 32K RAM (almost, always available) in 0100h..7FFFh
* 16K RAM (optional extension) in 8000h..BFFFh for a total of 48K of RAM.

