#!/usr/bin/env python3
# This software uploads a 256-byte program to
# a 68HC11 chip in bootstrap mode.

import argparse
import serial
import os
import time
import struct

parser = argparse.ArgumentParser('68hc11 uploader')
parser.add_argument('binary',
                    help='binary to upload (256 bytes)')
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
parser.add_argument('--reset',
                    default='no',
                    choices=['no', 'rts'],
                    help='reset before upload (default %(default)s)')
parser.add_argument('--run',
                    nargs=argparse.REMAINDER,
                    default=None,
                    help='program to run after bootstrap is done')

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
    raise "Cannot determine baud rate from xtal clock"

if minerr < 5.0:
    baud = baud_real
else:
    baud = int(baud)
    print("WARNING this is not a standard baud rate")

# Try opening the program to be bootloaded
f = open(args["binary"], "rb")
if f == None:
    raise "Cannot open:"+args[binary]
f.seek(0, os.SEEK_END)
size = f.tell()
f.seek(0, os.SEEK_SET)

if size != 256:
    raise "File size must be 256 bytes"
binary=f.read()
f.close()

# Try to open the serial port
print("Opening port: ", args["port"], "at speed: ", baud)
ser = serial.Serial(args["port"], baud, timeout=1)

#Optionnally, reset the board
if args["reset"] == 'rts':
    print("Resetting board via RTS (todo)")
    ser.rts=False
    time.sleep(0.1)
    ser.rts=True
    time.sleep(0.1)
    ser.reset_input_buffer() #eat the BREAK that appears as zero byte


# All good. Send the initial FF byte

ser.reset_output_buffer()
ser.write(b'\xFF')

# Now send the 256 bytes of the program, while checking echos
print("Sending...")
ser.reset_output_buffer()
ser.reset_input_buffer()
n = 0
for x in binary:
    n = n + 1
    ser.write(struct.pack("B",x))
    ser.flush()
    echo = ser.read(1)
    if len(echo) == 0:
        print("timeout waiting for echo of byte",n)
        continue
    echo = ord(echo)
    if echo != x:
        print("echo error at byte",n,"expected",x,"received",echo)
ser.close()

print("All good. program is running.")

if args["run"] != None:
    print("Starting program (todo): ", args["run"][0])

