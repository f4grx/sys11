sys11 circuit board
===================

The most important part of this subproject is the schematic, that defines all
the components and their connection.

The circuit board can either be as a real two-sided PCB with SMD components for
compacity (the full SMD system can fit in a ebay 100x76x35 aluminium box).

But it can also be built on a pair of 100x160 prototyping boards, using a
HE10-40 and a ribbon cable, or a 64-pin Euro rack mount system. In that case,
connections can be either soldered or wirewrapped(!).

Design philosophy
-----------------
The secret goal is to survive a technical apocalypse, even if such an event is
highly improbable and we would probably have more important priorities than
computers at that time.
Nevertheless, such a computer is interesting for educational purposes, and also
because it is likely much more robust and repairable than a modern computer.

BTW, if you want a more powerful computer, use an embedded ARM SoC (like stm32)
or a Raspberry Pi. However, if it breaks, dont ask me to fix it :p

Another goal of such a project is to allow programming of other computers
without having access to a more complex machine, so we will aim at developing
assemblers and uploaders for PIC, Arduino, and programmers for various memories
like EPROMS, EEPROMs, FLASHes, etc. that can be used in other designs.

Mini circuit board
------------------

The PCB and gerbers in this directory are suitable for integration in an ebay
aluminium box of dimensions 100x76x35mm, using SMD components in SOIC format.
The resulting project is compact and somewhat repairable but requires a lot of
care not to burn the fine PCB traces.

Connection between the boards is achieved by a ribbon cable using straight pins
through the pcb, putting the ribbon cable in a u-shape.

Euro circuit board build
------------------------

This option is much larger but is much more robust than the mini circuit board.
All components can be removed and replaced, and connections can be reworked with
a lot of ease (just taking care not to burn the insulation of nearby wires).

Connection between the board is achieved by a ribbon wire on the side of the
boards, similar to what is used to connect IDE hard drives, except the full 40
pins are used, so the key usually found on IDE ribbon connector has to be
removed. The PCB is cut so that the socket and the plug of the ribbon do not
protrude outside of the euro board. This allows the user to build a simple and
robust metallic case from aluminum strips and flats.

FAQ
---
Q: Why not use a 6502?

A: Because the 6502 has a 8-bit stack pointer which is ridiculously restrictive.
We are not in the situation where the 6800 was $179 and the 6502 was $29.

Q: Why not use a Z80?

A: Because I did not have one when I started this project. However, I plan to
have an assembler for this target.
