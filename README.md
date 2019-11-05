sys11
=====

This is a development board for the 68hc11 microcontroller.

The goal is to build a robus, simple, reliable, and repairable computer from
scrap, similar to what collapseOS is seeking.

CPU: 68HC11A0, the most basic one. This is what I found in my drawer, and is
more desirable than a basic 6502 or other because it has integrated peripherals.
I only choose chips that I have replacement for.

Clock speed: 8 MHz XTAL / OSC, which translates to 2 MHz internal and
bus clock (Cycle time 500 ns). It seems that 16 MHz (4 MHz E clock, 250 ns cycle)
is also working. May switch to that later.

Memory in extended mode:
* Internal RAM kept mapped at 0x0000
* Memory Mapped Registers mapped at 0x0000 (unusual). They overlay the internal
RAM, which means internal RAM locations 00h..3Fh are not usable (except the
locations that are reserved or related to PORTB and PORTC: 0002h-0007h).

* 32KB On-board Main SRAM (62256) in 0000h - 7FFFh CS_RAM = A15
* 16KB Off-board secondary SRAM (half 62256) in 8000h - BFFFh, possibly banked
for additional space using a IO register. Then we could use an even bigger RAM
chip. CS_XRAM = !(A15 & !A14)
* 8KB I/O space with 8 CS lines in C000h - DFFFh (1KB per CS)
CS_IO = !(A15 & A14 & !A13)
* 8KB EPROM in E000h - FFFFh - monitor and SPI flash loader. For development,
this chip can be replaced with a RAM/EEPROM/NVRAM, the board actually use a
DIL32 socket instead of 28, that exposes the nWR line. An additional pair of DIL
sockets can be used to route this signal to the actual /WR line of the debug
RAM. This is actually using a 27256 EPROM with 4 manually selected banks, to
allow for different boot images (asm, communications, shell, storage mgmt, TBD)
CS_ROM = !(A15 & A14 & A13)
Main address decoding can be done with a few NAND gates (7400 2-input, and 7410
3-input NAND gates).

Peripherals:
* Robust 5V power supply (LF50 LDO, max voltage 40V) with reverse voltage
protection
* Additional LM2575 step-down to reduce LDO heating, but can be bypassed.
* 3 outputs (PORTA) dedicated as SPI CS selector lines (3 bits + 74138 decoder
+ normal /SS line)
* SCI : (UART), TX and RX lines on a pin header for external communication.
10K pullups are installed to ensure proper behaviour in bootstrap mode where
PORTD is configured as open drain
* SPI : CS0 connected to a SPI flash chip, CS1 to a SD/MicroSD card.
* HE10-40 connector for external bus. All lines buffered by 74xx245.
* I2C bus using discrete N-MOS transistor on OC lines to drive the bus in open
drain, and IC lines used to read the bus.

Seriously planned features:
* Multi-VPP EPROM programmer
* Fully shielded aluminum enclosure for a 100x160 board

Planned vaporware features:
* removable SPI flash cardriges
* SCSI controller
* Additional UARTs
* Wireless communication interfaces (AX.25)
* Audio cassette program storage (Kansas)
* MCP 2515 CAN Bus on SPI

What is already done
--------------------
* Memory map
* I have some chips: 4x 68HC11A0, 1x 68HC11A1, some 74xx245, 573, 138, 00, 10,
27256, RAMs
* Ordered a 2864 EEPROM from ebay
* Schematic
* Bootstrap uploader in Python3.
* Clock, Reset, UART connection, power supply with decoupling
* The HC11 starts in bootstrap mode and successfully executes code to drive LED.

What is being done right now
----------------------------
* Prototype, using soldered wirewrap wire.
* Routing of high address bus

What remains to be done
-----------------------
* Address decoding logic using 74HC00 and 74HC10
* Decision to use shielded wire to route the E signal
* bootstrapped program to load more memory

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
