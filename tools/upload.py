#!/usr/bin/env python3
# This software uploads a 256-byte program to
# a 68HC11 chip in bootstrap mode.

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

#
################################################################################
#

def upload_srec(port, programdata):
    port.reset_output_buffer()
    port.reset_input_buffer()
    for l in programdata:
        print(l)
        port.write(l)
        ret=port.read(1)
        print("<%02X>" % ret[0])
        ret=port.read(1)
        print("<%02X>" % ret[0])
        ret=port.read(1)
        print("<%02X>" % ret[0])
        if l[1] == 48+0:
            ret = port.read(1)
            print(ret)
        elif l[1] == 48+1:
            ch = len(l) - 4
            ch = int(ch / 2)
            ch = ch - 4
            print(ch)
            for i in range(ch):
                ret = port.read(1)
                print("<%02X>" % ret[0])
            ret = port.read(1)
            print(ret)
        elif l[1] == 48+9:
            ret = port.read(1)
            print(ret)


#
################################################################################
#

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
parser.add_argument('--reset',
                    default='no',
                    choices=['no', 'rts'],
                    help='reset before upload (default %(default)s)')
parser.add_argument('--srec',
                    default=None,
                    help='program to upload using stage2-srec after bootstrap is done')
parser.add_argument('--term',
                    action='store_true',
                    help='Keep running in terminal mode after upload')
parser.add_argument('binary',
                    nargs='?',
                    default=None,
                    help='binary to upload, mandatory unless using stage2 (256 bytes)')

args = parser.parse_args()
args = vars(args)

#
# Compute the baud rate from the xtal clock
#

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

if minerr < 4.5: #maximum error tolerated in 8 bit mode according to the 68HC11RM 6.1
    baud = baud_real
else:
    baud = int(baud)
    print("WARNING this is not a standard baud rate")

#
# Determine the programs to be uploaded
#

if args["binary"] == None:
    #no binary given to program
    if args["srec"] != None:
        #use the well-known stage2 bootstrap binary
        args["binary"] = (os.path.dirname(os.path.realpath(__file__)))+"/stage2.bin"
    else:
        print("A bootstrap binary is required")
        sys.exit(1)

#
# Try opening the program to be bootloaded
#

f = open(args["binary"], "rb")
if f == None:
    print("Cannot open:",args["binary"])
    sys.exit(1)
f.seek(0, os.SEEK_END)
size = f.tell()
f.seek(0, os.SEEK_SET)
if size != 256:
    print("File size must be 256 bytes")
    sys.exit(1)
binary=f.read()
f.close()

if args["srec"] == None:
    srec=None
else:
    f = open(args["srec"], "rb")
    if f == None:
        print("Cannot open:",args["srec"])
        sys.exit(1)
    srec=f.readlines()
    f.close()

if srec != None:
    for l in srec:
        print(l)

# Try to open the serial port
print("Opening port: ", args["port"], "at speed: ", baud)
try:
    ser = serial.Serial(args["port"], baud, timeout=1)
except serial.serialutil.SerialException as e:
    print("Cannot open serial port")
    sys.exit(1)

#Optionnally, reset the board
if args["reset"] == 'rts':
    print("Resetting board via RTS")
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

print("All good. program is running.")

#
# The bootstrap program is the stage2 bootloader.
# Use that to upload a larger binary to ext mem.
#

if srec != None:
    print("Starting srec loading: ", args["srec"])
    #stage2 program is a s-record
    upload_srec(ser, srec)

#
# The user does not want to keep running in terminal mode
#

if args["term"] == None:
    ser.close()
    sys.exit(0)

#
# Start behaving as a serial terminal.
#

print("Serial terminal started")
while(True):
    x=ser.read(1)
    if len(x) == 0 : continue
    if x[0] < 0x20 and x[0] != 0x0D and x[0] != 0x0A:
        print("<%02X>" % x[0])
    else:
        print(chr(x[0]),end='')

ser.close()
print("Terminal stopped")

