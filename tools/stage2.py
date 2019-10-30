#!/usr/bin/env python3
#stage 2 boot loader for 68hc11
#The embedded counter part of this program is loaded by the bootstrap ROM
#and provides some way to upload a bigger program in an external RAM/EEPROM
#

import argparse,serial,os,sys


