Installing toolchain
====================

The last version of GNU binutils still supports 68hc11

```
# wget https://ftpmirror.gnu.org/binutils/binutils-2.33.1.tar.xz
# tar Jzvf binutils-2.33.1.tar.xz
# cd binutils-2.33.1
# ./configure --prefix=... --target=m68hc11-elf
# make -j
```

upload.py
=========

This tool can be used to upload a program to a 68HC11 in bootstrap mode. The binary to upload must be 256 bytes exactly.

Note that if your bootstrap program reinitializes the uart too fast, the upload tool will not receive acknowledgement for the last uploaded byte.

This directory also contains a bootstrap program that can be used to upload more elaborate programs, including writing to external RAM/EEPROM/Flash memories.

It can also be used during board prototyping to verify the behaviour of bus lines, address decoding, etc.

```
usage: ./upload.py [-h] [--port PORT] [--xtal XTAL] [--fast]
                   [--reset {no,rts}] [--run ...]
                   binary

positional arguments:
  binary            binary to upload (256 bytes)

optional arguments:
  -h, --help        show this help message and exit
  --port PORT       serial port to use (default /dev/ttyUSB0)
  --xtal XTAL       HC11 XTAL frequency in MHz (default 8)
  --fast            Use the fast /16 baud rate instead of usual /104
  --reset {no,rts}  reset before upload (default no)
  --run ...         program to run after bootstrap is done
```

