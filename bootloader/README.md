Bootloader
==========

This program is a second stage bootloader to test programs without having an
EPROM programmer.

In bootstrap mode, the 68HC11 will load 256 bytes from the SCI into the internal
RAM at address zero, and then run it.

The EPROM socket has 32 pins even if a 27256 is 28-pin. This header has the
~WR signal on it. This allows the use of a special small PCB header to use a
62256 in place of the normal 27256 (basically, switch around the A14, VPP, and
~WR lines).

With this setup, it is possible to load some code in the D000h.FFFFh range and
run it as if the system had real EPROM. This can be used to develop the first
programs that you will run on this board.

Of course this means that the program is volatile, unless you use some
battery-backed RAM from Dallas or others.

Loading algorithm:
------------------

At first, the program inits the UART.

Then, it waits for HDLC framed packets that contains 1-byte of command, followed
by parameters and a 16-bit CRC (could be simplified to a XOR FFh checksum).

The packet format is as follows, similar to PPP in HDLC framing (RFC 1662)
* Frame delimiter: 7Eh (can be sent consecutively)
* Escape character: 7Dh
* Escape modification: XOR 20h

The commands are:
* 00h: Clear RAM, parameters (16-bit address) (1 byte length)
* 01h: Write RAM, parameters (16-bit address) (N<=256 bytes data)
* 02h: Read RAM, parameters (16-bit address) (1 byte length)
* 03h: Run, parameters (16-bit address)

Addresses are encoded big-endian.

Command parameters are stored in a data RAM buffer before checking the CRC
or checksum. Command is only executed if the CRC/Checksum is correct.

The responses are:
* 00h: Clear RAM done, status: 00h OK, 01h out of range
* 01h: Write RAM done, status: 00h OK, 01h out of range
* 02h: Read RAM done, status: 00h OK followed by data, 01h out of range

More commands can be added later (eg, register access, step by step execution).

