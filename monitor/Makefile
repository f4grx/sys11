#runtime
monitor_OBJS=start.o vectors.o softregs.o
#drivers
monitor_OBJS+=sci.o
#lib
monitor_OBJS+=mman.o string.o

monitor_LKR=../link/full.ld

TARGETS=monitor

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 monitor.bin --term


