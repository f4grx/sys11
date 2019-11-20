sys11 hardware
===================

The most important part of this subproject is the schematic, that defines all
the components and their connection.

The circuit board can either be as a real two-sided PCB with SMD components for
compacity (the full SMD system can fit in a ebay 100x76x35 aluminium box).

But it can also be built on a pair of 100x160 prototyping boards, using a
HE10-40 and a ribbon cable, or a 64-pin Euro rack mount system. In that case,
connections can be either soldered or wirewrapped (!).

Design philosophy
-----------------
The secret goal of this project is to survive a technical apocalypse, even if
such an event is highly improbable and we would probably have more important
priorities than computers at that time.
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

The PCB and gerbers in this directory describe a board 100x70mm, that will fit
an ebay aluminium box of dimensions 100x76x35mm, using SMD components in SOIC
format. The resulting project is compact and somewhat repairable but requires a
lot of care not to burn the fine PCB traces. To help with repairs, each SMD
trace has two vias in it, which should reduce the risk of having lifted pads
when desoldering a SMD component. When desoldering a broken SMD component, it is
often desirable to limit PCB damage by cutting the chip leads with pliers, then
removing each of the still soldered leads one by one.

Connection between the two boards is achieved by a ribbon cable using straight pins
through the pcb, putting the ribbon cable in a u-shape. Board-to-board
stackable connectors can also be used.

Euro circuit board
------------------

This option is much larger but is much more robust than the mini circuit board.
All components can be removed and replaced, and connections can be reworked with
a lot of ease (just taking care not to burn the insulation of nearby wires).

Interconnections can use either soldered wirewrap 30AWG wire, or just plain
wirewrapping if sockets, tools and wires are available.

Connection between the two board is achieved by a ribbon wire on the side of the
boards, similar to what is used to connect IDE hard drives, except the full 40
pins are used, so the key usually found on IDE ribbon connector has to be
removed. The PCB is cut so that the socket and the plug of the ribbon do not
protrude outside of the euro board. This allows the user to build a simple and
robust metallic case from aluminum strips and flats.

Here is the current state of the prototype, top and bottom:

![Prototype top side](docs/pics/protoboard/15_proto_top.jpg)

![Prototype bottom side](docs/pics/protoboard/16_proto_bot.jpg)

Enclosure
---------

Since one of the goal of this project is to bootstrap electronic circuits after
a civilization collapse, it is important to make sure that it can itself survive
the kind of difficult environment that may cause this collapse, including a
severe EMP event.

The only way to do that is to shield everything. So a proper metallic enclosure
has to be used. ebay has lot of extruded aluminium boxes suitable for both the
mini board and 100x160 boards. If the builder has machining resources, then it
is also possible to mill a box or build one from aluminium sheet and bars.

Connections are much more critical than the board itself. Since an EMP event
will induce high voltage in any wire that is not properly shielded, any
connector and opening in the project box is critical. I will only be using a
single SUB-D connector with the least amount of pins (probably 15). The
connector itself will receive a  metallic cover and will be disconnected when
the computer is not actively used.

Additional protections will be added on each electrical line crossing the box
through this SUBD connector: the possible components are gas discharge cells,
series resistors, and bidirectional transil diodes. The goal is to minimize the
impact of high voltages by reducing them to a low value if possible.

The ideal situation would be a single UART communication channel using
opto-isolated signals, like TOSLINK plastic optical fibers. Only the power supply would
then need severe EMP protections.

Prototype Progress
------------------
![proto top 1](hardware/images/proto_top_1.jpg)
![proto top 2](hardware/images/proto_top_2.jpg)
![proto bot 3](hardware/images/proto_bot_3.jpg)
![proto top 4](hardware/images/proto_top_4.jpg)
![proto top 5](hardware/images/proto_top_5.jpg)
![proto bot 6](hardware/images/proto_bot_6.jpg)

