#runtime
forth_OBJS=main.o vectors.o

forth_LKR=../link/monitor.ld

TARGETS=forth

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast --stage2 forth.bin --term

.PHONY: sim
sim:
	../sim/sim --bin 0xE000:forth.bin

