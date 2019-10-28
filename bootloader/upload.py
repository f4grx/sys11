#!/usr/bin/env python3
# This software uploads a 256-byte program to
# a 68HC11 chip in bootstrap mode.

import argparse
import serial
import os

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

args = parser.parse_args()
args = vars(args)

# Compute the baud rate from the xtal clock

eclock    = ((args["xtal"] * 1000000) / 4)
baudclock = ( eclock / (13*8))
baud      = baudclock / 16

bauds     = [50,75,110,134,150,200,300,600,1200,1800,2400,4800,9600,19200,38400,
             57600,115200]
mindist   = 16777216
baud_real = None
for b in bauds:
    dist = b - baud
    if dist < 0: dist = -dist
    if dist < mindist:
        mindist = dist
        baud_real = b
if baud_real == None:
    raise "Cannot determine baud rate from xtal clock"
baud = baud_real

# Try opening the program to be bootloaded
print("Opening bootware: ", args["binary"])

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

ser = serial.Serial(args["port"], baud,
                    bytesize=8, parity='N', stopbits=1, timeout=1)

# All good. Send the initial FF byte

ser.reset_output_buffer()
ser.write(b'\xff')

# Now send the 256 bytes of the program, while checking echos

n = -1
for x in binary:
  n = n + 1
  ser.reset_output_buffer()
  ser.reset_input_buffer()
  ser.write(x)
  echo = ser.read()
  if len(echo) == 0:
    print("timeout waiting for echo of byte",n)
    continue
  echo = ord(echo)
  if echo != x:
    print("echo error at byte",n,"expected",x,"received",echo)

ser.close()

print("All good. program is running.")

