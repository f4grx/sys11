#runtime
monitor_OBJS=start.o vectors.o softregs.o

#drivers
monitor_OBJS+=serial.o spibus.o

#OS
#monitor_OBJS+=mm.o

#libs
monitor_OBJS+=stdio.o stdlib.o string.o

#apps for basic shell with modular apps
#monitor_OBJS+=shell.o
#monitor_OBJS+=as11.o
#monitor_OBJS+=device.o
#monitor_OBJS+=bdev_ram.o

#forth system
monitor_OBJS+=forth.o

monitor_LKR=../link/monitor.ld

TARGETS=monitor

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 monitor.bin --term

.PHONY: sim
sim:
	m68hc11-elf-run monitor.elf

