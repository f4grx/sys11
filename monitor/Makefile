#runtime
monitor_OBJS=start.o vectors.o softregs.o
#drivers
monitor_OBJS+=serial.o spibus.o
#lib
#monitor_OBJS+=mm.o
monitor_OBJS+=stdlib.o string.o
#app
monitor_OBJS+=shell.o

monitor_LKR=../link/monitor.ld

TARGETS=monitor

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 monitor.bin --term

.PHONY: sim
sim:
	m68hc11-elf-run monitor.elf

