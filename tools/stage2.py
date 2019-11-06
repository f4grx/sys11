#!/usr/bin/env python3
#stage 2 boot loader for 68hc11
#The embedded counter part of this program is loaded by the bootstrap ROM
#and provides some way to upload a bigger program in an external RAM/EEPROM
#

import sys
import os
import time
import struct
import argparse
try:
  import serial
except Exception as e:
  print("python3-serial not installed")
  sys.exit(1)

parser = argparse.ArgumentParser(sys.argv[0])
parser.add_argument('--port',
                    default='/dev/ttyUSB0',
                    help='serial port to use (default %(default)s)')
parser.add_argument('--xtal',
                    default='8',
                    type=float,
                    help='HC11 XTAL frequency in MHz (default %(default)s)')
parser.add_argument('--fast',
                    action='store_true',
                    help='Use the fast /16 baud rate instead of usual /104')
parser.add_argument('binary',
                    help='binary to upload')

args = parser.parse_args()
args = vars(args)

# Compute the baud rate from the xtal clock

if args["fast"] :
    divider = 16
else:
    divider = 13 * 8

eclock    = ((args["xtal"] * 1000000) / 4)
baudclock = (eclock / divider)
baud      = baudclock / 16

bauds     = [50,75,110,134,150,200,300,600,1200,1800,2400,4800,9600,19200,38400,
             57600,115200]
minerr   = 1e9
baud_real = None
for b in bauds:
    err = 100 * (b - baud) / b
    if err < 0: err = -err
    if err < minerr:
        minerr = err
        baud_real = b

if baud_real == None:
    print("Cannot determine baud rate from xtal clock")
    sys.exit(1)

if minerr < 4.5: #max error tolerated in 8 bit mode, see 68HC11RM 6.1
    baud = baud_real
else:
    baud = int(baud)
    print("WARNING this is not a standard baud rate")

# Try opening the program to be bootloaded
f = open(args["binary"], "rb")
if f == None:
    print("Cannot open:",args[binary])
    sys.exit(1)

f.seek(0, os.SEEK_END)
size = f.tell()
f.seek(0, os.SEEK_SET)

binary=f.read()
f.close()

# Try to open the serial port
print("Opening port: ", args["port"], "at speed: ", baud)
try:
    ser = serial.Serial(args["port"], baud, timeout=1)
except serial.serialutil.SerialException as e:
    print("Cannot open serial port")
    sys.exit(1)

# Now upload the binary

