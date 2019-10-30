stage2_OBJS=stage2.o
stage2_LKR=bootstrap.ld

TARGETS=stage2

include ../make/program.mk

.PHONY: boot
boot:
	../tools/upload.py --reset=rts --fast stage2.bin

