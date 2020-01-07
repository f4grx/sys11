Monitor
=======

The monitor has several roles. It mainly provides routines to be used by user
programs. It also manages the RAM and storage devices. It is really a very
little kernel.

Runtime environment
-------------------
Internal RAM AND Registers are both mapped at 0000h. Because of the access
priority enforced by the HC11, this means that both are available for
direct addressing, and there is no need to use X-relative addressing to compact
the code that uses register. X is then free for other uses.

* Addresses 00h .. 3Fh are the internal registers.
* Addresses 40h .. FFh are the internal RAM
* Addresses 0100h .. 7FFFh are the external RAM, managed by malloc
* Addresses 8000h .. BFFFh are the secondary RAM, usable as scratch data/code.
* Addresses C000h .. DFFFh are the Memory Mapped IO registers
* Addresses E000h .. FFFFh are the Monitor ROM.

Simulation
==========
gdb 8.3.1 still supports 68hc11 simulation. Program can be somewhat run using
the sim target but single stepping does not work. See the ../sim/README.md
document for how to use our custom simulator.

The gdb simulator currently has a bug that prevents me from remapping the I/O
registers via a write to INIT. It was reported to upstream with no feedback.

register relocation works on our custom simulator.

Application Binary Interface
============================

Overview and remarks
--------------------

It is important to ensure an homogenous function calling convention and binary
interface. This ABI should be easy to use in assembly and memory saving.

The HC11 has very few registers. Usually, X is used for index access to
peripheral registers mapped at 0x1000. We will avoid this by mapping registers
at 0x0000 (in the internal RAM) since it has several advantages:
* avoids having registers in the middle of the external RAM,
* allows to use the direct addressing mode
* free the X register for other uses.

Also the HC11 does not allow SP-relative addressing, so local vars and parameter
passing is not easy to achieve using the stack.

The ideal case would be to use X as stack pointer. However this use wont be
systematic.

Soft registers
--------------
The HC11 definitely does not have enough registers for comfortable programming.
We will increase the number of available registers by statically allocating some
internal RAM addresses as static registers. For the moment 8 16-bit soft regs
are allocated, sp0 to sp3 and st0 to st3. They are allocated in page0 to allow
access using the direct address mode, avoiding the extended mode.

Caling functions
----------------
* Parameters in soft registers sp0..sp3
* Local vars in st0..st3
* X,A/B/D,Y can be used for computations (Only A/B/D allows full arithmetics).
* If any reg has to be preserved across a call, it has to be saved on the stack.
* Return value in D

Shell
=====
The shell reads commands on the HC11 SCI, and then recognizes and execute them.

For the moment there is an echo command that can be used to switch local echo
on/off.

File System and File Storage
============================
A full uniform VFS like in Unix requires a root device or a RAMFS to define the
mount points. To save this, storage devices are mounted as volumes, like DOS.
However, unix-style paths are used to refer to files within volumes.

The following rules apply
* Absolute paths start with a /
* Relative paths start with something else
* The root directory / can not hold files, only mount points.
* It is possible to use ls on the root dir, this is used to enumerate active
mount points
* The mount points have single character numeric ASCII identifiers that map
directly to the volume ID.

VFS state includes the following variables:
* Current volume ID
* Current path inside current volume (max 40 chars). Root dir is an empty
string.

Some filesystems do not support nested directories. In that case all files are
directly stored in the root directory of the volume.

Block devices
-------------
Block devices are registered at boot. They are identified by an integer.
Devices are stored as a table of N pointers to a set of functions pointers:

* OpenDevice - Mark an open flag to ensure exclusive access
* CloseDevice - Release flag
* CountBlocks - Returns number of blocks
* ReadBlock - Does what it says
* WriteBlock - Does what it says

There is a ```block``` command that can list registered devices and dump them.

FS types
--------
The only FS type used at the moment is a simplified version of RRSF, with
identifier 0.
Filesystems are stored as a table of N pointers to a set of function pointers:
* OpenFile
* CloseFile
* Seek
* Read
* Write
* OpenDir
* GetNextDirEntry
* DeleteCurEntry
* RenameCurEntry
* CloseDir

Volumes
-------
Volumes can be registered dynamically using the mount command to bind together a
block device and a filesystem type with some state.

Volumes are stored in a table that holds the following info:
* Pointer to block device functions
* Pointer to FS functions
* TODO storage for volume state (cur dir, etc)

There is a set of commands to interacting with files in mounted volumes.
* mount,umount
* cd,ls,cat,mv,rm

assembler
=========
The assembler is critical to sys11 self hosting. It requires storage for the
source and binary files, but its main binary is stored in the monitor ROM.

Position independent executable format
======================================
sys11 can load executables from storage, load them in RAM and execute them.
In such a system, it is not possible for these programs to assume anything about
the address at which they are loaded, or about the address at which program
variables must be stored.

Relocations
-----------
Such a program is linked at address zero. A zone of the program registers every
offset in the program that holds an actual jump address.
Relocation is a process that runs before starting the program, where each
registered jump address is fixed by adding the start address of the program.

Data relocation does not happen this way. For future plans where a RTOS will be
implemented, we need to support parallel execution of multiple copies of the
same program. Because of this, the data start address is stored in the X
register, and all accesses to global variables of the program must happen
relative to X.

Disk Format of executables
--------------------------
On the disk, a program has this fields, in order:
* a 16-bit magic value TBD
* a 16-bit size of the text/rodata section
* a 16-bit size of the bss section
* (There is no support for initialized data yet)
* a 16-bit relocation count
* For each relocation, 16-bit offset to be patched in the program
* N-bytes of program data

System calls made available to the user
=======================================
User programs can call the ROM using software IRQs.

Parameters are set in sp0..sp3, function is stored in A, and SWI is executed.

A  | Function  | Params                     | Retval in D
---+-----------+----------------------------+----------------
00 | open      | Pointer to path, mode      | File descriptor
01 | close     | File descriptor            | Status
02 | read      | File Descriptor, Dest, len | Status
03 | write     | File Descriptor, Src, len  | Status
04 | mount     | DevID, Path                | Status
05 | malloc    | Size                       | Pointer
06 | realloc   | Pointer, Size              | Pointer
07 | mfree     | Pointer                    | void
08 | run       | Pointer to path            | Return code
09 | exit      | code                       | (noreturn)
0A | sleep     | milliseconds               | void

Built in peripherals
--------------------
*SPI
*I2C

malloc/free
===========
The 32K RAM in 0100..7FFFh is managed as a heap.

An additional 8K may be available at 8000..9FFFh (total 40k).

External memory at A000h..BFFFh can be a ROM extension or even more RAM
(total 48k).

Actually a few bytes are used in this RAM for the kernel. For the moment this
includes the current command line for the shell.

Algorithm
---------

The heap is managed as a linked list of free blocks. a separate 16-bit variable
is required to hold the head of the free list. This can be the first two bytes
of the heap itself, or some 16-bit variable in page0.
Each block (allocated or free) has a 2-byte header indicating the size of this
block. Each free block has also a pointer to the next free block.
The linked list of free blocks is kept ordered by increasing address.

Initialization: Define a single large free block.

Allocation: Traverse the list of free blocks. If free block is big enough,
* split block by creating a new free block after the bytes that will be reserved
* Return pointer to block

Deallocation: Find prev,next pointers at offset -2 from beginning of block.
* Insert this block in the free list
* Coalesce the list of blocks, starting from the head, restarting if anything
happens.

Reallocation: (TODO)
* Find the free block that follows the allocated block
* If contiguous and big enough, grow the block into this free space, fix sizes
and pointers, done
* If not contiguous or not big enough: Attempt to find a new block, then free
the old one.

RTOS:
* Add a task ID field in the header so all allocations of a killed task can be
freed automatically


exec
----
* If program already loaded, increment refcount of program, else
* Allocate RAM for code/rodata segment
* Load program segment from filesystem to program RAM
* Read and apply relocations
* After program is loaded/found:
* Allocate RAM for bss/stack segment
* Zeroize bss segment
* Put address of global var segment in known place
* Start executing program segment (RTOS: create task)
* After execution, free data segment
* Decrement refcount of program
* If refcount is zero, free code segment


For the moment this is a simple execution but in the future we will support
multiple tasks with code segment sharing.

kernel init
-----------
* Register sci as char device 0
* Open device 0 as FD 0: can be used for text input/output
* Try to mount a FS in I2C or SPI bus
* If that fails, start an internal emergency shell
* Try to exec the shell process in file /shell


