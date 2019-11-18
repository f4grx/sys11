monitor_OBJS=main.o vectors.o softregs.o
monitor_OBJS+=sci.o
monitor_OBJS+=mman.o string.o

monitor_LKR=../link/full.ld

TARGETS=monitor

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 monitor.bin --term


