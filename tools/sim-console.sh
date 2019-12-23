#!/usr/bin/sh
socat tcp:localhost:3334 stdio,raw,echo=0,escape=0x03
