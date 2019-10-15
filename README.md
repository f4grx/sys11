sys11
=====

This is a development board for the 68hc11 microcontroller.

The goal is to build a robus, simple, reliable, and repairable computer from scrap, similar to what collapseOS is seeking.

CPU: 68HC11A0, the most basic one. This is what I found in my drawer, and is more desirable than a basic 6502 or other because it has integrated peripherals. I only choose chips that I have replacement for.

Clock speed: 8/12/16 MHz XTAL / OSC, which translates to 2/3/4 MHz internal and bus clock (Cycle time 500/333/250 ns). Can be selected according to the speed grade of your chip.

Memory:
* 32KB Data SRAM (62256) in 0000h - 7FFFh CS_DRAM = !A15
* 16KB Code SRAM (half 62256) in 8000h - BFFFh, possibly banked for additional space. CS_CRAM = !(A15 & !A14)
* 8KB I/O space with 8 CS lines in C000h - DFFFh (1KB per CS) CS_IO = !(A15 & A14 & !A13)
* 8KB EPROM in E000h - FFFFh - monitor and SPI flash loader CS_ROM = !(A15 & A14 & A13)
Main address decoding can be done with a few NAND gates (7400 2-input, and 7410 3-input if available).

Peripherals:
* Robust 5V power supply (LF50 LDO, max voltage 40V) with reverse voltage protection
* 4 outputs (PORTA) dedicated as SPI CS lines (3 bits + 74138 decoder + OE line)
* SCI : (UART), TX and RX lines on a pin header for external communication
* SPI : CS0 connected to a SPI flash chip
* HE10-40 (IDE drive) connector for external bus. All lines buffered by 74244/74245.
* TODO: I2C bus, also available on external bus connector. Requires bidir lines.

Planned vaporware features:
* removable SPI flash cardriges
* Fully shielded aluminum enclosure for a 100x160 board
* SCSI controller
* Additional UARTs
* Multi-VPP EPROM programmer
* Wireless communication interfaces (AX.25)
* Audio cassette program storage (Kansas)
* MCP 2515 CAN Bus on SPI

What is already done
--------------------
* Memory map
* I have some chips: 3x 68HC11A0, 2x 68HC11A1, some 74LS245 and 74LS138

What is being done right now
----------------------------

* Schematic
* Prototype
