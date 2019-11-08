Monitor
=======

The monitor has several roles. It mainly provides routines to be used by user
programs. It also manages the RAM and storage devices. It is really a very
little kernel.

The functions of the kernel are triggered by the SWI instruction. A function
index has to be set in the A accumulator.

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

Application Binary Interface
----------------------------
* Pseudo register Z in iram 40h..41h
* Pseudo register T in iram 42h..43h
* Pseudo register U in iram 44h..45h
* Pseudo register V in iram 46h..47h

* Parameters in D,X,Z,T,U,V
* Y always contain pointer to BSS segment
* Return value in D
* Code has to be position independent
* Global variables for a program are relative to Y
* Local variables use RAM in zero page

System calls made available to the user
---------------------------------------
A  | Function  | Params                         | Retval
00 | open      | X=Pointer to path,B=mode       | B=File descriptor
01 | close     | B=File descriptor              | B=Status
02 | read      | B=File Descriptor,Z=Dest,T=len | B=Status
03 | write     | B=File Descriptor,Z=Src,T=len  | B=Status
04 | mount     | B=DevID,Z=Path                 | B=Status
05 | malloc    | X=Size                         | D=Pointer
06 | realloc   | X=Pointer, Z=Size              | D=Pointer
07 | mfree     | X=Pointer                      | void
08 | run       | X=Pointer to path              | B=Return code
09 | exit      | B=code                         | (noreturn)
0A | sleep     | X=milliseconds                 | void

Built in peripherals
--------------------
*SPI
*I2C

VFS
---
If a SPI/I2C EEPROM with a RRSF is available it is mounted in /
Else, nothing is mounted, we get an empty root dir where it is not possible to
create any file.
Registered Devices appear in the root as /dev-%d but they are not managed with
any inode.

malloc/free
-----------
The 32k RAM in 0100..7FFFh is managed as a heap.
Actually a few bytes are used in this RAM for the kernel.

open
----
* Try to find path: either device or file in mounted device
* Allocate an open file
* Call device functions with pointer to open file in D
* When opening a dir mode must be read only

close
-----
* Call close of device
* Deallocate file entry

read/write
----------
* check file access mode
* call the device function for the file
* When target is a directory, read behaves as readdir to enumerate childs
(dest buf has to be big enough to hold filename, rights, owner and size)

exec
----
* Load file from filesystem
* Allocate RAM for code/rodata segment
* Allocate RAM for bss/stack segment
* Zeroize bss segment
* Put address of global var segment in Y
* Copy program segment from filesystem to program RAM
* Start executing program segment
* After execution, free allocated segments.

For the moment this is a simple execution but in the future we will support
multiple tasks with code segment sharing.

kernel variables
----------------
* 4 char devices
* Struct for a device: 4 bytes name, then 4 pointers: open/close/read/write
* Used for SCI and UARTS on external bus

* 4 block devices
* Struct for a device: 4 bytes name, then 4 pointers: open/close/bread/bwrite
* Used for I2C and SPI

* 4 mount points
* Struct for mount point: 4 bytes mount point, 1 byte device

* 8 opened files
* Struct for file: 1 byte device, 1 byte mode, 2 bytes offset

kernel init
-----------
* Register console as device 0
* Open /cons device as file 0: can be used for text output
* Try to mount a FS in I2C or SPI bus
* If that fails, start an internal emergency shell
* Try to exec the shell process in file /shell


