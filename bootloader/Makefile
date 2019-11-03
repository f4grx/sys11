stage2_OBJS=stage2.o
stage2_LKR=../link/bootstrap.ld

TARGETS=stage2

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast stage2.bin
run:
	../tools/upload.py --reset=rts --term --fast stage2.bin

