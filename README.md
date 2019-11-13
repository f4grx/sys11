sys11
=====

This is a development board for the 68hc11 microcontroller.

The goal is to build a robus, simple, reliable, and repairable computer from
scrap, similar to what collapseOS is seeking.

CPU: 68HC11A0, the most basic one. This is what I found in my drawer, and is
more desirable than a basic 6502 or other because it has integrated peripherals.
I only choose chips that I have replacement for.

Clock speed: 8 MHz XTAL / OSC, which translates to 2 MHz internal and
bus clock (Cycle time 500 ns). It seems that 16 MHz (4 MHz E clock, 250 ns
cycle) is also working. May switch to that later.

Memory map (HC11 extended mode)
-------------------------------

* Internal RAM kept mapped at 0x0000
* Memory Mapped Registers mapped at 0x0000 (unusual). They overlay the internal
RAM, which means internal RAM locations 00h..3Fh are not usable (except the
locations that are reserved or related to PORTB and PORTC: 0002h-0007h).

* 32KB On-board Main SRAM (62256) in 0000h - 7FFFh CSRAM = A15
* 16KB Off-board secondary SRAM (half 62256) in 8000h - BFFFh, possibly banked
for additional space using a IO register. Then we could use an even bigger RAM
chip. CSXRAM = !(A15 & !A14)
* 8KB I/O space with 8 CS lines in C000h - DFFFh (1KB per CS)
CSIO = !(A15 & A14 & !A13)
* 8KB EPROM in E000h - FFFFh - monitor and SPI flash loader. For development,
this chip can be replaced with a RAM/EEPROM/NVRAM, the board actually use a
DIL32 socket instead of 28, that exposes the nWR line. An additional pair of DIL
sockets can be used to route this signal to the actual /WR line of the debug
RAM. This is actually using a 27256 EPROM with 4 manually selected banks, to
allow for different boot images (asm, communications, shell, storage mgmt, TBD)
CSROM = !(A15 & A14 & A13)
* External bus lines are only driven when external bus is actually addressed.
It is not driven when the on-board memory (32K RAM, 8K ROM) is addressed.
CSEXT = CSIO & CSXRAM

Main address decoding can be done with a few NAND gates (7400 2-input, and 7410
3-input NAND gates).

Peripherals
-----------

* 5V power from USB
* SCI : (UART), TX and RX lines on a pin header for external communication.
10K pullups are installed to ensure proper behaviour in bootstrap mode where
PORTD is configured as open drain.
* HE10-40 connector for external bus. All lines buffered by 74xx245.

Seriously planned features
--------------------------

* Robust 5V power supply (LF50 LDO, max voltage 40V) with reverse voltage
protection
* Additional LM2575 step-down to reduce LDO heating (bypassable).
* 3 outputs (PORTA) dedicated as SPI CS selector lines (3 bits + 74138 decoder +
normal /SS line)
* I2C bus using discrete N-MOS transistor on OC lines to drive the bus in open
* Multi-VPP EPROM programmer on a secondary board
* Fully shielded aluminum enclosure for a 100x160 board

Planned vaporware features
--------------------------

* Removable SPI/I2C flash cardriges
* SCSI controller
* Additional UARTs and various comm interfaces (fiber, RS422 RS485...)
* Ethernet
* Wireless communication interfaces (AX.25, 1200 bauds packet modem)
* Audio cassette program storage (Kansas)
* MCP 2515 CAN Bus on SPI
* Dual-port RAM in IO space for communication and shared mem with another HC11.
* Infinite ROM space for device drivers :)

What is already done
--------------------
* Memory map
* Schematic (includes errors)
* I have some chips: 4x 68HC11A0, 1x 68HC11A1, 1x 68HC11E2 (thanks vince),
some 74xx245, 573, 138, 00, 10, 27256, various 32Kx8 and 8Kx8 RAMs
* Ordered a 2864 EEPROM from ebay
* Bootstrap uploader in Python3.
* Working hardware prototype on 10x16 perforated board
* Clock, Reset, UART connection, power supply with decoupling
* Memory map decoder using logic gates
* The HC11 starts in bootstrap mode and successfully executes code to drive LED.
* The HC11 is able to select the expanded mode and test the 32K RAM without errors (and also a RAM in the dedicated 8K EPROM space).

Summary: The system is validated on the soldered wirewrap euro board.

What is being done right now
----------------------------
* IRQ lines from expansion board to cpu (currently broken due to lack of pullups)
* bootstrap program to load more memory. Initally planned to use a custom HDLC protocol, but will rather load S-records directly.
* Writing the monitor ROM including a RAM allocator

What remains to be done
-----------------------
* Determine next step for hardware. Secondary board with more RAM?
* NVRAM SSD using these old bq samples maxim generously offered me for free
multiple years ago (Unfortunately this board will be a one-of-a-kind add-on unless
you have these chips available or are able to find enough RAM chips)
* I2C bus hardware

Software roadmap
----------------
* Bootloader for extended mode - WIP
* Malloc
* Basic shell to manipulate memory
* SPI and I2C drivers
* Filesystem
* ed-based text editor
* Assembler
* Self host the system
* Definition of a position independent binary format
* IO kernel
* Program loader

SPI bus
-------

The HC11 hardware SPI bus wil be used in master mode.
* The SS line is used as OE for a 74138 decoder that provides 8 CS lines from
3 OC lines of port A.

I2C bus
-------

A proper i2c bus needs two bidirectionnal open drain lines, which are not
available on the HC11 if one wants to benefit from the hardware SPI block on
PORTD. Moreover, the HC11 does not have a hardware i2c, so bitbanging must be
used.

The i2c bus will be done on port A using Input Capture and Output Compare lines.
To read SDA and SCL, the IC1 and IC2 lines are used. To write the bus, the OC1
and OC2 lines are used to drive small BS170 MOSFETs. This requires inversion of
the bits to write, which is not a big problem for the driver.

FAQ
---
Q: Why not use a 6502?

A: Because the 6502 has a 8-bit stack pointer which is ridiculously restrictive.
We are not anymore in the situation where the 6800 was $179 and the 6502 was $29.

Q: Why not use a Z80?

A: Because I did not have one when I started this project. However, I plan to
have an assembler for this target. If you want to play with the Z80, have a
look at the RC2014 project and/or github.com/hsoft/collapseos

