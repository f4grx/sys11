monitor_OBJS=vectors.o main.o sci.o
monitor_LKR=../link/full.ld

TARGETS=monitor

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 monitor.bin --term


