sys11
=====

This is a development board for the 68hc11 microcontroller.

The goal is to build a robus, simple, reliable, and repairable computer from
scrap, similar to what collapseOS is seeking.

CPU: 68HC11A0, the most basic one. This is what I found in my drawer, and is
more desirable than a basic 6502 or other because it has integrated peripherals.
I only choose chips that I have replacement for.

Clock speed: 8/12/16 MHz XTAL / OSC, which translates to 2/3/4 MHz internal and
bus clock (Cycle time 500/333/250 ns). Can be selected according to the speed
grade of your chip.

Memory:
* 32KB On-board Main SRAM (62256) in 0000h - 7FFFh CS_RAM = A15
* 16KB Off-board secondary SRAM (half 62256) in 8000h - BFFFh, possibly banked
for additional space using a IO register. Then we could use an even bigger chip.
CS_XRAM = !(A15 & !A14)
* 8KB I/O space with 8 CS lines in C000h - DFFFh (1KB per CS)
CS_IO = !(A15 & A14 & !A13)
* 8KB EPROM in E000h - FFFFh - monitor and SPI flash loader. For development,
this chip can be replaced with a RAM or NVRAM, the board actually use a DIL32
socket instead of 28, that exposes the nWR line. An additional pair of DIL
sockets can be used to route this signal to the actual /WR line of the debug
RAM. CS_ROM = !(A15 & A14 & A13)
Main address decoding can be done with a few NAND gates (7400 2-input, and 7410
3-input NAND gates).

Peripherals:
* Robust 5V power supply (LF50 LDO, max voltage 40V) with reverse voltage
protection
* Additional LM2575 step-down to reduce LDO heating, but can be bypassed
* 4 outputs (PORTA) dedicated as SPI CS lines (3 bits + 74138 decoder + OE line)
* SCI : (UART), TX and RX lines on a pin header for external communication
* SPI : CS0 connected to a SPI flash chip, CS1 to a SD/MicroSD card.
* HE10-40 (IDE drive) connector for external bus. All lines buffered by 74xx245.
* I2C bus

Seriously planned features:
* Multi-VPP EPROM programmer

Planned vaporware features:
* removable SPI flash cardriges
* Fully shielded aluminum enclosure for a 100x160 board
* SCSI controller
* Additional UARTs
* Wireless communication interfaces (AX.25)
* Audio cassette program storage (Kansas)
* MCP 2515 CAN Bus on SPI

What is already done
--------------------
* Memory map
* I have some chips: 4x 68HC11A0, 1x 68HC11A1, some 74xx245, 573, 138, 00, 10

What is being done right now
----------------------------

* Schematic
* Prototype

SPI bus
-------

The hardware SPI bus is used in master mode.
* The SS line is used as CS for the internal flash memory (mass storage).
* The OC3 line is used as CS for the SD card

TODO: If I can find 4 outputs I will use a HC138 to create 8 CS lines and put the SPI bus on a SUBD-15 (MISO/MOSI/SCK+6CS+GND+5VCC)

I2C bus
-------

A proper i2c bus needs two bidirectionnal open drain lines, which are not available on the HC11 if one wants to benefit from
the hardware SPI block on PORTD. Moreover, the HC11 does not have a hardware i2c, so bitbanging must be used.

The i2c bus will be done on port A using Input Capture and Output Compare lines. To read SDA and SCL, the IC1 and IC2 lines are used. To write the bus, the OC1 and OC2 lines are used to drive small BS170 MOSFETs. This requires inversion of the bits to write, which is not a big problem.
