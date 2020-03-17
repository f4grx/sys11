#runtime
forth_OBJS=forth.o vectors.o

forth_LKR=../link/monitor.ld

TARGETS=forth

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 forth.bin --term=9600

.PHONY: sim
sim:
	../sim/sim --bin 0xE000,monitor.bin

